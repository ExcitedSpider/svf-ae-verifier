# Per-run JSON schema, version 1

Each file under `reports/<suite>/<mode>/` records one C input and one AE mode.
See [example-report.json](example-report.json) for a real run.

| Field | Meaning |
| --- | --- |
| `schema_version` | Integer, currently 1 |
| `tool_commit`, `svf_commit` | Git SHAs of the measurement code and pinned SVF snapshot |
| `suite`, `task`, `program` | Suite name, kit identifier and container source path |
| `mode` | `dense`, `semi-sparse`, or `sparse` |
| `command` | Argument vectors for Clang, opt and the verifier, in execution order |
| `status` | `ok`, `crash` (signal), `timeout` (deadline), or `error` (other failure) |
| `diagnostic` | Null on success, otherwise a failure message and available process output |
| `seconds` | Elapsed wall-clock seconds for compilation and analysis |
| `sites` | One object per check call on success, with fields below. Empty when verification fails. |
| `verdict` | `safe` if the run succeeded and all sites are safe, otherwise `unknown` |

| Site field | Meaning |
| --- | --- |
| `file`, `line`, `column`, `function` | LLVM source location and parent function. Verifier macro lines come from the explicit line argument. |
| `kind` | `verifier_assert`, `verifier_eq`, `verifier_unreachable`, or `reach_error` |
| `has_state` | Boolean presence of a final AE state at the call node |
| `intervals` | Named intervals with two inclusive bounds, using integers or the strings `"-inf"` and `"+inf"`. Null represents a missing value or a value without an interval. Assertions have `condition`, and equality sites also have `lhs` and `rhs`. The object is empty for unconditional checks. |
| `verdict`, `reason` | The per-site result and the applicable AE reason supplied by the verifier |

The verifier writes one site object per line on stdout and sends SVF diagnostics to stderr. The runner copies the site objects into the report after a successful process exit. The runner checks conformance line coverage against the manifest. When a conformance run fails, `sites.jsonl` records an `error` entry with `run-<status>` for each expected line.

For equality checks, `ae-condition-true` requires AE to report `[1, 1]` for the C comparison and the same finite singleton for both reporting operands. A condition of `[0, 0]` produces `ae-must-fail-if-reached`. The verifier uses the final call-node state and AE's own lookup for sparse operands. Cases with no usable interval produce `ae-condition-unknown` if the node has a state.

`sites.jsonl` and `tasks.jsonl` are derived from the reports. Witnesses are null. Conformance dependencies are empty because the driver adds no assumptions about earlier checks.
