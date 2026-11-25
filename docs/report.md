# JSON report

After a successful analysis the verifier writes one JSON object to stdout by default or to the path given with `-o`. Diagnostics go to stderr. Standalone help prints usage.

The machine-readable definition is [`schema/report.schema.json`](../schema/report.schema.json) (JSON Schema 2020-12). It fixes the field types, the verdicts, and the `kind`, `reason` and evidence keys allowed for each property; a new kind or reason needs a schema change. The container image ships it at `/usr/local/share/svf-ae-verifier/report.schema.json`, and each release attaches it. The test suite validates every report it produces against it, and a new `schema_version` means an incompatible change.

| Field | Meaning |
|---|---|
| `schema_version` | Integer report format version, currently `1`. |
| `tool_version`, `tool_commit` | Verifier version and Git commit captured at configuration. Release builds report the release version (set with `-DVERIFIER_RELEASE_VERSION`); other builds report the project version in `CMakeLists.txt`. Builds without Git metadata can pass `-DVERIFIER_GIT_COMMIT=<sha>`. If neither source is available, the revision is `unknown`. |
| `svf_version`, `svf_commit` | SVF package version and exact source revision. The revision is `unknown` when build metadata is unavailable. |
| `llvm_version` | Exact LLVM package version used to build the verifier. |
| `command_line` | Original argument vector, including the invoked executable. |
| `ae_mode` | `dense`, `semi-sparse` or `sparse`. |
| `status` | `ok` for a finished analysis. |
| `time_unix_ms` | Report completion time in milliseconds since the Unix epoch. |
| `properties` | Object mapping property names to program verdicts. |
| `sites` | Array with a record for every reported site. |

Each site has `property`, `kind`, `location`, `verdict`, `reason` and `evidence`. Location contains `file`, `line`, `column` and `function`. Unknown numbers use zero and unknown strings are empty. Reach intrinsics can supply a source line independent of debug columns.

The overflow kinds `load` and `store` identify ordinary memory instructions. `call-read` and `call-write` identify separate accesses at an annotated call and include the SVF callee name in `callee`.

Evidence contains named intervals, represented by two inclusive bounds or null for unavailable values. Finite bounds are JSON integers. Infinite bounds use the strings `"-inf"` and `"+inf"`. Overflow evidence uses load/store `width`, call `length`, and per-target `offset.<object-id>` and `size.<object-id>`, all in bytes. SVF object IDs are local to the run. The reason `ae-length-unknown` indicates that AE did not supply enough usable byte-length evidence to decide the call access.

## Example

This report was emitted for `verifier_assert(1)` on line 5 of `example.c` after canonical compilation and `mem2reg`.

```json
{
  "ae_mode": "sparse",
  "command_line": [
    "./svf-verifier",
    "-property=reach",
    "-ae-sparsity=sparse",
    "example.ll",
    "-stat=false"
  ],
  "llvm_version": "22.1.8",
  "properties": {
    "reach": "proved"
  },
  "schema_version": 1,
  "sites": [
    {
      "evidence": {
        "condition": [
          1,
          1
        ]
      },
      "kind": "verifier_assert",
      "location": {
        "column": 5,
        "file": "example.c",
        "function": "main",
        "line": 5
      },
      "property": "reach",
      "reason": "ae-condition-true",
      "verdict": "proved"
    }
  ],
  "status": "ok",
  "svf_commit": "a8a0f5181748059c2df2281e5e4ada63ccda3832",
  "svf_version": "3.4",
  "time_unix_ms": 1795565984309,
  "tool_commit": "unknown",
  "tool_version": "0.1.0"
}
```
