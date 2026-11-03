import unittest
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
from cp8_queue_worker import completed_owner_workers


class CP8CoordinatorWorkflowTests(unittest.TestCase):
    def test_coordinator_is_trusted_push_schedule_dispatch_only(self):
        text = (ROOT / ".github/workflows/energun-cp8-coordinator.yml").read_text()
        self.assertIn("branches:\n      - energun-development-orchestrator", text)
        self.assertIn('cron: "*/5 * * * *"', text)
        self.assertNotIn("pull_request:", text)
        self.assertIn("contents: write", text)

    def test_three_lane_workers_have_independent_concurrency_and_bounded_drain(self):
        text = (ROOT / ".github/workflows/energun-cp8-coordinator.yml").read_text()
        for lane in ("cp1", "cp2", "cp3"):
            self.assertIn(f"group: energun-cp8-worker-{lane}", text)
        self.assertIn("cancel-in-progress: false", text)
        self.assertIn('CP8_MAX_ITEMS_PER_RUN: "8"', text)

    def test_git_transport_recovers_abandoned_workers_by_lease_without_rest(self):
        class GitOnlyClient:
            pass
        class Store:
            client = GitOnlyClient()
            branch = "energun-ci-queue"
        self.assertEqual(completed_owner_workers(Store()), set())

    def test_worker_has_fixed_allow_list_and_no_shell_interpolation(self):
        text = (ROOT / "tools/cp8_queue_worker.py").read_text()
        self.assertIn('"fast-regression": ["sh", "tests/run_metal_core_fast.sh"]', text)
        self.assertIn("if lane not in ALLOWED_COMMANDS:", text)
        self.assertNotIn("shell=True", text)


if __name__ == "__main__":
    unittest.main()
