import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class CP8CoordinatorWorkflowTests(unittest.TestCase):
    def test_coordinator_is_trusted_push_schedule_dispatch_only(self):
        text = (ROOT / ".github/workflows/energun-cp8-coordinator.yml").read_text()
        self.assertIn("branches:\n      - energun-development-orchestrator", text)
        self.assertIn('cron: "*/5 * * * *"', text)
        self.assertNotIn("pull_request:", text)
        self.assertIn("contents: write", text)

    def test_coordinator_has_single_serial_consumer_and_bounded_drain(self):
        text = (ROOT / ".github/workflows/energun-cp8-coordinator.yml").read_text()
        self.assertIn("group: energun-cp8-global-queue-consumer", text)
        self.assertIn("cancel-in-progress: false", text)
        self.assertIn('CP8_MAX_ITEMS_PER_RUN: "4"', text)

    def test_worker_has_fixed_allow_list_and_no_shell_interpolation(self):
        text = (ROOT / "tools/cp8_queue_worker.py").read_text()
        self.assertIn('"fast-regression": ["sh", "tests/run_metal_core_fast.sh"]', text)
        self.assertIn("if command is None:", text)
        self.assertNotIn("shell=True", text)


if __name__ == "__main__":
    unittest.main()
