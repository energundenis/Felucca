import copy
import io
import os
import subprocess
import tempfile
import urllib.error
import threading
import unittest
from unittest.mock import patch
from datetime import datetime, timedelta, timezone

from cp8_ci_queue import make_idempotency_key
from cp8_ci_queue_store import (GitCliQueueClient, GitHubGitDataClient, GitRefCASQueueStore,
                                QueueCASConflict, QueueRefNotFound, QueueStoreError)

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


class GitCliQueueClientTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.remote = os.path.join(self.temp.name, "remote.git")
        self.repo = os.path.join(self.temp.name, "work")
        subprocess.run(["git", "init", "--bare", self.remote], check=True, capture_output=True)
        os.mkdir(self.repo)
        subprocess.run(["git", "init", self.repo], check=True, capture_output=True)
        subprocess.run(["git", "config", "user.name", "CP8 test"], cwd=self.repo, check=True)
        subprocess.run(["git", "config", "user.email", "cp8-test@example.invalid"], cwd=self.repo, check=True)
        with open(os.path.join(self.repo, "README"), "w", encoding="utf-8") as stream:
            stream.write("queue test base\\n")
        subprocess.run(["git", "add", "README"], cwd=self.repo, check=True)
        subprocess.run(["git", "commit", "-m", "base"], cwd=self.repo, check=True, capture_output=True)
        subprocess.run(["git", "remote", "add", "origin", self.remote], cwd=self.repo, check=True)
        subprocess.run(["git", "push", "origin", "HEAD:refs/heads/main"], cwd=self.repo, check=True, capture_output=True)
        self.base = subprocess.run(["git", "rev-parse", "HEAD"], cwd=self.repo,
                                   check=True, capture_output=True, text=True).stdout.strip()
        self.client = GitCliQueueClient(repo_dir=self.repo)

    def tearDown(self):
        self.temp.cleanup()

    def test_git_transport_initializes_and_reads_queue_ledger(self):
        with self.assertRaises(QueueRefNotFound):
            self.client.read_ref("energun-ci-queue")
        initial = {"schema_version": 1, "records": [], "workers": {}}
        self.client.commit_ledger(self.base, initial, "energun-ci-queue")
        head = self.client.read_ref("energun-ci-queue")
        self.assertEqual(self.client.read_ledger_at(head), initial)
        raw = subprocess.run(["git", "show", f"{head}:queue/ledger.json"], cwd=self.repo,
                             check=True, capture_output=True).stdout
        self.assertTrue(raw.endswith(b"\\n"), "queue ledger commit must end with one canonical newline")

    def test_git_transport_rejects_stale_queue_writer(self):
        self.client.commit_ledger(self.base, {"schema_version": 1, "records": [], "workers": {}},
                                  "energun-ci-queue")
        head = self.client.read_ref("energun-ci-queue")
        self.client.commit_ledger(head, {"schema_version": 1, "records": [{"id": 1}], "workers": {}},
                                  "energun-ci-queue")
        with self.assertRaises(QueueCASConflict):
            self.client.commit_ledger(head, {"schema_version": 1, "records": [{"id": 2}], "workers": {}},
                                      "energun-ci-queue")


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

    def test_missing_queue_ref_is_distinguished_from_other_api_errors(self):
        client = GitHubGitDataClient("energundenis/Felucca", "test-token")
        missing = urllib.error.HTTPError(
            "https://api.github.com/ref", 404, "Not Found", {},
            io.BytesIO(b'{"message":"Not Found"}'))
        with patch("cp8_ci_queue_store.urllib.request.urlopen", side_effect=missing):
            with self.assertRaises(QueueRefNotFound):
                client.read_ref("energun-ci-queue")

        limited = urllib.error.HTTPError(
            "https://api.github.com/ref", 403, "Forbidden", {},
            io.BytesIO(b'{"message":"API rate limit exceeded"}'))
        with patch("cp8_ci_queue_store.urllib.request.urlopen", side_effect=limited):
            with self.assertRaisesRegex(QueueStoreError, "403"):
                client.read_ref("energun-ci-queue")

    def test_bad_lease_and_retry_configuration_are_rejected(self):
        with self.assertRaisesRegex(ValueError, "at least 30"):
            self.store.claim_next("coordinator", 100, now=NOW, lease_seconds=1)
        with self.assertRaisesRegex(ValueError, "max_retries"):
            GitRefCASQueueStore(self.backend, max_retries=0)


if __name__ == "__main__":
    unittest.main()
