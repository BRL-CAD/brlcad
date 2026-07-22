#!/usr/bin/env python3
"""Run the supported BRL-CAD STEP converters over an external corpus.

AP242 and unknown schemas are recorded in the run manifest but deliberately
not passed to a converter.  Each supported input gets a structured converter
report, a diagnostic log, and a tab-separated status record consumed by
``step_corpus_summary.py``.  Existing complete records may be resumed.
"""

import argparse
import datetime
import json
import os
import re
import subprocess
import sys
import time
from pathlib import Path


STEP_SUFFIXES = {".stp", ".step", ".p21"}


def schema_name(path):
    with path.open("rb") as stream:
        header = stream.read(2 * 1024 * 1024).decode("latin-1", errors="replace")
    match = re.search(
        r"FILE_SCHEMA\s*\(\s*\(\s*'([^']+)'", header, re.IGNORECASE
    )
    return match.group(1).upper() if match else ""


def schema_family(name):
    if "AUTOMOTIVE_DESIGN" in name:
        return "AP214"
    if "CONFIG_CONTROL_DESIGN" in name:
        return "AP203"
    if "AP242" in name or "MANAGED_MODEL_BASED_3D_ENGINEERING" in name:
        return "AP242"
    return "unsupported"


def artifact_stem(path):
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", path.name) + "_"


def synthetic_report(input_path, status, message):
    return {
        "format": "brlcad-step-import-report-v1",
        "input": str(input_path),
        "output": "",
        "exit_status": status,
        "coverage": {
            "geometry_attempted": 0,
            "geometry_written": 0,
            "geometry_skipped": 0,
        },
        "validation": {
            "invalid_breps": 0,
            "output_failures": 0,
            "repairs": 0,
        },
        "diagnostics": [{
            "severity": "error",
            "entity_id": 0,
            "entity_type": "",
            "attribute": "",
            "message": message,
            "count": 1,
            "aggregated": False,
        }],
        "timings_us": {},
        "peak_rss_bytes": 0,
        "skipped_items": [],
        "corpus_run_incomplete": True,
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_directory", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--step-g", type=Path, default=Path(".build/bin/step-g"))
    parser.add_argument("--ap214-g", type=Path, default=Path(".build/bin/ap214-g"))
    parser.add_argument("-j", "--jobs", type=int, default=min(12, os.cpu_count() or 1),
                        help="geometry workers inside each converter")
    parser.add_argument("--previous", type=Path,
                        help="previous corpus-summary.json for trend deltas")
    parser.add_argument("--run-id", help="stable label for this run")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--exact", action="store_true")
    parser.add_argument("--only", help="regular expression matched against input names")
    parser.add_argument("--list", action="store_true",
                        help="classify inputs without running converters")
    args = parser.parse_args()

    if args.jobs < 1:
        parser.error("--jobs must be positive")
    input_directory = args.input_directory.resolve()
    output_directory = args.output_directory.resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    matcher = re.compile(args.only) if args.only else None
    inputs = sorted(
        path for path in input_directory.iterdir()
        if path.is_file() and path.suffix.lower() in STEP_SUFFIXES and
        (not matcher or matcher.search(path.name))
    )
    if not inputs:
        parser.error("no matching STEP files in {}".format(input_directory))

    run_id = args.run_id or datetime.datetime.now(
        datetime.timezone.utc
    ).strftime("%Y%m%dT%H%M%SZ")
    manifest = {
        "format": "brlcad-step-corpus-run-v1",
        "run_id": run_id,
        "input_directory": str(input_directory),
        "output_directory": str(output_directory),
        "options": {"jobs": args.jobs, "exact": args.exact},
        "files": [],
    }

    for index, input_path in enumerate(inputs, 1):
        declared_schema = schema_name(input_path)
        family = schema_family(declared_schema)
        record = {
            "input": str(input_path),
            "declared_schema": declared_schema,
            "schema": family,
        }
        if args.list:
            print("{}\t{}\t{}".format(family, declared_schema, input_path.name))
            manifest["files"].append(record)
            continue
        if family in ("AP242", "unsupported"):
            record["result"] = "skipped"
            record["reason"] = "known unsupported schema" if family == "AP242" else "unrecognized schema"
            manifest["files"].append(record)
            print("[{}/{}] skip {} ({})".format(index, len(inputs), input_path.name, family))
            continue

        stem = artifact_stem(input_path)
        report_path = output_directory / (stem + ".json")
        log_path = output_directory / (stem + ".log")
        status_path = output_directory / (stem + ".status")
        if args.resume and report_path.is_file() and status_path.is_file():
            record["result"] = "resumed"
            record["report"] = report_path.name
            manifest["files"].append(record)
            print("[{}/{}] keep {}".format(index, len(inputs), input_path.name))
            continue

        converter = args.ap214_g if family == "AP214" else args.step_g
        command = [
            str(converter.resolve()), "-D", "-j", str(args.jobs),
            "--report", str(report_path),
        ]
        if args.exact:
            command.append("--exact")
        command.append(str(input_path))
        print("[{}/{}] {} {}".format(index, len(inputs), family, input_path.name),
              flush=True)
        started = time.monotonic()
        with log_path.open("wb") as log:
            try:
                completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT)
                status = completed.returncode
                message = "converter exited without writing a structured report"
            except OSError as error:
                status = 127
                message = "could not execute converter: {}".format(error)
        elapsed = time.monotonic() - started
        if not report_path.is_file():
            report_path.write_text(
                json.dumps(synthetic_report(input_path, status, message), indent=2) + "\n",
                encoding="utf-8",
            )
        status_path.write_text(
            "{}\t{:.3f}\t{}\t{}\n".format(status, elapsed, family, input_path),
            encoding="utf-8",
        )
        record.update({
            "result": "completed",
            "process_exit_status": status,
            "elapsed_seconds": elapsed,
            "report": report_path.name,
            "log": log_path.name,
        })
        manifest["files"].append(record)

    manifest_path = output_directory / "run-manifest.json"
    manifest_path.write_text(json.dumps(manifest, indent=2, sort_keys=True) + "\n",
                             encoding="utf-8")
    if not args.list:
        summary_command = [
            sys.executable,
            str(Path(__file__).with_name("step_corpus_summary.py")),
            str(output_directory),
            "--run-id", run_id,
        ]
        if args.previous:
            summary_command.extend(["--previous", str(args.previous.resolve())])
        return subprocess.call(summary_command)
    return 0


if __name__ == "__main__":
    sys.exit(main())
