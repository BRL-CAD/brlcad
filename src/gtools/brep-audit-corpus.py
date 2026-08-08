#!/usr/bin/env python3

"""Run brep-audit over a database corpus with per-object isolation."""

import argparse
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time


def databases(corpus):
    for root, dirs, files in os.walk(corpus):
        dirs.sort()
        for name in sorted(files):
            if name.lower().endswith(".g"):
                yield Path(root) / name


def run(command, timeout, cwd=None):
    return subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        errors="replace",
        timeout=timeout,
        check=False,
        cwd=cwd,
    )


def audit_json(stdout):
    for line in reversed(stdout.splitlines()):
        try:
            value = json.loads(line)
        except json.JSONDecodeError:
            continue
        if isinstance(value, dict):
            return value
    return None


def failure_record(database, object_name, mode, status, **details):
    record = {
        "format": "brlcad-brep-corpus-audit-v1",
        "database": str(database),
        "object": object_name,
        "mode": mode,
        "status": status,
    }
    record.update(details)
    return record


def write_record(output, record):
    json.dump(record, output, sort_keys=True, separators=(",", ":"))
    output.write("\n")
    output.flush()


def resume_keys(path):
    completed = set()
    if not path.exists():
        return completed
    with path.open("r", encoding="utf-8", errors="replace") as existing:
        for line in existing:
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            key = (
                record.get("database"),
                record.get("object"),
                record.get("mode"),
            )
            if all(value is not None for value in key):
                completed.add(key)
    return completed


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Audit every B-Rep in a .g corpus. Each object and drawing mode "
            "runs in a separate process with a wall-clock timeout."
        )
    )
    parser.add_argument("corpus", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--audit", default="brep-audit")
    parser.add_argument(
        "--modes",
        nargs="+",
        choices=("wireframe", "shaded"),
        default=("wireframe", "shaded"),
    )
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument("--max-time-ms", type=int, default=5000)
    parser.add_argument("--max-result-mib", type=int, default=256)
    parser.add_argument("--max-points", type=int, default=4 * 1024 * 1024)
    parser.add_argument("--memory-limit-mib", type=int, default=1024)
    parser.add_argument("--database-timeout", type=float, default=30.0)
    parser.add_argument("--object-timeout", type=float, default=10.0)
    parser.add_argument(
        "--max-objects",
        type=int,
        default=0,
        help="Stop after this many objects; zero audits the full corpus",
    )
    parser.add_argument("--resume", action="store_true")
    args = parser.parse_args()
    numeric = (
        args.jobs,
        args.max_time_ms,
        args.max_result_mib,
        args.max_points,
        args.memory_limit_mib,
        args.database_timeout,
        args.object_timeout,
    )
    if any(value <= 0 for value in numeric):
        parser.error("resource limits and timeouts must be positive")
    if args.max_objects < 0:
        parser.error("--max-objects cannot be negative")
    return args


def main():
    args = parse_args()
    corpus = args.corpus.resolve()
    output_path = args.output.resolve()
    audit = shutil.which(args.audit)
    if not corpus.is_dir():
        print("Corpus directory does not exist: {}".format(corpus), file=sys.stderr)
        return 2
    if not audit:
        print("Unable to find brep-audit: {}".format(args.audit), file=sys.stderr)
        return 2
    audit = str(Path(audit).resolve())

    completed = resume_keys(output_path) if args.resume else set()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_mode = "a" if args.resume else "w"
    object_count = 0
    limit_reached = False
    with tempfile.TemporaryDirectory(prefix="brep-audit-corpus-") as run_dir, \
            output_path.open(output_mode, encoding="utf-8") as output:
        for database in databases(corpus):
            if limit_reached:
                break
            database_name = str(database.resolve())
            try:
                listing = run(
                    [audit, "--list", database_name], args.database_timeout
                )
            except subprocess.TimeoutExpired:
                write_record(
                    output,
                    failure_record(
                        database_name, None, None, "list_timeout"
                    ),
                )
                continue

            if listing.returncode != 0:
                write_record(
                    output,
                    failure_record(
                        database_name,
                        None,
                        None,
                        "list_error",
                        return_code=listing.returncode,
                        stderr=listing.stderr[-4096:],
                    ),
                )
                continue

            for object_name in listing.stdout.splitlines():
                if not object_name:
                    continue
                pending = any(
                    (database_name, object_name, mode) not in completed
                    for mode in args.modes
                )
                if not pending:
                    continue
                if args.max_objects and object_count >= args.max_objects:
                    limit_reached = True
                    break
                object_count += 1
                for mode in args.modes:
                    key = (database_name, object_name, mode)
                    if key in completed:
                        continue
                    command = [
                        audit,
                        "--mode",
                        mode,
                        "--jobs",
                        str(args.jobs),
                        "--max-time-ms",
                        str(args.max_time_ms),
                        "--max-result-mib",
                        str(args.max_result_mib),
                        "--max-points",
                        str(args.max_points),
                        "--memory-limit-mib",
                        str(args.memory_limit_mib),
                        database_name,
                        object_name,
                    ]
                    start = time.monotonic()
                    try:
                        result = run(command, args.object_timeout, cwd=run_dir)
                    except subprocess.TimeoutExpired:
                        write_record(
                            output,
                            failure_record(
                                database_name,
                                object_name,
                                mode,
                                "timeout",
                                wall_seconds=time.monotonic() - start,
                            ),
                        )
                        continue

                    record = audit_json(result.stdout)
                    if record is None:
                        record = failure_record(
                            database_name,
                            object_name,
                            mode,
                            "process_error",
                            return_code=result.returncode,
                            wall_seconds=time.monotonic() - start,
                            stderr=result.stderr[-4096:],
                        )
                    write_record(output, record)

    return 0


if __name__ == "__main__":
    sys.exit(main())
