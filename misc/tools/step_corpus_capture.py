#!/usr/bin/env python3
"""Capture standard swrast overview images for converted STEP databases.

Each database is opened in its own gsh process so the display manager and GED
state are isolated.  The two modes are written to separate directories and
use the database filename (without its final ``.g``) as the image name.
"""

import argparse
import json
import os
import re
import subprocess
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path


def image_stem(path):
    return path.name[:-2] if path.name.lower().endswith(".g") else path.stem


def capture(gsh, model, output, mode, azimuth, elevation, timeout):
    output.mkdir(parents=True, exist_ok=True)
    image = output / (image_stem(model) + ".png")
    try:
        tops = subprocess.run(
            [str(gsh), "--new-cmds", str(model), "tops"],
            text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            timeout=timeout,
        ).stdout.splitlines()
    except subprocess.TimeoutExpired:
        return {"model": str(model), "mode": mode, "image": str(image),
                "returncode": 124, "complete": False,
                "log": "timed out while listing top-level objects"}
    tops = [line.strip().rstrip("/") for line in tops
            if line.strip().endswith("/") and line.strip().count("/") == 1]
    if not tops:
        return {"model": str(model), "mode": mode, "image": str(image),
                "returncode": 1, "complete": False,
                "log": "database has no top-level objects"}
    commands = [
        "dm attach swrast SWDM",
        "draw -m{} {}".format(mode, " ".join(tops)),
        "autoview",
        "ae {} {}".format(azimuth, elevation),
        "screengrab -D SWDM {}".format(image),
        "quit",
    ]
    try:
        proc = subprocess.run(
            [str(gsh), "--new-cmds", str(model)],
            input="\n".join(commands) + "\n",
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            timeout=timeout,
        )
    except subprocess.TimeoutExpired as error:
        return {"model": str(model), "mode": mode, "image": str(image),
                "returncode": 124, "complete": False,
                "log": str(error.stdout or "")[-4000:] + "\ntimed out during capture"}
    return {
        "model": str(model),
        "mode": mode,
        "image": str(image),
        "returncode": proc.returncode,
        "complete": proc.returncode == 0 and image.is_file() and image.stat().st_size > 0,
        "log": proc.stdout[-4000:],
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input_directory", type=Path)
    parser.add_argument("output_directory", type=Path)
    parser.add_argument("--gsh", type=Path, default=Path(".build/bin/gsh"))
    parser.add_argument("--jobs", type=int, default=min(2, os.cpu_count() or 1))
    parser.add_argument("--timeout", type=int, default=300,
                        help="maximum seconds per model and view")
    parser.add_argument("--mode", choices=("wireframe", "shaded", "both"), default="both")
    parser.add_argument("--only", help="regular expression matched against model names")
    parser.add_argument("--azimuth", type=float, default=35.0)
    parser.add_argument("--elevation", type=float, default=25.0)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    if args.timeout < 1:
        parser.error("--timeout must be positive")
    if not args.gsh.is_file():
        parser.error("gsh not found: {}".format(args.gsh))

    matcher = re.compile(args.only) if args.only else None
    models = sorted(path for path in args.input_directory.glob("*.g")
                    if not matcher or matcher.search(path.name))
    if not models:
        parser.error("no converted .g models in {}".format(args.input_directory))
    modes = [("wireframe", 0), ("shaded", 1)] if args.mode == "both" else [
        (args.mode, 0 if args.mode == "wireframe" else 1)
    ]
    jobs = []
    for name, mode in modes:
        target = args.output_directory / name
        jobs.extend((model, target, mode) for model in models)

    with ThreadPoolExecutor(max_workers=args.jobs) as pool:
        results = list(pool.map(
            lambda item: capture(args.gsh, item[0], item[1], item[2],
                                  args.azimuth, args.elevation, args.timeout), jobs
        ))
    args.output_directory.mkdir(parents=True, exist_ok=True)
    manifest = {
        "format": "brlcad-step-corpus-capture-v1",
        "azimuth": args.azimuth,
        "elevation": args.elevation,
        "jobs": args.jobs,
        "results": results,
    }
    (args.output_directory / "capture-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    failed = [item for item in results if not item["complete"]]
    print("captured {}/{} images".format(len(results) - len(failed), len(results)))
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
