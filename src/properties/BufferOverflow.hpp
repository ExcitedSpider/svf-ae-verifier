#pragma once

#include "../Property.hpp"
#include "Util/ExtAPI.h"
#include <algorithm>
#include <limits>
#include <set>

namespace SVF {

class BufferOverflow final : public Property {
public:
    std::string_view name() const override { return "overflow"; }

    std::vector<Site> sites(const ICFG& icfg) const override
    {
        std::vector<Site> result;
        for (const auto& [id, node] : icfg)
        {
            if (const auto* call = SVFUtil::dyn_cast<CallICFGNode>(node))
            {
                if (const auto rule = memoryCall(*call))
                {
                    const auto& callee = call->getCalledFunction()->getName();
                    result.push_back({node, nullptr, "call-write", std::nullopt, callee});
                    if (rule->source)
                        result.push_back({node, nullptr, "call-read", std::nullopt, callee});
                }
                continue;
            }
            // Synthetic call and return effects are not individual access sites.
            if (SVFUtil::isa<RetICFGNode>(node))
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

    Result check(const Site& site, const StateView& state, const TargetFacts& facts) const override
    {
        Result result{site, {}, Verdict::Unproved, "ae-no-state", {}};
        if (!state.reached(site.node))
            return result;

        const auto* call = SVFUtil::dyn_cast<CallICFGNode>(site.node);
        const ValVar* pointer = nullptr;
        std::optional<IntervalValue> length;
        if (call)
        {
            const auto rule = *memoryCall(*call);
            const bool read = site.kind == "call-read";
            const auto position = read ? *rule.source : rule.destination;
            if (position < call->arg_size())
                pointer = call->getArgument(position);
            if (rule.count && *rule.count < call->arg_size())
            {
                length = state.interval(call->getArgument(*rule.count), call);
                if (length && (length->lb().is_infinity() || length->lb() < BoundedInt(0)))
                    length.reset();
                if (length && call->getCalledFunction()->getName() == "wmemset")
                {
                    const auto width = facts.wcharSize;
                    if (!width || (!length->ub().is_infinity() &&
                        length->ub().getNumeral() > std::numeric_limits<s64_t>::max() / *width))
                        length.reset();
                    else
                    {
                        const auto lower = BoundedInt(length->lb().getNumeral() * *width);
                        const auto upper = length->ub().is_infinity() ? BoundedInt::plus_infinity()
                            : BoundedInt(length->ub().getNumeral() * *width);
                        length = IntervalValue(lower, upper);
                    }
                }
                if (length && (rule.maximumOnly || (read && rule.sourceMaximum)))
                    length = IntervalValue(BoundedInt(0), length->ub());
            }
        }
        else
        {
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
            if (const auto width = type ? type->getByteSize() : 0)
                length.emplace(width);
        }
        result.evidence.push_back({call ? "length" : "width", length});
        const auto targets = state.addresses(pointer, site.node);
        const bool unknownLength = call && (!length || length->ub().is_infinity());
        result.reason = unknownLength ? "ae-length-unknown" : "ae-object-unknown";
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
            const auto offset = base ? byteOffset(pointer, *id, *base, state, facts) : std::nullopt;
            const auto size = state.byteSize(base);
            const auto target = std::to_string(*id);
            result.evidence.push_back({"offset." + target, offset});
            result.evidence.push_back({"size." + target, size});
            if (!offset || !size || !length)
            {
                missing = true;
                continue;
            }
            const BoundedInt zero(0);
            const auto lastStart = [](const BoundedInt& bound, const BoundedInt& bytes) {
                return BoundedInt(bound.getNumeral() - bytes.getNumeral());
            };
            allIn &= !length->ub().is_infinity() && offset->lb().geq(zero)
                     && offset->ub().leq(lastStart(size->lb(), length->ub()));
            someOut |= length->lb() > zero
                       && (offset->ub() < zero
                           || (!size->ub().is_infinity()
                               && offset->lb() > lastStart(size->ub(), length->lb())));
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
            result.reason = unknownLength ? "ae-length-unknown" : "ae-bounds-unknown";
        return result;
    }

private:
    struct CallRule {
        unsigned destination;
        std::optional<unsigned> source;
        std::optional<unsigned> count;
        bool maximumOnly = false;
        bool sourceMaximum = false;
    };

    static std::optional<CallRule> memoryCall(const CallICFGNode& call)
    {
        const auto* callee = call.getCalledFunction();
        if (!callee)
            return std::nullopt;
        auto* ext = ExtAPI::getExtAPI();
        // The predicate reads the annotation map without inserting missing entries.
        const auto annotated = [ext, callee](const char* family) {
            return ext->hasExtFuncAnnotation(callee, family);
        };
        const auto& name = callee->getName();
        if (annotated("STRCPY") || annotated("STRCAT"))
            return CallRule{0, 1, std::nullopt};
        if (annotated("MEMSET"))
            return CallRule{0, std::nullopt, 2,
                            name == "__memset_chk"};
        if (!annotated("MEMCPY"))
            return std::nullopt;
        if (name == "bcopy")
            return CallRule{1, 0, 2};
        if (name == "memccpy")
            return CallRule{0, 1, 3, true};
        if (name == "iconv")
            return CallRule{3, 1, std::nullopt};
        return CallRule{0, 1, 2, name == "__memcpy_chk" || name == "__memmove_chk", name == "strncpy"};
    }

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

    static std::optional<IntervalValue> gepByteOffset(const GepStmt& gep, const StateView& state, const TargetFacts& facts)
    {
        const auto* layout = facts.gepLayout(&gep);
        if (!layout)
            return std::nullopt;
        // Use wide intermediates and reject finite arithmetic that could wrap
        // at the target's pointer index width.
        const auto limit = static_cast<__int128>(1) << (layout->indexBits - 1);
        const auto accumulate = [limit](BoundedInt& sum, const BoundedInt& index, std::uint64_t stride) {
            if (!stride || sum.is_infinity())
                return true;
            if (index.is_infinity())
            {
                sum = index;
                return true;
            }
            const auto term = static_cast<__int128>(index.getNumeral()) * stride;
            const auto next = sum.getNumeral() + term;
            if (term < -limit || term >= limit || next < -limit || next >= limit)
                return false;
            sum = BoundedInt(static_cast<s64_t>(next));
            return true;
        };

        const auto& steps = gep.getOffsetVarAndGepTypePairVec();
        auto step = steps.begin();
        BoundedInt lower(0), upper(0);
        for (const auto& fact : layout->steps)
        {
            IntervalValue index(1);
            std::uint64_t stride;
            if (const auto* field = std::get_if<StructGepStep>(&fact))
                stride = field->offset;
            else
            {
                const auto& sequential = std::get<SequentialGepStep>(fact);
                stride = sequential.stride;
                if (sequential.constantIndex)
                    index = IntervalValue(*sequential.constantIndex);
                else
                {
                    const auto value = state.interval(step->first, gep.getICFGNode());
                    if (!value)
                        return std::nullopt;
                    index = *value;
                    const auto bits = std::min(layout->indexBits, sequential.operandBits);
                    const auto indexLimit = static_cast<__int128>(1) << (bits - 1);
                    if ((!index.lb().is_infinity() && index.lb().getNumeral() < -indexLimit)
                        || (!index.ub().is_infinity() && index.ub().getNumeral() >= indexLimit))
                        return std::nullopt;
                    if ((!layout->inbounds || bits < sequential.operandBits)
                        && (index.lb().is_infinity() || index.ub().is_infinity()))
                        return std::nullopt;
                }
            }
            if (!accumulate(lower, index.lb(), stride) || !accumulate(upper, index.ub(), stride))
                return std::nullopt;
            ++step;
        }
        return IntervalValue(lower, upper);
    }

    struct OffsetResult {
        bool known;
        std::optional<IntervalValue> offset;
    };

    static OffsetResult resolveOffset(const ValVar* pointer, const BaseObjVar& base,
                                      const StateView& state, const TargetFacts& facts, std::set<const ValVar*>& visited,
                                      bool encounteredGep = false)
    {
        if (!pointer)
            return {false, std::nullopt};
        if (SVFUtil::isa<ConstNullPtrValVar>(pointer))
            return {true, std::nullopt};
        if (!visited.insert(pointer).second)
            return {false, std::nullopt};
        const auto* def = definition(pointer);
        if (const auto* object = directObject(def))
        {
            visited.erase(pointer);
            return {true, object == &base ? std::optional{IntervalValue(0)} : std::nullopt};
        }
        if (const auto* copy = def ? SVFUtil::dyn_cast<CopyStmt>(def) : nullptr)
        {
            if (copy->getCopyKind() != CopyStmt::COPYVAL && copy->getCopyKind() != CopyStmt::BITCAST)
                return {false, std::nullopt};
            auto result = resolveOffset(copy->getRHSVar(), base, state, facts, visited, encounteredGep);
            visited.erase(pointer);
            return result;
        }
        if (const auto* phi = def ? SVFUtil::dyn_cast<PhiStmt>(def) : nullptr)
        {
            std::optional<IntervalValue> joined;
            for (const auto* input : phi->getOpndVars())
            {
                const auto result = resolveOffset(input, base, state, facts, visited, encounteredGep);
                if (!result.known)
                    return {false, std::nullopt};
                if (result.offset)
                {
                    if (joined)
                        joined->join_with(*result.offset);
                    else
                        joined = result.offset;
                }
            }
            visited.erase(pointer);
            return {true, joined};
        }
        const auto* gep = def ? SVFUtil::dyn_cast<GepStmt>(def) : nullptr;
        if (!gep)
            return {false, std::nullopt};
        if (!directObject(definition(gep->getRHSVar())))
        {
            if (encounteredGep)
                return {false, std::nullopt};
            const auto parent = resolveOffset(gep->getRHSVar(), base, state, facts, visited, true);
            const auto offset = gepByteOffset(*gep, state, facts);
            if (!parent.known || !parent.offset || !offset || !parent.offset->is_zero())
                return {false, std::nullopt};
            visited.erase(pointer);
            return {true, offset};
        }
        if (directObject(definition(gep->getRHSVar())) != &base)
        {
            visited.erase(pointer);
            return {true, std::nullopt};
        }
        const auto* node = gep->getICFGNode();
        const auto source = state.addresses(gep->getRHSVar(), node);
        if (!source || source->size() != 1 || state.objectId(*source->begin(), node) != base.getId())
            return {false, std::nullopt};
        const auto offset = gepByteOffset(*gep, state, facts);
        visited.erase(pointer);
        return {offset.has_value(), offset};
    }

    static std::optional<IntervalValue> byteOffset(const ValVar* pointer, NodeID target,
                                                  const BaseObjVar& base, const StateView& state, const TargetFacts& facts)
    {
        (void)target;
        std::set<const ValVar*> visited;
        return resolveOffset(pointer, base, state, facts, visited).offset;
    }
};

} // namespace SVF
