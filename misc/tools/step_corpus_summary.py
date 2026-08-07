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
import math
import os
import statistics
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
    if "no importer" in text or "unsupported" in text:
        return "unsupported"
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


def occurrence_depth(occurrences):
    """Return the maximum product-assembly depth represented by occurrences."""
    children = {}
    vertices = set()
    for occurrence in occurrences:
        parent = int(occurrence.get("parent_product_id") or 0)
        child = int(occurrence.get("child_product_id") or 0)
        if parent <= 0 or child <= 0:
            continue
        children.setdefault(parent, set()).add(child)
        vertices.update((parent, child))

    active = set()
    memo = {}

    def visit(vertex):
        if vertex in memo:
            return memo[vertex]
        if vertex in active:
            # A cycle is invalid, but retain a useful finite metric and let the
            # converter's structured diagnostics report the underlying issue.
            return 0
        active.add(vertex)
        depth = max((1 + visit(child)
                     for child in children.get(vertex, ())), default=0)
        active.remove(vertex)
        memo[vertex] = depth
        return depth

    return max((visit(vertex) for vertex in vertices), default=0)


def report_record(status_path):
    status = parse_status(status_path)
    report_path = status_path.with_suffix(".json")
    if not report_path.is_file():
        raise ValueError("missing report paired with {}".format(status_path))
    report = read_json(report_path)
    coverage = report.get("coverage") or {}
    validation = report.get("validation") or {}
    timings = report.get("timings_us") or {}
    performance = report.get("performance") or {}
    calibration = performance.get("calibration") or {}
    stage_timings = performance.get("stages") or {}
    skipped = report.get("skipped_items") or []
    occurrences = report.get("occurrence_details") or []
    configuration_records = report.get("configuration_records") or []
    representation_coverage = report.get("representation_coverage") or []
    attempted = int(coverage.get("geometry_attempted") or 0)
    written = int(coverage.get("geometry_written") or 0)
    skipped_count = int(coverage.get("geometry_skipped") or len(skipped))
    categories = Counter(failure_category(item.get("reason")) for item in skipped)
    invalid_breps = int(validation.get("invalid_breps") or 0)
    invalid_breps_written = int(validation.get("invalid_breps_written") or 0)
    invalid_breps_rejected = int(validation.get("invalid_breps_rejected") or 0)
    output_failures = int(validation.get("output_failures") or 0)
    inferred_curves = int(validation.get("inferred_curves") or 0)
    if invalid_breps_written:
        categories["invalid_brep_preserved"] += invalid_breps_written
    if output_failures:
        categories["output"] += output_failures
    configuration_types = Counter(
        item.get("type") or "UNKNOWN" for item in configuration_records
    )
    representation_statuses = Counter(
        item.get("status") or "unknown" for item in representation_coverage
    )
    converter_status = int(
        report.get("exit_status", status["process_exit_status"])
    )
    intentionally_empty = (
        status["process_exit_status"] == 6 and converter_status == 6 and
        attempted == 0 and bool(representation_coverage) and
        all(item.get("status") == "intentionally_non_geometric"
            for item in representation_coverage)
    )
    input_path = str(report.get("input") or status["input"])
    result = {
        "name": os.path.basename(input_path),
        "input": input_path,
        "schema": status["schema"],
        "report": report_path.name,
        "process_exit_status": status["process_exit_status"],
        "converter_exit_status": converter_status,
        "run_complete": status["process_exit_status"] not in
                        INCOMPLETE_PROCESS_STATUSES,
        "elapsed_seconds": status["elapsed_seconds"],
        "geometry_attempted": attempted,
        "geometry_written": written,
        "geometry_skipped": skipped_count,
        "success_fraction": (float(written) / attempted) if attempted else 0.0,
        "products": int(coverage.get("products") or 0),
        "occurrences": int(coverage.get("occurrences") or 0),
        "assembly_usage_count": len(report.get("assembly_usages") or []),
        "occurrence_detail_count": len(occurrences),
        "occurrence_max_depth": occurrence_depth(occurrences),
        "occurrence_methods": sorted({
            item.get("shape_method") or "unknown" for item in occurrences
        }),
        "styles_extracted": int(coverage.get("styles_extracted") or 0),
        "styles_applied": int(coverage.get("styles_applied") or 0),
        "layers_extracted": int(coverage.get("layers_extracted") or 0),
        "materials_extracted": int(coverage.get("materials_extracted") or 0),
        "properties_extracted": int(coverage.get("properties_extracted") or 0),
        "configuration_record_count": len(configuration_records),
        "configuration_record_types": dict(sorted(configuration_types.items())),
        "configuration_metadata_count": len(
            report.get("configuration_metadata") or {}
        ),
        "product_alternative_count": len(
            report.get("product_alternatives") or []
        ),
        "usage_substitute_count": len(report.get("usage_substitutes") or []),
        "entity_type_counts": dict(sorted(
            (coverage.get("entity_counts") or {}).items()
        )),
        "unsupported_entity_counts": dict(sorted(
            (coverage.get("unsupported_counts") or {}).items()
        )),
        "representation_status_counts": dict(sorted(
            representation_statuses.items()
        )),
        "clean": status["process_exit_status"] not in
                 INCOMPLETE_PROCESS_STATUSES and attempted > 0 and
                 written == attempted and
                 converter_status == 0 and
                 invalid_breps_written == 0 and output_failures == 0 and
                 inferred_curves == 0,
        "intentionally_empty": intentionally_empty,
        "invalid_breps": invalid_breps,
        "invalid_breps_written": invalid_breps_written,
        "invalid_breps_rejected": invalid_breps_rejected,
        "output_failures": output_failures,
        "repairs": int(validation.get("repairs") or 0),
        "inferred_curves": inferred_curves,
        "tolerance_mm": validation.get("tolerance_mm"),
        "peak_rss_bytes": int(report.get("peak_rss_bytes") or 0),
        "load_us": int(timings.get("load") or 0),
        "index_us": int(timings.get("index") or 0),
        "inventory_us": int(timings.get("inventory") or 0),
        "convert_us": int(timings.get("convert") or 0),
        "calibration": {
            "queries": int(calibration.get("queries") or 0),
            "elapsed_us": int(calibration.get("elapsed_us") or 0),
            "scalar_queries_per_second": float(
                calibration.get("scalar_queries_per_second") or 0.0
            ),
            "parallel_queries_per_second": float(
                calibration.get("parallel_queries_per_second") or 0.0
            ),
        },
        "stage_timings_us": {
            name: int(values.get("total_us") or 0)
            for name, values in sorted(stage_timings.items())
        },
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
    process_statuses = Counter(
        str(item["process_exit_status"]) for item in completed
    )
    converter_statuses = Counter(
        str(item["converter_exit_status"]) for item in completed
    )
    attempted = sum(item["geometry_attempted"] for item in completed)
    written = sum(item["geometry_written"] for item in completed)
    categories = Counter()
    entity_types = Counter()
    unsupported_types = Counter()
    configuration_types = Counter()
    representation_statuses = Counter()
    occurrence_methods = set()
    for item in completed:
        categories.update(item["failure_categories"])
        entity_types.update(item.get("entity_type_counts") or {})
        unsupported_types.update(item.get("unsupported_entity_counts") or {})
        configuration_types.update(item.get("configuration_record_types") or {})
        representation_statuses.update(
            item.get("representation_status_counts") or {}
        )
        occurrence_methods.update(item.get("occurrence_methods") or [])
    return {
        "files": len(files),
        "completed_files": len(completed),
        "inconclusive_files": len(incomplete),
        "clean_files": sum(1 for item in completed if item["clean"]),
        "intentionally_empty_files": sum(
            1 for item in completed if item.get("intentionally_empty")
        ),
        "process_exit_status_counts": dict(sorted(process_statuses.items())),
        "converter_exit_status_counts": dict(sorted(converter_statuses.items())),
        "geometry_attempted": attempted,
        "geometry_written": written,
        "geometry_skipped": sum(item["geometry_skipped"] for item in completed),
        "success_fraction": (float(written) / attempted) if attempted else 0.0,
        "products": sum(item.get("products", 0) for item in completed),
        "occurrences": sum(item.get("occurrences", 0) for item in completed),
        "assembly_usage_count": sum(
            item.get("assembly_usage_count", 0) for item in completed
        ),
        "occurrence_detail_count": sum(
            item.get("occurrence_detail_count", 0) for item in completed
        ),
        "occurrence_max_depth": max(
            (item.get("occurrence_max_depth", 0) for item in completed),
            default=0,
        ),
        "occurrence_methods": sorted(occurrence_methods),
        "styles_extracted": sum(
            item.get("styles_extracted", 0) for item in completed
        ),
        "styles_applied": sum(
            item.get("styles_applied", 0) for item in completed
        ),
        "layers_extracted": sum(
            item.get("layers_extracted", 0) for item in completed
        ),
        "materials_extracted": sum(
            item.get("materials_extracted", 0) for item in completed
        ),
        "properties_extracted": sum(
            item.get("properties_extracted", 0) for item in completed
        ),
        "configuration_record_count": sum(
            item.get("configuration_record_count", 0) for item in completed
        ),
        "configuration_metadata_count": sum(
            item.get("configuration_metadata_count", 0) for item in completed
        ),
        "product_alternative_count": sum(
            item.get("product_alternative_count", 0) for item in completed
        ),
        "usage_substitute_count": sum(
            item.get("usage_substitute_count", 0) for item in completed
        ),
        "invalid_breps": sum(
            item.get("invalid_breps", 0) for item in completed
        ),
        "invalid_breps_written": sum(
            item.get("invalid_breps_written", 0) for item in completed
        ),
        "invalid_breps_rejected": sum(
            item.get("invalid_breps_rejected", 0) for item in completed
        ),
        "output_failures": sum(
            item.get("output_failures", 0) for item in completed
        ),
        "repairs": sum(item.get("repairs", 0) for item in completed),
        "inferred_curves": sum(
            item.get("inferred_curves", 0) for item in completed
        ),
        "elapsed_seconds": sum(item["elapsed_seconds"] for item in completed),
        "load_seconds": sum(item.get("load_us", 0) for item in completed) / 1.0e6,
        "index_seconds": sum(item.get("index_us", 0) for item in completed) / 1.0e6,
        "inventory_seconds": sum(
            item.get("inventory_us", 0) for item in completed
        ) / 1.0e6,
        "convert_seconds": sum(
            item.get("convert_us", 0) for item in completed
        ) / 1.0e6,
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
        "entity_type_counts": dict(sorted(entity_types.items())),
        "unsupported_entity_counts": dict(sorted(unsupported_types.items())),
        "configuration_record_types": dict(sorted(configuration_types.items())),
        "representation_status_counts": dict(
            sorted(representation_statuses.items())
        ),
        "failure_categories": dict(sorted(categories.items())),
    }


def positive_ratio(current, previous):
    return (float(current) / float(previous)) if previous > 0 else None


def median_ratio(current_files, previous_files, field):
    ratios = [
        positive_ratio(current.get(field, 0), previous.get(field, 0))
        for current, previous in zip(current_files, previous_files)
    ]
    ratios = [value for value in ratios if value is not None and value > 0]
    return statistics.median(ratios) if ratios else None


def geometric_mean_ratio(current_files, previous_files, field):
    ratios = [
        positive_ratio(current.get(field, 0), previous.get(field, 0))
        for current, previous in zip(current_files, previous_files)
    ]
    ratios = [value for value in ratios if value is not None and value > 0]
    return math.exp(sum(math.log(value) for value in ratios) / len(ratios)) \
        if ratios else None


def performance_comparison(current_files, previous_files):
    current = totals_base(current_files)
    previous = totals_base(previous_files)
    fields = {
        "elapsed": "elapsed_seconds",
        "load": "load_seconds",
        "convert": "convert_seconds",
        "peak_file_rss": "peak_file_rss_bytes",
    }
    aggregate_ratios = {
        label: positive_ratio(current[field], previous[field])
        for label, field in fields.items()
    }
    per_file_fields = {
        "elapsed": "elapsed_seconds",
        "load": "load_us",
        "convert": "convert_us",
        "peak_rss": "peak_rss_bytes",
    }
    return {
        "comparable_files": len(current_files),
        "current": {field: current[field] for field in fields.values()},
        "previous": {field: previous[field] for field in fields.values()},
        "aggregate_ratios": aggregate_ratios,
        "median_file_ratios": {
            label: median_ratio(current_files, previous_files, field)
            for label, field in per_file_fields.items()
        },
        "geometric_mean_file_ratios": {
            label: geometric_mean_ratio(current_files, previous_files, field)
            for label, field in per_file_fields.items()
        },
    }


def converter_options(configuration):
    options = (configuration or {}).get("options") or {}
    return {
        "jobs": options.get("jobs"),
        "exact": bool(options.get("exact", False)),
        "stall_timeout_seconds": options.get("stall_timeout_seconds"),
    }


def converter_identity(configuration):
    configuration = configuration or {}
    executable = configuration.get("step_g") or {}
    plugins = configuration.get("schema_plugins") or []
    libraries = configuration.get("linked_libraries") or []
    return {
        "host": executable.get("sha256"),
        "schema_plugins": [
            (os.path.basename(item.get("path") or ""), item.get("sha256"))
            for item in plugins
        ],
        "linked_libraries": [
            (os.path.basename(item.get("path") or ""), item.get("sha256"))
            for item in libraries
        ],
    }


def host_identity(configuration):
    host = (configuration or {}).get("host") or {}
    return {
        key: host.get(key)
        for key in ("hostname", "platform", "machine", "cpu_model",
                    "logical_cpus")
    }


def host_observations(manifest):
    snapshots = []
    host = manifest.get("host") or {}
    for name in ("initial_snapshot", "final_snapshot"):
        if isinstance(host.get(name), dict):
            snapshots.append(host[name])
    for item in manifest.get("files", []):
        for name in ("host_before", "host_after"):
            if isinstance(item.get(name), dict):
                snapshots.append(item[name])

    loads = [item.get("load_average") for item in snapshots]
    loads = [item for item in loads if isinstance(item, list) and len(item) == 3]
    available = [
        (item.get("memory_bytes") or {}).get("MemAvailable")
        for item in snapshots
    ]
    available = [int(value) for value in available if value is not None]
    result = {"snapshot_count": len(snapshots)}
    if loads:
        labels = ("one_minute", "five_minutes", "fifteen_minutes")
        result["load_average"] = {
            label: {
                "minimum": min(float(item[index]) for item in loads),
                "mean": statistics.mean(float(item[index]) for item in loads),
                "maximum": max(float(item[index]) for item in loads),
            }
            for index, label in enumerate(labels)
        }
    if available:
        result["available_memory_bytes"] = {
            "minimum": min(available),
            "maximum": max(available),
        }
    return result


def compare(current_files, previous, current_configuration=None,
            previous_configuration=None):
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
        inferred_delta = current.get("inferred_curves", 0) - \
                         old.get("inferred_curves", 0)
        old_skips = {
            (item.get("entity_id"), item.get("entity_type"), item.get("reason"))
            for item in old.get("skipped_items", [])
        }
        new_skips = {
            (item.get("entity_id"), item.get("entity_type"), item.get("reason"))
            for item in current.get("skipped_items", [])
        }
        if (written_delta or skipped_delta or inferred_delta or
                bool(old.get("clean")) != current["clean"] or
                old_skips != new_skips):
            changes.append({
                "input": current["input"],
                "written_delta": written_delta,
                "skipped_delta": skipped_delta,
                "inferred_curves_delta": inferred_delta,
                "became_clean": current["clean"] and not old.get("clean"),
                "regressed_from_clean": bool(old.get("clean")) and not current["clean"],
                "fixed_skips": [list(item) for item in sorted(old_skips - new_skips)],
                "new_skips": [list(item) for item in sorted(new_skips - old_skips)],
            })
    previous_totals = totals(comparable_previous)
    current_totals = totals(comparable_current)
    input_hash_pairs = [
        (current.get("input_sha256"), old.get("input_sha256"))
        for current, old in zip(comparable_current, comparable_previous)
    ]
    input_hashes_verified = bool(input_hash_pairs) and all(
        current and old and current == old
        for current, old in input_hash_pairs
    )
    options_match = bool(
        current_configuration and previous_configuration
    ) and converter_options(current_configuration) == converter_options(
        previous_configuration
    )
    hosts_match = bool(
        current_configuration and previous_configuration
    ) and host_identity(current_configuration) == host_identity(
        previous_configuration
    )
    current_observations = (
        current_configuration or {}
    ).get("host_observations") or {}
    previous_observations = (
        previous_configuration or {}
    ).get("host_observations") or {}
    loads_recorded = bool(
        current_observations.get("load_average") and
        previous_observations.get("load_average")
    )
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
        "configuration": {
            "current_options": (current_configuration or {}).get("options"),
            "previous_options": (previous_configuration or {}).get("options"),
            "converter_options_match": options_match,
            "host_identity_match": hosts_match,
            "input_hashes_verified": input_hashes_verified,
            "load_observations_recorded": loads_recorded,
            "timing_context_comparable": options_match and hosts_match and
                                         input_hashes_verified and
                                         loads_recorded,
            "executable_match": bool(
                (current_configuration or {}).get("step_g") and
                (previous_configuration or {}).get("step_g")
            ) and converter_identity(current_configuration) ==
                  converter_identity(previous_configuration),
            "current_outer_timeout_seconds": (
                (current_configuration or {}).get("options") or {}
            ).get("timeout_seconds"),
            "previous_outer_timeout_seconds": (
                (previous_configuration or {}).get("options") or {}
            ).get("timeout_seconds"),
            "current_host_identity": host_identity(current_configuration),
            "previous_host_identity": host_identity(previous_configuration),
            "current_host_observations": current_observations,
            "previous_host_observations": previous_observations,
        },
        "performance": performance_comparison(
            comparable_current, comparable_previous
        ),
        "changes": changes,
    }


def run_configuration(run_directory):
    path = Path(run_directory) / "run-manifest.json"
    if not path.is_file():
        return None
    manifest = read_json(path)
    return {
        "manifest": str(path.resolve()),
        "options": manifest.get("options"),
        "step_g": manifest.get("step_g"),
        "schema_plugins": manifest.get("schema_plugins"),
        "linked_libraries": manifest.get("linked_libraries"),
        "host": manifest.get("host"),
        "host_observations": host_observations(manifest),
        "started_utc": manifest.get("started_utc"),
        "finished_utc": manifest.get("finished_utc"),
    }


def attach_input_identities(files, run_directory):
    path = Path(run_directory) / "run-manifest.json"
    if not path.is_file():
        return
    manifest = read_json(path)
    identities = {
        record.get("input"): record.get("input_identity") or {}
        for record in manifest.get("files") or []
    }
    for item in files:
        identity = identities.get(item["input"]) or {}
        if identity.get("sha256"):
            item["input_sha256"] = identity["sha256"]
            item["input_size"] = identity.get("size")


def markdown(summary):
    total = summary["totals"]
    def count_list(values):
        return ", ".join(
            "{} {}".format(count, name)
            for name, count in sorted(
                values.items(), key=lambda item: (-item[1], item[0])
            )
        )

    lines = [
        "# STEP corpus run {}".format(summary["run_id"]),
        "",
        "Revision: `{}`{}".format(
            summary["revision"], " (dirty)" if summary.get("dirty") else ""
        ),
        "",
        "| Schema | Files clean | Intentional empty | Geometry written | Success |",
        "|---|---:|---:|---:|---:|",
    ]
    for schema, values in sorted(total["by_schema"].items()):
        lines.append("| {} | {}/{} | {} | {}/{} | {:.2%} |".format(
            schema, values["clean_files"], values["completed_files"],
            values["intentionally_empty_files"],
            values["geometry_written"], values["geometry_attempted"],
            values["success_fraction"],
        ))
    lines.append("| Combined | {}/{} | {} | {}/{} | {:.2%} |".format(
        total["clean_files"], total["completed_files"],
        total["intentionally_empty_files"], total["geometry_written"],
        total["geometry_attempted"], total["success_fraction"],
    ))
    lines.extend([
        "",
        "Assembly coverage: {} product(s), {} occurrence(s), maximum depth "
        "{}, using {}.".format(
            total["products"], total["occurrences"],
            total["occurrence_max_depth"],
            ", ".join(total["occurrence_methods"]) or "no occurrence method",
        ),
        "Presentation and metadata: {} style(s) extracted, {} applied, {} "
        "layer(s), {} material(s), {} property value(s), and {} retained "
        "configuration record(s).".format(
            total["styles_extracted"], total["styles_applied"],
            total["layers_extracted"], total["materials_extracted"],
            total["properties_extracted"],
            total["configuration_record_count"],
        ),
        "Validation: {} invalid B-Rep(s), {} preserved and {} rejected; {} "
        "bounded repair(s) and {} tagged curve inference(s) applied.".format(
            total["invalid_breps"], total["invalid_breps_written"],
            total["invalid_breps_rejected"],
            total["repairs"],
            total["inferred_curves"],
        ),
    ])
    if total["failure_categories"]:
        lines.append("Geometry omission categories: {}.".format(
            count_list(total["failure_categories"])
        ))
    lines.append("Converter terminal statuses: {}.".format(
        count_list(total["converter_exit_status_counts"])
    ))
    if total["unsupported_entity_counts"]:
        lines.append("Unsupported STEP entity inventory: {}.".format(
            count_list(total["unsupported_entity_counts"])
        ))
    observations = ((summary.get("run_configuration") or {}).get(
        "host_observations") or {})
    one_minute = (observations.get("load_average") or {}).get("one_minute")
    if one_minute:
        lines.extend([
            "",
            "Host one-minute load over {} boundary samples: {:.2f} mean "
            "({:.2f}-{:.2f}).".format(
                observations["snapshot_count"], one_minute["mean"],
                one_minute["minimum"], one_minute["maximum"],
            ),
        ])
    if total["inconclusive_files"]:
        lines.append("")
        lines.append(
            "Inconclusive (timed out or interrupted): {} file(s); partial "
            "geometry is excluded from totals and deltas.".format(
                total["inconclusive_files"]
            )
        )
    lines.extend([
        "", "## Per file", "",
        "| Schema | Input | Status | Written | Skipped | Inferred | Reported load s | "
        "Reported convert s | Wall s | Peak RSS MiB |",
        "|---|---|---|---:|---:|---:|---:|---:|---:|---:|",
    ])
    for item in summary["files"]:
        lines.append("| {} | {} | {} | {}/{} | {} | {} | {:.1f} | {:.1f} | {:.1f} | {:.1f} |".format(
            item["schema"], item["name"].replace("|", "\\|"),
            "clean" if item["clean"] else
            ("intentional empty" if item.get("intentionally_empty") else
             ("partial" if run_complete(item) else "inconclusive")),
            item["geometry_written"], item["geometry_attempted"],
            item["geometry_skipped"], item.get("inferred_curves", 0),
            item["load_us"] / 1.0e6,
            item["convert_us"] / 1.0e6, item["elapsed_seconds"],
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
        performance = delta.get("performance") or {}
        ratios = performance.get("aggregate_ratios") or {}
        if performance.get("comparable_files"):
            lines.extend([
                "",
                "Performance over {} comparable files (current/previous): "
                "wall {:.3f}x and peak-file RSS {:.3f}x. Reported phase "
                "ratios are load {:.3f}x and conversion {:.3f}x; phase "
                "boundaries may differ across binaries.".format(
                    performance["comparable_files"],
                    ratios.get("elapsed") or 0.0,
                    ratios.get("peak_file_rss") or 0.0,
                    ratios.get("load") or 0.0,
                    ratios.get("convert") or 0.0,
                ),
            ])
        configuration = delta.get("configuration") or {}
        if not configuration.get("converter_options_match"):
            lines.append(
                "Run options differ or could not be verified; performance "
                "ratios are informational only."
            )
        if not configuration.get("host_identity_match"):
            lines.append(
                "Host identity differs or could not be verified; timing "
                "ratios are not controlled benchmarks."
            )
        if not configuration.get("input_hashes_verified"):
            lines.append(
                "Input hashes differ or were not recorded in both runs; "
                "timing ratios are informational only."
            )
        if not configuration.get("load_observations_recorded"):
            lines.append(
                "Both runs did not record host-load observations; "
                "timing ratios are not controlled benchmarks."
            )
        else:
            current_load = configuration["current_host_observations"][
                "load_average"
            ]["one_minute"]["mean"]
            previous_load = configuration["previous_host_observations"][
                "load_average"
            ]["one_minute"]["mean"]
            lines.append(
                "Recorded one-minute load mean was {:.2f} current versus "
                "{:.2f} previous.".format(current_load, previous_load)
            )
        if configuration.get("timing_context_comparable"):
            lines.append(
                "Host, input hashes, converter options, and host-load "
                "telemetry are comparable; unrecorded background activity "
                "may still affect timings."
            )
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
    attach_input_identities(files, run_directory)

    revision, dirty = git_state(Path(__file__).resolve().parents[2])
    previous = read_json(args.previous) if args.previous else None
    current_configuration = run_configuration(run_directory)
    previous_configuration = run_configuration(
        previous.get("run_directory")
    ) if previous and previous.get("run_directory") else None
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
        "run_configuration": current_configuration,
        "totals": totals(files),
        "files": files,
    }
    summary["delta"] = compare(
        files, previous, current_configuration, previous_configuration
    )

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
