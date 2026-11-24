# svf-ae-verifier: design

Status: draft for review. The proof of concept lives on the `poc` branch; this document replaces its structure.

## Goal

A property verifier built on SVF's abstract execution (AE). AE computes abstract states; the verifier reads AE's **final** state at the nodes a property cares about and decides a verdict from that state alone.

- It is a library plus a command-line tool, usable without any evaluation kit.
- It uses SVF unmodified, through its public API, so it builds against an installed SVF.
- Its core is written so that it could later move into SVF as `svf/include/Verifier` + `svf/lib/Verifier`.

## Non-goals

- Making AE more precise or more sound. If AE's state is wrong, the verdict is wrong; that is an AE issue, reported to SVF, not worked around here.
- Reasoning outside AE: no CFG or path proofs, no LLVM analyses, no IR rewriting, no testing or witness search.
- Benchmarking. Evaluation harnesses are separate (see [Open questions](#open-questions)).

## Properties

| Property | Sites | Safe when |
|---|---|---|
| `reach` | calls to `__verifier_assert`, `__verifier_eq`, `__verifier_unreachable`, `reach_error` | the check's condition is `[1, 1]` at the call (for `eq`, both operands are the same single value); for `unreachable` / `reach_error`, never (`safe` would need a coverage argument, which is out of scope) |
| `overflow` | loads and stores through a pointer | for every object the pointer may target, `[offset, offset + width)` lies within `[0, size)` |

Every other situation is `unknown`, with a reason saying what AE saw. Verdicts are `safe` and `unknown` only; `unsafe` is out of scope for now.

The property language for `reach` is shipped with the tool as `include/verifier.h` (macros, intrinsics, nondet functions, `verifier_assume`) and specified in `docs/properties.md`. Users include it; nothing depends on an external kit.

The canonical compilation is SVF's usual one: `clang -g -S -emit-llvm -Xclang -disable-O0-optnone -fno-discard-value-names`, then `opt -p=mem2reg`.

## Architecture

```
                 svf-verifier (tool)
   options -> SVF + AE setup -> Verifier -> Report -> JSON
                                   |
                     +-------------+-------------+
                     |                           |
                 Property                    StateView
          (Reachability, BufferOverflow)  (read-only AE queries)
                     |                           |
                  SVF IR                   AbstractInterpretation
```

| Module | Owns | Does not |
|---|---|---|
| `StateView` | The only code that talks to AE. Read-only, guarded queries of the final state: whether a node was reached, a value's interval, a pointer's address set, an object's byte size. Handles the sparse-mode def-site lookup. | Call any AE function that changes the state or the trace. |
| `Property` | An interface: which ICFG nodes are sites, and how one site is decided from a `StateView`. | Know about other properties, files, options or JSON. |
| `Reachability`, `BufferOverflow` | One file each: site discovery and the verdict table for that property. | Contain evaluation oracles (`SAFE_BUFACCESS`, expected labels, and so on). |
| `Result` | A site's location, kind, verdict, reason, and evidence. | Hold property-specific fields. |
| `Verifier` | Runs the selected properties over the ICFG after AE has finished; returns `Result`s. | Do I/O or set up SVF. |
| `svf-verifier` (tool) | Options, loading the module, the AE setup, running the `Verifier`, writing the **complete** report. | Decide verdicts. |

`include/` and `lib/` are LLVM-free: SVF IR and AE's public API only. LLVM appears only in the tool.

### Interfaces (sketch)

```cpp
namespace SVF {

enum class Verdict { Safe, Unknown };

struct SourceLocation { std::string file; unsigned line = 0, column = 0; std::string function; };

struct Evidence { std::string name; std::optional<IntervalValue> value; };  // e.g. "condition", "offset", "size"

struct Site { const ICFGNode* node; const SVFStmt* stmt; std::string kind; };  // stmt null for call sites

struct Result {
    Site site;
    SourceLocation location;
    Verdict verdict;
    std::string reason;
    std::vector<Evidence> evidence;
};

class StateView {
public:
    explicit StateView(AbstractInterpretation& ae);
    bool reached(const ICFGNode* node) const;
    std::optional<IntervalValue> interval(const ValVar* var, const ICFGNode* node) const;
    std::optional<AddressValue> addresses(const ValVar* var, const ICFGNode* node) const;
    std::optional<IntervalValue> byteSize(const BaseObjVar* obj) const;
};

class Property {
public:
    virtual ~Property() = default;
    virtual std::string_view name() const = 0;
    virtual std::vector<Site> sites(const ICFG& icfg) const = 0;
    virtual Result check(const Site& site, const StateView& state) const = 0;
};

class Verifier {
public:
    void add(std::unique_ptr<Property> property);
    std::vector<Result> run(const ICFG& icfg, AbstractInterpretation& ae) const;  // after runOnModule()
};

}
```

Adding a property means adding one `Property` subclass and registering its name in the tool. Nothing else changes.

### Source locations

SVF core exposes a node's source location only as a string (`getSourceLoc()`). Parsing it is done in one function next to `SourceLocation`, with a test, and listed as an SVF API gap. For `reach` sites, the line is taken from the intrinsic's `line` argument when present, so it is the user's source line rather than a macro expansion.

## Tool and report

```
svf-verifier -property=reach[,overflow] [AE options, e.g. -ae-sparsity=sparse] input.ll [-o report.json]
```

The tool writes one report per run (stdout by default). It is complete on its own:

- `schema_version`, tool version and commit, SVF version, the command line, the AE mode;
- `status` (`ok` / `error`) and the time;
- per site: property, kind, location, verdict, reason, evidence (named intervals);
- per property: the program verdict (`safe` if every site is `safe`, otherwise `unknown`).

The schema is documented in `docs/report.md`, with an example. SVF's own console output goes to stderr, so the report stream stays clean without redirecting file descriptors.

## Repository layout

```
include/verifier.h                       # property language for users' C code
include/Verifier/{Verifier,Property,StateView,Result}.h
lib/Verifier/{Verifier,StateView,Result}.cpp
lib/Verifier/Properties/{Reachability,BufferOverflow}.cpp
tools/svf-verifier/main.cpp
tests/reach/*.c, tests/overflow/*.c       # each file states its expected verdicts
docs/{properties,report}.md
CMakeLists.txt                           # find_package(SVF); library + tool + tests
```

No two source files share a name.

## Build and test

- Build against an installed SVF: `cmake -DSVF_DIR=<svf-install>/lib/cmake/SVF ..`. The supported SVF commit is recorded in `CMakeLists.txt` and the README. A container recipe may be provided, but is not required.
- `ctest` compiles each `tests/**/*.c` with the canonical command, runs `svf-verifier` in all three AE modes, and compares against the expected verdicts written in the test file (for example `// EXPECT: line 6 safe`). Tests cover each row of each verdict table.
- AE's known false results (for example an array read with an unbounded index) are not tests here.

## Known gaps in AE's public API

From the PoC (`poc` branch, `REPORT.md`):

- **Offset from the base object.** For `p = buf + i; q = p + j; *q`, the final state does not give `q`'s offset from `buf`. `BufOverflowDetector` tracks this internally during the analysis. This currently makes most `overflow` sites `unknown`.
- **`getGepObjAddrs` is not read-only.** It creates objects and changes the trace when called after the analysis, so `StateView` cannot use it.
- **`getGepByteOffset` clamps.** It caps dynamic indices at the field limit and drops negative index ranges.
- **Source locations** are strings only.

These are the candidates for a small upstream API addition.

## Open questions

1. **Evaluation harness:** keep a `bench/` directory here (SV-COMP and Test-Suite runners), or keep it with the evaluation kit, outside this repository?
2. **Exit codes:** should the tool's exit code reflect the verdict (as verifiers in SV-COMP style do) or only success or failure of the run?
