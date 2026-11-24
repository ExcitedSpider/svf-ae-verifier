#include "Verifier.hpp"
#include "properties/BufferOverflow.hpp"
#include "properties/Reachability.hpp"
#include "SVF-LLVM/PAGBuilder.h"
#include "Util/CommandLine.h"
#include "WPA/Andersen.h"
#include <llvm/IR/LLVMContext.h>
#include <llvm/IR/Module.h>
#include <llvm/IRReader/IRReader.h>
#include <llvm/Support/FormatVariadic.h>
#include <llvm/Support/JSON.h>
#include <llvm/Support/SourceMgr.h>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <set>

namespace {

using namespace SVF;
using llvm::json::Array;
using llvm::json::Object;
using JsonValue = llvm::json::Value;

Option<std::string> propertyOption("property", "Comma-separated properties: reach,overflow", "");
Option<std::string> outputOption("o", "Write the report to this path instead of stdout", "");

class ConsoleToStderr {
public:
    ConsoleToStderr() : original(std::cout.rdbuf(std::cerr.rdbuf())) {}
    ~ConsoleToStderr() { std::cout.rdbuf(original); }

private:
    std::streambuf* original;
};

const char* verdictName(Verdict verdict)
{
    return verdict == Verdict::Proved ? "proved" : "unproved";
}

JsonValue bound(const BoundedInt& value)
{
    if (value.is_minus_infinity())
        return "-inf";
    if (value.is_plus_infinity())
        return "+inf";
    return value.getNumeral();
}

JsonValue intervalJson(const std::optional<IntervalValue>& interval)
{
    if (!interval)
        return nullptr;
    return Array{bound(interval->lb()), bound(interval->ub())};
}

void addResults(Object& report, const std::vector<PropertyResults>& results)
{
    Array sites;
    Object properties;
    for (const auto& property : results)
    {
        auto verdict = Verdict::Proved;
        for (const auto& site : property.sites)
        {
            if (site.verdict == Verdict::Unproved)
                verdict = site.verdict;
            Object evidence;
            for (const auto& entry : site.evidence)
                evidence[entry.name] = intervalJson(entry.value);
            const auto& loc = site.location;
            Object entry{{"property", property.name}, {"kind", site.site.kind},
                         {"location", Object{{"file", loc.file}, {"line", loc.line},
                                             {"column", loc.column}, {"function", loc.function}}},
                         {"reason", site.reason}, {"verdict", verdictName(site.verdict)},
                         {"evidence", std::move(evidence)}};
            sites.push_back(std::move(entry));
        }
        properties[property.name] = verdictName(verdict);
    }
    report["sites"] = std::move(sites);
    report["properties"] = std::move(properties);
}

bool writeReport(Object report, const std::string& path = "")
{
    using namespace std::chrono;
    report["time_unix_ms"] = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    const auto json = llvm::formatv("{0:2}\n", JsonValue(std::move(report))).str();
    if (path.empty() || path == "-")
        return static_cast<bool>(std::cout << json << std::flush);
    std::ofstream output(path);
    output << json;
    output.close();
    if (output)
        return true;
    std::cerr << "Could not write report to " << path << '\n';
    return false;
}

int errorReport(Object report, const char* message, int code)
{
    std::cerr << message << '\n';
    report["status"] = "error";
    report["error"] = message;
    return writeReport(std::move(report), code == 2 ? "" : outputOption()) ? code : 1;
}

// SVF's option parser and some input/setup failures call exit(1).
// Keep the pending report and stdout buffer available to the exit handler.
Object* pendingReport = nullptr;
std::streambuf* reportBuffer = nullptr;
int pendingExitCode = 2;

void reportOnSvfExit()
{
    if (!pendingReport)
        return;
    std::cout.rdbuf(reportBuffer);
    const auto code = errorReport(std::move(*pendingReport),
        pendingExitCode == 2 ? "Invalid SVF options" : "SVF terminated during analysis", pendingExitCode);
    std::_Exit(code);
}

std::vector<PropertyResults> analyze(const std::vector<std::string>& inputs, const Verifier& verifier)
{
    // Mirrors svf-llvm/tools/AE/ae.cpp at the pinned commit: the same modeling
    // flags (set in main), module setup, Andersen and call-graph update.
    if (Options::ExtAPIPath().empty() && !ExtAPI::setExtBcPath(VERIFIER_EXTAPI_PATH))
        throw std::runtime_error("Could not locate installed SVF extapi.bc");
    LLVMModuleSet::getLLVMModuleSet()->buildSVFModule(inputs);
    PAGBuilder builder;
    auto* pag = builder.build();
    auto* andersen = AndersenWaveDiff::createAndersenWaveDiff(pag);
    builder.updateCallGraph(andersen->getCallGraph());
    const auto state = runAbstractExecution(*pag);
    return verifier.run(*pag->getICFG(), state);
}

bool loadable(const std::string& input, const char* program)
{
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    if (llvm::parseIRFile(input, diagnostic, context))
        return true;
    diagnostic.print(program, llvm::errs());
    return false;
}

bool sameFile(const std::string& input, const std::string& output)
{
    std::error_code error;
    if (std::filesystem::equivalent(input, output, error))
        return true;
    const auto inputPath = std::filesystem::weakly_canonical(input, error);
    if (error)
        return false;
    const auto outputPath = std::filesystem::weakly_canonical(output, error);
    return !error && inputPath == outputPath;
}

} // namespace

int main(int argc, char** argv)
{
    const char* synopsis = "-property=reach[,overflow] [SVF options] <input.ll> [-o report.json]";
    if (argc == 2 && (std::string_view(argv[1]) == "--help" ||
                      std::string_view(argv[1]) == "-help" || std::string_view(argv[1]) == "-h"))
    {
        char help[] = "-help";
        char* args[]{argv[0], help};
        OptionBase::parseOptions(2, args, "SVF AE verifier", synopsis);
    }

    Array command;
    for (int i = 0; i < argc; ++i)
        command.push_back(std::string(argv[i]));
    Object report{{"schema_version", 1}, {"tool_version", VERIFIER_VERSION},
                  {"tool_commit", VERIFIER_COMMIT}, {"svf_version", VERIFIER_SVF_VERSION},
                  {"svf_commit", VERIFIER_SVF_COMMIT}, {"command_line", std::move(command)},
                  {"ae_mode", nullptr}, {"properties", Object{}}, {"sites", Array{}}};

    char constFlag[] = "-model-consts=true";
    char arrayFlag[] = "-model-arrays=true";
    char fieldFlag[] = "-pre-field-sensitive=false";
    std::vector<char*> args{argv[0], constFlag, arrayFlag, fieldFlag};
    args.insert(args.end(), argv + 1, argv + argc);
    reportBuffer = std::cout.rdbuf();
    std::atexit(reportOnSvfExit);
    pendingReport = &report;
    std::vector<std::string> inputs;
    {
        const ConsoleToStderr console;
        inputs = OptionBase::parseOptions(args.size(), args.data(), "SVF AE verifier", synopsis);
    }
    pendingReport = nullptr;

    switch (Options::AESparsity())
    {
    case AbstractInterpretation::AESparsity::Dense: report["ae_mode"] = "dense"; break;
    case AbstractInterpretation::AESparsity::SemiSparse: report["ae_mode"] = "semi-sparse"; break;
    case AbstractInterpretation::AESparsity::Sparse: report["ae_mode"] = "sparse"; break;
    }

    Verifier verifier;
    const auto requested = propertyOption();
    std::set<std::string_view> seen;
    std::string_view remaining = requested;
    for (;;)
    {
        const auto comma = remaining.find(',');
        const auto name = remaining.substr(0, comma);
        if (!seen.insert(name).second)
            return errorReport(std::move(report), "Duplicate property", 2);
        if (name == "reach")
            verifier.add(std::make_unique<Reachability>());
        else if (name == "overflow")
            verifier.add(std::make_unique<BufferOverflow>());
        else
            return errorReport(std::move(report), "Expected -property=reach[,overflow]", 2);
        if (comma == remaining.npos)
            break;
        remaining.remove_prefix(comma + 1);
    }
    if (inputs.size() != 1)
        return errorReport(std::move(report), "Expected exactly one input module", 2);
    const auto output = outputOption();
    if (!output.empty() && output != "-" && sameFile(inputs.front(), output))
        return errorReport(std::move(report), "Input and report refer to the same file", 2);
    if (!loadable(inputs.front(), argv[0]))
        return errorReport(std::move(report), "Could not load input", 1);

    try
    {
        const ConsoleToStderr console;
        pendingExitCode = 1;
        pendingReport = &report;
        addResults(report, analyze(inputs, verifier));
        AndersenWaveDiff::releaseAndersenWaveDiff();
        LLVMModuleSet::releaseLLVMModuleSet();
        pendingReport = nullptr;
    }
    catch (const std::exception& error)
    {
        pendingReport = nullptr;
        return errorReport(std::move(report), error.what(), 1);
    }
    report["status"] = "ok";
    return writeReport(std::move(report), output) ? 0 : 1;
}
