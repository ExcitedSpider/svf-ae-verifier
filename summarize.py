"""Verify evaluation coverage and render the measurement tables."""

from collections import Counter
import json
from pathlib import Path
from statistics import median

from runner import KIT, MODES, SVF_COMMIT, read_lines


REASONS = (
    "ae-condition-true", "ae-must-fail-if-reached", "ae-condition-unknown",
    "ae-reachable", "ae-no-state",
)


def table(headers, rows):
    print("| " + " | ".join(headers) + " |")
    print("| " + " | ".join(["---"] * len(headers)) + " |")
    for row in rows:
        print("| " + " | ".join(map(str, row)) + " |")


def show(site):
    intervals = ", ".join(
        f"{key}={json.dumps(value)}" for key, value in site["intervals"].items()
    )
    return f'{site["verdict"]} · {intervals or site["reason"]}'


def summary(output):
    truth = json.loads((KIT / "cases/truth.json").read_text())
    tasks = json.loads((KIT / "svcomp/tasks.json").read_text())
    reports = {}
    for path in (output / "reports").rglob("*.json"):
        report = json.loads(path.read_text())
        key = tuple(report[field] for field in ("suite", "task", "mode"))
        assert key not in reports
        assert report["svf_commit"] == SVF_COMMIT
        assert report["verdict"] in ("safe", "unknown")
        reports[key] = report

    case_keys = {(case, mode, site["line"]) for case, sites in truth.items()
                 for site in sites for mode in MODES}
    task_keys = {(task["id"], mode) for task in tasks for mode in MODES}
    assert set(reports) == ({("cases", case, mode) for case in truth for mode in MODES}
                            | {("svcomp", task, mode) for task, mode in task_keys})
    tool_commits = {report["tool_commit"] for report in reports.values()}
    assert len(tool_commits) == 1
    assert next(iter(tool_commits)) in (output / "README").read_text()

    observed = {}
    reason_counts = {mode: Counter() for mode in MODES}
    for (suite, name, mode), report in reports.items():
        assert report["verdict"] == (
            "safe" if report["status"] == "ok"
            and all(site["verdict"] == "safe" for site in report["sites"]) else "unknown"
        )
        for site in report["sites"]:
            reason_counts[mode][site["reason"]] += 1
            assert site["verdict"] in ("safe", "unknown")
            if suite == "cases":
                key = name, mode, site["line"]
                assert key not in observed, key
                observed[key] = site
    assert set(observed) == case_keys

    site_rows = read_lines(output / "sites.jsonl")
    assert len(site_rows) == len(case_keys)
    assert {(row["task"], row["mode"], row["line"]) for row in site_rows} == case_keys
    for row in site_rows:
        site = observed[row["task"], row["mode"], row["line"]]
        assert (row["verdict"], row["reason"]) == (site["verdict"], site["reason"])
        assert row["witness"] is None and row["depends_on"] == []
    task_rows = read_lines(output / "tasks.jsonl")
    assert len(task_rows) == len(task_keys)
    assert {(row["task"], row["mode"]) for row in task_rows} == task_keys
    for row in task_rows:
        run = reports["svcomp", row["task"], row["mode"]]
        assert all(row[key] == run[key] for key in ("status", "verdict", "seconds"))
        assert row["sites"] == len(run["sites"]) and row["witness"] is None

    print("## Conformance\n")
    print("The table lists each verdict and the final intervals available at that check.\n")
    table(("Case and line", *MODES), (
        (f"{case}:{site['line']}", *(show(observed[case, mode, site["line"]]) for mode in MODES))
        for case, sites in sorted(truth.items()) for site in sites
    ))
    false_safe = [(case, site["line"], mode)
                  for case, sites in sorted(truth.items()) for site in sites for mode in MODES
                  if site["truth"] == "unsafe"
                  and observed[case, mode, site["line"]]["verdict"] == "safe"]
    if false_safe:
        details = ", ".join(f"`{case}:{line}` in {mode}" for case, line, mode in false_safe)
        print(f"\nThe kit marks these sites unsafe, and the table preserves AE's safe verdicts for {details}.")
    else:
        print("\nEvery conformance verdict of safe agrees with the kit's reference outcomes.")

    print("\n## SV-COMP\n")
    print("Safe and unknown count completed runs, and timing includes compilation and all run statuses.\n")
    timing = []
    for mode in MODES:
        rows = [row for row in task_rows if row["mode"] == mode]
        counts = Counter(row["verdict"] if row["status"] == "ok" else row["status"] for row in rows)
        seconds = [row["seconds"] for row in rows]
        timing.append((mode, *(counts[key] for key in ("safe", "unknown", "crash", "timeout", "error")),
                       f"{median(seconds):.3f}", f"{max(seconds):.3f}"))
    table(("Mode", "Safe", "Unknown", "Crash", "Timeout", "Error", "Median s", "Max s"), timing)

    print("\n## What AE saw\n")
    print("These counts include all observed sites across both suites.\n")
    reasons = list(REASONS) + sorted(set().union(*reason_counts.values()) - set(REASONS))
    table(("Reason", *MODES), (
        (reason, *(reason_counts[mode][reason] for mode in MODES)) for reason in reasons
    ))
    print(f"\nVerified {len(reports)} run reports, {len(site_rows)} conformance site rows, "
          f"and {len(task_rows)} SV-COMP task rows.")


if __name__ == "__main__":
    summary(Path("/results"))
