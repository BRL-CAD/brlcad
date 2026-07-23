#!/usr/bin/env python3
"""Summarize one BRL-CAD STEP converter corpus run and compare it to another.

The corpus runner writes one ``NAME.status`` record and one ``NAME.json``
converter report per input.  A status record is tab separated::

    exit_status<TAB>elapsed_seconds<TAB>schema<TAB>input_path

This utility intentionally consumes the converters' structured reports rather
than scraping diagnostics.  It writes a durable JSON record suitable for trend
plots and a compact Markdown companion for review.
"""

import argparse
import datetime
import json
import os
import subprocess
import sys
from collections import Counter
from pathlib import Path


FORMAT = "brlcad-step-corpus-summary-v2"


INCOMPLETE_PROCESS_STATUSES = {124, 130, 137, 143}


def run_complete(item):
    """Return whether an input reached a normal converter terminal state."""
    if "run_complete" in item:
        return bool(item["run_complete"])
    return int(item.get("process_exit_status", 0)) not in INCOMPLETE_PROCESS_STATUSES


def read_json(path):
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def git_state(start):
    try:
        revision = subprocess.check_output(
            ["git", "-C", str(start), "rev-parse", "HEAD"],
            stderr=subprocess.DEVNULL,
            text=True,
        ).strip()
        dirty = bool(
            subprocess.check_output(
                ["git", "-C", str(start), "status", "--porcelain"],
                stderr=subprocess.DEVNULL,
                text=True,
            ).strip()
        )
        return revision, dirty
    except (OSError, subprocess.CalledProcessError):
        return "unknown", None


def failure_category(reason):
    text = (reason or "").lower()
    if any(word in text for word in ("budget", "expired", "timed out", "timeout")):
        return "work_budget"
    if any(word in text for word in (
        "not solid", "non-solid", "solid validation", "validate as a solid"
    )):
        return "non_solid"
    if "structural" in text:
        return "structural"
    if "exact" in text or "conversion failed" in text:
        return "exact_conversion"
    if "opennurbs" in text:
        return "structural"
    if "output" in text or "database" in text or "write" in text:
        return "output"
    return "other"


def parse_status(path):
    fields = path.read_text(encoding="utf-8").rstrip("\n").split("\t", 3)
    if len(fields) != 4:
        raise ValueError("{} is not a four-field corpus status record".format(path))
    return {
        "process_exit_status": int(fields[0]),
        "elapsed_seconds": float(fields[1]),
        "schema": fields[2],
        "input": fields[3],
    }


def report_record(status_path):
    status = parse_status(status_path)
    report_path = status_path.with_suffix(".json")
    if not report_path.is_file():
        raise ValueError("missing report paired with {}".format(status_path))
    report = read_json(report_path)
    coverage = report.get("coverage") or {}
    validation = report.get("validation") or {}
    timings = report.get("timings_us") or {}
    skipped = report.get("skipped_items") or []
    attempted = int(coverage.get("geometry_attempted") or 0)
    written = int(coverage.get("geometry_written") or 0)
    skipped_count = int(coverage.get("geometry_skipped") or len(skipped))
    categories = Counter(failure_category(item.get("reason")) for item in skipped)
    input_path = str(report.get("input") or status["input"])
    result = {
        "name": os.path.basename(input_path),
        "input": input_path,
        "schema": status["schema"],
        "report": report_path.name,
        "process_exit_status": status["process_exit_status"],
        "converter_exit_status": int(report.get("exit_status", status["process_exit_status"])),
        "run_complete": status["process_exit_status"] not in
                        INCOMPLETE_PROCESS_STATUSES,
        "elapsed_seconds": status["elapsed_seconds"],
        "geometry_attempted": attempted,
        "geometry_written": written,
        "geometry_skipped": skipped_count,
        "success_fraction": (float(written) / attempted) if attempted else 0.0,
        "clean": status["process_exit_status"] not in
                 INCOMPLETE_PROCESS_STATUSES and attempted > 0 and
                 written == attempted and
                 int(report.get("exit_status", status["process_exit_status"])) == 0,
        "invalid_breps": int(validation.get("invalid_breps") or 0),
        "output_failures": int(validation.get("output_failures") or 0),
        "repairs": int(validation.get("repairs") or 0),
        "tolerance_mm": validation.get("tolerance_mm"),
        "peak_rss_bytes": int(report.get("peak_rss_bytes") or 0),
        "load_us": int(timings.get("load") or 0),
        "convert_us": int(timings.get("convert") or 0),
        "failure_categories": dict(sorted(categories.items())),
        "skipped_items": [
            {
                "entity_id": item.get("entity_id"),
                "entity_type": item.get("entity_type"),
                "reason": item.get("reason"),
                "category": failure_category(item.get("reason")),
            }
            for item in skipped
        ],
    }
    return result


def totals(files):
    by_schema = {}
    for schema in sorted(set(item["schema"] for item in files)):
        by_schema[schema] = totals_base(
            [item for item in files if item["schema"] == schema]
        )
    result = totals_base(files)
    result["by_schema"] = by_schema
    return result


def totals_base(files):
    completed = [item for item in files if run_complete(item)]
    incomplete = [item for item in files if not run_complete(item)]
    attempted = sum(item["geometry_attempted"] for item in completed)
    written = sum(item["geometry_written"] for item in completed)
    categories = Counter()
    for item in completed:
        categories.update(item["failure_categories"])
    return {
        "files": len(files),
        "completed_files": len(completed),
        "inconclusive_files": len(incomplete),
        "clean_files": sum(1 for item in completed if item["clean"]),
        "geometry_attempted": attempted,
        "geometry_written": written,
        "geometry_skipped": sum(item["geometry_skipped"] for item in completed),
        "success_fraction": (float(written) / attempted) if attempted else 0.0,
        "elapsed_seconds": sum(item["elapsed_seconds"] for item in completed),
        "inconclusive_elapsed_seconds": sum(
            item["elapsed_seconds"] for item in incomplete
        ),
        "observed_inconclusive_geometry_attempted": sum(
            item["geometry_attempted"] for item in incomplete
        ),
        "observed_inconclusive_geometry_written": sum(
            item["geometry_written"] for item in incomplete
        ),
        "observed_inconclusive_geometry_skipped": sum(
            item["geometry_skipped"] for item in incomplete
        ),
        "peak_file_rss_bytes": max(
            (item["peak_rss_bytes"] for item in files), default=0
        ),
        "failure_categories": dict(sorted(categories.items())),
    }


def compare(current_files, previous):
    if not previous:
        return None
    old_files = {item["input"]: item for item in previous.get("files", [])}
    changes = []
    comparable_current = []
    comparable_previous = []
    for current in current_files:
        old = old_files.get(current["input"])
        if not run_complete(current):
            changes.append({
                "input": current["input"],
                "change": "inconclusive",
                "process_exit_status": current["process_exit_status"],
            })
            continue
        if not old:
            changes.append({"input": current["input"], "change": "new"})
            continue
        if not run_complete(old):
            changes.append({"input": current["input"],
                            "change": "previous_inconclusive"})
            continue
        comparable_current.append(current)
        comparable_previous.append(old)
        written_delta = current["geometry_written"] - old.get("geometry_written", 0)
        skipped_delta = current["geometry_skipped"] - old.get("geometry_skipped", 0)
        old_skips = {
            (item.get("entity_id"), item.get("entity_type"), item.get("reason"))
            for item in old.get("skipped_items", [])
        }
        new_skips = {
            (item.get("entity_id"), item.get("entity_type"), item.get("reason"))
            for item in current.get("skipped_items", [])
        }
        if written_delta or skipped_delta or bool(old.get("clean")) != current["clean"] or old_skips != new_skips:
            changes.append({
                "input": current["input"],
                "written_delta": written_delta,
                "skipped_delta": skipped_delta,
                "became_clean": current["clean"] and not old.get("clean"),
                "regressed_from_clean": bool(old.get("clean")) and not current["clean"],
                "fixed_skips": [list(item) for item in sorted(old_skips - new_skips)],
                "new_skips": [list(item) for item in sorted(new_skips - old_skips)],
            })
    previous_totals = totals(comparable_previous)
    current_totals = totals(comparable_current)
    return {
        "previous_run_id": previous.get("run_id"),
        "geometry_written_delta": current_totals["geometry_written"] -
                                  int(previous_totals.get("geometry_written") or 0),
        "geometry_skipped_delta": current_totals["geometry_skipped"] -
                                  int(previous_totals.get("geometry_skipped") or 0),
        "clean_files_delta": current_totals["clean_files"] -
                             int(previous_totals.get("clean_files") or 0),
        "regressed_clean_files": [
            item["input"] for item in changes if item.get("regressed_from_clean")
        ],
        "newly_clean_files": [
            item["input"] for item in changes if item.get("became_clean")
        ],
        "inconclusive_files": [
            item["input"] for item in changes
            if item.get("change") == "inconclusive"
        ],
        "changes": changes,
    }


def markdown(summary):
    total = summary["totals"]
    lines = [
        "# STEP corpus run {}".format(summary["run_id"]),
        "",
        "Revision: `{}`{}".format(
            summary["revision"], " (dirty)" if summary.get("dirty") else ""
        ),
        "",
        "| Schema | Files clean | Geometry written | Success |",
        "|---|---:|---:|---:|",
    ]
    for schema, values in sorted(total["by_schema"].items()):
        lines.append("| {} | {}/{} | {}/{} | {:.2%} |".format(
            schema, values["clean_files"], values["completed_files"],
            values["geometry_written"], values["geometry_attempted"],
            values["success_fraction"],
        ))
    lines.append("| Combined | {}/{} | {}/{} | {:.2%} |".format(
        total["clean_files"], total["completed_files"], total["geometry_written"],
        total["geometry_attempted"], total["success_fraction"],
    ))
    if total["inconclusive_files"]:
        lines.append("")
        lines.append("Inconclusive (timed out or interrupted): {} file(s); partial geometry is excluded from totals and deltas.".format(
            total["inconclusive_files"]
        ))
    lines.extend(["", "## Per file", "", "| Schema | Input | Status | Written | Skipped | Seconds | Peak RSS MiB |", "|---|---|---|---:|---:|---:|---:|"])
    for item in summary["files"]:
        lines.append("| {} | {} | {} | {}/{} | {} | {:.1f} | {:.1f} |".format(
            item["schema"], item["name"].replace("|", "\\|"),
            "complete" if run_complete(item) else "inconclusive",
            item["geometry_written"], item["geometry_attempted"],
            item["geometry_skipped"], item["elapsed_seconds"],
            item["peak_rss_bytes"] / (1024.0 * 1024.0),
        ))
    delta = summary.get("delta")
    if delta:
        lines.extend([
            "", "## Delta", "",
            "Written geometry: {:+d}; skipped geometry: {:+d}; clean files: {:+d}.".format(
                delta["geometry_written_delta"], delta["geometry_skipped_delta"],
                delta["clean_files_delta"],
            ),
        ])
        if delta["regressed_clean_files"]:
            lines.append("Regressed clean files: {}.".format(
                ", ".join(delta["regressed_clean_files"])
            ))
        if delta["inconclusive_files"]:
            lines.append("Not compared because the run was incomplete: {}.".format(
                ", ".join(delta["inconclusive_files"])
            ))
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("--previous", type=Path,
                        help="previous corpus-summary.json for delta reporting")
    parser.add_argument("--output", type=Path,
                        help="output JSON (default: RUN_DIRECTORY/corpus-summary.json)")
    parser.add_argument("--run-id", help="stable run label; defaults to directory name")
    parser.add_argument("--no-markdown", action="store_true")
    args = parser.parse_args()

    run_directory = args.run_directory.resolve()
    status_paths = sorted(run_directory.glob("*.status"))
    if not status_paths:
        parser.error("{} contains no *.status corpus records".format(run_directory))
    try:
        files = [report_record(path) for path in status_paths]
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print("step_corpus_summary.py: {}".format(error), file=sys.stderr)
        return 2

    revision, dirty = git_state(Path(__file__).resolve().parents[2])
    previous = read_json(args.previous) if args.previous else None
    generated = datetime.datetime.now(datetime.timezone.utc).replace(
        microsecond=0
    ).isoformat()
    summary = {
        "format": FORMAT,
        "run_id": args.run_id or run_directory.name,
        "generated_utc": generated,
        "run_directory": str(run_directory),
        "revision": revision,
        "dirty": dirty,
        "totals": totals(files),
        "files": files,
    }
    summary["delta"] = compare(files, previous)

    output = args.output or (run_directory / "corpus-summary.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("w", encoding="utf-8") as stream:
        json.dump(summary, stream, indent=2, sort_keys=True)
        stream.write("\n")
    if not args.no_markdown:
        output.with_suffix(".md").write_text(markdown(summary), encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
