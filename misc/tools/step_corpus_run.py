#!/usr/bin/env python3
"""Run the schema-dispatching BRL-CAD STEP importer over an external corpus.

Unknown schemas are recorded in the run manifest but are not passed to the
converter.  Each supported input gets a structured report, a diagnostic log,
and a tab-separated status record consumed by ``step_corpus_summary.py``.
Nested input directories are scanned recursively, and existing complete
records may be resumed.
"""

import argparse
import datetime
import hashlib
import json
import os
import platform
import re
import signal
import subprocess
import sys
import time
from pathlib import Path


STEP_SUFFIXES = {".stp", ".step", ".p21"}
INCOMPLETE_PROCESS_STATUSES = {124, 130, 137, 143}


def schema_names(path):
    with path.open("rb") as stream:
        header = stream.read(2 * 1024 * 1024).decode("latin-1", errors="replace")
    match = re.search(
        r"FILE_SCHEMA\s*\(\s*\((.*?)\)\s*\)", header,
        re.IGNORECASE | re.DOTALL,
    )
    if not match:
        return []
    return [
        value.replace("''", "'").upper()
        for value in re.findall(r"'((?:''|[^'])*)'", match.group(1))
    ]


def schema_family(names):
    """Mirror the public host's supported-family and interim-profile routing."""
    families = set()
    legacy_ap203e2 = {
        "CONFIGURATION_CONTROL_3D_DESIGN_MIM_LF",
        "CCD_CLA_GVP_AST",
        "CCD_CLA_GVP_AST_ASD",
    }
    interim_companions = (
        "GEOMETRIC_VALIDATION_PROPERTIES_MIM",
        "GEOMETRIC_VALIDATION_PROPERTY_REPRESENTATION_MIM",
        "SHAPE_APPEARANCE_LAYER_MIM",
        "SHAPE_APPEARANCE_LAYERS_MIM",
    )
    for name in names:
        if "AP203_CONFIGURATION_CONTROLLED" in name or any(
                name.startswith(alias) for alias in legacy_ap203e2):
            families.add("AP203e2")
        elif "AUTOMOTIVE_DESIGN" in name:
            families.add("AP214")
        elif "CONFIG_CONTROL_DESIGN" in name:
            families.add("AP203")
        elif "AP242" in name or "MANAGED_MODEL_BASED_3D_ENGINEERING" in name:
            families.add("AP242")

    if families == {"AP203"} and any(
            name.startswith(companion)
            for name in names for companion in interim_companions):
        return "AP203e2"
    return next(iter(families)) if len(families) == 1 else "unsupported"


def artifact_stem(path, input_directory):
    relative = path.relative_to(input_directory).as_posix()
    readable = re.sub(r"[^A-Za-z0-9_.-]+", "_", relative)
    # Sanitizing path separators and punctuation is not injective.  Retain a
    # readable prefix while making nested and similarly named inputs safe.
    identity = hashlib.sha256(relative.encode("utf-8")).hexdigest()[:12]
    return "{}__{}_".format(readable, identity)


def legacy_artifact_stem(path, input_directory):
    """Return the pre-v2 stem so an in-progress corpus can be resumed."""
    relative = path.relative_to(input_directory).as_posix()
    return re.sub(r"[^A-Za-z0-9_.-]+", "_", relative) + "_"


def completed_status(path):
    try:
        status = int(path.read_text(encoding="utf-8").split("\t", 1)[0])
    except (OSError, ValueError):
        return False
    return status not in INCOMPLETE_PROCESS_STATUSES


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
            "inferred_curves": 0,
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


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def executable_identity(path):
    stat = path.stat()
    return {
        "path": str(path),
        "size": stat.st_size,
        "mtime_ns": stat.st_mtime_ns,
        "sha256": file_sha256(path),
    }


def schema_plugin_paths(step_g):
    """Locate build/install-tree schema modules used by the plugin host."""
    plugin_directory = step_g.parent.parent / "libexec" / "step"
    if not plugin_directory.is_dir():
        return []
    return sorted(
        path for path in plugin_directory.glob("libstep-schema-*")
        if path.is_file()
    )


def schema_plugin_identities(step_g):
    """Identify build/install-tree schema modules used by the plugin host."""
    return [executable_identity(path) for path in schema_plugin_paths(step_g)]


def linked_library_identities(executables, runtime_root):
    """Hash resolved runtime libraries supplied by the selected build tree.

    Converter and plugin hashes do not detect a rebuilt geometry or STEPcode
    shared library.  On platforms with ``ldd``, retain every resolved library
    below the executable's build/install root so a long run cannot be mistaken
    for a homogeneous baseline after one of those dependencies changes.
    """
    libraries = {}
    for executable in executables:
        try:
            result = subprocess.run(
                ["ldd", str(executable)], check=False, capture_output=True,
                text=True,
            )
        except OSError:
            return []
        if result.returncode:
            continue
        for line in result.stdout.splitlines():
            match = re.search(r"=>\s+(/\S+)", line)
            if not match:
                match = re.match(r"\s*(/\S+)", line)
            if not match:
                continue
            path = Path(match.group(1)).resolve()
            try:
                path.relative_to(runtime_root)
            except ValueError:
                continue
            if path.is_file():
                libraries[str(path)] = path
    return [executable_identity(libraries[name]) for name in sorted(libraries)]


def host_snapshot():
    """Capture lightweight run context without adding a monitoring dependency."""
    snapshot = {
        "utc": datetime.datetime.now(datetime.timezone.utc).replace(
            microsecond=0
        ).isoformat(),
    }
    try:
        snapshot["load_average"] = list(os.getloadavg())
    except (AttributeError, OSError):
        pass
    try:
        values = {}
        with Path("/proc/meminfo").open("r", encoding="ascii") as stream:
            for line in stream:
                key, value = line.split(":", 1)
                if key in ("MemTotal", "MemAvailable"):
                    values[key] = int(value.split()[0]) * 1024
        if values:
            snapshot["memory_bytes"] = values
    except (OSError, ValueError):
        pass
    return snapshot


def cpu_model():
    try:
        with Path("/proc/cpuinfo").open("r", encoding="ascii") as stream:
            for line in stream:
                if line.lower().startswith("model name"):
                    return line.split(":", 1)[1].strip()
    except (OSError, IndexError):
        pass
    return platform.processor()


def write_manifest(path, manifest):
    """Atomically preserve progress so an interrupted long run is auditable."""
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    temporary.replace(path)


def terminate_process(process, force=False):
    try:
        if os.name == "nt":
            process.kill() if force else process.terminate()
        else:
            os.killpg(process.pid, signal.SIGKILL if force else signal.SIGTERM)
    except ProcessLookupError:
        pass


def run_converter(command, log, timeout):
    """Run one converter and terminate its complete process group on timeout."""
    process = subprocess.Popen(
        command, stdout=log, stderr=subprocess.STDOUT,
        start_new_session=os.name != "nt",
    )
    try:
        return process.wait(timeout=timeout if timeout > 0 else None), False
    except subprocess.TimeoutExpired:
        terminate_process(process)
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            terminate_process(process, force=True)
            process.wait()
        return 124, True
    except BaseException:
        terminate_process(process)
        try:
            process.wait(timeout=30)
        except subprocess.TimeoutExpired:
            terminate_process(process, force=True)
            process.wait()
        raise


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_directory", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--step-g", type=Path, default=Path(".build/bin/step-g"))
    parser.add_argument("-j", "--jobs", type=int, default=min(12, os.cpu_count() or 1),
                        help="geometry workers inside each converter")
    parser.add_argument("--previous", type=Path,
                        help="previous corpus-summary.json for trend deltas")
    parser.add_argument("--expectations", type=Path,
                        help="checksum-pinned corpus expectation manifest")
    parser.add_argument("--allow-missing-expectations", action="store_true",
                        help="allow expectation entries absent from a filtered run")
    parser.add_argument("--run-id", help="stable label for this run")
    parser.add_argument("--resume", action="store_true")
    parser.add_argument("--exact", action="store_true")
    parser.add_argument("--timeout", type=float, default=0.0,
                        help="outer wall-clock limit per file in seconds (0 disables)")
    parser.add_argument("--stall-timeout", type=float,
                        help="override step-g's no-progress timeout in seconds")
    parser.add_argument(
        "--only", help="regular expression matched against relative input paths"
    )
    parser.add_argument("--list", action="store_true",
                        help="classify inputs without running converters")
    args = parser.parse_args()

    if args.jobs < 1 or args.timeout < 0:
        parser.error("--jobs must be positive and --timeout must be nonnegative")
    if args.stall_timeout is not None and args.stall_timeout <= 0:
        parser.error("--stall-timeout must be positive")
    input_directory = args.input_directory.resolve()
    output_directory = args.output_directory.resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    matcher = re.compile(args.only) if args.only else None
    inputs = sorted(
        path for path in input_directory.rglob("*")
        if path.is_file() and path.suffix.lower() in STEP_SUFFIXES and
        (not matcher or matcher.search(path.relative_to(input_directory).as_posix()))
    )
    if not inputs:
        parser.error("no matching STEP files in {}".format(input_directory))

    run_id = args.run_id or datetime.datetime.now(
        datetime.timezone.utc
    ).strftime("%Y%m%dT%H%M%SZ")
    step_g = args.step_g.resolve()
    if not step_g.is_file():
        parser.error("step-g not found: {}".format(step_g))
    plugin_paths = schema_plugin_paths(step_g)
    runtime_root = step_g.parent.parent.resolve()
    manifest = {
        "format": "brlcad-step-corpus-run-v1",
        "run_id": run_id,
        "started_utc": datetime.datetime.now(datetime.timezone.utc).replace(
            microsecond=0
        ).isoformat(),
        "host": {
            "hostname": platform.node(),
            "platform": platform.platform(),
            "machine": platform.machine(),
            "cpu_model": cpu_model(),
            "logical_cpus": os.cpu_count(),
            "initial_snapshot": host_snapshot(),
        },
        "input_directory": str(input_directory),
        "output_directory": str(output_directory),
        "options": {
            "jobs": args.jobs,
            "exact": args.exact,
            "recursive": True,
            "timeout_seconds": args.timeout,
            "stall_timeout_seconds": args.stall_timeout,
        },
        "step_g": executable_identity(step_g),
        "schema_plugins": [executable_identity(path) for path in plugin_paths],
        "linked_libraries": linked_library_identities(
            [step_g] + plugin_paths, runtime_root
        ),
        "expectations": str(args.expectations.resolve()) if args.expectations else None,
        "files": [],
    }
    manifest_path = output_directory / "run-manifest.json"
    write_manifest(manifest_path, manifest)

    for index, input_path in enumerate(inputs, 1):
        relative_input = input_path.relative_to(input_directory).as_posix()
        declared_schemas = schema_names(input_path)
        declared_schema = declared_schemas[0] if declared_schemas else ""
        family = schema_family(declared_schemas)
        record = {
            "input": str(input_path),
            "declared_schema": declared_schema,
            "declared_schemas": declared_schemas,
            "schema": family,
        }
        if args.list:
            print("{}\t{}\t{}".format(
                family, ";".join(declared_schemas), relative_input
            ))
            manifest["files"].append(record)
            write_manifest(manifest_path, manifest)
            continue
        input_stat = input_path.stat()
        record["input_identity"] = {
            "size": input_stat.st_size,
            "mtime_ns": input_stat.st_mtime_ns,
            "sha256": file_sha256(input_path),
        }
        if family == "unsupported":
            record["result"] = "skipped"
            record["reason"] = "unrecognized schema"
            manifest["files"].append(record)
            write_manifest(manifest_path, manifest)
            print("[{}/{}] skip {} ({})".format(
                index, len(inputs), relative_input, family
            ))
            continue

        stem = artifact_stem(input_path, input_directory)
        report_path = output_directory / (stem + ".json")
        log_path = output_directory / (stem + ".log")
        status_path = output_directory / (stem + ".status")
        if args.resume:
            legacy_stem = legacy_artifact_stem(input_path, input_directory)
            legacy_report = output_directory / (legacy_stem + ".json")
            legacy_status = output_directory / (legacy_stem + ".status")
            if legacy_report.is_file() and completed_status(legacy_status):
                stem = legacy_stem
                report_path = legacy_report
                log_path = output_directory / (stem + ".log")
                status_path = legacy_status
        if args.resume and report_path.is_file() and completed_status(status_path):
            record["result"] = "resumed"
            record["report"] = report_path.name
            manifest["files"].append(record)
            write_manifest(manifest_path, manifest)
            print("[{}/{}] keep {}".format(index, len(inputs), relative_input))
            continue

        command = [
            str(step_g), "-D", "-j", str(args.jobs),
            "--report", str(report_path),
        ]
        if args.exact:
            command.append("--exact")
        if args.stall_timeout is not None:
            command.extend(["--stall-timeout", str(args.stall_timeout)])
        command.append(str(input_path))
        print("[{}/{}] {} {}".format(index, len(inputs), family, relative_input),
              flush=True)
        record.update({"result": "running", "host_before": host_snapshot()})
        manifest["files"].append(record)
        write_manifest(manifest_path, manifest)
        started = time.monotonic()
        with log_path.open("wb") as log:
            try:
                status, timed_out = run_converter(command, log, args.timeout)
                message = "converter exited without writing a structured report"
                if timed_out:
                    message = "converter exceeded the {:.1f}-second outer timeout".format(
                        args.timeout
                    )
            except OSError as error:
                status = 127
                timed_out = False
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
            "timed_out": timed_out,
            "report": report_path.name,
            "log": log_path.name,
            "host_after": host_snapshot(),
        })
        write_manifest(manifest_path, manifest)

    manifest["finished_utc"] = datetime.datetime.now(
        datetime.timezone.utc
    ).replace(microsecond=0).isoformat()
    manifest["host"]["final_snapshot"] = host_snapshot()
    write_manifest(manifest_path, manifest)
    if not args.list:
        summary_command = [
            sys.executable,
            str(Path(__file__).with_name("step_corpus_summary.py")),
            str(output_directory),
            "--run-id", run_id,
        ]
        if args.previous:
            summary_command.extend(["--previous", str(args.previous.resolve())])
        summary_status = subprocess.call(summary_command)
        if summary_status or not args.expectations:
            return summary_status
        verify_command = [
            sys.executable,
            str(Path(__file__).with_name("step_corpus_verify.py")),
            str(output_directory),
            str(args.expectations.resolve()),
        ]
        if args.allow_missing_expectations:
            verify_command.append("--allow-missing")
        return subprocess.call(verify_command)
    return 0


if __name__ == "__main__":
    sys.exit(main())
