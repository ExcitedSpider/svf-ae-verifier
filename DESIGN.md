# svf-ae-verifier: design

The proof of concept lives on the `poc` branch.

## Goal

A property verifier built on SVF's abstract execution (AE). AE computes abstract states; the verifier reads AE's **final** state at the nodes a property cares about and decides a verdict from that state alone.

- It is a command-line tool, usable without any evaluation kit. It is not a library: nothing outside this project includes its classes.
- It uses SVF unmodified, through its public API, so it builds against an installed SVF.
- The code is header-only (`.hpp`) with a single `main.cpp`. No header/source split.

## Non-goals

- Making AE more precise or more sound. If AE's state is wrong, the verdict is wrong; that is an AE issue, reported to SVF, not worked around here.
- Reasoning outside AE: no CFG or path proofs, no LLVM analyses, no IR rewriting, no testing or witness search.
- Benchmarking code. Evaluation runs in SAAV-Bench (see [Evaluation](#evaluation)).

## Properties

| Property | Sites | Proved when |
|---|---|---|
| `reach` | calls to `__verifier_assert`, `__verifier_eq`, `__verifier_unreachable`, `reach_error` | the check's condition is `[1, 1]` at the call (for `eq`, both operands are the same finite singleton). Error sites always remain unproved. |
| `overflow` | loads and stores through a pointer, excluding synthetic call and return effects | every target has a known fixed non-heap size and its access range fits. The property recognizes direct base addresses and single GEPs from a direct base using AE index intervals and target layout. Heap targets leave the site unproved. |

Every other situation is `unproved`, with a reason saying what AE saw (for example `ae-must-fail-if-reached` when the condition is `[0, 0]`). Verdicts are `proved` and `unproved` only. `Proved` is sound with respect to AE: every execution AE models satisfies the check. `Unproved` claims nothing; in particular it is not a violation. Reporting violations (`unsafe`) would need evidence beyond AE's state, such as a replayed witness, and is out of scope.

The property language for `reach` is shipped with the tool as `include/verifier.h` (macros, intrinsics, nondet functions, `verifier_assume`) and specified in `docs/properties.md`. Users include it; nothing depends on an external kit.

Pinned SVF has no `__verifier_assume` model, so the shipped macro branches to `abort()` when its condition is false. The reason tables are in [the property reference](docs/properties.md).

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
| `StateView` | AE startup and guarded queries of the final state. Queries cover whether a node was reached, a value's interval, a pointer's address set, and object bounds. The value queries use SVF's virtual def-site lookup in the sparse modes. | Change the final state or trace during a query. |
| `Property` | An interface: which ICFG nodes are sites, and how one site is decided from a `StateView`. | Know about other properties, files, options or JSON. |
| `Reachability`, `BufferOverflow` | One file each: site discovery and the verdict table for that property. | Contain evaluation oracles (`SAFE_BUFACCESS`, expected labels, and so on). |
| `Result` | A site's location, kind, verdict, reason, and evidence. | Hold property-specific fields. |
| `Verifier` | Runs the selected properties over the ICFG after AE has finished and groups `Result`s by property, including selected properties with no sites. | Do I/O or set up SVF. |
| `svf-verifier` (tool) | Options, loading the module, the AE setup, running the `Verifier`, writing the **complete** report. | Decide verdicts. |

Only headers under `src/llvm/` may use LLVM, and only `main.cpp` includes those headers. The layout bridge builds a table of plain GEP layout facts once after the PAG is built. `StateView` exposes those facts to properties alongside its read-only AE queries. The overflow property owns the index interval reads and offset arithmetic.

### Interfaces (sketch)

```cpp
namespace SVF {

enum class Verdict { Proved, Unproved };

struct SourceLocation { std::string file; unsigned line = 0, column = 0; std::string function; };

struct Evidence { std::string name; std::optional<IntervalValue> value; };  // e.g. "condition", "offset", "size"

struct Site {
    const ICFGNode* node;
    const SVFStmt* stmt;  // null for call sites
    std::string kind;
    std::optional<unsigned> sourceLineArgument = std::nullopt;
};

struct Result {
    Site site;
    SourceLocation location;
    Verdict verdict;
    std::string reason;
    std::vector<Evidence> evidence;
};

class StateView {
public:
    StateView(AbstractInterpretation& ae, const PAG& pag, const GepLayouts& layouts);
    bool reached(const ICFGNode* node) const;
    std::optional<IntervalValue> interval(const ValVar* var, const ICFGNode* node) const;
    std::optional<AddressValue> addresses(const ValVar* var, const ICFGNode* node) const;
    std::optional<NodeID> objectId(u32_t address, const ICFGNode* node) const;
    const BaseObjVar* baseObject(NodeID id) const;
    std::optional<IntervalValue> byteSize(const BaseObjVar* obj) const;
    const GepLayout* gepLayout(const GepStmt* gep) const;
};

StateView runAbstractExecution(const PAG& pag, const GepLayouts& layouts);

class Property {
public:
    virtual ~Property() = default;
    virtual std::string_view name() const = 0;
    virtual std::vector<Site> sites(const ICFG& icfg) const = 0;
    virtual Result check(const Site& site, const StateView& state) const = 0;
};

struct PropertyResults { std::string name; std::vector<Result> sites; };

class Verifier {
public:
    void add(std::unique_ptr<Property> property);
    std::vector<PropertyResults> run(const ICFG& icfg, const StateView& state) const;
};

}
```

Adding a property means adding one `Property` subclass and registering its name in `main.cpp`. Nothing else changes.

### Source locations

SVF core exposes a node's source location only as a string (`getSourceLoc()`). Parsing it is done in one function next to `SourceLocation`, with a test, and listed as an SVF API gap. For `reach` sites, the line is taken from the intrinsic's `line` argument when present, so it is the user's source line rather than a macro expansion.

## Tool and report

```
svf-verifier -property=reach[,overflow] [AE options, e.g. -ae-sparsity=sparse] input.ll [-o report.json]
```

The tool writes one report per run (stdout by default). It is complete on its own:

- `schema_version`, tool and SVF versions and commits, the exact LLVM version, the command line, and the AE mode;
- `status` (`ok`) and the time;
- per site: property, kind, location, verdict, reason, evidence (named intervals);
- per property: the program verdict (`proved` if every site is `proved`, otherwise `unproved`).

The schema is documented in `docs/report.md`, with an example. SVF's analysis output and failure diagnostics go to stderr. The tool writes the report on success. Standalone help prints usage.

### Exit status

The exit code never reflects a verdict. Verdicts are only in the report.

| Code | Meaning |
|---|---|
| `0` | The analysis finished and the report was written, whatever the verdicts are |
| `1` | Something went wrong: the input could not be loaded, SVF or AE failed, or the report could not be written |
| `2` | Bad usage, such as an unknown property or option |

If AE crashes mid-analysis, the process dies with that signal and no report is written; the caller treats it as a failure.

## Repository layout

```
svf-ae-verifier/
├── CMakeLists.txt                # find_package(SVF); one executable + tests
├── README.md
├── DESIGN.md
├── include/
│   └── verifier.h                # property language for users' C code (macros, intrinsics, nondet)
├── src/
│   ├── main.cpp                  # options, module/PAG setup, AE, properties and reporting
│   ├── GepLayout.hpp             # plain per-GEP layout facts
│   ├── llvm/Layout.hpp           # reads target layout before AE
│   ├── Verifier.hpp              # runs properties over the ICFG after AE
│   ├── Property.hpp              # Property interface, Site
│   ├── StateView.hpp             # read-only queries of AE's final state; the only file that calls AE
│   ├── Result.hpp                # Verdict, SourceLocation (+ parsing), Evidence, Result
│   └── properties/
│       ├── Reachability.hpp      # reach sites + verdict table
│       └── BufferOverflow.hpp    # overflow sites + verdict table
├── schema/
│   └── report.schema.json        # JSON Schema of the report; ctest validates every report against it
├── tests/
│   ├── reach/*.c                 # each file states its expected verdicts
│   ├── overflow/*.c
│   ├── schema/invalid/*.json     # reports the schema must reject
│   └── validate-report.py        # schema check used by ctest
└── docs/
    ├── properties.md             # property language and verdict tables
    └── report.md                 # report fields + example
```

No two files share a name. `.hpp` files are included only by `main.cpp` and by each other.

## Build and test

- The root Dockerfile is the reference build. It is tested with SVF `master` and defaults to LLVM 22. It uses one compiler for SVF and the verifier, then runs CTest before packaging the runtime. The report's `svf_commit` identifies the exact revision. Manual builds use `cmake -DSVF_DIR=<svf-install>/lib/cmake/SVF ..` and the same C++ compiler as SVF.
- `ctest` compiles each `tests/**/*.c` with the canonical command, runs `svf-verifier` in all three AE modes, and compares against the expected verdicts written in the test file (for example `// EXPECT: line 6 proved`). Tests cover each row of each verdict table.
- AE's known false results (for example an array read with an unbounded index) are not tests here.

## Known gaps in AE's public API

From the PoC (`poc` branch, `REPORT.md`):

- **Offset from the base object.** For `p = buf + i; q = p + j; *q`, the final state does not give `q`'s offset from `buf`. `BufOverflowDetector` tracks this internally during the analysis. This currently makes most `overflow` sites `unproved`.
- **`getGepObjAddrs` is not read-only.** It creates objects and changes the trace when called after the analysis, so `StateView` cannot use it.
- **`getGepByteOffset` clamps.** It caps dynamic indices at the field limit and drops negative index ranges. The constant path also saturates at the field limit and sums struct field sizes without including inter-field padding. The verifier computes single-GEP offsets from AE index intervals and target layout without this helper.
- **Dynamic extents.** The allocation-size helper also saturates at the field limit. The verifier uses the fixed byte size of non-heap objects and leaves other extents unknown.
- **Source locations** are strings only.

These are the candidates for a small upstream API addition.

## Evaluation

Evaluation uses [SAAV-Bench](https://github.com/ExcitedSpider/SAAV-Bench), which scores *validators*: tools that take the alarms of a base analyzer (`svf-ae`) on Juliet and SV-COMP programs and give each alarm a verdict. This repository contains no benchmark code; the validator definition and its adapter live in SAAV-Bench under `validators/`.

| SAAV-Bench alarm class | Property | Verifier result → SAAV-Bench verdict |
|---|---|---|
| `out-of-bounds` (Juliet buffer CWEs) | `overflow`, the access at the alarm's location | `proved` → `refuted`; `unproved` → `inconclusive` |
| `assertion` (an injected `benchmark_assert(0)` at each SV-COMP `reach_error` call) | `reach` | see the note below |

The verifier never says `confirmed`, because `unproved` is not a violation. A failed run (non-zero exit) maps to `crashed`, `timeout` or `unsupported` as SAAV-Bench defines them.

Note on SV-COMP: refuting an assertion alarm means showing the `reach_error` call cannot be reached. The `reach` table above never proves that for `reach_error` (it would need a coverage argument), so the verifier currently cannot refute any SV-COMP alarm. Deciding whether "no AE state at the node" may count as unreachable, and under which conditions, is the main open design point for `reach`.
