#include "Verifier/Verifier.h"
#include "Graphs/ICFG.h"
#include <charconv>
#include <string_view>

namespace SVF
{

static unsigned locationNumber(std::string_view location, std::string_view field)
{
    const auto start = location.find(field);
    if (start == std::string_view::npos)
        return 0;
    unsigned value = 0;
    std::from_chars(location.data() + start + field.size(),
                    location.data() + location.size(), value);
    return value;
}

void Verifier::setLocation(const CallICFGNode& call, SiteResult& result)
{
    // SVF stores the source location as a string containing ln, cl and fl.
    const auto location = call.getSourceLoc();
    result.line = locationNumber(location, "\"ln\": ");
    result.column = locationNumber(location, "\"cl\": ");
    const std::string_view fileField = "\"fl\": \"";
    const auto start = location.find(fileField);
    if (start != std::string::npos)
    {
        const auto filename = start + fileField.size();
        const auto end = location.rfind('"');
        if (end >= filename)
            result.file = location.substr(filename, end - filename);
    }
    result.function = call.getCaller()->getName();
}

std::vector<SiteResult> Verifier::collect(const ICFG& icfg, AbstractInterpretation& ae)
{
    std::vector<SiteResult> results;
    for (const auto& entry : icfg)
    {
        if (const auto* call = SVFUtil::dyn_cast<CallICFGNode>(entry.second))
        {
            if (auto result = check(*call, ae))
                results.push_back(std::move(*result));
        }
    }
    return results;
}

} // namespace SVF
