# SVF AE final-state evaluation

Measurement commit `ee2157d459189ca4132747c19bb60dc9cfef8210`

## Bindings

| Attempt | Result |
| --- | --- |
| SVF-Python / `pysvf` | The specified image had no importable module, and the image, workspace and local caches had no separate bindings source to build against the pin. |
| C++ fallback | The 125-line extractor links the pinned SVF and emits locations, state presence and operand intervals. |

The GitHub restriction ruled out fetching the separate bindings repository, so the required final-state API could not be tested from Python.

## Conformance

The table lists each verdict and the final intervals available at that check.

| Case and line | dense | semi-sparse | sparse |
| --- | --- | --- | --- |
| c01_holds:4 | safe · condition=[1, 1] | safe · condition=[1, 1] | safe · condition=[1, 1] |
| c02_fails:4 | unknown · condition=[0, 0] | unknown · condition=[0, 0] | unknown · condition=[0, 0] |
| c03_input:4 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c04_assume:5 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c05_assume_prunes:6 | unknown · ae-reachable | unknown · ae-reachable | unknown · ae-reachable |
| c06_stop:4 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c06_stop:5 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c07_eq64:5 | unknown · condition=["-inf", "+inf"], lhs=null, rhs=null | unknown · condition=["-inf", "+inf"], lhs=null, rhs=null | unknown · condition=["-inf", "+inf"], lhs=null, rhs=null |
| c07_eq64:6 | unknown · condition=["-inf", "+inf"], lhs=null, rhs=[0, 0] | unknown · condition=["-inf", "+inf"], lhs=null, rhs=[0, 0] | unknown · condition=["-inf", "+inf"], lhs=null, rhs=[0, 0] |
| c08_dead_branch:5 | unknown · ae-no-state | unknown · ae-no-state | unknown · ae-no-state |
| c09_loop:6 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c10_loop_unsafe:8 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c11_relational:7 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c12_interproc:4 | safe · condition=[1, 1] | safe · condition=[1, 1] | safe · condition=[1, 1] |
| c12_interproc:7 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c13_external_callback:4 | unknown · ae-no-state | unknown · ae-no-state | unknown · ae-no-state |
| c14_fnptr:4 | unknown · ae-reachable | unknown · ae-reachable | unknown · ae-reachable |
| c15_recursion:10 | safe · condition=[1, 1] | safe · condition=[1, 1] | safe · condition=[1, 1] |
| c16_stop_same_input:5 | unknown · ae-reachable | unknown · ae-reachable | unknown · ae-reachable |
| c16_stop_same_input:6 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c17_abort_path:7 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |
| c18_global:5 | safe · condition=[1, 1] | safe · condition=[1, 1] | safe · condition=[1, 1] |
| c18_global:7 | safe · condition=[1, 1] | safe · condition=[1, 1] | safe · condition=[1, 1] |
| c19_array:6 | safe · condition=[1, 1] | safe · condition=[1, 1] | safe · condition=[1, 1] |
| c19_array:7 | safe · condition=[1, 1] | safe · condition=[1, 1] | safe · condition=[1, 1] |
| c20_dead_function:3 | unknown · ae-no-state | unknown · ae-no-state | unknown · ae-no-state |
| c20_dead_function:7 | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] | unknown · condition=["-inf", "+inf"] |

AE returned `[1, 1]` for `c19_array:7` in all three modes, although `a[i] < 4` fails when `i = 3`. The reports preserve those three false-safe verdicts.

## SV-COMP

Safe and unknown count completed runs, and timing includes compilation and all run statuses.

| Mode | Safe | Unknown | Crash | Timeout | Error | Median s | Max s |
| --- | --- | --- | --- | --- | --- | --- | --- |
| dense | 0 | 227 | 0 | 0 | 0 | 0.080 | 4.420 |
| semi-sparse | 0 | 227 | 0 | 0 | 0 | 0.065 | 1.131 |
| sparse | 0 | 227 | 0 | 0 | 0 | 0.064 | 0.914 |

## What AE saw

These counts include all observed sites across both suites.

| Reason | dense | semi-sparse | sparse |
| --- | --- | --- | --- |
| ae-condition-true | 7 | 7 | 7 |
| ae-must-fail-if-reached | 1 | 1 | 1 |
| ae-condition-unknown | 13 | 13 | 13 |
| ae-reachable | 258 | 268 | 268 |
| ae-no-state | 89 | 79 | 79 |

Verified 741 run reports, 81 conformance site rows, and 681 SV-COMP task rows.

## SVF clean-tree proof

The local clone came from the pinned upstream objects already in the workspace.

```console
$ git -C svf rev-parse HEAD
a8a0f5181748059c2df2281e5e4ada63ccda3832
$ git -C svf status --porcelain
```

## Reproduce

From the `ae-reach-py` repository, run the following command with the pinned clone in `svf/` and the evaluation kit in the adjacent `ae-reach-eval/` directory.

```sh
mkdir -p build ../ae-reach-results/py &&
podman run --rm --pull=never --network=none --name ae-reach-py-evaluation \
  -v "$PWD:/src:ro" -v "$PWD/svf:/svf:ro" \
  -v "$PWD/build:/work/build:rw" -v "$PWD/../ae-reach-eval:/e:ro" \
  -v "$PWD/../ae-reach-results/py:/results:rw" \
  localhost/svf22-z3:llvm22 python3 /src/runner/runner.py "$(git rev-parse 585096e)"
```

## A2 verifier and runner refactor

| File | Lines | Responsibility |
| --- | ---: | --- |
| `verifier.cpp` | 150 | Runs AE on one module in one mode and emits each check's verdict, reason and interval evidence. |
| `runner.py` | 176 | Builds the verification tasks, compiles and runs them under the process limits, and aggregates the emitted verdicts into reports and JSONL. |

All 741 runs completed successfully. The 1,104 site records matched the baseline exactly, including all verdicts, reasons and intervals across 81 conformance records and 1,023 SV-COMP records. All 741 program verdicts and all 681 SV-COMP task verdicts matched. The baseline is preserved in `../ae-reach-results/py-before-a2/`.

The measurement commit recorded in the new results is `22979d9113fb4f8db3207cf74a019bef7cf4a94c`.

```console
$ git -C svf rev-parse HEAD
a8a0f5181748059c2df2281e5e4ada63ccda3832
$ git -C svf status --porcelain
```

## A2 Verifier module

| File | Lines | Responsibility |
| --- | ---: | --- |
| `include/Verifier/Verifier.h` | 44 | Declares the final-state collection API and typed site results. |
| `lib/Verifier/Verifier.cpp` | 52 | Visits SVF call nodes and reads their source locations. |
| `lib/Verifier/Reachability.cpp` | 80 | Identifies checks and evaluates verdicts using the public AE API. |
| `tools/verifier/verifier.cpp` | 80 | Loads the module, sets up and runs AE, and prints site results as JSON. |
| `runner/runner.py` | 177 | Compiles and runs verification tasks under the process limits and writes the reports and JSONL files. |

`grep -rn llvm include/Verifier lib/Verifier` returned no matches. The `Verifier` library built as a separate CMake target. Both library compilation commands contain only project and SvfCore include paths, as recorded in `build/wrapper/compile_commands.json`.

All 741 runs succeeded. All 1,104 site records matched the saved baseline, including verdicts, reasons, intervals, and locations. All 741 program verdicts and 681 SV-COMP task rows matched, with elapsed time excluded from the task-row comparison. ICFG iteration changed the site order in 21 reports, so site records were compared as multisets. The baseline is preserved in `../ae-reach-results/py-before-verifier-module/`.

The final measurement commit is `585096e37855b04b960438b413b214e93b374943`.

```console
$ git -C svf rev-parse HEAD
a8a0f5181748059c2df2281e5e4ada63ccda3832
$ git -C svf status --porcelain
```

## A2 publication

After accepting the repository invitation, the regular push to `ExcitedSpider/svf-ae-verifier` succeeded and established tracking for `origin/main`.

```text
$ git ls-files
.gitignore
CMakeLists.txt
PROGRESS.md
README.md
REPORT.md
SCHEMA.md
include/Verifier/Verifier.h
lib/Verifier/Reachability.cpp
lib/Verifier/Verifier.cpp
runner/runner.py
summarize.py
tools/verifier/verifier.cpp
```

```text
$ git log --oneline origin/main | head
667199c Record the A2 publication attempt
6410689 Prepare the A2 verifier for public use
09f187f Document the Verifier module and baseline comparison
585096e Separate the LLVM-free SVF Verifier library from its tool
865641d Record A2 comparison and update the verifier documentation
22979d9 Move final-state site verdicts into the C++ verifier
2c31b11 Document and verify the 741-run AE evaluation
ee2157d Construct LLVM 22 JSON objects explicitly
305fcf8 Prepare the SVF build-tree include path and verify extractor output
19e9105 Run isolated AE measurements and generate JSON reports
```

The published tree excludes generated reports. The preserved history contains the original conformance example report.

## A3 buffer overflow

All 264 verifier runs and all 264 stock AE runs succeeded on 88 supplied bitcode tests across three modes. The runs used Test-Suite commit `27bd85e7c4be8e1685e2a8358c0e4ecf956b9afa` and the primary CTest flags, including `-field-limit=1024`.

### Oracle labels

| Mode | SAFE correct safe | SAFE unknown | UNSAFE false safe | UNSAFE unknown |
| --- | ---: | ---: | ---: | ---: |
| Dense | 146 | 30 | 4 | 79 |
| Semi-sparse | 146 | 30 | 4 | 79 |
| Sparse | 146 | 30 | 4 | 79 |

Three char-alloca copy cases supply 10 bytes for a 10-byte object, and `ExtAPI_strcat_04:10` supplies 15 bytes for a 15-byte object. The unsafe labels reflect the preceding string or copy operation, while the literal oracle widths fit at offset zero. The reports preserve the bounds verdicts and count them as false safes under the supplied labels.

### Load and store reasons

| Reason | Dense | Semi-sparse | Sparse |
| --- | ---: | ---: | ---: |
| `ae-in-bounds` | 1,671 | 1,671 | 1,671 |
| `ae-out-of-bounds-if-reached` | 0 | 0 | 0 |
| `ae-bounds-unknown` | 5 | 5 | 5 |
| `ae-no-state` | 0 | 0 | 0 |
| `ae-object-unknown` | 4,922 | 4,922 | 4,922 |

The verifier checks SVF load and store statements and labelled oracle calls. Library calls such as `memcpy`, `strcpy` and `strcat` are outside its access model.

### Stock AE comparison

| Mode | Safe with stock alarm | Unknown without stock alarm | Safe without stock alarm | Unknown with stock alarm | Stock alarms without an access match | Accesses without source coordinates |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Dense | 29 | 273 | 1,378 | 7 | 26 of 58 | 4,911 |
| Semi-sparse | 29 | 273 | 1,378 | 7 | 26 of 58 | 4,911 |
| Sparse | 29 | 273 | 1,378 | 7 | 28 of 60 | 4,911 |

The comparison matches exact file, line and column within each run. Stock AE commonly reports on a GEP or an external call, so a shared coordinate can refer to different IR statements.

### Public AE evidence and gaps

| Fact | Public API at the pinned commit |
| --- | --- |
| Final pointer and integer values | `hasAbsState` and guarded `hasAbsValue` and `getAbsValue` read the retained trace. Sparse value lookups use the definition node. A missing value, null address or black-hole address leaves the object unresolved. |
| Base-relative bytes | `getGepByteOffset` reads a single GEP's operands at its node. For `p = buf + i; q = p + j`, the second offset is relative to `p`. The final address set exposes field-object IDs, and collapsed fields can represent several offsets. Interior pointers read back from memory have the same gap. |
| Read-only address lookup | `getGepObjAddrs` computes forward targets and inserts state and field objects. For a pointer into an array, calling it after AE would alter the trace. Collection reads existing addresses. |
| Dynamic offset precision | The GEP helper clamps dynamic index terms to the field limit and clamps negative variable terms to zero. For an index spanning negative and positive values, the public helper's interval can lose the negative portion. |
| Object extent | `getAllocaInstByteSize` uses exact metadata for a constant stack or global object. Its dynamic path reads the defining state but caps bounds and uses an element width of one. For `int a[n]`, collection uses the alloca element count and SVF type width. Heap extents use the final size arguments for `malloc`, `calloc` and `realloc`. An opaque allocator returning a pointer has no readable byte extent. |
| Access width | Load and store widths come from the SVF type of the value being transferred, and oracle widths come from AE. A synthetic statement with no usable type width has no width evidence. |

Collection uses a single GEP offset when the final input points directly to identifiable base objects and leaves missing base-relative offsets unknown.

### Verification and reproduction

| File | Lines | Change |
| --- | ---: | --- |
| `lib/Verifier/BufferOverflow.cpp` | 244 | Discovers accesses, reads final facts and applies the bounds table. |
| `tools/verifier/verifier.cpp` | 107 | Adds property selection, passes AE flags through and serializes the evidence. |
| `runner/runner.py` | 263 | Runs Test-Suite bitcode and stock AE under the process limits and writes their reports. |

The reachability regression matched A2 across 741 runs, 1,104 complete site records, and all 681 task verdicts. The overflow results contain 20,571 site rows. The library built without LLVM include paths in all three compilation commands, and `grep -rn llvm include/Verifier lib/Verifier` returned no matches.

Measurement commit `8bd93c11f50156736285664cfe2ecf82912c9876`.

```console
$ git -C svf rev-parse HEAD
a8a0f5181748059c2df2281e5e4ada63ccda3832
$ git -C svf status --porcelain
```

Run from the repo root with `../SVF/Test-Suite` at the recorded revision.

```sh
mkdir -p build ../ae-reach-results/overflow &&
podman run --rm --pull=never --network=none --name ae-reach-py-overflow-evaluation \
  -v "$PWD:/src:ro" -v "$PWD/svf:/svf:ro" -v "$PWD/build:/work/build:rw" \
  -v "$PWD/../SVF/Test-Suite:/tests:ro" -v "$PWD/../ae-reach-results/overflow:/results:rw" \
  localhost/svf22-z3:llvm22 python3 /src/runner/runner.py "$(git rev-parse 8bd93c1)" --property overflow
```
