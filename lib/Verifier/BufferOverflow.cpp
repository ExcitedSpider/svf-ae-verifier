#include "Verifier/Verifier.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/PAG.h"

namespace SVF
{

static const AbstractValue* lookup(AbstractInterpretation& ae, const SVFVar* var,
                                   const ICFGNode* node)
{
    if (!node || !var || !ae.hasAbsValue(var, node))
        return nullptr;
    return &ae.getAbsValue(var, node);
}

static std::optional<IntervalValue> nonnegativeSize(AbstractInterpretation& ae,
        const SVFVar* var, const ICFGNode* node)
{
    const auto* value = lookup(ae, var, node);
    if (!value || !value->isInterval() || !value->getInterval().lb().geq(BoundedInt(0)))
        return std::nullopt;
    return value->getInterval();
}

static std::optional<IntervalValue> objectSize(AbstractInterpretation& ae,
        const BaseObjVar& object)
{
    const auto* def = object.getICFGNode();
    if (!def || !ae.hasAbsState(def))
        return std::nullopt;
    if (object.isHeap())
    {
        const auto* call = SVFUtil::dyn_cast<CallICFGNode>(def);
        if (!call || !call->getCalledFunction() || call->arg_empty())
            return std::nullopt;
        const auto& name = call->getCalledFunction()->getName();
        if (name == "malloc")
            return nonnegativeSize(ae, call->getArgument(0), def);
        if (name == "realloc" && call->arg_size() >= 2)
            return nonnegativeSize(ae, call->getArgument(1), def);
        if (name == "calloc" && call->arg_size() >= 2)
        {
            const auto count = nonnegativeSize(ae, call->getArgument(0), def);
            const auto width = nonnegativeSize(ae, call->getArgument(1), def);
            if (count && width)
                return *count * *width;
        }
        return std::nullopt;
    }
    for (const auto* stmt : def->getSVFStmts())
    {
        const auto* addr = SVFUtil::dyn_cast<AddrStmt>(stmt);
        if (!addr || addr->getRHSVar() != &object)
            continue;
        if (object.isConstantByteSize())
        {
            const auto size = ae.getAllocaInstByteSize(addr);
            return IntervalValue(size.lowerBound, size.upperBound);
        }
        // Dynamic alloca records an element count, not a byte extent.
        if (object.isStack() && addr->getArrSize().size() == 1 && object.getType() &&
                object.getType()->getByteSize())
        {
            const auto count = nonnegativeSize(ae, addr->getArrSize().front(), def);
            if (count)
                return *count * IntervalValue(object.getType()->getByteSize());
        }
    }
    return std::nullopt;
}

static bool hasUniqueDefinition(const ValVar& pointer, SVFStmt::PEDGEK kind)
{
    return pointer.getInEdges().size() == 1 && pointer.hasIncomingEdges(kind);
}

static bool isBaseAddress(const ValVar& pointer, const BaseObjVar& object)
{
    // A collapsed object can also represent interior pointers.
    if (hasUniqueDefinition(pointer, SVFStmt::Addr))
        return SVFUtil::cast<AddrStmt>(*pointer.getIncomingEdgesBegin(SVFStmt::Addr))
               ->getRHSVar() == &object;
    return !object.isFieldInsensitive() && !Options::FirstFieldEqBase();
}

static std::optional<IntervalValue> directGepOffset(AbstractInterpretation& ae,
        const ValVar& pointer, const PAG& pag)
{
    if (!hasUniqueDefinition(pointer, SVFStmt::Gep))
        return std::nullopt;
    const auto* gep = SVFUtil::cast<GepStmt>(*pointer.getIncomingEdgesBegin(SVFStmt::Gep));
    const auto* def = gep->getICFGNode();
    if (!def || !ae.hasAbsState(def))
        return std::nullopt;
    const auto* source = lookup(ae, gep->getRHSVar(), def);
    if (!source || !source->isAddr())
        return std::nullopt;
    for (const auto addr : source->getAddrs())
    {
        if (!AbstractState::isVirtualMemAddress(addr))
            return std::nullopt;
        const auto id = ae.getAbsState(def).getIDFromAddr(addr);
        if (pag.isNullOrBlkObj(id) || !pag.hasGNode(id))
            return std::nullopt;
        const auto* base = SVFUtil::dyn_cast<BaseObjVar>(pag.getSVFVar(id));
        if (!base || !isBaseAddress(*gep->getRHSVar(), *base))
            return std::nullopt;
    }
    if (!gep->isConstantOffset())
    {
        for (const auto& entry : gep->getOffsetVarAndGepTypePairVec())
        {
            if (SVFUtil::isa<ConstIntValVar>(entry.first))
                continue;
            const auto* index = lookup(ae, entry.first, def);
            if (!index || !index->isInterval())
                return std::nullopt;
        }
    }
    return ae.getGepByteOffset(gep);
}

static void assessAccess(SiteResult& site, const ICFGNode& node, const ValVar* pointer,
                         AbstractInterpretation& ae)
{
    site.verdict = "unknown";
    site.hasState = ae.hasAbsState(&node);
    if (!site.hasState)
    {
        site.reason = "ae-no-state";
        return;
    }
    site.reason = "ae-object-unknown";
    const auto* value = lookup(ae, pointer, &node);
    if (!value || !value->isAddr())
        return;

    const auto& pag = *PAG::getPAG();
    const auto direct = directGepOffset(ae, *pointer, pag);
    bool missing = false;
    bool allIn = true;
    bool anyOut = false;
    for (const auto addr : value->getAddrs())
    {
        if (!AbstractState::isVirtualMemAddress(addr))
        {
            missing = true;
            continue;
        }
        const auto id = ae.getAbsState(&node).getIDFromAddr(addr);
        ObjectBounds target{id, std::nullopt, std::nullopt};
        if (!pag.isNullOrBlkObj(id) && pag.hasGNode(id))
        {
            if (const auto* base = pag.getBaseObject(id))
            {
                target.object = base->getId();
                target.size = objectSize(ae, *base);
                target.offset = direct;
                if (!pointer->hasIncomingEdges(SVFStmt::Gep) &&
                        id == base->getId() && isBaseAddress(*pointer, *base))
                    target.offset = IntervalValue(0);
            }
        }
        site.objects.push_back(target);
        if (!target.offset || !target.size || target.offset->isBottom())
        {
            missing = true;
            continue;
        }
        if (!site.accessSize)
        {
            allIn = false;
            continue;
        }
        const auto& offset = *target.offset;
        const auto& size = *target.size;
        const auto& width = *site.accessSize;
        const BoundedInt zero(0);
        allIn &= offset.lb().geq(zero) && !offset.ub().is_infinity() &&
                 !width.ub().is_infinity() && !size.lb().is_infinity() &&
                 offset.ub().leq(size.lb()) && width.ub().leq(size.lb() - offset.ub());
        anyOut |= width.lb() > zero &&
                  ((!size.ub().is_infinity() && offset.lb().geq(size.ub())) ||
                   (!width.ub().is_infinity() && offset.ub().leq(zero - width.ub())) ||
                   size.ub() == zero);
    }
    if (anyOut)
        site.reason = "ae-out-of-bounds-if-reached";
    else if (!missing && !site.objects.empty())
    {
        site.verdict = allIn ? "safe" : "unknown";
        site.reason = allIn ? "ae-in-bounds" : "ae-bounds-unknown";
    }
}

void Verifier::checkAccesses(const ICFGNode& node, AbstractInterpretation& ae,
                             std::vector<SiteResult>& results)
{
    for (const auto* stmt : node.getSVFStmts())
    {
        if (SVFUtil::isa<CallICFGNode>(&node) || SVFUtil::isa<RetICFGNode>(&node))
            break;
        SiteResult site;
        const ValVar* pointer;
        const SVFType* type;
        if (const auto* load = SVFUtil::dyn_cast<LoadStmt>(stmt))
        {
            site.kind = "load";
            pointer = load->getRHSVar();
            type = load->getLHSVar()->getType();
        }
        else if (const auto* store = SVFUtil::dyn_cast<StoreStmt>(stmt))
        {
            site.kind = "store";
            pointer = store->getLHSVar();
            type = store->getRHSVar()->getType();
        }
        else
            continue;
        site.statement = stmt->getEdgeID();
        if (type && type->getByteSize())
            site.accessSize = IntervalValue(type->getByteSize());
        setLocation(node, site);
        assessAccess(site, node, pointer, ae);
        results.push_back(std::move(site));
    }

    const auto* call = SVFUtil::dyn_cast<CallICFGNode>(&node);
    if (!call || !call->getCalledFunction())
        return;
    const auto& name = call->getCalledFunction()->getName();
    if (name != "SAFE_BUFACCESS" && name != "UNSAFE_BUFACCESS")
        return;
    SiteResult site;
    site.kind = "oracle";
    site.expected = name == "SAFE_BUFACCESS" ? "safe" : "unsafe";
    setLocation(node, site);
    if (call->arg_size() >= 2 && ae.hasAbsState(&node))
        site.accessSize = nonnegativeSize(ae, call->getArgument(1), &node);
    assessAccess(site, node, call->arg_empty() ? nullptr : call->getArgument(0), ae);
    results.push_back(std::move(site));
}

} // namespace SVF
