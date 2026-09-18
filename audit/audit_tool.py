#!/usr/bin/env python3
import os
import sys
import json
import time
import argparse

REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
AUDIT_DIR = os.path.join(REPO_ROOT, "audit")
FILES_LEDGER = os.path.join(AUDIT_DIR, "files_ledger.json")
ISSUES_LEDGER = os.path.join(AUDIT_DIR, "issues_ledger.json")

def load_files():
    if not os.path.exists(FILES_LEDGER):
        return {"files": []}
    with open(FILES_LEDGER, "r", encoding="utf-8") as f:
        return json.load(f)

def save_files(data):
    with open(FILES_LEDGER, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

def load_issues():
    if not os.path.exists(ISSUES_LEDGER):
        return {"issues": []}
    with open(ISSUES_LEDGER, "r", encoding="utf-8") as f:
        return json.load(f)

def save_issues(data):
    with open(ISSUES_LEDGER, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)

def mark_reviewed(file_paths):
    fdata = load_files()
    path_map = {f["path"]: f for f in fdata.get("files", [])}
    ts = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    updated = 0
    for p in file_paths:
        norm_p = os.path.relpath(os.path.join(REPO_ROOT, p), REPO_ROOT)
        if norm_p in path_map:
            path_map[norm_p]["status"] = "REVIEWED"
            path_map[norm_p]["reviewed_at"] = ts
            updated += 1
        else:
            print(f"Warning: file not found in ledger: {norm_p}")
    save_files(fdata)
    print(f"Marked {updated} file(s) as REVIEWED.")

def log_issue(file_path, line_range, severity, category, description):
    fdata = load_files()
    idata = load_issues()
    issues = idata.get("issues", [])
    next_id_num = len(issues) + 1
    issue_id = f"SEC-{next_id_num:04d}"
    
    norm_p = os.path.relpath(os.path.join(REPO_ROOT, file_path), REPO_ROOT)
    
    new_issue = {
        "id": issue_id,
        "file": norm_p,
        "lines": line_range,
        "severity": int(severity),
        "category": category,
        "description": description,
        "verification_status": "PENDING",
        "verification_test": None,
        "test_result": None,
        "fix_status": "UNFIXED",
        "fix_description": None,
        "commit_hash": None,
        "commit_message": None,
        "created_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    }
    issues.append(new_issue)
    idata["issues"] = issues
    save_issues(idata)

    # Link to file in files_ledger
    path_map = {f["path"]: f for f in fdata.get("files", [])}
    if norm_p in path_map:
        path_map[norm_p]["issues_count"] = path_map[norm_p].get("issues_count", 0) + 1
        if "issue_ids" not in path_map[norm_p]:
            path_map[norm_p]["issue_ids"] = []
        path_map[norm_p]["issue_ids"].append(issue_id)
        save_files(fdata)

    print(f"Logged issue {issue_id}: [{category}] Severity {severity} in {norm_p}:{line_range}")
    return issue_id

def verify_issue(issue_id, status, test_desc, test_result):
    idata = load_issues()
    for issue in idata.get("issues", []):
        if issue["id"] == issue_id:
            issue["verification_status"] = status
            issue["verification_test"] = test_desc
            issue["test_result"] = test_result
            issue["verified_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
            save_issues(idata)
            print(f"Updated {issue_id} verification status: {status}")
            return
    print(f"Error: Issue {issue_id} not found.")

def record_fix(issue_id, fix_desc, commit_msg, commit_hash=None):
    idata = load_issues()
    for issue in idata.get("issues", []):
        if issue["id"] == issue_id:
            issue["fix_status"] = "FIXED" if commit_hash else "PENDING_COMMIT"
            issue["fix_description"] = fix_desc
            issue["commit_message"] = commit_msg
            issue["commit_hash"] = commit_hash
            issue["fixed_at"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
            save_issues(idata)
            print(f"Updated {issue_id} fix status: {issue['fix_status']}")
            return
    print(f"Error: Issue {issue_id} not found.")

def main():
    parser = argparse.ArgumentParser(description="Audit Ledger Management Tool")
    subparsers = parser.add_subparsers(dest="action", required=True)

    # mark-reviewed
    rev_parser = subparsers.add_parser("mark-reviewed")
    rev_parser.add_argument("files", nargs="+", help="Files to mark as reviewed")

    # log-issue
    log_parser = subparsers.add_parser("log-issue")
    log_parser.add_argument("--file", required=True, help="File path")
    log_parser.add_argument("--lines", required=True, help="Line range (e.g. 88-94)")
    log_parser.add_argument("--severity", required=True, type=int, choices=[1, 2, 3], help="Severity 1-3")
    log_parser.add_argument("--category", required=True, help="Vulnerability category")
    log_parser.add_argument("--description", required=True, help="1-sentence description")

    # verify-issue
    ver_parser = subparsers.add_parser("verify-issue")
    ver_parser.add_argument("--id", required=True, help="Issue ID (e.g. SEC-0001)")
    ver_parser.add_argument("--status", required=True, choices=["CONFIRMED", "DISPROVEN", "PENDING"], help="Status")
    ver_parser.add_argument("--test-desc", required=True, help="Test description")
    ver_parser.add_argument("--test-result", required=True, help="Test outcome summary")

    # record-fix
    fix_parser = subparsers.add_parser("record-fix")
    fix_parser.add_argument("--id", required=True, help="Issue ID (e.g. SEC-0001)")
    fix_parser.add_argument("--fix-desc", required=True, help="Description of the fix")
    fix_parser.add_argument("--commit-msg", required=True, help="Proposed lowercase commit message")
    fix_parser.add_argument("--commit-hash", default=None, help="Commit hash if committed")

    args = parser.parse_args()
    if args.action == "mark-reviewed":
        mark_reviewed(args.files)
    elif args.action == "log-issue":
        log_issue(args.file, args.lines, args.severity, args.category, args.description)
    elif args.action == "verify-issue":
        verify_issue(args.id, args.status, args.test_desc, args.test_result)
    elif args.action == "record-fix":
        record_fix(args.id, args.fix_desc, args.commit_msg, args.commit_hash)

if __name__ == "__main__":
    main()
