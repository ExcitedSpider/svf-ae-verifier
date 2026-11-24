#pragma once

#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/PAG.h"
#include <optional>

namespace SVF {

class StateView {
public:
    explicit StateView(AbstractInterpretation& ae, const PAG& pag) : ae(ae), pag(pag) {}

    // AE owns a process-wide singleton. Set the mode before starting it.
    static StateView analyze(const PAG& pag)
    {
        auto& ae = AbstractInterpretation::getAEInstance();
        ae.runOnModule();
        return StateView(ae, pag);
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
        if (!id || pointer->getInEdges().size() != 1)
            return std::nullopt;
        if (directObject(pointer) == pag.getSVFVar(*id))
            return IntervalValue(0);
        if (!pointer->hasIncomingEdges(SVFStmt::Gep))
            return std::nullopt;

        const auto* gep = SVFUtil::cast<GepStmt>(*pointer->getIncomingEdgesBegin(SVFStmt::Gep));
        const auto* base = directObject(gep->getRHSVar());
        const auto* def = gep->getICFGNode();
        if (!base || base != pag.getBaseObject(*id) || !reached(def) || !gep->isConstantOffset())
            return std::nullopt;
        const auto source = addresses(gep->getRHSVar(), def);
        if (!source || source->size() != 1 ||
                objectId(*source->begin(), def) != base->getId())
            return std::nullopt;
        // Dynamic offsets may be clamped or have their negative range dropped.
        return ae.getGepByteOffset(gep);
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

    static const BaseObjVar* directObject(const ValVar* pointer)
    {
        if (pointer->getInEdges().size() != 1 || !pointer->hasIncomingEdges(SVFStmt::Addr))
            return nullptr;
        const auto* addr = SVFUtil::cast<AddrStmt>(*pointer->getIncomingEdgesBegin(SVFStmt::Addr));
        return SVFUtil::dyn_cast<BaseObjVar>(addr->getRHSVar());
    }

    AbstractInterpretation& ae;
    const PAG& pag;
};

} // namespace SVF
