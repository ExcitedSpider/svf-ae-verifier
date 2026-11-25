#pragma once

#include "GepLayout.hpp"

#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/PAG.h"
#include "Util/Options.h"
#include <optional>
#include <limits>

namespace SVF {

class StateView {
public:
    StateView(AbstractInterpretation& ae, const PAG& pag, const GepLayouts& layouts,
              std::optional<unsigned> wcharSize)
        : ae(ae), pag(pag), layouts(layouts), wideSize(wcharSize) {}

    std::optional<unsigned> wcharSize() const { return wideSize; }

    const GepLayout* gepLayout(const GepStmt* gep) const
    {
        const auto it = layouts.find(gep);
        return it == layouts.end() ? nullptr : &it->second;
    }

    bool reached(const ICFGNode* node) const
    {
        return node && ae.hasAbsState(node);
    }

    std::optional<IntervalValue> interval(const ValVar* var, const ICFGNode* node) const
    {
        const auto* value = lookup(var, node);
        if (value && value->isInterval() && !value->getInterval().isBottom())
            return value->getInterval();
        return std::nullopt;
    }

    std::optional<AddressValue> addresses(const ValVar* var, const ICFGNode* node) const
    {
        const auto* value = lookup(var, node);
        if (value && value->isAddr())
            return value->getAddrs();
        return std::nullopt;
    }

    const BaseObjVar* baseObject(NodeID id) const
    {
        return pag.isNullOrBlkObj(id) ? nullptr : pag.getBaseObject(id);
    }

    std::optional<IntervalValue> byteSize(const BaseObjVar* object) const
    {
        if (object && object->isConstantByteSize() && !object->isHeap())
        {
            const auto size = object->getByteSizeOfObj();
            const auto limit = Options::MaxFieldLimit();
            // SVF's cap makes an exact fit indistinguishable from a larger object.
            if (size >= limit)
                return IntervalValue(BoundedInt(limit), BoundedInt::plus_infinity());
            return IntervalValue(size);
        }
        if (!object || !object->isHeap())
            return std::nullopt;
        const AddrStmt* allocation = nullptr;
        for (const auto* edge : object->getOutEdges())
        {
            const auto* addr = SVFUtil::dyn_cast<AddrStmt>(edge);
            if (!addr || addr->getRHSVar() != object)
                continue;
            if (allocation)
                return std::nullopt;
            allocation = addr;
        }
        if (!allocation)
            return std::nullopt;
        const auto* call = SVFUtil::dyn_cast<CallICFGNode>(allocation->getICFGNode());
        if (!call || !call->getCalledFunction())
            return std::nullopt;
        const auto& name = call->getCalledFunction()->getName();
        const auto& operands = allocation->getArrSize();
        if (!((name == "malloc" && operands.size() == 1) ||
              (name == "calloc" && operands.size() == 2)))
            return std::nullopt;
        s64_t lower = 1;
        BoundedInt upper(1);
        for (const auto* operand : operands)
        {
            const auto* value = SVFUtil::dyn_cast<ValVar>(operand);
            const auto amount = interval(value, allocation->getICFGNode());
            if (!amount || amount->ub() < BoundedInt(0))
                return std::nullopt;
            const s64_t low = amount->lb().is_minus_infinity() ? 0 : std::max<s64_t>(0, amount->lb().getNumeral());
            if (__builtin_mul_overflow(lower, low, &lower))
                return std::nullopt;
            if (amount->ub().is_plus_infinity() || upper.is_plus_infinity())
                upper = BoundedInt::plus_infinity();
            else
            {
                s64_t high;
                if (__builtin_mul_overflow(upper.getNumeral(), amount->ub().getNumeral(), &high))
                    return std::nullopt;
                upper = BoundedInt(high);
            }
        }
        return IntervalValue(BoundedInt(lower), upper);
    }

    std::optional<NodeID> objectId(u32_t address, const ICFGNode* node) const
    {
        if (!reached(node) || !AbstractState::isVirtualMemAddress(address))
            return std::nullopt;
        const auto id = static_cast<const AbstractState&>(ae.getAbsState(node)).getIDFromAddr(address);
        if (!pag.hasGNode(id) || !SVFUtil::isa<ObjVar>(pag.getSVFVar(id)))
            return std::nullopt;
        return id;
    }

private:
    const AbstractValue* lookup(const ValVar* var, const ICFGNode* node) const
    {
        // The virtual ValVar overloads perform the sparse-mode def-site lookup.
        if (var && reached(node) && ae.hasAbsValue(var, node))
            return &ae.getAbsValue(var, node);
        return nullptr;
    }

    AbstractInterpretation& ae;
    const PAG& pag;
    const GepLayouts& layouts;
    std::optional<unsigned> wideSize;
};

// AE owns a process-wide singleton. Configure its mode before starting it.
inline StateView runAbstractExecution(const PAG& pag, const GepLayouts& layouts,
                                      std::optional<unsigned> wcharSize)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    ae.runOnModule();
    return StateView(ae, pag, layouts, wcharSize);
}

} // namespace SVF
