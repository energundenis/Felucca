import copy
import unittest

from cp8_ci_queue import make_idempotency_key, validate_queue_record, validate_transition


SHA = "a" * 40


def record(state="queued"):
    lanes = ["fast-regression", "baseline-build"]
    value = {
        "schema_version": 1,
        "request_id": "request-001",
        "idempotency_key": make_idempotency_key("energundenis/Felucca", lanes, "refs/pull/4/merge", SHA),
        "repository": "energundenis/Felucca",
        "ref": "refs/pull/4/merge",
        "commit_sha": SHA,
        "source": {"event": "pull_request", "pull_request_number": 4, "workflow_run_id": None},
        "requested_lanes": lanes,
        "state": state,
        "attempts": 0,
        "created_at": "2026-11-03T10:00:00Z",
        "updated_at": "2026-11-03T10:00:00Z",
        "claim": None,
        "result": None,
    }
    if state in {"claimed", "running"}:
        value["attempts"] = 1
        value["claim"] = {
            "owner_workflow": "ci-coordinator",
            "owner_run_id": 12345,
            "claimed_at": "2026-11-03T10:01:00Z",
            "heartbeat_at": "2026-11-03T10:02:00Z",
            "lease_expires_at": "2026-11-03T10:10:00Z",
        }
    if state == "passed":
        value["result"] = {
            "outcome": "passed",
            "tested_sha": SHA,
            "finished_at": "2026-11-03T10:20:00Z",
            "summary": "Both requested lanes passed.",
            "evidence": ["https://github.com/energundenis/Felucca/actions/runs/12345"],
        }
    if state in {"blocked", "failed"}:
        value["result"] = {
            "outcome": state,
            "finished_at": "2026-11-03T10:20:00Z",
            "summary": "Validation did not pass.",
        }
    if state == "superseded":
        value["result"] = {"outcome": "superseded", "summary": "A newer request replaced this unstarted request."}
    return value


class CP8CIQueueTests(unittest.TestCase):
    def test_valid_queued_record(self):
        self.assertEqual(validate_queue_record(record())["state"], "queued")

    def test_idempotency_is_order_independent_for_lanes(self):
        first = make_idempotency_key("energundenis/Felucca", ["fast-regression", "baseline-build"], "refs/heads/test", SHA)
        second = make_idempotency_key("energundenis/Felucca", ["baseline-build", "fast-regression"], "refs/heads/test", SHA)
        self.assertEqual(first, second)

    def test_idempotency_changes_when_exact_sha_changes(self):
        first = make_idempotency_key("energundenis/Felucca", ["fast-regression"], "refs/heads/test", SHA)
        second = make_idempotency_key("energundenis/Felucca", ["fast-regression"], "refs/heads/test", "b" * 40)
        self.assertNotEqual(first, second)

    def test_duplicate_lanes_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "duplicates"):
            make_idempotency_key("energundenis/Felucca", ["fast-regression", "fast-regression"], "refs/heads/test", SHA)

    def test_unsupported_lane_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "unsupported lane"):
            make_idempotency_key("energundenis/Felucca", ["arbitrary-shell"], "refs/heads/test", SHA)

    def test_short_or_malformed_sha_is_rejected(self):
        with self.assertRaisesRegex(ValueError, "40-character"):
            make_idempotency_key("energundenis/Felucca", ["fast-regression"], "refs/heads/test", "abc")

    def test_tampered_idempotency_key_is_rejected(self):
        item = record()
        item["idempotency_key"] = "0" * 64
        with self.assertRaisesRegex(ValueError, "does not match"):
            validate_queue_record(item)

    def test_claimed_record_requires_owner_and_live_lease_fields(self):
        item = record("claimed")
        self.assertEqual(validate_queue_record(item)["state"], "claimed")
        item["claim"]["owner_run_id"] = 0
        with self.assertRaisesRegex(ValueError, "owner_run_id"):
            validate_queue_record(item)

    def test_live_lease_must_expire_after_heartbeat(self):
        item = record("running")
        item["claim"]["lease_expires_at"] = item["claim"]["heartbeat_at"]
        with self.assertRaisesRegex(ValueError, "claimed_at <= heartbeat_at"):
            validate_queue_record(item)

    def test_pass_requires_exact_sha_and_evidence(self):
        item = record("passed")
        self.assertEqual(validate_queue_record(item)["state"], "passed")
        item["result"]["tested_sha"] = "b" * 40
        with self.assertRaisesRegex(ValueError, "exact requested"):
            validate_queue_record(item)

    def test_pass_without_evidence_is_rejected(self):
        item = record("passed")
        item["result"]["evidence"] = []
        with self.assertRaisesRegex(ValueError, "at least one evidence"):
            validate_queue_record(item)

    def test_queued_record_cannot_hold_a_claim(self):
        item = record()
        item["claim"] = {"owner_workflow": "stale"}
        with self.assertRaisesRegex(ValueError, "must not retain"):
            validate_queue_record(item)

    def test_timestamp_order_is_checked(self):
        item = record()
        item["updated_at"] = "2026-11-02T10:00:00Z"
        with self.assertRaisesRegex(ValueError, "must not precede"):
            validate_queue_record(item)

    def test_malformed_event_and_state_types_are_rejected_cleanly(self):\n        item = record()\n        item["source"]["event"] = []\n        with self.assertRaisesRegex(ValueError, "source.event"):\n            validate_queue_record(item)\n        item = record()\n        item["state"] = []\n        with self.assertRaisesRegex(ValueError, "state is unsupported"):\n            validate_queue_record(item)\n\n    def test_superseded_record_has_a_terminal_timestamp(self):\n        item = record("superseded")\n        self.assertEqual(validate_queue_record(item)["state"], "superseded")\n        item["result"].pop("finished_at")\n        with self.assertRaisesRegex(ValueError, "finished_at"):\n            validate_queue_record(item)\n\n    def test_state_transition_allowlist(self):
        validate_transition("queued", "claimed")
        validate_transition("claimed", "running")
        validate_transition("running", "passed")
        with self.assertRaisesRegex(ValueError, "illegal queue transition"):
            validate_transition("passed", "running")
        with self.assertRaisesRegex(ValueError, "illegal queue transition"):
            validate_transition("queued", "running")


if __name__ == "__main__":
    unittest.main()
