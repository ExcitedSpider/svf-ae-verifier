#include "Verifier.hpp"
#include "llvm/Layout.hpp"
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
#include <cerrno>
#include <fstream>
#include <set>
#include <sstream>
#include <sys/wait.h>
#include <unistd.h>

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

int fail(const char* message, int code)
{
    std::cerr << message << '\n';
    return code;
}

int validateOptions(std::vector<char*>& args, const char* synopsis)
{
    // SVF's parser exits the process on bad options. Validate in a child so
    // the verifier can map that usage error to exit 2.
    const pid_t child = fork();
    if (child < 0)
        return fail("Could not start SVF option validation", 1);
    if (child == 0)
    {
        // The parent prints usage when it parses a help option.
        std::ostringstream discard;
        std::cout.rdbuf(discard.rdbuf());
        OptionBase::parseOptions(args.size(), args.data(), "SVF AE verifier", synopsis);
        _exit(0);
    }
    int status;
    pid_t result;
    do {
        result = waitpid(child, &status, 0);
    } while (result < 0 && errno == EINTR);
    if (result < 0 || !WIFEXITED(status))
        return fail("SVF option validation failed", 1);
    return WEXITSTATUS(status) == 0 ? 0 : 2;
}

std::vector<PropertyResults> analyze(llvm::Module& module, const Verifier& verifier)
{
    // Mirrors svf-llvm/tools/AE/ae.cpp's modeling flags, Andersen setup and
    // call-graph update. The verifier passes its already loaded module to SVF.
    if (Options::ExtAPIPath().empty() && !ExtAPI::setExtBcPath(VERIFIER_EXTAPI_PATH))
        throw std::runtime_error("Could not locate installed SVF extapi.bc");
    LLVMModuleSet::buildSVFModule(module);
    PAGBuilder builder;
    auto* pag = builder.build();
    const auto layouts = buildGepLayouts(*pag);
    auto* andersen = AndersenWaveDiff::createAndersenWaveDiff(pag);
    builder.updateCallGraph(andersen->getCallGraph());
    const auto state = runAbstractExecution(*pag, layouts);
    return verifier.run(*pag->getICFG(), state);
}

} // namespace

int main(int argc, char** argv)
{
    const char* synopsis = "-property=reach[,overflow] [SVF options] <input.ll> [-o report.json]";
    Array command;
    for (int i = 0; i < argc; ++i)
        command.push_back(std::string(argv[i]));
    Object report{{"schema_version", 1}, {"tool_version", VERIFIER_VERSION},
                  {"tool_commit", VERIFIER_COMMIT}, {"svf_version", VERIFIER_SVF_VERSION},
                  {"svf_commit", VERIFIER_SVF_COMMIT}, {"llvm_version", VERIFIER_LLVM_VERSION},
                  {"command_line", std::move(command)},
                  {"ae_mode", nullptr}, {"properties", Object{}}, {"sites", Array{}}};

    char constFlag[] = "-model-consts=true";
    char arrayFlag[] = "-model-arrays=true";
    char fieldFlag[] = "-pre-field-sensitive=false";
    std::vector<char*> args{argv[0], constFlag, arrayFlag, fieldFlag};
    args.insert(args.end(), argv + 1, argv + argc);
    if (const int status = validateOptions(args, synopsis))
        return status;
    const auto inputs = OptionBase::parseOptions(args.size(), args.data(), "SVF AE verifier", synopsis);

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
            return fail("Duplicate property", 2);
        if (name == "reach")
            verifier.add(std::make_unique<Reachability>());
        else if (name == "overflow")
            verifier.add(std::make_unique<BufferOverflow>());
        else
            return fail("Expected -property=reach[,overflow]", 2);
        if (comma == remaining.npos)
            break;
        remaining.remove_prefix(comma + 1);
    }
    if (inputs.size() != 1)
        return fail("Expected exactly one input module", 2);
    llvm::LLVMContext context;
    llvm::SMDiagnostic diagnostic;
    auto module = llvm::parseIRFile(inputs.front(), diagnostic, context);
    if (!module)
    {
        diagnostic.print(argv[0], llvm::errs());
        return 1;
    }

    try
    {
        const ConsoleToStderr console;
        addResults(report, analyze(*module, verifier));
        AndersenWaveDiff::releaseAndersenWaveDiff();
        LLVMModuleSet::releaseLLVMModuleSet();
    }
    catch (const std::exception& error)
    {
        return fail(error.what(), 1);
    }
    report["status"] = "ok";
    return writeReport(std::move(report), outputOption()) ? 0 : 1;
}
