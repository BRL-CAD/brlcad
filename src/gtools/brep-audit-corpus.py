#!/usr/bin/env python3

"""Run brep-audit over a database corpus with per-object isolation."""

import argparse
import concurrent.futures
import json
import os
from pathlib import Path
import queue
import shutil
import subprocess
import sys
import tempfile
import threading
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


def text_tail(value, limit=4096):
    if value is None:
        return ""
    if isinstance(value, bytes):
        value = value.decode("utf-8", errors="replace")
    return value[-limit:]


def last_phase(stderr):
    prefix = "brep-audit: phase="
    for line in reversed(text_tail(stderr).splitlines()):
        if line.startswith(prefix):
            return line[len(prefix):]
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


def repair_output_tail(path):
    """Remove only an incomplete final JSONL record before resuming."""
    if not path.exists() or not path.stat().st_size:
        return
    with path.open("rb+") as output:
        output.seek(0, os.SEEK_END)
        end = output.tell()
        output.seek(end - 1)
        if output.read(1) == b"\n":
            return
        record_start = 0
        scan_end = end
        while scan_end > 0:
            scan_start = max(0, scan_end - 64 * 1024)
            output.seek(scan_start)
            chunk = output.read(scan_end - scan_start)
            newline = chunk.rfind(b"\n")
            if newline >= 0:
                record_start = scan_start + newline + 1
                break
            scan_end = scan_start
        output.seek(record_start)
        record = output.read(end - record_start)
        try:
            json.loads(record.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            output.truncate(record_start)
            return
        output.seek(0, os.SEEK_END)
        output.write(b"\n")


def append_repair_options(command, args):
    if not args.quality_repair:
        return
    command.extend((
        "--quality-repair",
        "--repair-hole-area-percent",
        str(args.repair_hole_area_percent),
        "--repair-hole-edges",
        str(args.repair_hole_edges),
        "--repair-area-change-percent",
        str(args.repair_area_change_percent),
        "--repair-max-deviation",
        str(args.repair_max_deviation),
        "--repair-deviation-samples",
        str(args.repair_deviation_samples),
        "--repair-poisson-depth",
        str(args.repair_poisson_depth),
        "--repair-poisson-scale",
        str(args.repair_poisson_scale),
    ))
    if args.repair_max_deviation_rel > 0.0:
        command.extend((
            "--repair-max-deviation-rel",
            str(args.repair_max_deviation_rel),
        ))
    if args.repair_allow_untrimmed:
        command.append("--repair-allow-untrimmed")
    if args.repair_full_fast:
        command.append("--repair-full-fast")
    if args.repair_poisson:
        command.append("--repair-poisson")
    if args.repair_union_components:
        command.append("--repair-union-components")
    if args.repair_allow_self_intersections:
        command.append("--repair-allow-self-intersections")
    if args.repair_require_manifold:
        command.append("--repair-require-manifold")
    if args.repair_no_fast:
        command.append("--repair-no-fast")


def selected_objects(path, failures_only, input_class):
    selected = {}
    if path is None:
        return selected
    with path.open("r", encoding="utf-8", errors="replace") as source:
        for line in source:
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            database = record.get("database")
            object_name = record.get("object")
            if not database or not object_name:
                continue
            if failures_only and record.get("status") == "ok":
                continue
            eligible = (record.get("input") or {}).get("quality_eligible")
            input_valid = (record.get("input") or {}).get("valid")
            if input_class == "valid" and eligible is False:
                continue
            if input_class == "invalid" and eligible is not False:
                continue
            if input_class == "input-valid" and input_valid is not True:
                continue
            if input_class == "input-invalid" and input_valid is not False:
                continue
            database = str(Path(database).resolve())
            selected.setdefault(database, set()).add(object_name)
    return selected


def audit_one(audit, args, run_dir, database_name, object_name, mode):
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
    ]
    if args.valid_solids_only:
        command.append("--valid-solids-only")
    append_repair_options(command, args)
    command.extend((database_name, object_name))
    start = time.monotonic()
    try:
        result = run(command, args.object_timeout, cwd=run_dir)
    except subprocess.TimeoutExpired as timeout_error:
        stderr = text_tail(timeout_error.stderr)
        return failure_record(
            database_name,
            object_name,
            mode,
            "timeout",
            wall_seconds=time.monotonic() - start,
            last_phase=last_phase(stderr),
            stderr=stderr,
        )
    except OSError as launch_error:
        return failure_record(
            database_name,
            object_name,
            mode,
            "launch_error",
            wall_seconds=time.monotonic() - start,
            error=str(launch_error),
        )

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
    return record


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


def resume_batch_starts(path):
    indices = {}
    if not path.exists():
        return indices
    with path.open("r", encoding="utf-8", errors="replace") as existing:
        for line in existing:
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            database = record.get("database")
            task_index = record.get("task_index")
            if database is None or record.get("object") is None or \
                    record.get("mode") is None or \
                    not isinstance(task_index, int) or task_index < 0:
                continue
            indices.setdefault(database, set()).add(task_index)
    starts = {}
    for database, completed in indices.items():
        start = 0
        while start in completed:
            start += 1
        starts[database] = start
    return starts


def stream_reader(stream, stream_name, events):
    try:
        for line in stream:
            events.put((stream_name, line))
    finally:
        events.put((stream_name, None))


def audit_database(audit, args, run_dir, database, start_index, sink,
                   object_file=None):
    database_name = str(database.resolve())
    if len(args.modes) == 1:
        batch_mode = args.modes[0]
    elif set(args.modes) == {"wireframe", "shaded"}:
        batch_mode = "both"
    else:
        batch_mode = "all"
    next_index = start_index
    no_progress_restarts = 0
    process_retries = {}

    while True:
        command = [
            audit,
            "--batch",
            "--batch-start",
            str(next_index),
            "--mode",
            batch_mode,
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
        ]
        if args.valid_solids_only:
            command.append("--valid-solids-only")
        append_repair_options(command, args)
        if object_file is not None:
            command.extend(("--batch-object-file", str(object_file)))
        command.append(database_name)
        try:
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                errors="replace",
                bufsize=1,
                cwd=run_dir,
            )
        except OSError as error:
            sink(failure_record(
                database_name, None, None, "launch_error", error=str(error)
            ))
            return

        events = queue.Queue()
        readers = [
            threading.Thread(
                target=stream_reader,
                args=(process.stdout, "stdout", events),
                daemon=True,
            ),
            threading.Thread(
                target=stream_reader,
                args=(process.stderr, "stderr", events),
                daemon=True,
            ),
        ]
        for reader in readers:
            reader.start()

        current = None
        current_phase = None
        stderr_tail = ""
        closed_streams = set()
        deadline = time.monotonic() + args.database_timeout
        restart = False
        made_progress = False

        while len(closed_streams) < 2 or process.poll() is None:
            remaining = deadline - time.monotonic()
            if remaining <= 0.0:
                process.kill()
                process.wait()
                if current is None:
                    sink(failure_record(
                        database_name,
                        None,
                        None,
                        "batch_timeout",
                        task_index=next_index,
                        stderr=stderr_tail,
                    ))
                    return
                sink(failure_record(
                    database_name,
                    current.get("object"),
                    current.get("mode"),
                    "timeout",
                    task_index=current.get("task_index"),
                    wall_seconds=args.object_timeout,
                    last_phase=current_phase,
                    stderr=stderr_tail,
                ))
                next_index = current["task_index"] + 1
                restart = True
                break

            try:
                stream_name, line = events.get(timeout=min(remaining, 0.5))
            except queue.Empty:
                continue
            if line is None:
                closed_streams.add(stream_name)
                continue
            if stream_name == "stderr":
                stderr_tail = text_tail(stderr_tail + line)
                phase = last_phase(line)
                if phase is not None:
                    current_phase = phase
                continue

            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                stderr_tail = text_tail(stderr_tail + line)
                continue
            if record.get("format") == "brlcad-brep-audit-progress-v1":
                current = record
                current_phase = None
                deadline = time.monotonic() + args.object_timeout
                continue
            if record.get("format") != "brlcad-brep-realization-audit-v1":
                continue
            sink(record)
            task_index = record.get("task_index")
            if isinstance(task_index, int) and task_index >= 0:
                next_index = task_index + 1
                process_retries.pop(task_index, None)
            peak_rss = (record.get(record.get("mode")) or {}).get(
                "peak_rss_bytes", 0
            ) or 0
            current = None
            current_phase = None
            stderr_tail = ""
            deadline = time.monotonic() + args.database_timeout
            made_progress = True
            restart_bytes = args.batch_restart_rss_mib * 1024 * 1024
            if restart_bytes and peak_rss >= restart_bytes:
                sink(failure_record(
                    database_name,
                    None,
                    None,
                    "batch_rollover",
                    next_task_index=next_index,
                    peak_rss_bytes=peak_rss,
                ))
                if process.poll() is None:
                    process.terminate()
                    process.wait()
                restart = True
                break

        if restart:
            continue

        return_code = process.wait()
        if return_code == 0:
            return
        if current is not None:
            task_index = current.get("task_index")
            retry_count = process_retries.get(task_index, 0)
            if retry_count < 1:
                process_retries[task_index] = retry_count + 1
                sink(failure_record(
                    database_name,
                    None,
                    None,
                    "batch_retry",
                    task_index=task_index,
                    retry_object=current.get("object"),
                    retry_mode=current.get("mode"),
                    return_code=return_code,
                    last_phase=current_phase,
                    stderr=stderr_tail,
                ))
                next_index = task_index
                continue
            sink(failure_record(
                database_name,
                current.get("object"),
                current.get("mode"),
                "process_error",
                task_index=task_index,
                return_code=return_code,
                last_phase=current_phase,
                stderr=stderr_tail,
            ))
            process_retries.pop(task_index, None)
            next_index = task_index + 1
            continue

        if made_progress:
            no_progress_restarts = 0
        else:
            no_progress_restarts += 1
        sink(failure_record(
            database_name,
            None,
            None,
            "batch_restart",
            task_index=next_index,
            return_code=return_code,
            stderr=stderr_tail,
        ))
        if no_progress_restarts >= 3:
            return


def parse_args():
    parser = argparse.ArgumentParser(
        description=(
            "Audit every B-Rep in a .g corpus with bounded processes and "
            "per-object wall-clock timeouts."
        )
    )
    parser.add_argument("corpus", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--audit", default="brep-audit")
    parser.add_argument(
        "--modes",
        nargs="+",
        choices=("wireframe", "shaded", "quality"),
        default=("wireframe", "shaded"),
    )
    parser.add_argument("--jobs", type=int, default=8)
    parser.add_argument(
        "--processes",
        type=int,
        default=1,
        help="Maximum concurrent object or database audit processes",
    )
    parser.add_argument(
        "--batch-databases",
        action="store_true",
        help="Open each database once and audit its objects in one process",
    )
    parser.add_argument(
        "--batch-restart-rss-mib",
        type=int,
        default=0,
        help="Restart a batch between tasks after reaching this peak RSS",
    )
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
    parser.add_argument(
        "--max-databases",
        type=int,
        default=0,
        help="Stop after this many databases; zero audits the full corpus",
    )
    parser.add_argument("--resume", action="store_true")
    parser.add_argument(
        "--valid-solids-only",
        action="store_true",
        help=(
            "Classify inputs first and exclude B-Reps outside the valid "
            "closed-solid quality contract"
        ),
    )
    parser.add_argument(
        "--selection-jsonl",
        type=Path,
        help="Audit only database/object pairs present in this JSONL file",
    )
    parser.add_argument(
        "--selection-failures-only",
        action="store_true",
        help="From the selection JSONL, retain only non-ok records",
    )
    parser.add_argument(
        "--selection-class",
        choices=("all", "valid", "invalid", "input-valid", "input-invalid"),
        default="all",
        help=(
            "Filter selected records by quality eligibility (valid/invalid) "
            "or the B-Rep validity flag (input-valid/input-invalid)"
        ),
    )
    parser.add_argument("--quality-repair", action="store_true")
    parser.add_argument("--repair-hole-area-percent", type=float, default=1.0)
    parser.add_argument("--repair-hole-edges", type=int, default=256)
    parser.add_argument("--repair-area-change-percent", type=float, default=1.0)
    parser.add_argument("--repair-max-deviation", type=float, default=0.0)
    parser.add_argument(
        "--repair-max-deviation-rel", type=float, default=0.0
    )
    parser.add_argument("--repair-deviation-samples", type=int, default=4096)
    parser.add_argument("--repair-allow-untrimmed", action="store_true")
    parser.add_argument("--repair-full-fast", action="store_true")
    parser.add_argument("--repair-poisson", action="store_true")
    parser.add_argument("--repair-poisson-depth", type=int, default=8)
    parser.add_argument("--repair-poisson-scale", type=float, default=0.0)
    parser.add_argument("--repair-union-components", action="store_true")
    parser.add_argument(
        "--repair-allow-self-intersections", action="store_true"
    )
    parser.add_argument("--repair-require-manifold", action="store_true")
    parser.add_argument("--repair-no-fast", action="store_true")
    args = parser.parse_args()
    numeric = (
        args.jobs,
        args.processes,
        args.max_time_ms,
        args.max_result_mib,
        args.max_points,
        args.memory_limit_mib,
        args.database_timeout,
        args.object_timeout,
    )
    if any(value <= 0 for value in numeric):
        parser.error("resource limits and timeouts must be positive")
    if args.max_objects < 0 or args.max_databases < 0 or \
            args.batch_restart_rss_mib < 0:
        parser.error("corpus limits cannot be negative")
    if args.selection_jsonl and not args.selection_jsonl.is_file():
        parser.error("--selection-jsonl must name an existing file")
    if args.selection_failures_only and not args.selection_jsonl:
        parser.error("--selection-failures-only needs --selection-jsonl")
    if args.selection_class != "all" and not args.selection_jsonl:
        parser.error("--selection-class needs --selection-jsonl")
    if args.repair_hole_area_percent <= 0.0 or \
            args.repair_hole_edges < 3 or \
            args.repair_area_change_percent < 0.0 or \
            args.repair_max_deviation < 0.0 or \
            args.repair_max_deviation_rel < 0.0 or \
            args.repair_deviation_samples <= 0 or \
            not 5 <= args.repair_poisson_depth <= 10 or \
            (args.repair_poisson_scale != 0.0 and
             not 1.0 <= args.repair_poisson_scale <= 2.0):
        parser.error("invalid repair bounds")
    if args.repair_max_deviation > 0.0 and \
            args.repair_max_deviation_rel > 0.0:
        parser.error("absolute and relative repair deviation conflict")
    if args.repair_no_fast and \
            (args.repair_full_fast or args.repair_poisson):
        parser.error("--repair-no-fast conflicts with whole-fast repair")
    if (args.repair_poisson or args.repair_full_fast or
            args.repair_union_components or
            args.repair_allow_self_intersections or
            args.repair_require_manifold or args.repair_no_fast) and \
            not args.quality_repair:
        parser.error("repair strategy options need --quality-repair")
    if args.batch_databases and args.max_objects:
        parser.error("--max-objects is not available with --batch-databases")
    selected_modes = set(args.modes)
    supported_batch_sets = (
        {"wireframe"}, {"shaded"}, {"quality"},
        {"wireframe", "shaded"},
        {"wireframe", "shaded", "quality"},
    )
    if args.batch_databases and selected_modes not in supported_batch_sets:
        parser.error("batch mode supports one mode, wireframe+shaded, or all modes")
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
    selection = selected_objects(
        args.selection_jsonl,
        args.selection_failures_only,
        args.selection_class,
    )
    if args.selection_jsonl and not selection:
        print("Selection JSONL did not identify any matching objects",
              file=sys.stderr)
        return 2

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_mode = "a" if args.resume else "w"
    if args.resume:
        repair_output_tail(output_path)

    if args.batch_databases:
        starts = resume_batch_starts(output_path) if args.resume else {}
        if selection:
            database_list = [Path(name) for name in sorted(selection)]
        else:
            database_list = list(databases(corpus))
        if args.max_databases:
            database_list = database_list[:args.max_databases]
        with tempfile.TemporaryDirectory(
                prefix="brep-audit-corpus-") as run_dir, \
                output_path.open(output_mode, encoding="utf-8") as output, \
                concurrent.futures.ThreadPoolExecutor(
                    max_workers=args.processes
                ) as executor:
            output_lock = threading.Lock()

            def sink(record):
                with output_lock:
                    write_record(output, record)

            pending_databases = {}

            def finish_databases(done):
                for future in done:
                    database = pending_databases.pop(future)
                    try:
                        future.result()
                    except Exception as error:
                        sink(failure_record(
                            str(database.resolve()),
                            None,
                            None,
                            "runner_error",
                            error=repr(error),
                        ))

            for database_index, database in enumerate(database_list):
                database_name = str(database.resolve())
                object_file = None
                if selection:
                    object_file = Path(run_dir) / (
                        "batch-objects-{}.txt".format(database_index)
                    )
                    with object_file.open("w", encoding="utf-8") as names:
                        for object_name in sorted(selection[database_name]):
                            names.write(object_name + "\n")
                future = executor.submit(
                    audit_database,
                    audit,
                    args,
                    run_dir,
                    database,
                    starts.get(database_name, 0),
                    sink,
                    object_file,
                )
                pending_databases[future] = database
                if len(pending_databases) >= args.processes * 2:
                    done, _ = concurrent.futures.wait(
                        pending_databases,
                        return_when=concurrent.futures.FIRST_COMPLETED,
                    )
                    finish_databases(done)

            while pending_databases:
                done, _ = concurrent.futures.wait(
                    pending_databases,
                    return_when=concurrent.futures.FIRST_COMPLETED,
                )
                finish_databases(done)
        return 0

    completed = resume_keys(output_path) if args.resume else set()
    object_count = 0
    limit_reached = False
    with tempfile.TemporaryDirectory(prefix="brep-audit-corpus-") as run_dir, \
            output_path.open(output_mode, encoding="utf-8") as output, \
            concurrent.futures.ThreadPoolExecutor(
                max_workers=args.processes
            ) as executor:
        pending = {}

        def write_done(done):
            for future in done:
                database_name, object_name, mode = pending.pop(future)
                try:
                    record = future.result()
                except Exception as error:
                    record = failure_record(
                        database_name,
                        object_name,
                        mode,
                        "runner_error",
                        error=repr(error),
                    )
                write_record(output, record)

        if selection:
            database_iterator = (Path(name) for name in sorted(selection))
        else:
            database_iterator = databases(corpus)
        for database in database_iterator:
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
                if selection and object_name not in selection.get(
                        database_name, set()):
                    continue
                object_pending = any(
                    (database_name, object_name, mode) not in completed
                    for mode in args.modes
                )
                if not object_pending:
                    continue
                if args.max_objects and object_count >= args.max_objects:
                    limit_reached = True
                    break
                object_count += 1
                for mode in args.modes:
                    key = (database_name, object_name, mode)
                    if key in completed:
                        continue
                    future = executor.submit(
                        audit_one,
                        audit,
                        args,
                        run_dir,
                        database_name,
                        object_name,
                        mode,
                    )
                    pending[future] = key
                    if len(pending) >= args.processes * 2:
                        done, _ = concurrent.futures.wait(
                            pending,
                            return_when=concurrent.futures.FIRST_COMPLETED,
                        )
                        write_done(done)

        while pending:
            done, _ = concurrent.futures.wait(
                pending,
                return_when=concurrent.futures.FIRST_COMPLETED,
            )
            write_done(done)

    return 0


if __name__ == "__main__":
    sys.exit(main())
