"""Build the verifier and run it on the read-only evaluation kit."""

import argparse
from concurrent.futures import ThreadPoolExecutor
import json
import os
from pathlib import Path
import re
import signal
import subprocess
import tempfile
import time

MODES = ("dense", "semi-sparse", "sparse")
SVF_COMMIT = "a8a0f5181748059c2df2281e5e4ada63ccda3832"
KIT = Path("/e")
BUILD = Path("/work/build")
LLVM = Path("/opt/llvm/bin")
VERIFIER = BUILD / "wrapper/verifier"
STOCK_AE = BUILD / "bin/ae"
OVERFLOW_TESTS = Path("/tests/test_cases_bc/ae_overflow_tests")
ALARM_HEADER = re.compile(r"^#+Buffer Overflow \((\d+) found\)#+$", re.MULTILINE)
ALARM_LOCATION = re.compile(r'"ln": (\d+), "cl": (\d+), "fl": "([^"]*)"')
LIMIT = 120
MEMORY = 4 * 1024**3


def build(property_name="reach"):
    # SVF's build-tree export advertises this generated include directory.
    (BUILD / "include/SVF").mkdir(parents=True, exist_ok=True)
    settings = ["-DCMAKE_BUILD_TYPE=Release", "-DLLVM_DIR=/opt/llvm/lib/cmake/llvm"]
    commands = [
        ["cmake", "-S", "/svf", "-B", BUILD, "-DSVF_WARN_AS_ERROR=OFF", *settings],
        ["cmake", "--build", BUILD, "--target", "SvfLLVM", "-j", "8"],
        ["cmake", "-S", "/src", "-B", BUILD / "wrapper",
         "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON", *settings],
        ["cmake", "--build", BUILD / "wrapper", "-j", "8"],
    ]
    if property_name == "overflow":
        commands.append(["cmake", "--build", BUILD, "--target", "ae", "-j", "8"])
    for command in commands:
        subprocess.run(command, check=True)


def read_lines(path):
    return [json.loads(line) for line in path.read_text().splitlines()]


def write_json(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(".tmp")
    temporary.write_text(json.dumps(data, indent=2, allow_nan=False) + "\n")
    temporary.replace(path)


def write_lines(path, rows):
    with path.open("w") as output:
        for row in rows:
            print(json.dumps(row, allow_nan=False), file=output)


def run_process(command, directory, deadline, stdout=None, combined=False):
    with tempfile.TemporaryFile() as log:
        process = subprocess.Popen(
            ["prlimit", f"--as={MEMORY}:{MEMORY}", "--", *command],
            cwd=directory, stdout=log if stdout is None else stdout,
            stderr=subprocess.STDOUT if combined else log,
            start_new_session=True,
        )
        try:
            code = process.wait(timeout=max(0, deadline - time.monotonic()))
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            process.wait()
            return "timeout", f"Exceeded {LIMIT} seconds"
        if not code:
            return "ok", None
        log.seek(max(0, log.tell() - 4096))
        return ("crash" if code < 0 else "error",
                f"Exit status {code}\n" + log.read().decode(errors="replace"))


def measure(source, mode, expected_lines, property_name="reach"):
    start = time.monotonic()
    commands = [
        [str(LLVM / "clang"), "-g", "-S", "-c", "-emit-llvm", "-Xclang",
         "-disable-O0-optnone", "-fno-discard-value-names", "-I/e/include",
         str(source), "-o", "f.ll"],
        [str(LLVM / "opt"), "-S", "-p=mem2reg", "f.ll", "-o", "f.ll"],
        [str(VERIFIER), f"-ae-sparsity={mode}", "f.ll"],
    ] if property_name == "reach" else [
        [str(VERIFIER), "-property=overflow", f"-ae-sparsity={mode}",
         "-field-limit=1024", str(source)],
    ]
    status, diagnostic, sites = "ok", None, []
    with tempfile.TemporaryDirectory(prefix="ae-reach-py-") as directory:
        try:
            for command in commands[:-1]:
                status, diagnostic = run_process(command, directory, start + LIMIT)
                if status != "ok":
                    break
            if status == "ok":
                output = Path(directory) / "verdicts"
                with output.open("wb") as stdout:
                    status, diagnostic = run_process(commands[-1], directory, start + LIMIT, stdout)
                if status == "ok":
                    sites = read_lines(output)
                    if any(site["verdict"] not in ("safe", "unknown") or not site["reason"]
                           for site in sites):
                        raise ValueError("Invalid site verdict from verifier")
                    if expected_lines is not None and sorted(site["line"] for site in sites) != sorted(expected_lines):
                        raise ValueError("Verifier site lines differ from the conformance manifest")
        except (OSError, ValueError, KeyError, TypeError) as error:
            status, diagnostic, sites = "error", str(error), []
    return dict(mode=mode, command=commands, status=status, diagnostic=diagnostic,
                seconds=round(time.monotonic() - start, 6), sites=sites)


def stock_overflow(bitcode, mode):
    command = [str(STOCK_AE)]
    if mode != "dense":
        command.append(f"-ae-sparsity={mode}")
    command.extend(["-overflow", "-field-limit=1024", str(bitcode)])
    start = time.monotonic()
    with tempfile.TemporaryDirectory(prefix="ae-reach-stock-") as directory:
        log = Path(directory) / "ae.log"
        try:
            with log.open("wb") as output:
                status, diagnostic = run_process(command, directory, start + LIMIT,
                                                 output, combined=True)
            text = log.read_text(errors="replace")
        except OSError as error:
            status, diagnostic, text = "error", str(error), ""
    if status != "ok":
        diagnostic = "\n".join(filter(None, (diagnostic, text[-4096:])))
    header = ALARM_HEADER.search(text)
    alarms = []
    if header:
        blocks = [block.strip() for block in re.split(r"^-{20,}$", text[header.end():],
                                                     flags=re.MULTILINE) if block.strip()]
        for block in blocks[:int(header[1])]:
            matches = list(ALARM_LOCATION.finditer(block))
            if matches:
                line, column, file = matches[-1].groups()
                alarms.append(dict(file=file, line=int(line), column=int(column)))
            else:
                alarms.append(dict(location=None, diagnostic=block))
    return dict(command=command, status=status, diagnostic=diagnostic, alarms=alarms,
                alarm_count=int(header[1]) if header else 0,
                seconds=round(time.monotonic() - start, 6))


def evaluate_overflow(output, tool_commit, jobs):
    tests = sorted(path for path in OVERFLOW_TESTS.glob("*.c.bc")
                   if not path.name.endswith("_no-main.c.bc"))
    if not tests:
        raise ValueError(f"No Test-Suite bitcode found under {OVERFLOW_TESTS}")

    def run(job):
        bitcode, mode = job
        test = bitcode.name.removesuffix(".c.bc")
        report = measure(bitcode, mode, None, "overflow")
        verdict = ("safe" if report["status"] == "ok" and all(
            site["verdict"] == "safe" for site in report["sites"]) else "unknown")
        report.update(schema_version=1, tool_commit=tool_commit, svf_commit=SVF_COMMIT,
                      suite="overflow", property="overflow", task=test, program=str(bitcode),
                      verdict=verdict, stock_ae=stock_overflow(bitcode, mode))
        write_json(output / "reports" / mode / f"{test}.json", report)
        return report

    rows = []
    tasks = [(test, mode) for test in tests for mode in MODES]
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        for count, report in enumerate(executor.map(run, tasks), 1):
            for site in report["sites"]:
                row = {field: site[field] for field in ("line", "kind", "verdict", "reason")}
                row.update(test=report["task"], mode=report["mode"])
                if "expected" in site:
                    row["expected"] = site["expected"]
                rows.append(row)
            if count % 50 == 0 or count == len(tasks):
                print(f"Completed {count}/{len(tasks)} overflow pairs", flush=True)
    write_lines(output / "sites.jsonl", rows)
    (output / "README").write_text(
        f"Tool commit {tool_commit}\nSVF commit {SVF_COMMIT}\n"
        f"{len(tasks)} per-run JSON reports in reports/.\n"
    )


def evaluate(output, tool_commit, jobs):
    # Use the conformance manifest for required line IDs only. Outcomes are
    # compared separately when the aggregate report is generated.
    manifest = json.loads((KIT / "cases/truth.json").read_text())
    tasks = json.loads((KIT / "svcomp/tasks.json").read_text())
    programs = [
        ("cases", name, KIT / "cases" / f"{name}.c", {s["line"] for s in entries})
        for name, entries in sorted(manifest.items())
    ] + [
        ("svcomp", task["id"], KIT / "svcomp/src" / task["input"], None)
        for task in tasks
    ]

    def run(job):
        (suite, task, source, lines), mode = job
        report = measure(source, mode, lines)
        sites = report["sites"]
        verdict = ("safe" if report["status"] == "ok" and all(
            site["verdict"] == "safe" for site in sites) else "unknown")
        report.update(schema_version=1, tool_commit=tool_commit, svf_commit=SVF_COMMIT,
                      suite=suite, task=task, program=str(source), verdict=verdict)
        write_json(output / "reports" / suite / mode / f"{task}.json", report)
        return report

    site_rows, task_rows = [], []
    with ThreadPoolExecutor(max_workers=jobs) as executor:
        runs = ((program, mode) for program in programs for mode in MODES)
        for count, report in enumerate(executor.map(run, runs), 1):
            common = {key: report[key] for key in ("task", "mode")}
            if report["suite"] == "cases":
                sites = report["sites"]
                if report["status"] != "ok":
                    # The JSONL format requires a row even when verification failed.
                    sites = [dict(line=entry["line"], verdict="error",
                                  reason=f'run-{report["status"]}')
                             for entry in manifest[report["task"]]]
                for site in sites:
                    site_rows.append(dict(common, line=site["line"], verdict=site["verdict"],
                                          reason=site["reason"], witness=None, depends_on=[]))
            else:
                status, verdict = report["status"], report["verdict"]
                reason = f"run-{status}" if status != "ok" else (
                    "all-sites-safe" if verdict == "safe" else "site-unknown")
                task_rows.append(dict(common, status=status, verdict=verdict, witness=None,
                                      seconds=report["seconds"], sites=len(report["sites"]),
                                      reason=reason))
            if count % 50 == 0 or count == len(programs) * len(MODES):
                print(f"Completed {count}/{len(programs) * len(MODES)} runs", flush=True)
    write_lines(output / "sites.jsonl", site_rows)
    write_lines(output / "tasks.jsonl", task_rows)
    (output / "README").write_text(
        f"Tool commit {tool_commit}\nSVF commit {SVF_COMMIT}\n"
        f"{len(programs) * len(MODES)} per-run JSON reports in reports/.\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("tool_commit")
    parser.add_argument("--jobs", type=int, choices=range(1, 9), default=8)
    parser.add_argument("--output", type=Path, default=Path("/results"))
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--property", choices=("reach", "overflow"), default="reach")
    args = parser.parse_args()
    if not args.skip_build:
        build(args.property)
    if args.property == "overflow":
        evaluate_overflow(args.output, args.tool_commit, args.jobs)
    else:
        evaluate(args.output, args.tool_commit, args.jobs)


if __name__ == "__main__":
    main()
