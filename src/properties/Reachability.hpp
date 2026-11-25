#pragma once

#include "../Property.hpp"

namespace SVF {

class Reachability final : public Property {
public:
    std::string_view name() const override { return "reach"; }

    std::vector<Site> sites(const ICFG& icfg) const override
    {
        std::vector<Site> result;
        for (const auto& [id, node] : icfg)
        {
            const auto* call = SVFUtil::dyn_cast<CallICFGNode>(node);
            if (!call || !call->getCalledFunction())
                continue;
            const auto& callee = call->getCalledFunction()->getName();
            if (callee == "__verifier_assert" || callee == "__verifier_eq" ||
                    callee == "__verifier_unreachable")
            {
                const unsigned lineArgument = callee == "__verifier_eq" ? 3 :
                                              callee == "__verifier_assert" ? 1 : 0;
                result.push_back({node, nullptr, callee.substr(2), lineArgument});
            }
            else if (callee == "reach_error")
                result.push_back({node, nullptr, callee});
        }
        return result;
    }

    Result check(const Site& site, const StateView& state, const TargetFacts&) const override
    {
        const auto& call = *SVFUtil::cast<CallICFGNode>(site.node);
        const bool equality = site.kind == "verifier_eq";
        const bool assertion = site.kind == "verifier_assert";
        Result result{site, {}, Verdict::Unproved, "ae-no-state", {}};
        if (!state.reached(&call))
            return result;
        if (!equality && !assertion)
        {
            result.reason = "ae-reachable";
            return result;
        }

        auto argument = [&](unsigned index) {
            return index < call.arg_size() ? state.interval(call.getArgument(index), &call)
                                           : std::nullopt;
        };
        const auto condition = argument(0);
        result.evidence.push_back({"condition", condition});
        bool equalSingletons = false;
        if (equality)
        {
            const auto lhs = argument(1);
            const auto rhs = argument(2);
            result.evidence.push_back({"lhs", lhs});
            result.evidence.push_back({"rhs", rhs});
            equalSingletons = lhs && rhs && lhs->is_numeral() && !lhs->is_infinite() &&
                              lhs->equals(*rhs);
        }

        result.reason = "ae-condition-unknown";
        if (condition && condition->equals(IntervalValue(0)))
            result.reason = "ae-must-fail-if-reached";
        else if (condition && condition->equals(IntervalValue(1)) && (!equality || equalSingletons))
        {
            result.verdict = Verdict::Proved;
            result.reason = "ae-condition-true";
        }
        return result;
    }
};

} // namespace SVF
