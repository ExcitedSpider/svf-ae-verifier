#pragma once

#include "../GepLayout.hpp"
#include "Graphs/PAG.h"
#include "SVF-LLVM/LLVMModule.h"
#include <llvm/IR/GetElementPtrTypeIterator.h>
#include <utility>

namespace SVF {

inline std::optional<GepLayout> readGepLayout(const GepStmt& gep, LLVMModuleSet& modules)
{
    if (!modules.hasLLVMValue(gep.getLHSVar()))
        return std::nullopt;
    const auto* op = llvm::dyn_cast<llvm::GEPOperator>(modules.getLLVMValue(gep.getLHSVar()));
    if (!op || op->getType()->isVectorTy() || !modules.hasValueNode(op->getPointerOperand())
        || modules.getValueNode(op->getPointerOperand()) != gep.getRHSVar()->getId())
        return std::nullopt;

    const auto& layout = modules.getMainLLVMModule()->getDataLayout();
    const auto bits = layout.getIndexSizeInBits(op->getPointerAddressSpace());
    const auto& steps = gep.getOffsetVarAndGepTypePairVec();
    if (bits == 0 || bits > 64 || steps.size() != op->getNumIndices())
        return std::nullopt;

    GepLayout result{bits, op->isInBounds(), {}};
    auto step = steps.begin();
    for (auto it = llvm::gep_type_begin(op); it != llvm::gep_type_end(op); ++it, ++step)
    {
        const auto* operand = it.getOperand();
        const auto* constant = llvm::dyn_cast<llvm::ConstantInt>(operand);
        if (auto* structure = it.getStructTypeOrNull())
        {
            if (!constant || !structure->isSized())
                return std::nullopt;
            const auto field = constant->getValue().getLimitedValue();
            if (field >= structure->getNumElements())
                return std::nullopt;
            const auto offset = layout.getStructLayout(structure)->getElementOffset(field);
            if (offset.isScalable())
                return std::nullopt;
            result.steps.emplace_back(StructGepStep{offset.getFixedValue()});
            continue;
        }
        if (it.isVector() || !it.getIndexedType()->isSized())
            return std::nullopt;
        const auto stride = it.getSequentialElementStride(layout);
        if (stride.isScalable())
            return std::nullopt;
        SequentialGepStep sequential{stride.getFixedValue(), operand->getType()->getIntegerBitWidth(),
                                     std::nullopt};
        if (constant)
            sequential.constantIndex = constant->getValue().sextOrTrunc(bits).getSExtValue();
        else if (!step->first || !modules.hasValueNode(operand)
                 || modules.getValueNode(operand) != step->first->getId())
            return std::nullopt;
        result.steps.emplace_back(sequential);
    }
    return result;
}

inline GepLayouts buildGepLayouts(PAG& pag)
{
    auto& modules = *LLVMModuleSet::getLLVMModuleSet();
    GepLayouts layouts;
    for (const auto* stmt : pag.getSVFStmtSet(SVFStmt::Gep))
    {
        const auto* gep = SVFUtil::cast<GepStmt>(stmt);
        if (auto layout = readGepLayout(*gep, modules))
            layouts.emplace(gep, std::move(*layout));
    }
    return layouts;
}

} // namespace SVF
