#pragma once

#include "Util/GeneralType.h"
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <variant>
#include <vector>

namespace SVF {

class GepStmt;

struct SequentialGepStep {
    std::uint64_t stride;
    unsigned operandBits;
    std::optional<s64_t> constantIndex;
};

struct StructGepStep {
    std::uint64_t offset;
};

struct GepLayout {
    unsigned indexBits;
    bool inbounds;
    std::vector<std::variant<SequentialGepStep, StructGepStep>> steps;
};

using GepLayouts = std::unordered_map<const GepStmt*, GepLayout>;

struct TargetFacts {
    GepLayouts layouts;
    std::optional<unsigned> wcharSize;

    const GepLayout* gepLayout(const GepStmt* gep) const
    {
        const auto it = layouts.find(gep);
        return it == layouts.end() ? nullptr : &it->second;
    }
};

} // namespace SVF
