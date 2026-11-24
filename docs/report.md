# JSON report

The verifier writes one JSON object. Analysis diagnostics go to stderr. Reports go to stdout by default or to the path given with `-o`. Bad-usage reports always go to stdout. Standalone help prints usage. A fatal signal or a report output failure can prevent report creation.

| Field | Meaning |
|---|---|
| `schema_version` | Integer report format version, currently `1`. |
| `tool_version`, `tool_commit` | Verifier version and Git commit captured at configuration. Builds without Git metadata can pass `-DVERIFIER_GIT_COMMIT=<sha>`. If neither source is available, the revision is `unknown`. |
| `svf_version`, `svf_commit` | SVF package version and supported revision. |
| `command_line` | Original argument vector, including the invoked executable. |
| `ae_mode` | `dense`, `semi-sparse` or `sparse`. It is null if options could not be parsed. |
| `status` | `ok` for a finished analysis, `error` for a reported failure. |
| `time_unix_ms` | Report completion time in milliseconds since the Unix epoch. |
| `error` | Diagnostic summary on an error report. |
| `properties` | Object mapping property names to program verdicts. |
| `sites` | Array with a record for every reported site. |

Each site has `property`, `kind`, `location`, `verdict`, `reason` and `evidence`. Location contains `file`, `line`, `column` and `function`. Unknown numbers use zero and unknown strings are empty. Reach intrinsics can supply a source line independent of debug columns.

Evidence contains named intervals, represented by two inclusive bounds or null for unavailable values. Finite bounds are JSON integers. Infinite bounds use the strings `"-inf"` and `"+inf"`. Overflow evidence uses `width` and per-target `offset.<address>` and `size.<address>` keys. Addresses are SVF abstract identifiers local to the run.
