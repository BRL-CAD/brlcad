#!/usr/bin/env python3
import os
import json
import time

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
AUDIT_DIR = os.path.join(REPO_ROOT, "audit")
FILES_LEDGER = os.path.join(AUDIT_DIR, "files_ledger.json")

EXTENSIONS = {".c", ".cpp", ".cxx", ".cc", ".h", ".hpp", ".hxx", ".hh"}
EXCLUDE_DIRS = {".git", "build", "audit", ".github"}

def get_subsystem(rel_path):
    parts = rel_path.split(os.sep)
    if len(parts) > 1 and parts[0] == "src":
        if parts[1] == "conv" and len(parts) > 2:
            return f"src/conv/{parts[2]}"
        return f"src/{parts[1]}"
    elif len(parts) > 1 and parts[0] == "include":
        return "include"
    elif len(parts) > 1:
        return parts[0]
    return "root"

def count_lines(filepath):
    try:
        with open(filepath, "r", encoding="utf-8", errors="ignore") as f:
            return sum(1 for _ in f)
    except Exception:
        return 0

def main():
    existing_ledger = {}
    if os.path.exists(FILES_LEDGER):
        try:
            with open(FILES_LEDGER, "r", encoding="utf-8") as f:
                data = json.load(f)
                for item in data.get("files", []):
                    existing_ledger[item["path"]] = item
        except Exception as e:
            print(f"Warning: could not load existing ledger: {e}")

    files = []
    for root, dirs, filenames in os.walk(REPO_ROOT):
        dirs[:] = [d for d in dirs if d not in EXCLUDE_DIRS and not d.startswith("build")]
        for fn in filenames:
            ext = os.path.splitext(fn)[1].lower()
            if ext in EXTENSIONS:
                full_path = os.path.join(root, fn)
                rel_path = os.path.relpath(full_path, REPO_ROOT)
                files.append(rel_path)

    files.sort()

    ledger_entries = []
    for idx, rel_path in enumerate(files, 1):
        if rel_path in existing_ledger:
            entry = existing_ledger[rel_path]
            entry["id"] = idx
            ledger_entries.append(entry)
        else:
            full_path = os.path.join(REPO_ROOT, rel_path)
            lines = count_lines(full_path)
            subsystem = get_subsystem(rel_path)
            ledger_entries.append({
                "id": idx,
                "path": rel_path,
                "subsystem": subsystem,
                "lines": lines,
                "status": "PENDING",
                "reviewed_at": None,
                "issues_count": 0,
                "issue_ids": []
            })

    output_data = {
        "metadata": {
            "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "total_files": len(ledger_entries),
            "extensions": sorted(list(EXTENSIONS))
        },
        "files": ledger_entries
    }

    with open(FILES_LEDGER, "w", encoding="utf-8") as f:
        json.dump(output_data, f, indent=2)

    print(f"Successfully indexed {len(ledger_entries)} C/C++ files into {FILES_LEDGER}")

if __name__ == "__main__":
    main()
