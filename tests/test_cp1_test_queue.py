#!/usr/bin/env python3
"""Unit tests for the CP1 sequential queue controller."""
import contextlib
import importlib.util
import io
import json
import os
from pathlib import Path
import tempfile
import unittest

MODULE_PATH = Path(__file__).with_name("run_cp1_test_queue.py")
SPEC = importlib.util.spec_from_file_location("cp1_queue", MODULE_PATH)
QUEUE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(QUEUE)


class CP1QueueTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)
        self.old = (QUEUE.ROOT, QUEUE.MANIFEST, QUEUE.OUT)
        QUEUE.ROOT = self.root
        QUEUE.MANIFEST = self.root / "tests" / "cp1_test_queue.json"
        QUEUE.OUT = self.root / "build" / "cp1-test-queue"
        QUEUE.MANIFEST.parent.mkdir(parents=True)
        self.env = dict(os.environ)
        os.environ.pop("GITHUB_STEP_SUMMARY", None)
        os.environ.pop("GITHUB_OUTPUT", None)

    def tearDown(self):
        QUEUE.ROOT, QUEUE.MANIFEST, QUEUE.OUT = self.old
        os.environ.clear()
        os.environ.update(self.env)
        self.temp.cleanup()

    def run_manifest(self, stages):
        evidence = self.root / "build" / "metal-core-fast" / "results.txt"
        evidence.parent.mkdir(parents=True, exist_ok=True)
        evidence.write_text("PASS: storage\nPASS: fm6bank\n", encoding="utf-8")
        QUEUE.MANIFEST.write_text(json.dumps({
            "schema_version": 1,
            "queue_id": "TEST",
            "stages": stages,
        }), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            result = QUEUE.main()
        report = json.loads((QUEUE.OUT / "report.json").read_text(encoding="utf-8"))
        return result, report

    def test_blocked_stage_stops_queue_and_reports_all_dependents(self):
        stages = [
            {"id": "storage", "title": "Storage", "state": "evidence",
             "evidence_file": "build/metal-core-fast/results.txt", "evidence_contains": "PASS: storage"},
            {"id": "sidecar", "title": "Sidecar", "state": "blocked",
             "depends_on": ["storage"], "blocker": "Not implemented."},
            {"id": "pair", "title": "Pair", "state": "ready", "depends_on": ["sidecar"],
             "command": "python3 -c \"raise SystemExit(99)\""},
        ]
        code, report = self.run_manifest(stages)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "BLOCKED")
        self.assertEqual([x["result"] for x in report["stages"]], ["PASS", "BLOCKED", "BLOCKED"])

    def test_ready_stages_execute_in_dependency_order(self):
        stages = [
            {"id": "storage", "title": "Storage", "state": "evidence",
             "evidence_file": "build/metal-core-fast/results.txt", "evidence_contains": "PASS: storage"},
            {"id": "sidecar", "title": "Sidecar", "state": "ready", "depends_on": ["storage"],
             "command": "python3 -c \"print('sidecar acceptance passed')\""},
            {"id": "pair", "title": "Pair", "state": "ready", "depends_on": ["sidecar"],
             "command": "python3 -c \"print('pair acceptance passed')\""},
        ]
        code, report = self.run_manifest(stages)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual([x["result"] for x in report["stages"]], ["PASS", "PASS", "PASS"])

    def test_failed_stage_stops_dependents(self):
        stages = [
            {"id": "storage", "title": "Storage", "state": "evidence",
             "evidence_file": "build/metal-core-fast/results.txt", "evidence_contains": "PASS: storage"},
            {"id": "sidecar", "title": "Sidecar", "state": "ready", "depends_on": ["storage"],
             "command": "python3 -c \"import sys; sys.exit(7)\""},
            {"id": "pair", "title": "Pair", "state": "ready", "depends_on": ["sidecar"],
             "command": "python3 -c \"raise SystemExit(99)\""},
        ]
        code, report = self.run_manifest(stages)
        self.assertEqual(code, 1)
        self.assertEqual(report["status"], "FAIL")
        self.assertEqual([x["result"] for x in report["stages"]], ["PASS", "FAIL", "BLOCKED"])

    def test_forward_dependency_is_rejected(self):
        stages = [
            {"id": "first", "title": "First", "state": "blocked", "depends_on": ["later"]},
            {"id": "later", "title": "Later", "state": "blocked"},
        ]
        QUEUE.MANIFEST.write_text(json.dumps({"schema_version": 1, "queue_id": "TEST", "stages": stages}), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            code = QUEUE.main()
        self.assertEqual(code, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
