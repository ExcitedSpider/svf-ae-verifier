#include "Verifier/Verifier.h"
#include "AE/Svfexe/AbstractInterpretation.h"
#include "SVF-LLVM/PAGBuilder.h"
#include "Util/CommandLine.h"
#include "WPA/Andersen.h"
#include <llvm/Support/JSON.h>
#include <unistd.h>

namespace SVF
{

static llvm::json::Value bound(const BoundedInt& value)
{
    if (value.is_minus_infinity())
        return "-inf";
    if (value.is_plus_infinity())
        return "+inf";
    return value.getNumeral();
}

static llvm::json::Value interval(const std::optional<IntervalValue>& range)
{
    if (!range)
        return nullptr;
    return llvm::json::Array{bound(range->lb()), bound(range->ub())};
}

static llvm::json::Object siteJson(const SiteResult& site)
{
    llvm::json::Object intervals;
    if (site.kind == "verifier_assert" || site.kind == "verifier_eq")
        intervals["condition"] = interval(site.condition);
    if (site.kind == "verifier_eq")
    {
        intervals["lhs"] = interval(site.lhs);
        intervals["rhs"] = interval(site.rhs);
    }
    return llvm::json::Object{{"file", site.file}, {"line", site.line}, {"column", site.column},
        {"function", site.function}, {"kind", site.kind}, {"has_state", site.hasState},
        {"verdict", site.verdict}, {"reason", site.reason}, {"intervals", std::move(intervals)}};
}

static std::vector<SiteResult> runAE(char** argv)
{
    // Mirrors svf-llvm/tools/AE/ae.cpp's main() at a8a0f51.
    char constFlag[] = "-model-consts=true";
    char arrayFlag[] = "-model-arrays=true";
    char fieldFlag[] = "-pre-field-sensitive=false";
    char* args[] = {argv[0], argv[1], argv[2], constFlag, arrayFlag, fieldFlag};
    const auto inputs = OptionBase::parseOptions(6, args, "AE reachability verifier",
                        "[options] <input>");
    LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(inputs);
    PAGBuilder builder;
    auto* pag = builder.build();
    auto* andersen = AndersenWaveDiff::createAndersenWaveDiff(pag);
    builder.updateCallGraph(andersen->getCallGraph());
    auto& ae = AbstractInterpretation::getAEInstance();
    ae.runOnModule();
    auto results = Verifier::collect(*pag->getICFG(), ae);
    AndersenWaveDiff::releaseAndersenWaveDiff();
    LLVMModuleSet::releaseLLVMModuleSet();
    return results;
}

} // namespace SVF

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    // Reserve the original stdout for site objects and send SVF's output to stderr.
    const int output = dup(STDOUT_FILENO);
    if (output < 0 || dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
        return 2;
    llvm::raw_fd_ostream observations(output, true);
    for (const auto& site : SVF::runAE(argv))
        observations << llvm::json::Value(SVF::siteJson(site)) << '\n';
    observations.close();
    return 0;
}
