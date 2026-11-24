#ifndef SVF_VERIFIER_H
#define SVF_VERIFIER_H

#include "AE/Core/IntervalValue.h"
#include <optional>
#include <string>
#include <vector>

namespace SVF
{

class AbstractInterpretation;
class CallICFGNode;
class ICFG;

struct SiteResult
{
    std::string file;
    unsigned line = 0;
    unsigned column = 0;
    std::string function;
    std::string kind;
    std::string verdict;
    std::string reason;
    bool hasState = false;
    std::optional<IntervalValue> condition;
    std::optional<IntervalValue> lhs;
    std::optional<IntervalValue> rhs;
};

class Verifier
{
public:
    /// Collect check results after AE has finished running on this ICFG.
    static std::vector<SiteResult> collect(const ICFG& icfg, AbstractInterpretation& ae);

private:
    static std::optional<SiteResult> check(const CallICFGNode& call, AbstractInterpretation& ae);
    static void setLocation(const CallICFGNode& call, SiteResult& result);
};

} // namespace SVF

#endif
