#!/usr/bin/env python3

"""Validate and summarize two BRep corpus audit result sets."""

import argparse
import collections
import hashlib
import json
import math
import statistics
import sys
from pathlib import Path


REALIZATION_FORMAT = "brlcad-brep-realization-audit-v1"
MODES = ("wireframe", "shaded", "quality")
TOP_LEVEL_FIELDS = (
    "database", "object", "task_index", "status", "mode",
    "ratio_limits", "tessellation_tolerance", "memory_limit_mib",
    "input", "fast_options", "wire_options", "generators", "reference",
    "wireframe", "shaded", "quality", "images", "issues",
)
RESULT_FIELDS = (
    "return_code", "seconds", "vertices", "primitives", "commands",
    "peak_rss_bytes", "peak_working_bytes", "invalid_indices", "finite",
    "bbox_valid", "bbox_min", "bbox_max", "dimensions",
    "dimension_ratios", "limits", "solid_validation", "repair", "issues",
)
NONNEGATIVE_RESULT_FIELDS = (
    "seconds", "vertices", "primitives", "commands", "peak_rss_bytes",
    "peak_working_bytes", "invalid_indices",
)
PAIR_CONFIG_FIELDS = (
    ("memory_limit_mib",),
    ("ratio_limits",),
    ("tessellation_tolerance",),
    ("quality_options", "max_face_time_ms"),
)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline-name", default="main")
    parser.add_argument("--candidate-name", default="branch")
    parser.add_argument(
        "--baseline", nargs="+", type=Path, required=True,
        help="baseline JSONL files in chronological escalation order",
    )
    parser.add_argument(
        "--candidate", nargs="+", type=Path, required=True,
        help="candidate JSONL files in chronological escalation order",
    )
    parser.add_argument("--output", type=Path)
    parser.add_argument(
        "--allow-incomplete", action="store_true",
        help="report unequal task sets without returning an error",
    )
    return parser.parse_args()


def field(record, path):
    value = record
    for component in path:
        if not isinstance(value, dict) or component not in value:
            return None
        value = value[component]
    return value


def finite_nonnegative(value):
    return isinstance(value, (int, float)) and not isinstance(value, bool) \
        and math.isfinite(value) and value >= 0


def validate_vector(value):
    return isinstance(value, list) and len(value) == 3 and \
        all(isinstance(component, (int, float)) and
            not isinstance(component, bool) and math.isfinite(component)
            for component in value)


def validate_realization(record):
    errors = []
    for name in TOP_LEVEL_FIELDS:
        if name not in record:
            errors.append("missing top-level field '{}'".format(name))
    mode = record.get("mode")
    if mode not in MODES:
        errors.append("invalid mode {!r}".format(mode))
        return errors
    result = record.get(mode)
    if not isinstance(result, dict):
        errors.append("mode result '{}' is not an object".format(mode))
        return errors
    for name in RESULT_FIELDS:
        if name not in result:
            errors.append("missing {}.{}".format(mode, name))
    for name in NONNEGATIVE_RESULT_FIELDS:
        value = result.get(name)
        if not finite_nonnegative(value):
            errors.append("invalid {}.{} value {!r}".format(
                mode, name, value
            ))
    if not isinstance(result.get("return_code"), int):
        errors.append("invalid {}.return_code".format(mode))
    for name in ("finite", "bbox_valid"):
        if not isinstance(result.get(name), bool):
            errors.append("invalid {}.{}".format(mode, name))
    if result.get("bbox_valid"):
        for name in ("bbox_min", "bbox_max", "dimensions"):
            if not validate_vector(result.get(name)):
                errors.append("invalid {}.{}".format(mode, name))
    if not isinstance(result.get("issues"), list):
        errors.append("invalid {}.issues".format(mode))
    if not isinstance(record.get("issues"), list):
        errors.append("invalid top-level issues")
    if record.get("status") == "ok":
        if result.get("issues") or record.get("issues"):
            errors.append("ok record contains issues")
        if not result.get("bbox_valid"):
            errors.append("ok record has no bounding box")
        if result.get("vertices", 0) == 0 or result.get("primitives", 0) == 0:
            errors.append("ok record has empty geometry")
        if mode == "quality":
            solid = result.get("solid_validation")
            if not isinstance(solid, dict) or not solid.get("checked") or \
                    not solid.get("solid"):
                errors.append("ok quality record lacks solid validation")
    return errors


def file_digest(path):
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_records(paths):
    records = {}
    errors = []
    inputs = []
    for path in paths:
        inputs.append((path, file_digest(path)))
        with path.open("r", encoding="utf-8", errors="replace") as source:
            for line_number, line in enumerate(source, 1):
                try:
                    record = json.loads(line)
                except json.JSONDecodeError as error:
                    errors.append("{}:{}: invalid JSON: {}".format(
                        path, line_number, error
                    ))
                    continue
                database = record.get("database")
                object_name = record.get("object")
                mode = record.get("mode")
                if not database or not object_name or mode not in MODES:
                    continue
                key = (database, object_name, mode)
                if record.get("format") == REALIZATION_FORMAT:
                    for error in validate_realization(record):
                        errors.append("{}:{}: {}: {}".format(
                            path, line_number, "/".join(key), error
                        ))
                elif record.get("status") in (None, "ok"):
                    errors.append("{}:{}: unrecognized result format".format(
                        path, line_number
                    ))
                records[key] = record
    return records, errors, inputs


def percentile(values, fraction):
    if not values:
        return None
    ordered = sorted(values)
    index = math.ceil(fraction * len(ordered)) - 1
    return ordered[max(0, min(index, len(ordered) - 1))]


def fmt_number(value):
    if value is None:
        return "-"
    if abs(value) >= 1000:
        return "{:.0f}".format(value)
    return "{:.6g}".format(value)


def timing_values(records, mode):
    values = []
    for record in records.values():
        if record.get("mode") != mode or \
                record.get("format") != REALIZATION_FORMAT:
            continue
        seconds = field(record, (mode, "seconds"))
        if finite_nonnegative(seconds):
            values.append(seconds)
    return values


def status_counts(records, mode):
    counts = collections.Counter()
    for record in records.values():
        if record.get("mode") == mode:
            counts[record.get("status", "unknown")] += 1
    return counts


def failure_category(record):
    status = record.get("status", "unknown")
    if status in ("ok", "excluded"):
        return status
    mode = record.get("mode")
    mode_issues = field(record, (mode, "issues")) or []
    top_issues = record.get("issues") or []
    if mode_issues:
        return mode_issues[0]
    if top_issues:
        return top_issues[0]
    return status


def nested_equal(left, right, path):
    return field(left, path) == field(right, path)


def comparison_report(args, baseline, candidate, baseline_inputs,
                      candidate_inputs, schema_errors):
    baseline_keys = set(baseline)
    candidate_keys = set(candidate)
    common_keys = baseline_keys & candidate_keys
    baseline_only = sorted(baseline_keys - candidate_keys)
    candidate_only = sorted(candidate_keys - baseline_keys)
    config_errors = []
    for key in sorted(common_keys):
        left = baseline[key]
        right = candidate[key]
        if left.get("format") != REALIZATION_FORMAT or \
                right.get("format") != REALIZATION_FORMAT:
            continue
        for path in PAIR_CONFIG_FIELDS:
            if not nested_equal(left, right, path):
                config_errors.append(
                    "{}: {} differs: {!r} versus {!r}".format(
                        "/".join(key), ".".join(path), field(left, path),
                        field(right, path)
                    )
                )

    lines = []
    lines.append("BRep audit comparison")
    lines.append("=====================")
    lines.append("")
    lines.append("Inputs")
    lines.append("------")
    for name, inputs in (
            (args.baseline_name, baseline_inputs),
            (args.candidate_name, candidate_inputs)):
        for path, digest in inputs:
            lines.append("{}  {}  {}".format(name, digest, path))
    lines.append("")
    lines.append("Validation")
    lines.append("----------")
    lines.append("schema errors: {}".format(len(schema_errors)))
    lines.append("configuration mismatches: {}".format(len(config_errors)))
    lines.append("matched tasks: {}".format(len(common_keys)))
    lines.append("{}-only tasks: {}".format(
        args.baseline_name, len(baseline_only)
    ))
    lines.append("{}-only tasks: {}".format(
        args.candidate_name, len(candidate_only)
    ))
    lines.append("")
    lines.append("Outcome counts")
    lines.append("--------------")
    lines.append("revision       mode       total      ok    fail  timeout excluded other")
    for name, records in (
            (args.baseline_name, baseline),
            (args.candidate_name, candidate)):
        for mode in MODES:
            counts = status_counts(records, mode)
            known = sum(counts[name] for name in
                        ("ok", "fail", "timeout", "excluded"))
            total = sum(counts.values())
            lines.append(
                "{:<14} {:<10} {:>5} {:>7} {:>7} {:>8} {:>8} {:>5}".format(
                    name, mode, total, counts["ok"], counts["fail"],
                    counts["timeout"], counts["excluded"], total - known
                )
            )
    lines.append("")
    lines.append("Generator timing (seconds, completed process records)")
    lines.append("-----------------------------------------------------")
    lines.append("revision       mode           n       total      median         p95         max")
    for name, records in (
            (args.baseline_name, baseline),
            (args.candidate_name, candidate)):
        for mode in MODES:
            values = timing_values(records, mode)
            lines.append(
                "{:<14} {:<10} {:>5} {:>11} {:>11} {:>11} {:>11}".format(
                    name, mode, len(values), fmt_number(sum(values)),
                    fmt_number(statistics.median(values) if values else None),
                    fmt_number(percentile(values, 0.95)),
                    fmt_number(max(values) if values else None),
                )
            )
    lines.append("")
    lines.append("Matched successful timing ratios")
    lines.append("---------------------------------")
    lines.append("mode           n  candidate/baseline median         p95")
    for mode in MODES:
        ratios = []
        for key in common_keys:
            if key[2] != mode:
                continue
            left = baseline[key]
            right = candidate[key]
            if left.get("status") != "ok" or right.get("status") != "ok":
                continue
            left_seconds = field(left, (mode, "seconds"))
            right_seconds = field(right, (mode, "seconds"))
            if finite_nonnegative(left_seconds) and left_seconds > 0 and \
                    finite_nonnegative(right_seconds):
                ratios.append(right_seconds / left_seconds)
        lines.append("{:<10} {:>5} {:>26} {:>11}".format(
            mode, len(ratios),
            fmt_number(statistics.median(ratios) if ratios else None),
            fmt_number(percentile(ratios, 0.95)),
        ))
    lines.append("")
    lines.append("Failure categories")
    lines.append("------------------")
    for name, records in (
            (args.baseline_name, baseline),
            (args.candidate_name, candidate)):
        counts = collections.Counter(
            (record.get("mode"), failure_category(record))
            for record in records.values() if record.get("status") != "ok"
        )
        if not counts:
            lines.append("{}: none".format(name))
            continue
        for (mode, category), count in sorted(counts.items()):
            lines.append("{} {:<10} {:>5} {}".format(
                name, mode, count, category
            ))
    if schema_errors or config_errors or baseline_only or candidate_only:
        lines.append("")
        lines.append("Validation details")
        lines.append("------------------")
        lines.extend(schema_errors)
        lines.extend(config_errors)
        for key in baseline_only:
            lines.append("{} only: {}".format(
                args.baseline_name, "/".join(key)
            ))
        for key in candidate_only:
            lines.append("{} only: {}".format(
                args.candidate_name, "/".join(key)
            ))
    return "\n".join(lines) + "\n", bool(
        schema_errors or config_errors or baseline_only or candidate_only
    )


def main():
    args = parse_args()
    baseline, baseline_errors, baseline_inputs = load_records(args.baseline)
    candidate, candidate_errors, candidate_inputs = load_records(
        args.candidate
    )
    report, invalid = comparison_report(
        args, baseline, candidate, baseline_inputs, candidate_inputs,
        baseline_errors + candidate_errors,
    )
    if args.output:
        args.output.write_text(report, encoding="utf-8")
    else:
        sys.stdout.write(report)
    return 1 if invalid and not args.allow_incomplete else 0


if __name__ == "__main__":
    sys.exit(main())
