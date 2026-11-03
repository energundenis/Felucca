import copy
import threading
import unittest
from datetime import datetime, timedelta, timezone

from cp8_ci_queue_store import GitRefCASQueueStore, QueueCASConflict, QueueStoreError
from cp8_worker_runtime import (
    claim_next_for_worker, finish_worker_task, mark_worker_running, register_worker, release_worker,
)

SHA = "a" * 40
NOW = datetime(2026, 11, 3, 12, 0, tzinfo=timezone.utc)


class Backend:
    def __init__(self):
        self.lock = threading.Lock()
        self.head = "0" * 40
        self.ledgers = {self.head: {"schema_version": 1, "records": []}}
        self.n = 0
    def read_ref(self, branch):
        with self.lock:
            return self.head
    def read_ledger_at(self, head):
        with self.lock:
            return copy.deepcopy(self.ledgers[head])
    def commit_ledger(self, expected_head, ledger, branch):
        with self.lock:
            if self.head != expected_head:
                raise QueueCASConflict("CAS conflict")
            self.n += 1
            head = f"{self.n:040x}"
            self.ledgers[head] = copy.deepcopy(ledger)
            self.head = head
            return head


class WorkerRuntimeTests(unittest.TestCase):
    def setUp(self):
        self.backend = Backend()
        self.store = GitRefCASQueueStore(self.backend, sleep=lambda _: None)
        for lane in ("cp1-persistence", "cp2-dsp-vca", "cp3-sequencer"):
            self.store.enqueue("energundenis/Felucca", [lane], "refs/heads/test",
                               SHA, "push", now=NOW)

    def worker(self, lane, run_id):
        worker_id = f"{lane}-run-{run_id}"
        register_worker(self.store, worker_id, run_id, lane, ["test-automation"], now=NOW)
        return worker_id

    def test_registry_is_persistent_and_tracks_available_busy_offline(self):
        worker_id = self.worker("cp1-persistence", 101)
        ledger = self.backend.ledgers[self.backend.head]
        self.assertEqual(ledger["workers"][worker_id]["state"], "available")
        claim = claim_next_for_worker(self.store, worker_id, 101, {}, now=NOW + timedelta(seconds=1))
        self.assertIsNotNone(claim)
        self.assertEqual(self.backend.ledgers[self.backend.head]["workers"][worker_id]["state"], "busy")
        finish_worker_task(self.store, claim["request_id"], worker_id, 101, "passed",
                           "ok", ["https://example.invalid/run"], SHA,
                           now=NOW + timedelta(seconds=2))
        release_worker(self.store, worker_id, 101, now=NOW + timedelta(seconds=3))
        self.assertEqual(self.backend.ledgers[self.backend.head]["workers"][worker_id]["state"], "offline")

    def test_dag_blocks_child_until_parent_passes(self):
        parent = self.worker("cp1-persistence", 101)
        child = self.worker("cp2-dsp-vca", 102)
        self.assertIsNone(claim_next_for_worker(
            self.store, child, 102, {"cp2-dsp-vca": ["cp1-persistence"]},
            now=NOW + timedelta(seconds=1)))
        parent_claim = claim_next_for_worker(self.store, parent, 101, {}, now=NOW + timedelta(seconds=1))
        finish_worker_task(self.store, parent_claim["request_id"], parent, 101, "passed",
                           "parent passed", ["https://example.invalid/1"], SHA,
                           now=NOW + timedelta(seconds=2))
        child_claim = claim_next_for_worker(
            self.store, child, 102, {"cp2-dsp-vca": ["cp1-persistence"]},
            now=NOW + timedelta(seconds=3))
        self.assertIsNotNone(child_claim)
        self.assertEqual(child_claim["requested_lanes"], ["cp2-dsp-vca"])

    def test_three_independent_workers_claim_three_tasks(self):
        workers = [self.worker(lane, 200+i) for i, lane in enumerate(
            ("cp1-persistence", "cp2-dsp-vca", "cp3-sequencer"))]
        claims = []
        for i, worker_id in enumerate(workers):
            lane = ("cp1-persistence", "cp2-dsp-vca", "cp3-sequencer")[i]
            claims.append(claim_next_for_worker(self.store, worker_id, 200+i, {lane: []},
                                                now=NOW + timedelta(seconds=1)))
        self.assertEqual(len({c["request_id"] for c in claims if c}), 3)
        self.assertTrue(all(claims))

    def test_recovery_fences_old_worker_and_prevents_double_claim(self):
        old = self.worker("cp1-persistence", 301)
        first = claim_next_for_worker(self.store, old, 301, {}, lease_seconds=30,
                                      now=NOW + timedelta(seconds=1))
        new = self.worker("cp1-persistence", 302)
        recovered = claim_next_for_worker(self.store, new, 302, {},
                                          completed_owner_workers={(old, 301)},
                                          now=NOW + timedelta(seconds=2))
        self.assertEqual(first["request_id"], recovered["request_id"])
        self.assertEqual(recovered["attempts"], 2)
        with self.assertRaisesRegex(QueueStoreError, "fenced"):
            finish_worker_task(self.store, first["request_id"], old, 301, "passed",
                               "stale", ["https://example.invalid/old"], SHA,
                               now=NOW + timedelta(seconds=3))
        with self.assertRaisesRegex(QueueStoreError, "not available"):
            claim_next_for_worker(self.store, old, 301, {}, now=NOW + timedelta(seconds=4))

    def test_two_distinct_workers_cannot_claim_the_same_task(self):
        workers = [self.worker("cp1-persistence", 401),
                   self.worker("cp1-persistence", 402)]
        claims = []
        barrier = threading.Barrier(2)
        def claim(index):
            barrier.wait()
            claims.append(claim_next_for_worker(
                self.store, workers[index], 401 + index, {},
                now=NOW + timedelta(seconds=1)))
        threads = [threading.Thread(target=claim, args=(0,)),
                   threading.Thread(target=claim, args=(1,))]
        for thread in threads: thread.start()
        for thread in threads: thread.join()
        self.assertEqual(sum(c is not None for c in claims), 1)

    def test_same_actions_run_cannot_cross_mark_another_lane_running(self):
        cp1 = self.worker("cp1-persistence", 601)
        cp2 = self.worker("cp2-dsp-vca", 601)
        claim = claim_next_for_worker(self.store, cp1, 601, {}, now=NOW + timedelta(seconds=1))
        with self.assertRaisesRegex(QueueStoreError, "another worker"):
            mark_worker_running(self.store, claim["request_id"], cp2, 601,
                                now=NOW + timedelta(seconds=2))
        mark_worker_running(self.store, claim["request_id"], cp1, 601,
                            now=NOW + timedelta(seconds=2))

    def test_pass_requires_exact_sha_and_only_one_terminal_result(self):
        worker_id = self.worker("cp1-persistence", 501)
        claim = claim_next_for_worker(self.store, worker_id, 501, {}, now=NOW + timedelta(seconds=1))
        with self.assertRaisesRegex(ValueError, "exact requested SHA"):
            finish_worker_task(self.store, claim["request_id"], worker_id, 501, "passed",
                               "wrong sha", ["https://example.invalid/run"], "b"*40,
                               now=NOW + timedelta(seconds=2))
        finish_worker_task(self.store, claim["request_id"], worker_id, 501, "passed",
                           "ok", ["https://example.invalid/run"], SHA,
                           now=NOW + timedelta(seconds=2))
        with self.assertRaises(QueueStoreError):
            finish_worker_task(self.store, claim["request_id"], worker_id, 501, "passed",
                               "duplicate", ["https://example.invalid/run"], SHA,
                               now=NOW + timedelta(seconds=3))


if __name__ == "__main__":
    unittest.main()
