#!/usr/bin/env python3
"""Unit tests for the dependency-aware parallel CP1 queue controller."""
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

    def run_manifest(self, stages, max_parallel=3):
        evidence = self.root / "build" / "metal-core-fast" / "results.txt"
        evidence.parent.mkdir(parents=True, exist_ok=True)
        evidence.write_text("PASS: storage\nPASS: fm6bank\n", encoding="utf-8")
        QUEUE.MANIFEST.write_text(json.dumps({
            "schema_version": 1,
            "queue_id": "TEST",
            "max_parallel": max_parallel,
            "stages": stages,
        }), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            result = QUEUE.main()
        report = json.loads((QUEUE.OUT / "report.json").read_text(encoding="utf-8"))
        return result, report

    def test_blocked_stage_blocks_only_its_dependents(self):
        stages = [
            {"id": "storage", "title": "Storage", "state": "evidence",
             "evidence_file": "build/metal-core-fast/results.txt", "evidence_contains": "PASS: storage"},
            {"id": "blocked", "title": "Blocked lane", "state": "blocked",
             "depends_on": ["storage"], "blocker": "Not implemented."},
            {"id": "dependent", "title": "Dependent", "state": "ready", "depends_on": ["blocked"],
             "command": "python3 -c \"raise SystemExit(99)\""},
            {"id": "independent", "title": "Independent", "state": "ready", "depends_on": ["storage"],
             "command": "python3 -c \"print('independent passed')\""},
        ]
        code, report = self.run_manifest(stages)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "BLOCKED")
        results = {x["id"]: x["result"] for x in report["stages"]}
        self.assertEqual(results, {
            "storage": "PASS", "blocked": "BLOCKED",
            "dependent": "BLOCKED", "independent": "PASS",
        })

    def test_parallel_independent_stages_join_before_dependent(self):
        wait_a = (
            "import pathlib,time; a=pathlib.Path(\"a.started\"); b=pathlib.Path(\"b.started\"); "
            "a.write_text(\"1\"); d=time.time()+3; "
            "exec(\"while not b.exists() and time.time()<d: time.sleep(.01)\"); "
            "assert b.exists(); pathlib.Path(\"a.done\").write_text(\"1\")"
        )
        wait_b = (
            "import pathlib,time; a=pathlib.Path(\"a.started\"); b=pathlib.Path(\"b.started\"); "
            "b.write_text(\"1\"); d=time.time()+3; "
            "exec(\"while not a.exists() and time.time()<d: time.sleep(.01)\"); "
            "assert a.exists(); pathlib.Path(\"b.done\").write_text(\"1\")"
        )
        join = (
            "import pathlib; assert pathlib.Path(\"a.done\").exists(); "
            "assert pathlib.Path(\"b.done\").exists(); pathlib.Path(\"joined\").write_text(\"1\")"
        )
        stages = [
            {"id": "lane-a", "title": "Lane A", "state": "ready",
             "command": "python3 -c '" + wait_a + "'"},
            {"id": "lane-b", "title": "Lane B", "state": "ready",
             "command": "python3 -c '" + wait_b + "'"},
            {"id": "join", "title": "Join", "state": "ready",
             "depends_on": ["lane-a", "lane-b"],
             "command": "python3 -c '" + join + "'"},
        ]
        code, report = self.run_manifest(stages, max_parallel=2)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "PASS")
        self.assertEqual([x["result"] for x in report["stages"]], ["PASS", "PASS", "PASS"])
        self.assertTrue((self.root / "joined").is_file())

    def test_failed_stage_blocks_dependents_but_not_already_running_peer(self):
        stages = [
            {"id": "failing", "title": "Failing", "state": "ready",
             "command": "python3 -c \"import sys; sys.exit(7)\""},
            {"id": "peer", "title": "Peer", "state": "ready",
             "command": "python3 -c \"open('peer.done','w').write('1')\""},
            {"id": "dependent", "title": "Dependent", "state": "ready",
             "depends_on": ["failing"], "command": "python3 -c \"raise SystemExit(99)\""},
        ]
        code, report = self.run_manifest(stages, max_parallel=2)
        self.assertEqual(code, 1)
        results = {x["id"]: x["result"] for x in report["stages"]}
        self.assertEqual(results["failing"], "FAIL")
        self.assertEqual(results["dependent"], "BLOCKED")
        self.assertTrue((self.root / "peer.done").is_file())

    def test_forward_dependencies_are_supported(self):
        stages = [
            {"id": "second", "title": "Second", "state": "ready",
             "depends_on": ["first"], "command": "python3 -c \"print('second')\""},
            {"id": "first", "title": "First", "state": "ready",
             "command": "python3 -c \"print('first')\""},
        ]
        code, report = self.run_manifest(stages)
        self.assertEqual(code, 0)
        self.assertEqual(report["status"], "PASS")

    def test_repository_manifest_runs_quantization_as_independent_stage(self):
        manifest_path = self.old[1]
        manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
        stages = {stage["id"]: stage for stage in manifest["stages"]}
        quantization = stages["parameter-quantization"]
        self.assertEqual(manifest["max_parallel"], 3)
        self.assertEqual(quantization["state"], "ready")
        self.assertEqual(quantization["depends_on"], ["fm6-staging-baseline"])
        self.assertEqual(quantization["command"],
                         "sh tests/run_cp1_parameter_quantization_test.sh")
        self.assertTrue((manifest_path.parent /
                         "run_cp1_parameter_quantization_test.sh").is_file())

    def test_dependency_cycle_is_rejected(self):
        stages = [
            {"id": "first", "title": "First", "state": "blocked", "depends_on": ["second"]},
            {"id": "second", "title": "Second", "state": "blocked", "depends_on": ["first"]},
        ]
        QUEUE.MANIFEST.write_text(json.dumps({
            "schema_version": 1, "queue_id": "TEST", "stages": stages,
        }), encoding="utf-8")
        with contextlib.redirect_stdout(io.StringIO()):
            code = QUEUE.main()
        self.assertEqual(code, 1)


if __name__ == "__main__":
    unittest.main(verbosity=2)
