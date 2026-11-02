import json
import tempfile
import unittest
from unittest.mock import patch, Mock
from pathlib import Path
from threading import Barrier

from run_development_orchestrator import build_plan, load_and_validate, read_cp1_status, execute_ready_workstreams


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

    def test_cp1_pass_evidence_does_not_bypass_cp0_dependency(self):
        data = manifest([
            task("cp0-baseline", "checking"),
            task("cp1-persistence", "ready", deps=["cp0-baseline"]),
        ])
        plan = build_plan(data, ["cp0-baseline", "cp1-persistence"], "PASS", "CP1 queue passed.")
        states = {row["id"]: row["state"] for row in plan["workstreams"]}
        self.assertEqual(states["cp1-persistence"], "WAITING")

    def test_cp1_blocked_evidence_is_preserved(self):
        data = manifest([task("cp1-persistence")])
        plan = build_plan(data, ["cp1-persistence"], "BLOCKED", "Pair Commit Ordering is blocked.")
        self.assertEqual(plan["workstreams"][0]["state"], "BLOCKED")
        self.assertIn("Pair Commit Ordering", plan["workstreams"][0]["detail"])

    def test_cp1_failed_evidence_is_preserved(self):
        data = manifest([task("cp1-persistence")])
        plan = build_plan(data, ["cp1-persistence"], "FAIL", "CP1 command failed.")
        self.assertEqual(plan["workstreams"][0]["state"], "FAILED")

    def test_overall_plan_status_is_failed_when_a_workstream_fails(self):
        data = manifest([task("cp1-persistence")])
        plan = build_plan(data, ["cp1-persistence"], "FAIL", "CP1 command failed.")
        self.assertEqual(plan["status"], "FAILED")


    def test_executor_runs_only_exact_allowlisted_argv_without_shell(self):
        data = manifest([task("a")], max_parallel=1)
        data["execution_policy"]["allowed_commands"] = [["python3", "safe_task.py"]]
        data["workstreams"][0]["command"] = ["python3", "safe_task.py"]
        plan = build_plan(data, ["a"])
        fake = Mock(returncode=0, stdout="ok", stderr="")
        with patch("run_development_orchestrator.subprocess.run", return_value=fake) as run:
            result = execute_ready_workstreams(data, plan)
        self.assertEqual(result["workstreams"][0]["state"], "PASSED")
        self.assertFalse(run.call_args.kwargs["shell"])
        self.assertEqual(run.call_args.args[0], ["python3", "safe_task.py"])


    def test_executor_overlaps_independent_workers(self):
        data = manifest([task("a"), task("b")], max_parallel=2)
        data["execution_policy"]["allowed_commands"] = [["python3", "worker.py"]]
        for item in data["workstreams"]:
            item["command"] = ["python3", "worker.py"]
        plan = build_plan(data, ["a", "b"])
        barrier = Barrier(2)
        def overlap(*args, **kwargs):
            barrier.wait(timeout=3)
            return Mock(returncode=0, stdout="parallel", stderr="")
        with patch("run_development_orchestrator.subprocess.run", side_effect=overlap) as run:
            result = execute_ready_workstreams(data, plan)
        self.assertEqual(run.call_count, 2)
        self.assertEqual(result["execution"]["started"], 2)
        self.assertTrue(all(row["state"] == "PASSED" for row in result["workstreams"]))

    def test_executor_refuses_unregistered_command(self):
        data = manifest([task("a")])
        data["workstreams"][0]["command"] = ["python3", "-c", "print('unsafe')"]
        plan = build_plan(data, ["a"])
        with patch("run_development_orchestrator.subprocess.run") as run:
            result = execute_ready_workstreams(data, plan)
        self.assertEqual(result["workstreams"][0]["state"], "BLOCKED")
        run.assert_not_called()

    def test_executor_does_not_run_workstream_while_dependency_waits(self):
        data = manifest([task("parent", "checking"), task("child", deps=["parent"])])
        data["execution_policy"]["allowed_commands"] = [["echo", "test"]]
        plan = build_plan(data, ["parent", "child"])
        with patch("run_development_orchestrator.subprocess.run") as run:
            result = execute_ready_workstreams(data, plan)
        self.assertEqual(result["execution"]["started"], 0)
        run.assert_not_called()

    def test_cp1_report_status_is_read_from_evidence_file(self):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "cp1-report.json"
            path.write_text(json.dumps({"queue_id": "ENERGUN-FM1-CP1", "status": "BLOCKED"}), encoding="utf-8")
            self.assertEqual(read_cp1_status(path), ("BLOCKED", "Imported explicit CP1 queue result: BLOCKED."))


if __name__ == "__main__":
    unittest.main()
