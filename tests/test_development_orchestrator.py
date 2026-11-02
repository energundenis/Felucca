import json
import tempfile
import unittest
from pathlib import Path

from run_development_orchestrator import build_plan, load_and_validate


def manifest(tasks, max_parallel=3):
    return {
        "schema_version": 1,
        "orchestrator_id": "TEST",
        "execution_policy": {"max_parallel": max_parallel},
        "workstreams": tasks,
    }


def task(task_id, state="ready", deps=None, command="echo test"):
    return {
        "id": task_id,
        "checkpoint": "CPX",
        "title": task_id,
        "state": state,
        "depends_on": deps or [],
        "parallel_group": "test",
        "executor": "test",
        "command": command,
    }


class DevelopmentOrchestratorTests(unittest.TestCase):
    def test_parallel_independent_tasks_are_both_candidates(self):
        data = manifest([task("a"), task("b")], max_parallel=2)
        plan = build_plan(data, ["a", "b"])
        self.assertEqual(plan["dispatch_candidates"], ["a", "b"])

    def test_dependency_waits_until_parent_passes(self):
        data = manifest([task("parent", "checking"), task("child", deps=["parent"])])
        plan = build_plan(data, ["parent", "child"])
        states = {row["id"]: row["state"] for row in plan["workstreams"]}
        self.assertEqual(states["child"], "WAITING")

    def test_blocked_parent_blocks_descendant(self):
        data = manifest([task("parent", "blocked"), task("child", deps=["parent"])])
        plan = build_plan(data, ["parent", "child"])
        states = {row["id"]: row["state"] for row in plan["workstreams"]}
        self.assertEqual(states["child"], "BLOCKED")

    def test_ready_task_without_command_is_not_dispatched(self):
        data = manifest([task("audit", command=None)])
        plan = build_plan(data, ["audit"])
        self.assertEqual(plan["dispatch_candidates"], [])
        self.assertEqual(plan["workstreams"][0]["state"], "READY_NO_EXECUTOR")

    def test_unknown_dependency_is_rejected(self):
        data = manifest([task("a", deps=["missing"])])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "unknown dependencies"):
                load_and_validate(path)

    def test_cycle_is_rejected(self):
        data = manifest([task("a", deps=["b"]), task("b", deps=["a"])])
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "manifest.json"
            path.write_text(json.dumps(data), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "cycle"):
                load_and_validate(path)

    def test_max_parallel_is_enforced_in_plan(self):
        data = manifest([task("a"), task("b"), task("c")], max_parallel=2)
        plan = build_plan(data, ["a", "b", "c"])
        self.assertEqual(plan["dispatch_candidates"], ["a", "b"])
        self.assertEqual(plan["available_slots"], 0)


if __name__ == "__main__":
    unittest.main()
