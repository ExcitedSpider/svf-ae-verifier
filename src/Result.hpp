#pragma once

#include "AE/Core/IntervalValue.h"
#include "Graphs/ICFGNode.h"
#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace SVF {

enum class Verdict { Proved, Unproved };

struct SourceLocation {
    std::string file;
    unsigned line = 0;
    unsigned column = 0;
    std::string function;
};

inline SourceLocation parseSourceLocation(std::string_view text, std::string function)
{
    auto number = [text](std::string_view field) {
        unsigned value = 0;
        if (const auto pos = text.find(field); pos != text.npos)
            std::from_chars(text.data() + pos + field.size(), text.data() + text.size(), value);
        return value;
    };
    SourceLocation result{"", number("\"ln\": "), number("\"cl\": "), std::move(function)};
    constexpr std::string_view fileField = "\"fl\": \"";
    if (const auto pos = text.find(fileField); pos != text.npos)
    {
        const auto start = pos + fileField.size();
        const auto end = text.rfind('"');
        if (start <= end && end != text.npos)
            result.file = text.substr(start, end - start);
    }
    return result;
}

struct Evidence {
    std::string name;
    std::optional<IntervalValue> value;
};

struct Site {
    const ICFGNode* node;
    const SVFStmt* stmt;
    std::string kind;
};

struct Result {
    Site site;
    SourceLocation location;
    Verdict verdict = Verdict::Unproved;
    std::string reason;
    std::vector<Evidence> evidence;
};

} // namespace SVF
