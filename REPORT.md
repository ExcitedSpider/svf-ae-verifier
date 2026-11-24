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
  localhost/svf22-z3:llvm22 python3 /src/run.py "$(git rev-parse ee2157d)"
```
