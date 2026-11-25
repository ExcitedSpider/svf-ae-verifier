#pragma once

#include "GepLayout.hpp"

#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/PAG.h"
#include <optional>

namespace SVF {

class StateView {
public:
    StateView(AbstractInterpretation& ae, const PAG& pag, const GepLayouts& layouts)
        : ae(ae), pag(pag), layouts(layouts) {}

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
            return IntervalValue(object->getByteSizeOfObj());
        return std::nullopt;
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
};

// AE owns a process-wide singleton. Configure its mode before starting it.
inline StateView runAbstractExecution(const PAG& pag, const GepLayouts& layouts)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    ae.runOnModule();
    return StateView(ae, pag, layouts);
}

} // namespace SVF
