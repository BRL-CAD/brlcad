#!/usr/bin/env python3
"""Verify a recorded STEP corpus run against checksum-pinned expectations.

External STEP samples are often valuable but not suitable for copying into
BRL-CAD.  This tool keeps their evidence reproducible without copying the
files: an expectation manifest records the source, accepted local filenames,
SHA-256 digest, and report assertions.  The verifier consumes the structured
reports and run manifest produced by ``step_corpus_run.py``.
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path


FORMAT = "brlcad-step-corpus-verification-v1"


def read_json(path):
    with path.open("r", encoding="utf-8") as stream:
        return json.load(stream)


def file_sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def dotted_value(document, path):
    value = document
    for component in path.split("."):
        if not isinstance(value, dict) or component not in value:
            raise KeyError(path)
        value = value[component]
    return value


def occurrence_depth(occurrences):
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
            raise ValueError("occurrence product graph contains a cycle")
        active.add(vertex)
        depth = max((1 + visit(child) for child in children.get(vertex, ())),
                    default=0)
        active.remove(vertex)
        memo[vertex] = depth
        return depth

    return max((visit(vertex) for vertex in vertices), default=0)


def derived_values(report):
    occurrences = report.get("occurrence_details") or []
    metadata = report.get("product_metadata") or []
    materials = [
        material
        for product in metadata
        for material in product.get("materials") or []
    ]
    material_properties = [
        prop
        for material in materials
        for prop in material.get("properties") or []
    ]
    material_numeric_values = sorted({
        value
        for prop in material_properties
        for value in prop.get("values") or []
        if isinstance(value, (int, float))
    })
    return {
        "assembly_usage_count": len(report.get("assembly_usages") or []),
        "occurrence_detail_count": len(occurrences),
        "occurrence_max_depth": occurrence_depth(occurrences),
        "occurrence_methods": sorted({
            item.get("shape_method") or "" for item in occurrences
        }),
        "skipped_entity_ids": sorted({
            int(item.get("entity_id") or 0)
            for item in report.get("skipped_items") or []
            if int(item.get("entity_id") or 0) > 0
        }),
        "product_metadata_count": len(metadata),
        "material_count": len(materials),
        "material_identifiers": sorted({
            material.get("identifier") or "" for material in materials
        }),
        "material_names": sorted({
            material.get("name") or "" for material in materials
        }),
        "material_property_count": len(material_properties),
        "material_property_names": sorted({
            prop.get("name") or "" for prop in material_properties
        }),
        "material_property_units": sorted({
            prop.get("units") or "" for prop in material_properties
            if prop.get("units")
        }),
        "material_property_numeric_values": material_numeric_values,
        "validation_property_count": sum(
            len(product.get("validation_properties") or [])
            for product in metadata
        ),
    }


def compare_expected(report, expected):
    combined = dict(report)
    combined["derived"] = derived_values(report)
    checks = []
    errors = []
    for path, wanted in sorted((expected.get("equals") or {}).items()):
        try:
            actual = dotted_value(combined, path)
        except KeyError:
            errors.append("{} is missing".format(path))
            continue
        checks.append(path)
        if actual != wanted:
            errors.append("{} is {!r}, expected {!r}".format(
                path, actual, wanted
            ))
    for path, wanted in sorted((expected.get("minimum") or {}).items()):
        try:
            actual = dotted_value(combined, path)
        except KeyError:
            errors.append("{} is missing".format(path))
            continue
        checks.append(path)
        if not isinstance(actual, (int, float)) or actual < wanted:
            errors.append("{} is {!r}, expected at least {!r}".format(
                path, actual, wanted
            ))
    return checks, errors, combined["derived"]


def markdown(result):
    lines = [
        "# STEP external corpus verification",
        "",
        "Expectation manifest: `{}`".format(result["expectations"]),
        "",
        "| Input | Source/producer | Result | Checks |",
        "|---|---|---:|---:|",
    ]
    for item in result["files"]:
        source = item.get("producer") or item.get("source_page") or ""
        lines.append("| {} | {} | {} | {} |".format(
            item["name"].replace("|", "\\|"),
            source.replace("|", "\\|"),
            item["result"], len(item.get("checks") or []),
        ))
        for error in item.get("errors") or []:
            lines.append("  - **{}:** {}".format(item["name"], error))
    lines.extend([
        "",
        "Verified {}/{} present files; {} manifest entries were missing.".format(
            result["passed_files"], result["present_files"],
            result["missing_files"],
        ),
    ])
    return "\n".join(lines) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("run_directory", type=Path)
    parser.add_argument("expectations", type=Path)
    parser.add_argument("--allow-missing", action="store_true",
                        help="do not fail for expectation entries absent from the run")
    parser.add_argument("--output", type=Path,
                        help="output JSON (default: RUN/corpus-verification.json)")
    args = parser.parse_args()

    run_directory = args.run_directory.resolve()
    manifest_path = run_directory / "run-manifest.json"
    if not manifest_path.is_file():
        parser.error("{} does not contain run-manifest.json".format(run_directory))
    expectations_path = args.expectations.resolve()
    run = read_json(manifest_path)
    expectations = read_json(expectations_path)
    if expectations.get("format") != "brlcad-step-corpus-expectations-v1":
        parser.error("unsupported expectation manifest format")

    run_files = {}
    for record in run.get("files") or []:
        input_name = Path(record.get("input") or "").name
        if input_name:
            run_files[input_name] = record

    results = []
    passed = 0
    present = 0
    missing = 0
    for expectation in expectations.get("files") or []:
        names = [expectation["name"]] + list(expectation.get("aliases") or [])
        record = next((run_files[name] for name in names if name in run_files), None)
        item = {
            "name": expectation["name"],
            "source_page": expectation.get("source_page"),
            "producer": expectation.get("producer"),
            "checks": [],
            "errors": [],
        }
        if record is None:
            missing += 1
            item["result"] = "missing"
            if not args.allow_missing:
                item["errors"].append("input was not present in the corpus run")
            results.append(item)
            continue

        present += 1
        input_path = Path(record["input"])
        actual_hash = file_sha256(input_path) if input_path.is_file() else None
        item["input"] = str(input_path)
        item["sha256"] = actual_hash
        item["checks"].append("sha256")
        recorded_hash = (record.get("input_identity") or {}).get("sha256")
        if recorded_hash and recorded_hash != actual_hash:
            item["errors"].append(
                "input changed after the run manifest recorded its SHA-256"
            )
        if actual_hash != expectation.get("sha256"):
            item["errors"].append("SHA-256 is {}, expected {}".format(
                actual_hash, expectation.get("sha256")
            ))

        report_name = record.get("report")
        report_path = run_directory / report_name if report_name else None
        if not report_path or not report_path.is_file():
            item["errors"].append("structured converter report is missing")
        else:
            report = read_json(report_path)
            checks, errors, derived = compare_expected(
                report, expectation.get("expected") or {}
            )
            item["checks"].extend(checks)
            item["errors"].extend(errors)
            item["derived"] = derived
            item["report"] = str(report_path)

        item["result"] = "pass" if not item["errors"] else "fail"
        if item["result"] == "pass":
            passed += 1
        results.append(item)

    failed = sum(1 for item in results if item.get("errors"))
    result = {
        "format": FORMAT,
        "expectations": str(expectations_path),
        "run_directory": str(run_directory),
        "passed_files": passed,
        "present_files": present,
        "missing_files": missing,
        "failed_files": failed,
        "files": results,
    }
    output = args.output or (run_directory / "corpus-verification.json")
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n",
                      encoding="utf-8")
    output.with_suffix(".md").write_text(markdown(result), encoding="utf-8")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
