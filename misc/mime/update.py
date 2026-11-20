#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-2-Clause
"""Refresh the pinned mime-db snapshot without changing the local supplement."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import urllib.request

ROOT = Path(__file__).resolve().parent
UPSTREAM = "https://raw.githubusercontent.com/jshttp/mime-db/{commit}/{file}"
HEADER = "# MIME name | comma-separated extensions | upstream source\n"
TOKEN = re.compile(r"[a-z0-9][a-z0-9!#$&^_.+-]*\Z")


def render(data):
    rows = []
    for name, record in sorted(json.loads(data).items()):
        parts = name.split("/")
        extensions = record.get("extensions", [])
        source = record.get("source", "custom")
        if (len(parts) != 2 or not all(TOKEN.fullmatch(p) for p in parts)
                or source not in ("iana", "apache", "nginx", "custom")
                or any(not TOKEN.fullmatch(e) for e in extensions)):
            raise ValueError(f"Invalid upstream entry: {name}")
        rows.append(f"{name}|{','.join(extensions)}|{source}\n")
    return HEADER + "".join(rows)


def upstream_bytes(commit, filename, local):
    if local:
        return local.read_bytes()
    with urllib.request.urlopen(UPSTREAM.format(commit=commit, file=filename), timeout=60) as response:
        return response.read()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--commit", help="Audited upstream commit (40 hex digits)")
    parser.add_argument("--source", type=Path, help="Use a previously downloaded db.json")
    parser.add_argument("--license", type=Path, help="Use the matching upstream LICENSE")
    parser.add_argument("--check", action="store_true", help="Verify the pinned snapshot without modifying files")
    args = parser.parse_args()
    previous = json.loads((ROOT / "upstream.json").read_text())
    commit = args.commit or previous["commit"]
    if not re.fullmatch(r"[0-9a-f]{40}", commit):
        parser.error("--commit must be a full lowercase commit hash")
    data = upstream_bytes(commit, "db.json", args.source)
    license_data = upstream_bytes(commit, "LICENSE", args.license)
    metadata = {
        "project": "mime-db", "commit": commit,
        "url": UPSTREAM.format(commit=commit, file="db.json"),
        "sha256": hashlib.sha256(data).hexdigest(), "license": "MIT",
    }
    outputs = {
        "upstream.types": render(data).encode(),
        "upstream.json": (json.dumps(metadata, indent=2) + "\n").encode(),
        "mime-db.LICENSE": license_data,
    }
    if args.check:
        mismatches = [name for name, content in outputs.items() if (ROOT / name).read_bytes() != content]
        if mismatches:
            parser.error("Snapshot differs: " + ", ".join(mismatches))
    else:
        for name, content in outputs.items():
            (ROOT / name).write_bytes(content)


if __name__ == "__main__":
    main()
