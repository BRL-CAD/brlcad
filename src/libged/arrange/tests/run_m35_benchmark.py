#!/usr/bin/env python3
"""Benchmark arrange nest with physical regions from BRL-CAD's M35 model."""

import argparse
import csv
import json
import math
import os
import platform
import re
import resource
import shutil
import subprocess
import sys
import time
from datetime import datetime, timezone
from pathlib import Path


DEFAULT_COUNTS = (10, 100, 887, 1000, 10000, 20000)
DEFAULT_SHAPES = ("convex", "concave")
CORPUS_REFERENCE_COUNT = 887
REFERENCE_SIDE = 1200.0
MINIMUM_SIDE = 320.0
CONCAVE_AREA_FRACTION = 0.744
DEFAULT_CELL_SIZE = 4.0
DEFAULT_CLEARANCE = 0.0
DEFAULT_DEMONSTRATION_CELL_SIZE = 4.0
DEFAULT_DEMONSTRATION_CLEARANCE = 4.0
DEFAULT_DEMONSTRATION_SCALE = 1.5
DEFAULT_DEMONSTRATION_QUALITY = "fast"
DEFAULT_TIMEOUT_SECONDS = 3600.0
CONTAINER_LOW_Z = -500.0
CONCAVE_NOTCH_START_X_FRACTION = 0.42
CONCAVE_NOTCH_LOW_Y_FRACTION = 0.28
CONCAVE_NOTCH_HIGH_Y_FRACTION = 0.72
REPORT_PATTERN = re.compile(
    r"arrange nest: placed (\d+) of (\d+) objects?; value ([0-9.eE+-]+); "
    r"raster fill ([0-9.]+)%"
)
REGION_PATTERN = re.compile(r"r(\d+)")
CMAKE_BUILD_TYPE_PATTERN = re.compile(
    r"^CMAKE_BUILD_TYPE:STRING=(.+)$", re.MULTILINE
)


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path, required=True,
                        help="path to m35.g")
    parser.add_argument("--mged", type=Path, required=True,
                        help="path to the mged executable")
    parser.add_argument("--output-dir", type=Path, required=True,
                        help="directory for CSV, JSON, logs, and demonstration databases")
    parser.add_argument("--counts", type=int, nargs="+", default=DEFAULT_COUNTS)
    parser.add_argument("--shapes", choices=DEFAULT_SHAPES, nargs="+",
                        default=DEFAULT_SHAPES)
    parser.add_argument("--write-counts", type=int, nargs="*",
                        default=(CORPUS_REFERENCE_COUNT,),
                        help="also write packed demonstration databases at these counts")
    parser.add_argument("--exclude-regions", nargs="*", default=(),
                        help="regions independently verified to have no ray-traceable volume")
    parser.add_argument("--cell-size", type=float, default=DEFAULT_CELL_SIZE)
    parser.add_argument("--clearance", type=float, default=DEFAULT_CLEARANCE,
                        help="timed-run separation in model units")
    parser.add_argument("--demonstration-cell-size", type=float,
                        default=DEFAULT_DEMONSTRATION_CELL_SIZE,
                        help="cell size used by retained demonstrations")
    parser.add_argument("--demonstration-quality",
                        choices=("fast", "balanced", "thorough"),
                        default=DEFAULT_DEMONSTRATION_QUALITY,
                        help="search quality used by retained demonstrations")
    parser.add_argument("--demonstration-clearance", type=float,
                        help="override the demonstration separation")
    parser.add_argument("--demonstration-scale", type=float,
                        default=DEFAULT_DEMONSTRATION_SCALE,
                        help="container side multiplier for retained demonstrations")
    parser.add_argument("--timeout", type=float,
                        default=DEFAULT_TIMEOUT_SECONDS)
    args = parser.parse_args()
    if args.cell_size <= 0.0:
        parser.error("--cell-size must be positive")
    if args.clearance < 0.0:
        parser.error("--clearance must be nonnegative")
    if args.demonstration_cell_size <= 0.0:
        parser.error("--demonstration-cell-size must be positive")
    if (args.demonstration_clearance is not None and
            args.demonstration_clearance < 0.0):
        parser.error("--demonstration-clearance must be nonnegative")
    if args.demonstration_scale < 1.0:
        parser.error("--demonstration-scale must be at least one")
    if any(count <= 0 for count in args.counts):
        parser.error("--counts values must be positive")
    return args


def run_mged(mged, database, script, timeout):
    started = time.perf_counter()
    usage_before = resource.getrusage(resource.RUSAGE_CHILDREN)
    completed = subprocess.run(
        [str(mged), "-c", str(database)],
        input=script,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        timeout=timeout,
        check=False,
    )
    usage_after = resource.getrusage(resource.RUSAGE_CHILDREN)
    return {
        "returncode": completed.returncode,
        "output": completed.stdout,
        "wall_seconds": time.perf_counter() - started,
        "user_seconds": usage_after.ru_utime - usage_before.ru_utime,
        "system_seconds": usage_after.ru_stime - usage_before.ru_stime,
    }


def physical_regions(mged, model, timeout, exclusions):
    result = run_mged(
        mged,
        model,
        "search | -type region ! -attr aircode\nq\n",
        timeout,
    )
    if result["returncode"] != 0:
        raise RuntimeError("MGED could not enumerate M35 regions")
    regions = {
        match.group(0): int(match.group(1))
        for match in REGION_PATTERN.finditer(result["output"])
    }
    ordered = [name for name in sorted(regions, key=regions.get)
               if name not in exclusions]
    if not ordered:
        raise RuntimeError("no numbered, non-air regions found in the model")
    return ordered


def build_configuration(mged):
    cache = mged.resolve().parents[1] / "CMakeCache.txt"
    if not cache.is_file():
        return "unknown"
    match = CMAKE_BUILD_TYPE_PATTERN.search(
        cache.read_text(encoding="utf-8", errors="replace")
    )
    return match.group(1) if match else "unknown"


def container_side(count, shape):
    side = max(MINIMUM_SIDE,
               REFERENCE_SIDE * math.sqrt(count / CORPUS_REFERENCE_COUNT))
    if shape == "concave":
        side /= math.sqrt(CONCAVE_AREA_FRACTION)
    return math.ceil(side)


def container_script(shape, side, cell_size):
    low_z = CONTAINER_LOW_Z
    high_z = low_z + cell_size
    commands = [
        f"in arrange-bench-container.s rpp 0 {side} 0 {side} {low_z} {high_z}"
    ]
    expression = "u arrange-bench-container.s"
    if shape == "concave":
        notch_x = CONCAVE_NOTCH_START_X_FRACTION * side
        notch_low_y = CONCAVE_NOTCH_LOW_Y_FRACTION * side
        notch_high_y = CONCAVE_NOTCH_HIGH_Y_FRACTION * side
        commands.append(
            f"in arrange-bench-notch.s rpp {notch_x} {side + cell_size} "
            f"{notch_low_y} {notch_high_y} {low_z - cell_size} "
            f"{high_z + cell_size}"
        )
        expression += " - arrange-bench-notch.s"
    commands.extend([
        f"r arrange-bench-container.r {expression}",
        "mater arrange-bench-container.r \"plastic sh=20 sp=.2\" 55 65 78 0",
    ])
    return "\n".join(commands)


def arrange_command(objects, cell_size, clearance, quality, dry_run,
                    conservative=False):
    dry_option = "-n " if dry_run else ""
    conservative_option = "-C " if conservative else ""
    return (
        f"arrange nest {dry_option}{conservative_option}"
        f"-d 2 -a z -s {cell_size:g} "
        f"-c {clearance:g} "
        f"-o fixed -q {quality} arrange-bench-packed.c arrange-bench-container.r "
        + " ".join(objects)
    )


def demonstration_clearance(args):
    if args.demonstration_clearance is not None:
        return args.demonstration_clearance
    return DEFAULT_DEMONSTRATION_CLEARANCE


def benchmark_case(args, regions, shape, count):
    side = container_side(count, shape)
    objects = [regions[index % len(regions)] for index in range(count)]
    database = args.output_dir / f"m35-{shape}-{count}.g"
    log_path = args.output_dir / f"m35-{shape}-{count}.log"
    shutil.copy2(args.model, database)

    setup = container_script(shape, side, args.cell_size)
    script = "\n".join([
        setup,
        arrange_command(objects, args.cell_size, args.clearance, "fast", True),
        "q",
        "",
    ])
    placed = 0
    try:
        run = run_mged(args.mged, database, script, args.timeout)
        report = REPORT_PATTERN.search(run["output"])
        placed = int(report.group(1)) if report else 0
        if run["returncode"] != 0 or not report:
            status = "error"
        else:
            status = "ok" if placed == count else "partial"
    except subprocess.TimeoutExpired as exc:
        output = exc.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        run = {
            "returncode": None,
            "output": output,
            "wall_seconds": args.timeout,
            "user_seconds": None,
            "system_seconds": None,
        }
        report = None
        status = "timeout"

    log_path.write_text(run["output"], encoding="utf-8")
    row = {
        "shape": shape,
        "requested": count,
        "unique_source_regions": min(count, len(regions)),
        "corpus_regions": len(regions),
        "container_side_model_units": side,
        "cell_size_model_units": args.cell_size,
        "clearance_model_units": args.clearance,
        "demonstration_conservative_bounds": "",
        "demonstration_cell_size_model_units": "",
        "demonstration_clearance_model_units": "",
        "wall_seconds": round(run["wall_seconds"], 6),
        "user_seconds": (round(run["user_seconds"], 6)
                         if run["user_seconds"] is not None else ""),
        "system_seconds": (round(run["system_seconds"], 6)
                           if run["system_seconds"] is not None else ""),
        "placed": placed,
        "raster_value": float(report.group(3)) if report else "",
        "raster_fill_percent": float(report.group(4)) if report else "",
        "objects_per_second": (round(count / run["wall_seconds"], 3)
                               if run["wall_seconds"] else ""),
        "status": status,
        "dry_run": True,
        "log": log_path.name,
        "database": "",
    }

    database.unlink(missing_ok=True)
    if status == "ok" and count in args.write_counts:
        demonstration_side = math.ceil(side * args.demonstration_scale)
        demo_clearance = demonstration_clearance(args)
        row["demonstration_conservative_bounds"] = True
        row["demonstration_cell_size_model_units"] = args.demonstration_cell_size
        row["demonstration_clearance_model_units"] = demo_clearance
        shutil.copy2(args.model, database)
        demonstration = "\n".join([
            container_script(shape, demonstration_side,
                             args.demonstration_cell_size),
            arrange_command(objects, args.demonstration_cell_size,
                            demo_clearance, args.demonstration_quality, False,
                            conservative=True),
            "g arrange-bench-scene.c arrange-bench-container.r arrange-bench-packed.c",
            "q",
            "",
        ])
        demo_run = run_mged(args.mged, database, demonstration, args.timeout)
        demo_log = args.output_dir / f"m35-{shape}-{count}-demonstration.log"
        demo_log.write_text(demo_run["output"], encoding="utf-8")
        demo_report = REPORT_PATTERN.search(demo_run["output"])
        if (demo_run["returncode"] == 0 and demo_report and
                int(demo_report.group(1)) == count):
            row["database"] = database.name
        else:
            row["status"] = "demonstration-error"
    if not row["database"]:
        database.unlink(missing_ok=True)
    return row


def write_results(args, regions, rows):
    csv_path = args.output_dir / "arrange-m35-benchmark.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as stream:
        writer = csv.DictWriter(stream, fieldnames=rows[0].keys())
        writer.writeheader()
        writer.writerows(rows)

    metadata = {
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "platform": platform.platform(),
        "processor": platform.processor(),
        "logical_cpus": os.cpu_count(),
        "model": str(args.model.resolve()),
        "mged": str(args.mged.resolve()),
        "build_configuration": build_configuration(args.mged),
        "cell_size_model_units": args.cell_size,
        "clearance_model_units": args.clearance,
        "default_demonstration_clearance_model_units":
            DEFAULT_DEMONSTRATION_CLEARANCE,
        "demonstration_cell_size_model_units": args.demonstration_cell_size,
        "demonstration_clearance_override": args.demonstration_clearance,
        "demonstration_container_scale": args.demonstration_scale,
        "demonstration_quality": args.demonstration_quality,
        "demonstration_conservative_bounds": True,
        "region_filter": "numbered regions with region flag and no aircode",
        "excluded_regions": args.exclude_regions,
        "corpus_region_count": len(regions),
        "counts": args.counts,
        "shapes": args.shapes,
        "quality": "fast",
        "orientation": "fixed",
        "dimension": "2D projected along z",
        "timing_scope": "fresh MGED process; container creation, rasterization, and dry-run search",
        "rows": rows,
    }
    (args.output_dir / "arrange-m35-benchmark.json").write_text(
        json.dumps(metadata, indent=2) + "\n", encoding="utf-8"
    )


def main():
    args = parse_args()
    args.output_dir.mkdir(parents=True, exist_ok=True)
    if not args.model.is_file():
        raise FileNotFoundError(args.model)
    if not args.mged.is_file():
        raise FileNotFoundError(args.mged)
    regions = physical_regions(args.mged, args.model, args.timeout,
                               set(args.exclude_regions))
    print(f"M35 physical-region corpus: {len(regions)}", flush=True)
    rows = []
    for shape in args.shapes:
        for count in args.counts:
            print(f"{shape:7s} {count:6d}: ", end="", flush=True)
            row = benchmark_case(args, regions, shape, count)
            rows.append(row)
            print(f"{row['status']} in {row['wall_seconds']:.3f} s; "
                  f"placed {row['placed']}/{count}", flush=True)
    write_results(args, regions, rows)
    return 0 if all(row["status"] == "ok" for row in rows) else 1


if __name__ == "__main__":
    try:
        sys.exit(main())
    except (OSError, RuntimeError) as exc:
        print(f"error: {exc}", file=sys.stderr)
        sys.exit(1)
