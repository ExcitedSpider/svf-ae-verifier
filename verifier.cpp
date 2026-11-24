#include "AE/Svfexe/AbstractInterpretation.h"
#include "SVF-LLVM/PAGBuilder.h"
#include "Util/CommandLine.h"
#include "WPA/Andersen.h"
#include <llvm/IR/DebugInfoMetadata.h>
#include <llvm/IR/InstIterator.h>
#include <llvm/Support/JSON.h>
#include <unistd.h>

using namespace SVF;
using namespace llvm;

static json::Value bound(const BoundedInt& value)
{
    if (value.is_minus_infinity())
        return "-inf";
    if (value.is_plus_infinity())
        return "+inf";
    return value.getNumeral();
}

static json::Value interval(AbstractInterpretation& ae, LLVMModuleSet& modules,
                            PAG& pag, const CallICFGNode* node, const Value* operand)
{
    if (!modules.hasValueNode(operand))
        return nullptr;
    const auto* var = pag.getSVFVar(modules.getValueNode(operand));
    // Virtual ValVar lookup resolves the defining node in both sparse modes.
    if (!ae.hasAbsValue(var, node))
        return nullptr;
    const auto& value = ae.getAbsValue(var, node);
    if (!value.isInterval())
        return nullptr;
    const auto& range = value.getInterval();
    return json::Array{bound(range.lb()), bound(range.ub())};
}

struct Site
{
    const CallBase* call;
    std::string kind;

    json::Object location() const
    {
        auto debug = call->getDebugLoc();
        unsigned line = debug ? debug.getLine() : 0;
        unsigned column = debug ? debug.getCol() : 0;
        if (kind != "reach_error")
            if (const auto* macroLine = dyn_cast<ConstantInt>(
                    call->getArgOperand(call->arg_size() - 1)))
                line = macroLine->getZExtValue();
        std::string file = debug ? debug->getFilename().str()
                                 : call->getModule()->getSourceFileName();
        return json::Object{{"file", file}, {"line", line}, {"column", column},
                            {"function", call->getFunction()->getName()}, {"kind", kind}};
    }
};

static StringRef verdictReason(StringRef kind, bool hasState, const json::Object& values)
{
    if (!hasState)
        return "ae-no-state";
    if (kind == "verifier_unreachable" || kind == "reach_error")
        return "ae-reachable";
    const auto& condition = *values.get("condition");
    if (condition == json::Array{0, 0})
        return "ae-must-fail-if-reached";
    if (condition == json::Array{1, 1})
    {
        if (kind != "verifier_eq")
            return "ae-condition-true";
        const auto* lhs = values.getArray("lhs");
        if (lhs && (*lhs)[0].getAsInteger() && (*lhs)[0] == (*lhs)[1] &&
                *values.get("lhs") == *values.get("rhs"))
            return "ae-condition-true";
    }
    return "ae-condition-unknown";
}

static PAG* prepareAE(char** argv, LLVMModuleSet& modules)
{
    // Mirrors svf-llvm/tools/AE/ae.cpp's main() at a8a0f51.
    char constFlag[] = "-model-consts=true";
    char arrayFlag[] = "-model-arrays=true";
    char fieldFlag[] = "-pre-field-sensitive=false";
    char* args[] = {argv[0], argv[1], argv[2], constFlag, arrayFlag, fieldFlag};
    auto inputs = OptionBase::parseOptions(6, args, "AE reachability verifier",
                                           "[options] <input>");
    modules.buildSVFModule(inputs);
    PAGBuilder builder;
    auto* pag = builder.build();
    auto* andersen = AndersenWaveDiff::createAndersenWaveDiff(pag);
    builder.updateCallGraph(andersen->getCallGraph());
    return pag;
}

int main(int argc, char** argv)
{
    if (argc != 3)
        return 2;
    // Reserve the original stdout for site objects and send SVF's output to stderr.
    const int output = dup(STDOUT_FILENO);
    if (output < 0 || dup2(STDERR_FILENO, STDOUT_FILENO) < 0)
        return 2;
    raw_fd_ostream observations(output, true);

    auto& modules = *LLVMModuleSet::getLLVMModuleSet();
    auto* pag = prepareAE(argv, modules);
    std::vector<Site> sites;
    for (const auto& function : *modules.getMainLLVMModule())
        for (const auto& instruction : instructions(function))
        {
            const auto* call = dyn_cast<CallBase>(&instruction);
            if (!call || !call->getCalledFunction())
                continue;
            const auto name = call->getCalledFunction()->getName();
            if (name == "__verifier_assert" || name == "__verifier_eq" ||
                name == "__verifier_unreachable" || name == "reach_error")
                sites.push_back({call, name.starts_with("__") ? name.drop_front(2).str() : name.str()});
        }
    auto& ae = AbstractInterpretation::getAEInstance();
    ae.runOnModule();
    for (const auto& site : sites)
    {
        auto row = site.location();
        auto* node = modules.getCallICFGNode(site.call);
        const bool hasState = ae.hasAbsState(node);
        row["has_state"] = hasState;
        json::Object values;
        if (site.kind == "verifier_assert" || site.kind == "verifier_eq")
        {
            values["condition"] = hasState
                ? interval(ae, modules, *pag, node, site.call->getArgOperand(0)) : nullptr;
            if (site.kind == "verifier_eq")
                for (unsigned index = 1; index <= 2; ++index)
                    values[index == 1 ? "lhs" : "rhs"] = hasState
                        ? interval(ae, modules, *pag, node, site.call->getArgOperand(index))
                        : nullptr;
        }
        const auto reason = verdictReason(site.kind, hasState, values);
        row["reason"] = reason;
        row["verdict"] = reason == "ae-condition-true" ? "safe" : "unknown";
        row["intervals"] = std::move(values);
        observations << json::Value(std::move(row)) << '\n';
    }
    observations.close();
    AndersenWaveDiff::releaseAndersenWaveDiff();
    LLVMModuleSet::releaseLLVMModuleSet();
    return 0;
}
