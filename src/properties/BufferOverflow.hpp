#pragma once

#include "../Property.hpp"

namespace SVF {

// Layout information for this SVF statement is obtained at the LLVM boundary.
std::optional<IntervalValue> gepByteOffset(const GepStmt& gep, const StateView& state);

class BufferOverflow final : public Property {
public:
    std::string_view name() const override { return "overflow"; }

    std::vector<Site> sites(const ICFG& icfg) const override
    {
        std::vector<Site> result;
        for (const auto& [id, node] : icfg)
        {
            // Call and return nodes can contain modeled effects of external functions.
            if (SVFUtil::isa<CallICFGNode>(node) || SVFUtil::isa<RetICFGNode>(node))
                continue;
            for (const auto* stmt : node->getSVFStmts())
            {
                if (SVFUtil::isa<LoadStmt>(stmt))
                    result.push_back({node, stmt, "load"});
                else if (SVFUtil::isa<StoreStmt>(stmt))
                    result.push_back({node, stmt, "store"});
            }
        }
        return result;
    }

    Result check(const Site& site, const StateView& state) const override
    {
        Result result{site, {}, Verdict::Unproved, "ae-no-state", {}};
        if (!state.reached(site.node))
            return result;

        const ValVar* pointer;
        const SVFType* type;
        if (const auto* load = SVFUtil::dyn_cast<LoadStmt>(site.stmt))
        {
            pointer = load->getRHSVar();
            type = load->getLHSVar()->getType();
        }
        else
        {
            const auto* store = SVFUtil::cast<StoreStmt>(site.stmt);
            pointer = store->getLHSVar();
            type = store->getRHSVar()->getType();
        }
        const auto width = type ? type->getByteSize() : 0;
        result.evidence.push_back({"width", width ? std::optional<IntervalValue>(width) : std::nullopt});
        const auto targets = state.addresses(pointer, site.node);
        result.reason = "ae-object-unknown";
        if (!targets || targets->empty())
            return result;

        bool missing = false;
        bool hasObject = false;
        bool allIn = true;
        bool someOut = false;
        for (const auto address : *targets)
        {
            const auto id = state.objectId(address, site.node);
            if (!id)
            {
                missing = true;
                continue;
            }
            const auto* base = state.baseObject(*id);
            hasObject |= base != nullptr;
            const auto offset = base ? byteOffset(pointer, *id, *base, state) : std::nullopt;
            const auto size = state.byteSize(base);
            const auto target = std::to_string(*id);
            result.evidence.push_back({"offset." + target, offset});
            result.evidence.push_back({"size." + target, size});
            if (!offset || !size || !width)
            {
                missing = true;
                continue;
            }
            const BoundedInt zero(0);
            const BoundedInt lastStart(size->lb().getNumeral() - static_cast<s64_t>(width));
            allIn &= offset->lb().geq(zero) && offset->ub().leq(lastStart);
            someOut |= offset->ub() < zero || offset->lb() > lastStart;
        }
        if (!hasObject)
            return result;
        if (someOut)
            result.reason = "ae-out-of-bounds-if-reached";
        else if (!missing && allIn)
        {
            result.verdict = Verdict::Proved;
            result.reason = "ae-in-bounds";
        }
        else
            result.reason = "ae-bounds-unknown";
        return result;
    }

private:
    static const SVFStmt* definition(const ValVar* pointer)
    {
        const SVFStmt* result = nullptr;
        for (const auto* edge : pointer->getInEdges())
        {
            // A store uses the pointer as an address.
            if (SVFUtil::isa<StoreStmt>(edge))
                continue;
            if (result)
                return nullptr;
            result = edge;
        }
        return result;
    }

    static const BaseObjVar* directObject(const SVFStmt* def)
    {
        const auto* addr = def ? SVFUtil::dyn_cast<AddrStmt>(def) : nullptr;
        return addr ? SVFUtil::dyn_cast<BaseObjVar>(addr->getRHSVar()) : nullptr;
    }

    static std::optional<IntervalValue> byteOffset(const ValVar* pointer, NodeID target,
                                                  const BaseObjVar& base, const StateView& state)
    {
        const auto* def = definition(pointer);
        if (target == base.getId() && directObject(def) == &base)
            return IntervalValue(0);
        const auto* gep = def ? SVFUtil::dyn_cast<GepStmt>(def) : nullptr;
        if (!gep || directObject(definition(gep->getRHSVar())) != &base)
            return std::nullopt;
        const auto* node = gep->getICFGNode();
        const auto source = state.addresses(gep->getRHSVar(), node);
        if (!source || source->size() != 1 || state.objectId(*source->begin(), node) != base.getId())
            return std::nullopt;
        return gepByteOffset(*gep, state);
    }
};

} // namespace SVF
