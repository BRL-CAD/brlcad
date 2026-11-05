#!/usr/bin/env python3
# BRL-CAD
# Copyright (c) 2026 United States Government as represented by
# the U.S. Army Research Laboratory.
# Distributed under the terms of the GNU Lesser General Public License
# (LGPL), version 2.1.

"""Exercise bounded corpus retries with a deterministic audit subprocess."""

import importlib.util
import json
import os
from pathlib import Path
import shutil
import sys
import tempfile
import time
import unittest
from unittest.mock import patch


def fixture():
    batch = "--batch" in sys.argv
    database = Path(sys.argv[-1] if batch else sys.argv[-2])
    scenario = json.loads(database.read_text())
    start = int(sys.argv[sys.argv.index("--batch-start") + 1]) if batch else 0
    with database.with_suffix(".calls").open("a") as calls:
        calls.write(json.dumps({"batch": batch, "start": start,
                                "arguments": sys.argv[1:]}) + "\n")
    if not batch:
        previous_pid = database.with_suffix(".pid")
        if previous_pid.exists():
            try:
                os.kill(int(previous_pid.read_text()), 0)
            except ProcessLookupError:
                pass
            else:
                raise RuntimeError("batch still running during isolated retry")
    objects = scenario["objects"]
    indices = range(start, len(objects)) if batch else [int(sys.argv[-1])]
    for index in indices:
        details = objects[index]
        record = {"format": "brlcad-brep-realization-audit-v1",
                  "database": str(database), "object": str(index),
                  "mode": "quality", "task_index": index if batch else -1,
                  "status": "ok", "quality": {}}
        if batch or not details.get("recovers", True):
            record.update(details["result"])
        if batch:
            progress = dict(record, format="brlcad-brep-audit-progress-v1")
            print(json.dumps(progress), flush=True)
        should_wait = batch and index > start and details.get("retry", False)
        if should_wait:
            database.with_suffix(".pid").write_text(str(os.getpid()))
        print(json.dumps(record), flush=True)
        if should_wait:
            # The parent must stop this process before the isolated attempt.
            time.sleep(10)
            raise RuntimeError("parent did not stop the failed batch")
    if batch:
        print(json.dumps({"format": "brlcad-brep-audit-complete-v1",
                          "task_count": len(objects)}), flush=True)


@unittest.skipUnless(os.name == "posix", "executable subprocess fixture")
class CorpusResourceRetry(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        source = Path(__file__).resolve().parent.parent / "brep-audit-corpus.py"
        spec = importlib.util.spec_from_file_location("brep_audit_corpus", source)
        cls.corpus = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(cls.corpus)

    def check_retry(self, failure, recovers=True, previous=True, retry=True):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            audit = root / "audit"
            shutil.copyfile(__file__, audit)
            audit.chmod(0o755)
            objects = ([{"result": {}}] if previous else [])
            objects.extend([{"result": failure, "recovers": recovers,
                             "retry": retry}, {"result": {}}])
            database = root / "fixture.g"
            database.write_text(json.dumps({"objects": objects}))
            with patch.object(sys, "argv", ["corpus", str(root),
                                            str(root / "results.jsonl"),
                                            "--modes", "quality", "--jobs", "1",
                                            "--object-timeout", "3"]):
                args = self.corpus.parse_args()
            records = []
            self.corpus.audit_database(
                str(audit), args, root, database, 0, records.append,
                tasks=[(str(i), "quality") for i in range(len(objects))],
            )
            self.assertEqual([r["task_index"] for r in records],
                             list(range(len(objects))))
            self.assertEqual(records[-1]["status"], "ok")
            calls = [json.loads(line) for line in
                     database.with_suffix(".calls").read_text().splitlines()]
            failed_index = int(previous)
            if previous and retry:
                self.assertEqual([c["batch"] for c in calls], [True, False, True])
                self.assertEqual(calls[-1]["start"], failed_index + 1)
                isolated = records[failed_index]
                self.assertEqual(isolated["status"], "ok" if recovers else "fail")
                self.assertEqual(isolated["batch_attempt"]["status"], "fail")
                self.assertIn("batch_fallback_reason", isolated)
                for option in ("--jobs", "--memory-limit-mib", "--max-time-ms"):
                    values = [c["arguments"][c["arguments"].index(option) + 1]
                              for c in calls]
                    self.assertEqual(values, [values[0]] * len(values))
            else:
                self.assertEqual(len(calls), 1)
                self.assertNotIn("batch_attempt", records[failed_index])
                self.assertEqual(records[failed_index]["status"], failure["status"])

    def test_memory_retry_preserves_history_and_limits(self):
        self.check_retry({"status": "fail", "quality": {"limits": {"memory": True}}})

    def test_repair_memory_history_triggers_retry(self):
        self.check_retry({"status": "fail", "quality": {"repair": {
            "resource_limits": self.corpus.REPAIR_LIMIT_MEMORY}}})

    def test_load_failure_retries(self):
        self.check_retry({"status": "fail", "issues": ["database_internal_load_failed"]})

    def test_fresh_process_failure_remains_a_failure(self):
        self.check_retry({"status": "fail", "quality": {"limits": {"memory": True}}},
                         recovers=False)

    def test_first_object_is_already_isolated(self):
        self.check_retry({"status": "fail", "quality": {"limits": {"memory": True}}},
                         previous=False)

    def test_geometry_failure_is_not_retried(self):
        self.check_retry({"status": "fail", "quality": {"issues": ["open_mesh"]}},
                         retry=False)

    def test_success_with_resource_history_is_not_retried(self):
        self.check_retry({"status": "ok", "quality": {"limits": {"memory": True}}},
                         retry=False)


if __name__ == "__main__":
    if "--mode" in sys.argv:
        fixture()
    else:
        unittest.main()
