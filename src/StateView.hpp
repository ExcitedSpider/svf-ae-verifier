#pragma once

#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/PAG.h"
#include <optional>

namespace SVF {

class StateView {
public:
    explicit StateView(AbstractInterpretation& ae, const PAG& pag) : ae(ae), pag(pag) {}

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

    const BaseObjVar* baseObject(u32_t address, const ICFGNode* node) const
    {
        const auto id = objectId(address, node);
        return id ? pag.getBaseObject(*id) : nullptr;
    }

    std::optional<IntervalValue> byteSize(const BaseObjVar* object) const
    {
        if (object && object->isConstantByteSize() && !object->isHeap())
            return IntervalValue(object->getByteSizeOfObj());
        return std::nullopt;
    }

    std::optional<IntervalValue> byteOffset(const ValVar* pointer, u32_t address,
                                           const ICFGNode* node) const
    {
        const auto id = objectId(address, node);
        if (!id)
            return std::nullopt;
        const auto* defStmt = definition(pointer);
        if (directObject(defStmt) == pag.getSVFVar(*id))
            return IntervalValue(0);
        const auto* gep = defStmt ? SVFUtil::dyn_cast<GepStmt>(defStmt) : nullptr;
        if (!gep)
            return std::nullopt;

        const auto* base = directObject(definition(gep->getRHSVar()));
        const auto* def = gep->getICFGNode();
        if (!base || base != pag.getBaseObject(*id) || !reached(def) || !gep->isConstantOffset())
            return std::nullopt;
        const auto source = addresses(gep->getRHSVar(), def);
        if (!source || source->size() != 1 ||
                objectId(*source->begin(), def) != base->getId())
            return std::nullopt;
        for (const auto& step : gep->getOffsetVarAndGepTypePairVec())
            if (!step.second || !SVFUtil::isa<SVFPointerType, SVFArrayType>(step.second))
                return std::nullopt;
        // Dynamic offsets may lose negative ranges. Even the constant helper
        // saturates at the field limit and does not account for struct padding.
        const auto offset = ae.getGepByteOffset(gep);
        if (offset.ub().geq(BoundedInt(Options::MaxFieldLimit())))
            return std::nullopt;
        return offset;
    }

private:
    const AbstractValue* lookup(const ValVar* var, const ICFGNode* node) const
    {
        // The virtual ValVar overloads perform the sparse-mode def-site lookup.
        if (var && reached(node) && ae.hasAbsValue(var, node))
            return &ae.getAbsValue(var, node);
        return nullptr;
    }

    std::optional<NodeID> objectId(u32_t address, const ICFGNode* node) const
    {
        if (!reached(node) || !AbstractState::isVirtualMemAddress(address))
            return std::nullopt;
        const auto id = static_cast<const AbstractState&>(ae.getAbsState(node)).getIDFromAddr(address);
        if (!pag.hasGNode(id) || pag.isNullOrBlkObj(id) ||
                !SVFUtil::isa<ObjVar>(pag.getSVFVar(id)))
            return std::nullopt;
        return id;
    }

    static const SVFStmt* definition(const ValVar* pointer)
    {
        const SVFStmt* result = nullptr;
        for (const auto* edge : pointer->getInEdges())
        {
            // A store's destination edge uses the pointer as an address.
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

    AbstractInterpretation& ae;
    const PAG& pag;
};

// AE owns a process-wide singleton. Configure its mode before starting it.
inline StateView runAbstractExecution(const PAG& pag)
{
    auto& ae = AbstractInterpretation::getAEInstance();
    ae.runOnModule();
    return StateView(ae, pag);
}

} // namespace SVF
