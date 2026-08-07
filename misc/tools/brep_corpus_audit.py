#!/usr/bin/env python3
"""Audit BRep wireframe and fast shaded realizations in a directory of .g files.

The companion ``brep-audit`` executable does one BRep per process.  This
driver inventories each database, runs those isolated checks, checkpoints
every result to JSON Lines, and writes JSON and Markdown summaries.  Geometry
findings do not make the driver fail: they are the intended work list.
"""

import argparse
import datetime
import json
import os
import re
import signal
import subprocess
import sys
import tempfile
import time
from collections import Counter
from concurrent.futures import ThreadPoolExecutor, as_completed
from pathlib import Path


FORMAT = "brlcad-brep-corpus-audit-v1"
HELPER_FORMAT = "brlcad-brep-realization-audit-v1"


def file_identity(path):
    try:
        info = path.stat()
        return {"size": info.st_size, "mtime_ns": info.st_mtime_ns}
    except OSError:
        return None


def text_tail(value, limit=8000):
    return (value or "")[-limit:]


def last_phase(stderr):
    phases = re.findall(r"^brep-audit: phase=([a-z]+)$", stderr or "", re.MULTILINE)
    return phases[-1] if phases else None


def process_rss_bytes(pid):
    """Read current Linux RSS, returning zero when it is unavailable."""
    try:
        with open("/proc/{}/status".format(pid), "r", encoding="ascii") as stream:
            for line in stream:
                if line.startswith("VmRSS:"):
                    return int(line.split()[1]) * 1024
    except (OSError, ValueError, IndexError):
        pass
    return 0


def kill_process(process):
    try:
        if os.name == "posix":
            os.killpg(process.pid, signal.SIGKILL)
        else:
            process.kill()
    except (OSError, ProcessLookupError):
        pass


def file_tail(stream, limit):
    stream.flush()
    size = os.fstat(stream.fileno()).st_size
    stream.seek(max(0, size - limit))
    return stream.read().decode("utf-8", errors="replace"), size > limit


def run_process(command, timeout, max_rss_mib=0):
    """Run and monitor one helper, retaining enough detail for diagnostics."""
    started = time.monotonic()
    with tempfile.TemporaryFile() as stdout_file, \
            tempfile.TemporaryFile() as stderr_file:
        try:
            process = subprocess.Popen(
                command,
                stdout=stdout_file,
                stderr=stderr_file,
                start_new_session=(os.name == "posix"),
            )
        except OSError as error:
            return {
                "returncode": 127,
                "stdout": "",
                "stdout_truncated": False,
                "stderr": str(error),
                "elapsed_seconds": time.monotonic() - started,
                "observed_peak_rss_bytes": 0,
                "termination_reason": "launch_error",
            }

        peak_rss = 0
        termination_reason = None
        while True:
            elapsed = time.monotonic() - started
            remaining = timeout - elapsed
            if remaining <= 0:
                termination_reason = "timeout"
                kill_process(process)
                process.wait()
                break
            try:
                process.wait(timeout=min(0.1, remaining))
                peak_rss = max(peak_rss, process_rss_bytes(process.pid))
                break
            except subprocess.TimeoutExpired:
                rss = process_rss_bytes(process.pid)
                peak_rss = max(peak_rss, rss)
                if max_rss_mib and rss > max_rss_mib * 1024 * 1024:
                    termination_reason = "rss_limit"
                    kill_process(process)
                    process.wait()
                    break

        stdout, stdout_truncated = file_tail(
            stdout_file, 16 * 1024 * 1024
        )
        stderr, _ = file_tail(stderr_file, 64 * 1024)

    return {
        "returncode": process.returncode,
        "stdout": stdout,
        "stdout_truncated": stdout_truncated,
        "stderr": stderr,
        "elapsed_seconds": time.monotonic() - started,
        "observed_peak_rss_bytes": peak_rss,
        "termination_reason": termination_reason,
    }


def synthetic_record(database, object_name, status, issue, run, signature):
    return {
        "format": HELPER_FORMAT,
        "scope": "brep" if object_name else "database",
        "database": str(database),
        "object": object_name,
        "status": status,
        "reference": None,
        "wireframe": None,
        "shaded": None,
        "issues": [issue],
        "audit_signature": signature,
        "database_identity": file_identity(database),
        "process": {
            "return_code": run["returncode"],
            "elapsed_seconds": run["elapsed_seconds"],
            "observed_peak_rss_bytes": run["observed_peak_rss_bytes"],
            "termination_reason": run["termination_reason"],
            "last_phase": last_phase(run["stderr"]),
            "stdout_tail": text_tail(run["stdout"]),
            "stderr_tail": text_tail(run["stderr"]),
        },
    }


def parse_helper_json(output):
    for line in reversed(output.splitlines()):
        if not line.lstrip().startswith("{"):
            continue
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict):
            return value
    return None


def audit_one(helper, database, object_name, args, signature):
    command = [
        str(helper),
        "--ratio-min", str(args.ratio_min),
        "--ratio-max", str(args.ratio_max),
        "--tess-abs", str(args.tess_abs),
        "--tess-rel", str(args.tess_rel),
        "--tess-norm", str(args.tess_norm),
    ]
    if args.address_space_limit_mib:
        command.extend([
            "--memory-limit-mib", str(args.address_space_limit_mib)
        ])
    command.extend([str(database), object_name])
    run = run_process(command, args.timeout, args.max_rss_mib)

    if run["termination_reason"] == "timeout":
        phase = last_phase(run["stderr"])
        return synthetic_record(
            database, object_name, "timeout",
            "{}_timeout".format(phase) if phase else "process_timeout",
            run, signature
        )
    if run["termination_reason"] == "rss_limit":
        phase = last_phase(run["stderr"])
        return synthetic_record(
            database, object_name, "memory_limit",
            "{}_rss_limit_exceeded".format(phase)
            if phase else "process_rss_limit_exceeded",
            run, signature
        )
    if run["termination_reason"] == "launch_error":
        return synthetic_record(
            database, object_name, "error", "helper_launch_failed", run, signature
        )

    record = parse_helper_json(run["stdout"])
    if record is None:
        issue = (
            "process_signal_{}".format(-run["returncode"])
            if run["returncode"] is not None and run["returncode"] < 0
            else "helper_output_not_json"
        )
        return synthetic_record(
            database, object_name, "error", issue, run, signature
        )
    if record.get("format") != HELPER_FORMAT:
        return synthetic_record(
            database, object_name, "error", "unexpected_helper_format",
            run, signature
        )

    record["scope"] = "brep"
    record["audit_signature"] = signature
    record["database_identity"] = file_identity(database)
    record["process"] = {
        "return_code": run["returncode"],
        "elapsed_seconds": run["elapsed_seconds"],
        "observed_peak_rss_bytes": run["observed_peak_rss_bytes"],
        "termination_reason": None,
        "last_phase": last_phase(run["stderr"]),
        "stdout_tail": "",
        "stderr_tail": text_tail(run["stderr"]),
    }
    expected_return = 0 if record.get("status") == "ok" else 1
    if run["returncode"] != expected_return:
        record["status"] = "error"
        record.setdefault("issues", []).append("helper_exit_status_mismatch")
    return record


def list_breps(helper, database, timeout, signature):
    run = run_process([str(helper), "--list", str(database)], timeout)
    if run["termination_reason"] == "timeout":
        return [], synthetic_record(
            database, "", "timeout", "brep_inventory_timeout", run, signature
        )
    if run["returncode"] != 0:
        return [], synthetic_record(
            database, "", "error", "brep_inventory_failed", run, signature
        )
    if run["stdout_truncated"]:
        return [], synthetic_record(
            database, "", "error", "brep_inventory_output_too_large",
            run, signature
        )
    names = sorted(set(line.strip() for line in run["stdout"].splitlines()
                       if line.strip()))
    return names, None


def git_state(start):
    try:
        revision = subprocess.check_output(
            ["git", "-C", str(start), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL, text=True,
        ).strip()
        dirty = bool(subprocess.check_output(
            ["git", "-C", str(start), "status", "--porcelain"],
            stderr=subprocess.DEVNULL, text=True,
        ).strip())
        return revision, dirty
    except (OSError, subprocess.CalledProcessError):
        return "unknown", None


def read_checkpoint(path, signature):
    records = {}
    if not path.is_file():
        return records
    with path.open("r", encoding="utf-8") as stream:
        for number, line in enumerate(stream, 1):
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                print("ignoring malformed checkpoint line {} in {}".format(
                    number, path), file=sys.stderr)
                continue
            if record.get("audit_signature") != signature:
                continue
            records[(record.get("database", ""), record.get("object", ""))] = record
    return records


def all_issues(record):
    issues = list(record.get("issues") or [])
    for mode in ("wireframe", "shaded"):
        result = record.get(mode)
        if result:
            issues.extend(result.get("issues") or [])
    return list(dict.fromkeys(str(issue) for issue in issues))


def format_vector(values):
    if not values:
        return "-"
    return " × ".join("{:.5g}".format(value) for value in values)


def format_geometry(result, primitive):
    if not result:
        return "-"
    ratios = result.get("dimension_ratios")
    ratio_text = ""
    if ratios:
        ratio_text = "; ratios " + "/".join(
            "-" if value is None else "{:.3g}".format(value)
            for value in ratios
        )
    return "{} points, {} {}{}".format(
        result.get("vertices", 0), result.get("primitives", 0),
        primitive, ratio_text,
    )


def markdown_escape(value):
    return str(value).replace("|", "\\|").replace("\n", "<br>")


def worklist_markdown(summary, input_directory):
    totals = summary["totals"]
    lines = [
        "# BRep realization audit",
        "",
        "Revision: `{}`{}".format(
            summary["revision"], " (dirty)" if summary.get("dirty") else ""
        ),
        "",
        (
            "{} databases, {} BReps: {} okay, {} geometry findings, "
            "{} timeouts, {} memory-limit stops, and {} process errors."
        ).format(
            totals["databases"], totals["breps"], totals["ok"], totals["fail"],
            totals["timeout"], totals["memory_limit"], totals["error"],
        ),
        "",
        (
            "Accepted generated/reference dimension ratios: {}–{}. "
            "Shaded tessellation tolerance: abs {}, rel {}, norm {}."
        ).format(
            summary["options"]["ratio_min"], summary["options"]["ratio_max"],
            summary["options"]["tess_abs"], summary["options"]["tess_rel"],
            summary["options"]["tess_norm"],
        ),
        "",
        "## Work list",
        "",
    ]
    findings = [
        record for record in summary["results"]
        if record.get("status") != "ok"
    ]
    if not findings:
        lines.append("No findings.")
        return "\n".join(lines) + "\n"

    lines.extend([
        "| Database | BRep | Status | Reference dimensions | Wireframe | Shaded | Peak RSS MiB | Issues |",
        "|---|---|---|---|---|---|---:|---|",
    ])
    for record in findings:
        database = Path(record.get("database", ""))
        try:
            database_name = str(database.relative_to(input_directory))
        except ValueError:
            database_name = str(database)
        wire = record.get("wireframe")
        shaded = record.get("shaded")
        peaks = [
            int(result.get("peak_rss_bytes") or 0)
            for result in (wire, shaded) if result
        ]
        process = record.get("process") or {}
        peaks.append(int(process.get("observed_peak_rss_bytes") or 0))
        reference = record.get("reference") or {}
        lines.append("| {} | {} | {} | {} | {} | {} | {:.1f} | {} |".format(
            markdown_escape(database_name),
            markdown_escape(record.get("object") or "(inventory)"),
            markdown_escape(record.get("status", "error")),
            markdown_escape(format_vector(reference.get("dimensions"))),
            markdown_escape(format_geometry(wire, "segments")),
            markdown_escape(format_geometry(shaded, "triangles")),
            max(peaks, default=0) / (1024.0 * 1024.0),
            markdown_escape(", ".join(all_issues(record)) or "unspecified"),
        ))
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_directory", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument(
        "--brep-audit", type=Path, default=Path(".build/bin/brep-audit")
    )
    parser.add_argument("-j", "--jobs", type=int, default=1,
                        help="BRep processes to run concurrently (default: 1)")
    parser.add_argument("--timeout", type=float, default=300.0,
                        help="seconds allowed per BRep")
    parser.add_argument("--list-timeout", type=float, default=120.0,
                        help="seconds allowed to inventory one database")
    parser.add_argument("--ratio-min", type=float, default=0.5)
    parser.add_argument("--ratio-max", type=float, default=2.0)
    parser.add_argument("--tess-abs", type=float, default=0.0)
    parser.add_argument("--tess-rel", type=float, default=0.01)
    parser.add_argument("--tess-norm", type=float, default=0.0)
    parser.add_argument(
        "--max-rss-mib", type=int, default=0,
        help="kill a BRep helper above this observed RSS (zero disables)"
    )
    parser.add_argument(
        "--address-space-limit-mib", type=int, default=0,
        help="set the helper RLIMIT_AS in MiB (zero disables)"
    )
    parser.add_argument("--only-file",
                        help="regular expression matched against .g filenames")
    parser.add_argument("--only-object",
                        help="regular expression matched against BRep names")
    parser.add_argument("--resume", action="store_true",
                        help="retain checkpointed results with identical options")
    args = parser.parse_args()

    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.timeout <= 0 or args.list_timeout <= 0:
        parser.error("timeouts must be positive")
    if args.ratio_min <= 0 or args.ratio_max < args.ratio_min:
        parser.error("dimension ratio limits are invalid")
    if min(args.tess_abs, args.tess_rel, args.tess_norm) < 0:
        parser.error("tessellation tolerances cannot be negative")
    if args.max_rss_mib < 0 or args.address_space_limit_mib < 0:
        parser.error("memory limits cannot be negative")

    input_directory = args.input_directory.resolve()
    output_directory = args.output_directory.resolve()
    helper = args.brep_audit.resolve()
    if not helper.is_file():
        parser.error("brep-audit not found: {}".format(helper))
    file_matcher = re.compile(args.only_file) if args.only_file else None
    object_matcher = re.compile(args.only_object) if args.only_object else None
    databases = sorted(
        path.resolve() for path in input_directory.iterdir()
        if path.is_file() and path.suffix.lower() == ".g" and
        (not file_matcher or file_matcher.search(path.name))
    )
    if not databases:
        parser.error("no matching .g files in {}".format(input_directory))

    options = {
        "ratio_min": args.ratio_min,
        "ratio_max": args.ratio_max,
        "tess_abs": args.tess_abs,
        "tess_rel": args.tess_rel,
        "tess_norm": args.tess_norm,
        "timeout": args.timeout,
        "list_timeout": args.list_timeout,
        "max_rss_mib": args.max_rss_mib,
        "address_space_limit_mib": args.address_space_limit_mib,
        "only_file": args.only_file,
        "only_object": args.only_object,
        "helper": str(helper),
        "helper_size": helper.stat().st_size,
        "helper_mtime_ns": helper.stat().st_mtime_ns,
    }
    signature = options
    output_directory.mkdir(parents=True, exist_ok=True)
    checkpoint_path = output_directory / "brep-audit-results.jsonl"
    records = (
        read_checkpoint(checkpoint_path, signature) if args.resume else {}
    )

    tasks = []
    for index, database in enumerate(databases, 1):
        print("[inventory {}/{}] {}".format(
            index, len(databases), database.name), flush=True)
        names, error_record = list_breps(
            helper, database, args.list_timeout, signature
        )
        if error_record:
            for key in list(records):
                if key[0] == str(database):
                    records.pop(key)
            records[(str(database), "")] = error_record
            continue
        records.pop((str(database), ""), None)
        if object_matcher:
            names = [name for name in names if object_matcher.search(name)]
        current_names = set(names)
        identity = file_identity(database)
        for key, record in list(records.items()):
            if key[0] != str(database):
                continue
            if key[1] not in current_names or \
                    record.get("database_identity") != identity:
                records.pop(key)
        tasks.extend((database, name) for name in names)

    # Rewrite a resumed checkpoint with only the selected run signature, then
    # append each new result immediately so an interrupted corpus run is usable.
    with checkpoint_path.open("w", encoding="utf-8") as checkpoint:
        for record in sorted(
                records.values(),
                key=lambda item: (item.get("database", ""), item.get("object", ""))):
            checkpoint.write(json.dumps(record, sort_keys=True) + "\n")
        checkpoint.flush()

    pending = [
        task for task in tasks if (str(task[0]), task[1]) not in records
    ]
    resumed = len(tasks) - len(pending)
    if resumed:
        print("resuming {} completed BRep records".format(resumed), flush=True)

    completed = 0
    with checkpoint_path.open("a", encoding="utf-8") as checkpoint:
        with ThreadPoolExecutor(max_workers=args.jobs) as pool:
            futures = {
                pool.submit(
                    audit_one, helper, database, object_name, args, signature
                ): (database, object_name)
                for database, object_name in pending
            }
            for future in as_completed(futures):
                database, object_name = futures[future]
                try:
                    record = future.result()
                except Exception as error:  # Preserve the rest of a corpus run.
                    run = {
                        "returncode": 125,
                        "stdout": "",
                        "stderr": repr(error),
                        "elapsed_seconds": 0.0,
                        "observed_peak_rss_bytes": 0,
                        "termination_reason": "driver_error",
                    }
                    record = synthetic_record(
                        database, object_name, "error",
                        "audit_driver_exception", run, signature
                    )
                records[(str(database), object_name)] = record
                checkpoint.write(json.dumps(record, sort_keys=True) + "\n")
                checkpoint.flush()
                completed += 1
                print("[audit {}/{}] {}:{} {}".format(
                    completed, len(pending), database.name, object_name,
                    record["status"],
                ), flush=True)

    results = sorted(
        records.values(),
        key=lambda item: (item.get("database", ""), item.get("object", "")),
    )
    statuses = Counter(
        record.get("status", "error")
        for record in results if record.get("scope") == "brep"
    )
    database_errors = [
        record for record in results if record.get("scope") == "database"
    ]
    revision, dirty = git_state(Path(__file__).resolve().parents[2])
    summary = {
        "format": FORMAT,
        "generated_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "revision": revision,
        "dirty": dirty,
        "input_directory": str(input_directory),
        "output_directory": str(output_directory),
        "helper": str(helper),
        "options": options,
        "totals": {
            "databases": len(databases),
            "database_inventory_errors": len(database_errors),
            "breps": sum(statuses.values()),
            "ok": statuses["ok"],
            "fail": statuses["fail"],
            "timeout": statuses["timeout"],
            "memory_limit": statuses["memory_limit"],
            "error": statuses["error"] + len(database_errors),
        },
        "results": results,
    }
    summary_path = output_directory / "brep-audit-summary.json"
    worklist_path = output_directory / "brep-audit-worklist.md"
    summary_path.write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    worklist_path.write_text(
        worklist_markdown(summary, input_directory), encoding="utf-8"
    )
    totals = summary["totals"]
    print(
        "audited {} BReps: {} okay; {} work items; reports in {}".format(
            totals["breps"], totals["ok"],
            totals["breps"] - totals["ok"] + len(database_errors),
            output_directory,
        )
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
