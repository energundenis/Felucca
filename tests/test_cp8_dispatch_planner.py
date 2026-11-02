import copy
import unittest

from run_cp8_dispatch_planner import build_dispatch_plan, validate_registry, validate_tasks


def registry(workers=None):
    return {
        "schema_version": 1,
        "policy": {
            "require_isolated_branch": True,
            "allow_main_write": False,
            "allow_auto_merge": False,
            "allow_physical_device_access": False,
            "allow_flash_write": False,
            "require_review": True,
            "max_parallel_assignments": 2,
        },
        "capability_catalog": ["dsp", "storage", "ci-infrastructure", "test-automation"],
        "workers": workers or [],
    }


def worker(worker_id="worker-a", capabilities=None, allowed_paths=None, limit=1, state="active"):
    return {
        "worker_id": worker_id,
        "state": state,
        "version": "test-1",
        "capabilities": capabilities or ["ci-infrastructure", "test-automation"],
        "max_concurrency": limit,
        "allowed_paths": allowed_paths or ["tests/", "docs/"],
    }


def task(task_id="task-a", deps=None, paths=None, priority=10):
    return {
        "task_id": task_id,
        "checkpoint": "CP8.X",
        "title": task_id,
        "required_capabilities": ["ci-infrastructure"],
        "depends_on": deps or [],
        "state": "ready",
        "priority": priority,
        "allowed_paths": paths or ["tests/"],
    }


class CP8DispatchPlannerTests(unittest.TestCase):
    def test_empty_registry_never_invents_or_invokes_workers(self):
        plan = build_dispatch_plan(registry(), {"schema_version": 1, "tasks": [task()]})
        self.assertEqual(plan["status"], "NO_WORKERS_REGISTERED")
        self.assertEqual(plan["assignments"], [])
        self.assertFalse(plan["side_effects"]["workers_invoked"])

    def test_capable_worker_receives_deterministic_dry_run_proposal(self):
        plan = build_dispatch_plan(registry([worker()]), {"schema_version": 1, "tasks": [task()]})
        self.assertEqual(plan["assignments"][0]["worker_id"], "worker-a")
        self.assertEqual(plan["tasks"][0]["state"], "ASSIGNMENT_PROPOSED")
        self.assertFalse(plan["assignments"][0]["auto_merge"])

    def test_inactive_worker_is_not_eligible(self):
        plan = build_dispatch_plan(registry([worker(state="disabled")]), {"schema_version": 1, "tasks": [task()]})
        self.assertEqual(plan["tasks"][0]["state"], "WAITING_NO_ELIGIBLE_WORKER")

    def test_missing_capability_is_not_eligible(self):
        plan = build_dispatch_plan(registry([worker(capabilities=["storage"])]), {"schema_version": 1, "tasks": [task()]})
        self.assertEqual(plan["tasks"][0]["state"], "WAITING_NO_ELIGIBLE_WORKER")

    def test_dependency_must_be_explicitly_completed(self):
        data = {"schema_version": 1, "tasks": [task("parent"), task("child", deps=["parent"])]}
        plan = build_dispatch_plan(registry([worker(limit=2)]), data)
        child = next(row for row in plan["tasks"] if row["task_id"] == "child")
        self.assertEqual(child["state"], "WAITING_DEPENDENCIES")

    def test_path_scope_must_cover_task_paths(self):
        plan = build_dispatch_plan(registry([worker(allowed_paths=["docs/"])]), {"schema_version": 1, "tasks": [task(paths=["tests/"])]})
        self.assertEqual(plan["tasks"][0]["state"], "WAITING_NO_ELIGIBLE_WORKER")

    def test_unsafe_path_traversal_is_rejected(self):
        data = {"schema_version": 1, "tasks": [task(paths=["../firmware/"])]}
        with self.assertRaisesRegex(ValueError, "unsafe allowed path"):
            build_dispatch_plan(registry([worker()]), data)

    def test_unsafe_registry_policy_is_rejected(self):
        data = registry([worker()])
        data["policy"]["allow_main_write"] = True
        with self.assertRaisesRegex(ValueError, "allow_main_write"):
            validate_registry(data)

    def test_duplicate_worker_id_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicate worker_id"):
            validate_registry(registry([worker(), worker()]))

    def test_duplicate_task_id_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicate task_id"):
            validate_tasks({"schema_version": 1, "tasks": [task(), task()]}, set(registry()["capability_catalog"]))

    def test_dependency_cycle_is_rejected(self):
        data = {"schema_version": 1, "tasks": [task("a", deps=["b"]), task("b", deps=["a"])]}
        with self.assertRaisesRegex(ValueError, "cycle"):
            build_dispatch_plan(registry([worker(limit=2)]), data)

    def test_global_and_worker_concurrency_limits_are_enforced(self):
        data = {"schema_version": 1, "tasks": [task("a", priority=20), task("b", priority=10), task("c", priority=1)]}
        plan = build_dispatch_plan(registry([worker(limit=1), worker("worker-b", limit=1)]), data)
        self.assertEqual(len(plan["assignments"]), 2)
        self.assertEqual({a["worker_id"] for a in plan["assignments"]}, {"worker-a", "worker-b"})
        self.assertEqual(next(r for r in plan["tasks"] if r["task_id"] == "c")["state"], "WAITING_CAPACITY")


if __name__ == "__main__":
    unittest.main()
