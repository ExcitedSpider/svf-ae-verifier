#include "Verifier/Verifier.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "Graphs/ICFGNode.h"

namespace SVF
{

static std::optional<IntervalValue> interval(AbstractInterpretation& ae,
        const CallICFGNode& call, unsigned index)
{
    if (index >= call.arg_size())
        return std::nullopt;
    const auto* arg = call.getArgument(index);
    // Virtual ValVar lookup resolves the defining node in both sparse modes.
    if (!ae.hasAbsValue(arg, &call))
        return std::nullopt;
    const auto& value = ae.getAbsValue(arg, &call);
    if (!value.isInterval())
        return std::nullopt;
    return value.getInterval();
}

static bool isFiniteSingleton(const IntervalValue& range)
{
    return range.is_numeral() && !range.is_infinite();
}

static const char* verdictReason(const SiteResult& result)
{
    if (!result.hasState)
        return "ae-no-state";
    if (result.kind == "verifier_unreachable" || result.kind == "reach_error")
        return "ae-reachable";
    if (result.condition && result.condition->equals(IntervalValue(0)))
        return "ae-must-fail-if-reached";
    if (result.condition && result.condition->equals(IntervalValue(1)))
    {
        if (result.kind != "verifier_eq")
            return "ae-condition-true";
        if (result.lhs && result.rhs && isFiniteSingleton(*result.lhs) &&
                result.lhs->equals(*result.rhs))
            return "ae-condition-true";
    }
    return "ae-condition-unknown";
}

std::optional<SiteResult> Verifier::check(const CallICFGNode& call, AbstractInterpretation& ae)
{
    const auto* callee = call.getCalledFunction();
    if (!callee)
        return std::nullopt;
    const auto& name = callee->getName();
    if (name != "__verifier_assert" && name != "__verifier_eq" &&
            name != "__verifier_unreachable" && name != "reach_error")
        return std::nullopt;

    SiteResult result;
    result.kind = name == "reach_error" ? name : name.substr(2);
    setLocation(call, result);
    if (result.kind != "reach_error" && !call.arg_empty())
    {
        if (const auto* line = SVFUtil::dyn_cast<ConstIntValVar>(call.getActualParms().back()))
            result.line = line->getZExtValue();
    }
    result.hasState = ae.hasAbsState(&call);
    if (result.hasState && (result.kind == "verifier_assert" || result.kind == "verifier_eq"))
    {
        result.condition = interval(ae, call, 0);
        if (result.kind == "verifier_eq")
        {
            result.lhs = interval(ae, call, 1);
            result.rhs = interval(ae, call, 2);
        }
    }
    result.reason = verdictReason(result);
    result.verdict = result.reason == "ae-condition-true" ? "safe" : "unknown";
    return result;
}

} // namespace SVF
