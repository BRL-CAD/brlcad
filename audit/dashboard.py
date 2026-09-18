#!/usr/bin/env python3
import os
import json
import time
from collections import defaultdict

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
AUDIT_DIR = os.path.join(REPO_ROOT, "audit")
FILES_LEDGER = os.path.join(AUDIT_DIR, "files_ledger.json")
ISSUES_LEDGER = os.path.join(AUDIT_DIR, "issues_ledger.json")
DASHBOARD_MD = os.path.join(AUDIT_DIR, "DASHBOARD.md")
AUDIT_LEDGER_MD = os.path.join(AUDIT_DIR, "AUDIT_LEDGER.md")

def load_json(filepath, default):
    if not os.path.exists(filepath):
        return default
    with open(filepath, "r", encoding="utf-8") as f:
        return json.load(f)

def generate_dashboard():
    fdata = load_json(FILES_LEDGER, {"files": []})
    idata = load_json(ISSUES_LEDGER, {"issues": []})

    files = fdata.get("files", [])
    issues = idata.get("issues", [])

    total_files = len(files)
    reviewed_files = sum(1 for f in files if f.get("status") == "REVIEWED")
    pending_files = total_files - reviewed_files
    pct_complete = (reviewed_files / total_files * 100) if total_files > 0 else 0.0

    # Issues breakdown
    sev_counts = {1: 0, 2: 0, 3: 0}
    status_counts = {"PENDING": 0, "CONFIRMED": 0, "DISPROVEN": 0, "FIXED": 0}
    cat_counts = defaultdict(int)

    for iss in issues:
        sev = iss.get("severity", 1)
        sev_counts[sev] = sev_counts.get(sev, 0) + 1
        
        vstat = iss.get("verification_status", "PENDING")
        fstat = iss.get("fix_status", "UNFIXED")
        if fstat == "FIXED":
            status_counts["FIXED"] += 1
        else:
            status_counts[vstat] = status_counts.get(vstat, 0) + 1

        cat = iss.get("category", "Unknown")
        cat_counts[cat] += 1

    # Subsystem breakdown
    subsys_stats = defaultdict(lambda: {"total": 0, "reviewed": 0, "issues": 0})
    for f in files:
        ss = f.get("subsystem", "other")
        subsys_stats[ss]["total"] += 1
        if f.get("status") == "REVIEWED":
            subsys_stats[ss]["reviewed"] += 1
        subsys_stats[ss]["issues"] += f.get("issues_count", 0)

    # Render Markdown Dashboard
    now_str = time.strftime("%Y-%m-%d %H:%M:%S UTC", time.gmtime())
    md_lines = [
        "# BRL-CAD RMF/STIG Cat 1 Security Audit Dashboard",
        f"**Last Updated:** {now_str}",
        "",
        "## Overall Progress",
        f"- **Total C/C++ Files:** {total_files}",
        f"- **Files Reviewed:** {reviewed_files} ({pct_complete:.1f}%)",
        f"- **Files Pending Review:** {pending_files}",
        f"- **Total Issues Identified:** {len(issues)}",
        "",
        "### Issues by Severity Potential",
        "| Severity Level | Count | Description |",
        "|:---:|:---:|:---|",
        f"| **3 (High)** | {sev_counts[3]} | Likely exploit or crash potential; widespread/library exposure |",
        f"| **2 (Medium)** | {sev_counts[2]} | Possible exploit or crash under specific circumstances |",
        f"| **1 (Low)** | {sev_counts[1]} | Localized / low-impact vulnerability |",
        "",
        "### Issues by Verification Status",
        f"- **Confirmed:** {status_counts.get('CONFIRMED', 0)}",
        f"- **Fixed (Committed):** {status_counts.get('FIXED', 0)}",
        f"- **Pending Verification:** {status_counts.get('PENDING', 0)}",
        f"- **Disproven:** {status_counts.get('DISPROVEN', 0)}",
        "",
        "## Subsystem Review Progress",
        "| Subsystem | Total Files | Reviewed | Progress | Issues Found |",
        "|:---|:---:|:---:|:---:|:---:|"
    ]

    for ss in sorted(subsys_stats.keys()):
        stats = subsys_stats[ss]
        pct = (stats["reviewed"] / stats["total"] * 100) if stats["total"] > 0 else 0
        md_lines.append(f"| `{ss}` | {stats['total']} | {stats['reviewed']} | {pct:.1f}% | {stats['issues']} |")

    if issues:
        md_lines.extend([
            "",
            "## Identified Issues Summary",
            "| ID | Severity | Category | File & Location | Verification | Nature of Issue |",
            "|:---|:---:|:---|:---|:---:|:---|"
        ])
        for iss in issues:
            v_badge = iss.get("verification_status", "PENDING")
            if iss.get("fix_status") == "FIXED":
                v_badge = "FIXED"
            md_lines.append(
                f"| `{iss['id']}` | **Sev {iss['severity']}** | {iss['category']} | `{iss['file']}:{iss['lines']}` | `{v_badge}` | {iss['description']} |"
            )

    md_content = "\n".join(md_lines) + "\n"
    with open(DASHBOARD_MD, "w", encoding="utf-8") as f:
        f.write(md_content)

    # Render Markdown Ledger
    ledger_lines = [
        "# BRL-CAD RMF/STIG Cat 1 Vulnerability Tracking Ledger",
        f"**Last Synchronized:** {now_str}",
        "",
        "| Issue ID | File | Lines | Cat 1 Type | Sev | Verification | Fix Status | 1-Sentence Nature of Issue |",
        "|:---|:---|:---:|:---|:---:|:---:|:---:|:---|"
    ]
    for iss in issues:
        v_badge = iss.get("verification_status", "PENDING")
        fix_badge = iss.get("fix_status", "UNFIXED")
        ledger_lines.append(
            f"| `{iss['id']}` | `{iss['file']}` | `{iss['lines']}` | {iss['category']} | {iss['severity']} | `{v_badge}` | `{fix_badge}` | {iss['description']} |"
        )
    
    with open(AUDIT_LEDGER_MD, "w", encoding="utf-8") as f:
        f.write("\n".join(ledger_lines) + "\n")

    # Terminal Output
    print("=" * 68)
    print(" BRL-CAD RMF/STIG Cat 1 Security Audit Dashboard")
    print("=" * 68)
    print(f" Total C/C++ Files:    {total_files}")
    print(f" Files Reviewed:       {reviewed_files} / {total_files} ({pct_complete:.1f}%)")
    print(f" Files Pending:        {pending_files}")
    print(f" Total Issues:         {len(issues)}")
    print(f" Severity Breakdown:   High (3): {sev_counts[3]} | Med (2): {sev_counts[2]} | Low (1): {sev_counts[1]}")
    print(f" Verification Status:  Confirmed: {status_counts.get('CONFIRMED', 0)} | Fixed: {status_counts.get('FIXED', 0)} | Pending: {status_counts.get('PENDING', 0)} | Disproven: {status_counts.get('DISPROVEN', 0)}")
    print("=" * 68)
    print(f" Updated dashboard: {DASHBOARD_MD}")
    print(f" Updated ledger:    {AUDIT_LEDGER_MD}")
    print("=" * 68)

if __name__ == "__main__":
    generate_dashboard()
