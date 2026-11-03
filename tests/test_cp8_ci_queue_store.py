import copy
import threading
import unittest
from datetime import datetime, timedelta, timezone

from cp8_ci_queue import make_idempotency_key
from cp8_ci_queue_store import GitRefCASQueueStore, QueueCASConflict, QueueStoreError

SHA = "a" * 40
NOW = datetime(2026, 11, 3, 12, 0, tzinfo=timezone.utc)


class FakeGitRefBackend:
    """Models atomic non-fast-forward ref rejection for competing writers."""
    def __init__(self):
        self.lock = threading.Lock()
        self.head = "0" * 40
        self.ledgers = {self.head: {"schema_version": 1, "records": []}}
        self.sequence = 0

    def read_ref(self, branch):
        with self.lock:
            return self.head

    def read_ledger_at(self, head):
        with self.lock:
            return copy.deepcopy(self.ledgers[head])

    def commit_ledger(self, expected_head, ledger, branch):
        with self.lock:
            if self.head != expected_head:
                raise QueueCASConflict("non-fast-forward")
            self.sequence += 1
            new_head = f"{self.sequence:040x}"
            self.ledgers[new_head] = copy.deepcopy(ledger)
            self.head = new_head
            return new_head


class ConflictOnceBackend(FakeGitRefBackend):
    def __init__(self):
        super().__init__()
        self.conflict_once = True

    def commit_ledger(self, expected_head, ledger, branch):
        if self.conflict_once:
            self.conflict_once = False
            raise QueueCASConflict("simulated competing writer")
        return super().commit_ledger(expected_head, ledger, branch)


class CP8QueueStoreTests(unittest.TestCase):
    def setUp(self):
        self.backend = FakeGitRefBackend()
        self.store = GitRefCASQueueStore(self.backend, sleep=lambda _: None)

    def test_enqueue_persists_record_and_is_idempotent(self):
        first = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/pull/4/merge",
                                   SHA, "pull_request", now=NOW)
        second = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/pull/4/merge",
                                    SHA, "pull_request", now=NOW + timedelta(seconds=1))
        self.assertTrue(first["created"])
        self.assertFalse(second["created"])
        self.assertEqual(self.backend.sequence, 1, "duplicate enqueue must not create a no-op commit")
        self.assertEqual(len(self.backend.ledgers[self.backend.head]["records"]), 1)

    def test_cas_conflict_retries_from_latest_head(self):
        backend = ConflictOnceBackend()
        store = GitRefCASQueueStore(backend, sleep=lambda _: None)
        result = store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/retry",
                               SHA, "push", now=NOW)
        self.assertTrue(result["created"])
        self.assertEqual(len(backend.ledgers[backend.head]["records"]), 1)

    def test_different_exact_sha_creates_a_distinct_request(self):
        self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                           SHA, "push", now=NOW)
        result = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                                    "b" * 40, "push", now=NOW)
        self.assertTrue(result["created"])
        self.assertEqual(len(self.backend.ledgers[self.backend.head]["records"]), 2)

    def test_lane_scoped_workers_claim_only_matching_requests(self):
        cp1 = self.store.enqueue("energundenis/Felucca", ["cp1-persistence"], "refs/heads/lanes",
                                 SHA, "push", now=NOW)["record"]
        cp2 = self.store.enqueue("energundenis/Felucca", ["cp2-dsp-vca"], "refs/heads/lanes",
                                 SHA, "push", now=NOW)["record"]
        claim = self.store.claim_next("cp1-worker", 301, now=NOW + timedelta(seconds=1),
                                      required_lane="cp1-persistence")
        self.assertEqual(claim["request_id"], cp1["request_id"])
        self.assertNotEqual(claim["request_id"], cp2["request_id"])
        self.assertIsNone(self.store.claim_next("cp1-worker", 302, now=NOW + timedelta(seconds=2),
                                                required_lane="cp1-persistence"))
        cp2_claim = self.store.claim_next("cp2-worker", 303, now=NOW + timedelta(seconds=2),
                                          required_lane="cp2-dsp-vca")
        self.assertEqual(cp2_claim["request_id"], cp2["request_id"])

    def test_claim_is_exclusive_and_owner_bound(self):
        request = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                                     SHA, "push", now=NOW)["record"]
        claim = self.store.claim_next("coordinator", 100, now=NOW + timedelta(seconds=1))
        self.assertEqual(claim["request_id"], request["request_id"])
        with self.assertRaisesRegex(QueueStoreError, "ownership mismatch"):
            self.store.heartbeat(request["request_id"], 101, now=NOW + timedelta(seconds=2))

    def test_claimed_request_is_not_claimed_again_while_lease_is_live(self):
        self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                           SHA, "push", now=NOW)
        self.store.claim_next("coordinator", 100, now=NOW + timedelta(seconds=1))
        self.assertIsNone(self.store.claim_next("coordinator", 101, now=NOW + timedelta(seconds=2)))

    def test_completed_owner_claim_is_recovered_before_lease_expiry(self):
        request = self.store.enqueue("energundenis/Felucca", ["cp1-persistence"], "refs/heads/orphan",
                                     SHA, "push", now=NOW)["record"]
        self.store.claim_next("cp1-worker", 100, now=NOW + timedelta(seconds=1),
                              required_lane="cp1-persistence")
        recovered = self.store.claim_next("cp1-worker", 101, now=NOW + timedelta(seconds=2),
                                          required_lane="cp1-persistence",
                                          recover_owner_run_ids={100})
        self.assertEqual(recovered["request_id"], request["request_id"])
        self.assertEqual(recovered["attempts"], 2)
        self.assertEqual(recovered["claim"]["owner_run_id"], 101)

    def test_expired_lease_is_recovered_by_new_owner(self):
        request = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                                     SHA, "push", now=NOW)["record"]
        self.store.claim_next("coordinator", 100, now=NOW + timedelta(seconds=1), lease_seconds=30)
        recovered = self.store.claim_next("coordinator", 101, now=NOW + timedelta(seconds=32))
        self.assertEqual(recovered["request_id"], request["request_id"])
        self.assertEqual(recovered["attempts"], 2)
        self.assertEqual(recovered["claim"]["owner_run_id"], 101)

    def test_expired_owner_cannot_mark_running_or_finish(self):
        request = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/expired",
                                     SHA, "push", now=NOW)["record"]
        self.store.claim_next("coordinator", 100, now=NOW + timedelta(seconds=1), lease_seconds=30)
        with self.assertRaisesRegex(QueueStoreError, "lease has expired"):
            self.store.mark_running(request["request_id"], 100, now=NOW + timedelta(seconds=32))
        with self.assertRaisesRegex(QueueStoreError, "lease has expired"):
            self.store.finish(request["request_id"], 100, "failed", "late result",
                              now=NOW + timedelta(seconds=32))

    def test_heartbeat_extends_lease_only_for_current_owner(self):
        request = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                                     SHA, "push", now=NOW)["record"]
        self.store.claim_next("coordinator", 100, now=NOW + timedelta(seconds=1))
        extended = self.store.heartbeat(request["request_id"], 100, now=NOW + timedelta(seconds=20))
        self.assertEqual(extended["claim"]["owner_run_id"], 100)
        self.assertGreater(datetime.fromisoformat(extended["claim"]["lease_expires_at"].replace("Z", "+00:00")),
                           NOW + timedelta(seconds=900))

    def test_mark_running_and_finish_require_owner(self):
        request = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                                     SHA, "push", now=NOW)["record"]
        self.store.claim_next("coordinator", 100, now=NOW + timedelta(seconds=1))
        self.store.mark_running(request["request_id"], 100, now=NOW + timedelta(seconds=2))
        finished = self.store.finish(request["request_id"], 100, "passed", "tests passed",
                                     ["https://github.com/example/run/1"], SHA,
                                     now=NOW + timedelta(seconds=3))
        self.assertEqual(finished["state"], "passed")
        self.assertIsNone(finished["claim"])

    def test_pass_cannot_claim_a_different_sha(self):
        request = self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/feature",
                                     SHA, "push", now=NOW)["record"]
        self.store.claim_next("coordinator", 100, now=NOW + timedelta(seconds=1))
        with self.assertRaisesRegex(ValueError, "exact requested"):
            self.store.finish(request["request_id"], 100, "passed", "tests passed",
                              ["https://github.com/example/run/1"], "b" * 40,
                              now=NOW + timedelta(seconds=2))

    def test_two_concurrent_enqueues_do_not_lose_either_request(self):
        barrier = threading.Barrier(2)
        errors = []
        def writer(commit_sha):
            barrier.wait()
            try:
                self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/parallel",
                                   commit_sha, "push", now=NOW)
            except Exception as exc:
                errors.append(exc)
        threads = [threading.Thread(target=writer, args=(SHA,)),
                   threading.Thread(target=writer, args=("b" * 40,))]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(errors, [])
        self.assertEqual(len(self.backend.ledgers[self.backend.head]["records"]), 2)

    def test_two_concurrent_claims_never_receive_the_same_request(self):
        self.store.enqueue("energundenis/Felucca", ["fast-regression"], "refs/heads/one",
                           SHA, "push", now=NOW)
        barrier = threading.Barrier(2)
        claims = []
        def claimant(run_id):
            barrier.wait()
            claims.append(self.store.claim_next("coordinator", run_id, now=NOW + timedelta(seconds=1)))
        threads = [threading.Thread(target=claimant, args=(201,)),
                   threading.Thread(target=claimant, args=(202,))]
        for thread in threads:
            thread.start()
        for thread in threads:
            thread.join()
        self.assertEqual(sum(item is not None for item in claims), 1)

    def test_bad_lease_and_retry_configuration_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "at least 30"):
            self.store.claim_next("coordinator", 100, now=NOW, lease_seconds=1)
        with self.assertRaisesRegex(ValueError, "max_retries"):
            GitRefCASQueueStore(self.backend, max_retries=0)


if __name__ == "__main__":
    unittest.main()
