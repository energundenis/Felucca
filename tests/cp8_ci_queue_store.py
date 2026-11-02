"""Durable CP8 CI queue ledger backed by a dedicated Git branch.

Writers build commits from the observed branch head and update the ref with
force=False. Competing writers based on the same parent cannot both advance the
ref: the loser receives a non-fast-forward conflict and retries from the new
head. This module does not start workflows or execute repository code.
"""
from __future__ import annotations

import base64
import json
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone
from typing import Any, Callable

from cp8_ci_queue import make_idempotency_key, validate_queue_record

LEDGER_PATH = "queue/ledger.json"
DEFAULT_BRANCH = "energun-ci-queue"
MAX_CAS_RETRIES = 8


class QueueStoreError(RuntimeError):
    pass


class QueueCASConflict(QueueStoreError):
    pass


class GitHubGitDataClient:
    """Minimal Git Database API client. Requires a trusted token with contents:write."""

    def __init__(self, repository: str, token: str, api_url: str = "https://api.github.com",
                 timeout: int = 20) -> None:
        if not token:
            raise ValueError("a GitHub token is required")
        if "/" not in repository or repository.count("/") != 1:
            raise ValueError("repository must use owner/name form")
        self.repository = repository
        self.base_url = api_url.rstrip("/") + "/repos/" + repository
        self.token = token
        self.timeout = timeout

    def request(self, method: str, path: str, payload: dict[str, Any] | None = None) -> dict[str, Any]:
        data = None if payload is None else json.dumps(payload, separators=(",", ":")).encode("utf-8")
        req = urllib.request.Request(
            self.base_url + path,
            data=data,
            method=method,
            headers={
                "Accept": "application/vnd.github+json",
                "Authorization": "Bearer " + self.token,
                "X-GitHub-Api-Version": "2022-11-28",
                "Content-Type": "application/json",
                "User-Agent": "ENERGUN-CP8-CI-Queue",
            },
        )
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as response:
                raw = response.read()
        except urllib.error.HTTPError as exc:
            detail = exc.read().decode("utf-8", errors="replace")[:500]
            if exc.code in {409, 422}:
                raise QueueCASConflict(f"Git ref update conflict ({exc.code}): {detail}") from exc
            raise QueueStoreError(f"GitHub API request failed ({exc.code}): {detail}") from exc
        except urllib.error.URLError as exc:
            raise QueueStoreError(f"GitHub API connection failed: {exc.reason}") from exc
        if not raw:
            return {}
        try:
            return json.loads(raw.decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise QueueStoreError("GitHub API returned invalid JSON") from exc

    def read_ref(self, branch: str) -> str:
        encoded = urllib.parse.quote(branch, safe="/")
        result = self.request("GET", "/git/ref/heads/" + encoded)
        try:
            return result["object"]["sha"]
        except (KeyError, TypeError) as exc:
            raise QueueStoreError("GitHub ref response has no object SHA") from exc

    def read_ledger_at(self, commit_sha: str) -> dict[str, Any]:
        commit = self.request("GET", "/git/commits/" + commit_sha)
        tree_sha = commit.get("tree", {}).get("sha")
        if not tree_sha:
            raise QueueStoreError("Git commit response has no tree SHA")
        tree = self.request("GET", "/git/trees/" + tree_sha)
        entry = next((item for item in tree.get("tree", []) if item.get("path") == LEDGER_PATH), None)
        if entry is None:
            return {"schema_version": 1, "records": []}
        if entry.get("type") != "blob" or not entry.get("sha"):
            raise QueueStoreError("queue ledger path is not a Git blob")
        blob = self.request("GET", "/git/blobs/" + entry["sha"])
        try:
            raw = base64.b64decode(blob["content"], validate=False)
            ledger = json.loads(raw.decode("utf-8"))
        except (KeyError, ValueError, UnicodeDecodeError, json.JSONDecodeError) as exc:
            raise QueueStoreError("queue ledger blob is invalid") from exc
        if not isinstance(ledger, dict) or ledger.get("schema_version") != 1 or not isinstance(ledger.get("records"), list):
            raise QueueStoreError("queue ledger schema is invalid")
        return ledger

    def commit_ledger(self, expected_head: str, ledger: dict[str, Any], branch: str) -> str:
        parent = self.request("GET", "/git/commits/" + expected_head)
        base_tree = parent.get("tree", {}).get("sha")
        if not base_tree:
            raise QueueStoreError("parent commit response has no tree SHA")
        encoded = json.dumps(ledger, sort_keys=True, separators=(",", ":")) + "\n"
        blob = self.request("POST", "/git/blobs", {"content": encoded, "encoding": "utf-8"})
        tree = self.request("POST", "/git/trees", {
            "base_tree": base_tree,
            "tree": [{
                "path": LEDGER_PATH,
                "mode": "100644",
                "type": "blob",
                "sha": blob["sha"],
            }],
        })
        commit = self.request("POST", "/git/commits", {
            "message": "ci(queue): update durable CP8 request ledger",
            "tree": tree["sha"],
            "parents": [expected_head],
        })
        # Non-forced ref update is the final compare-and-swap: a competing
        # commit based on the same old head is not a fast-forward.
        self.request("PATCH", "/git/refs/heads/" + urllib.parse.quote(branch, safe="/"), {
            "sha": commit["sha"],
            "force": False,
        })
        return commit["sha"]


class GitRefCASQueueStore:
    def __init__(self, client: Any, branch: str = DEFAULT_BRANCH,
                 max_retries: int = MAX_CAS_RETRIES, sleep: Callable[[float], None] = time.sleep) -> None:
        if max_retries < 1:
            raise ValueError("max_retries must be positive")
        self.client = client
        self.branch = branch
        self.max_retries = max_retries
        self.sleep = sleep

    def _transact(self, mutate: Callable[[dict[str, Any]], Any]) -> Any:
        for attempt in range(self.max_retries):
            head = self.client.read_ref(self.branch)
            ledger = self.client.read_ledger_at(head)
            result = mutate(ledger)
            try:
                self.client.commit_ledger(head, ledger, self.branch)
                return result
            except QueueCASConflict:
                if attempt + 1 == self.max_retries:
                    raise
                self.sleep(min(0.05 * (2 ** attempt), 1.0))
        raise QueueStoreError("queue transaction exhausted retries")

    def enqueue(self, repository: str, lanes: list[str], ref: str, commit_sha: str,
                event: str, now: datetime | None = None,
                pull_request_number: int | None = None,
                workflow_run_id: int | None = None) -> dict[str, Any]:
        timestamp = _iso(now or datetime.now(timezone.utc))
        key = make_idempotency_key(repository, lanes, ref, commit_sha)

        def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
            for existing in ledger["records"]:
                if existing.get("idempotency_key") == key:
                    return {"created": False, "record": existing}
            record = {
                "schema_version": 1,
                "request_id": key[:24],
                "idempotency_key": key,
                "repository": repository,
                "ref": ref,
                "commit_sha": commit_sha,
                "source": {
                    "event": event,
                    "pull_request_number": pull_request_number,
                    "workflow_run_id": workflow_run_id,
                },
                "requested_lanes": sorted(lanes),
                "state": "queued",
                "attempts": 0,
                "created_at": timestamp,
                "updated_at": timestamp,
                "claim": None,
                "result": None,
            }
            validate_queue_record(record)
            ledger["records"].append(record)
            return {"created": True, "record": record}
        return self._transact(mutate)

    def claim_next(self, owner_workflow: str, owner_run_id: int, now: datetime | None = None,
                   lease_seconds: int = 900) -> dict[str, Any] | None:
        current = now or datetime.now(timezone.utc)
        if lease_seconds < 30:
            raise ValueError("lease_seconds must be at least 30")
        timestamp = _iso(current)
        expiry = _iso(current + timedelta(seconds=lease_seconds))

        def mutate(ledger: dict[str, Any]) -> dict[str, Any] | None:
            candidates = []
            for item in ledger["records"]:
                state = item.get("state")
                if state == "queued":
                    candidates.append(item)
                elif state in {"claimed", "running"}:
                    lease = item.get("claim", {}).get("lease_expires_at")
                    if lease and _parse_iso(lease) <= current:
                        candidates.append(item)
            if not candidates:
                return None
            selected = min(candidates, key=lambda item: (item["created_at"], item["request_id"]))
            selected["state"] = "claimed"
            selected["attempts"] += 1
            selected["updated_at"] = timestamp
            selected["claim"] = {
                "owner_workflow": owner_workflow,
                "owner_run_id": owner_run_id,
                "claimed_at": timestamp,
                "heartbeat_at": timestamp,
                "lease_expires_at": expiry,
            }
            selected["result"] = None
            validate_queue_record(selected)
            return selected
        return self._transact(mutate)

    def heartbeat(self, request_id: str, owner_run_id: int, now: datetime | None = None,
                  lease_seconds: int = 900) -> dict[str, Any]:
        current = now or datetime.now(timezone.utc)
        timestamp = _iso(current)
        expiry = _iso(current + timedelta(seconds=lease_seconds))

        def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
            item = _find_record(ledger, request_id)
            if item["state"] not in {"claimed", "running"} or item["claim"]["owner_run_id"] != owner_run_id:
                raise QueueStoreError("claim ownership mismatch")
            if _parse_iso(item["claim"]["lease_expires_at"]) <= current:
                raise QueueStoreError("claim lease has expired")
            item["claim"]["heartbeat_at"] = timestamp
            item["claim"]["lease_expires_at"] = expiry
            item["updated_at"] = timestamp
            validate_queue_record(item)
            return item
        return self._transact(mutate)

    def mark_running(self, request_id: str, owner_run_id: int, now: datetime | None = None) -> dict[str, Any]:
        timestamp = _iso(now or datetime.now(timezone.utc))

        def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
            item = _find_record(ledger, request_id)
            if item["state"] != "claimed" or item["claim"]["owner_run_id"] != owner_run_id:
                raise QueueStoreError("claim ownership mismatch or invalid state")
            item["state"] = "running"
            item["updated_at"] = timestamp
            validate_queue_record(item)
            return item
        return self._transact(mutate)

    def finish(self, request_id: str, owner_run_id: int, outcome: str, summary: str,
               evidence: list[str] | None = None, tested_sha: str | None = None,
               now: datetime | None = None) -> dict[str, Any]:
        if outcome not in {"passed", "blocked", "failed"}:
            raise ValueError("outcome must be passed, blocked or failed")
        timestamp = _iso(now or datetime.now(timezone.utc))

        def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
            item = _find_record(ledger, request_id)
            if item["state"] not in {"claimed", "running"} or item["claim"]["owner_run_id"] != owner_run_id:
                raise QueueStoreError("claim ownership mismatch or invalid state")
            item["state"] = outcome
            item["updated_at"] = timestamp
            item["claim"] = None
            result = {"outcome": outcome, "finished_at": timestamp, "summary": summary}
            if outcome == "passed":
                result["tested_sha"] = tested_sha
                result["evidence"] = evidence
            item["result"] = result
            validate_queue_record(item)
            return item
        return self._transact(mutate)


def _find_record(ledger: dict[str, Any], request_id: str) -> dict[str, Any]:
    for item in ledger["records"]:
        if item.get("request_id") == request_id:
            return item
    raise QueueStoreError("queue request not found")


def _iso(value: datetime) -> str:
    if value.tzinfo is None:
        raise ValueError("timestamps must be timezone-aware")
    return value.astimezone(timezone.utc).isoformat().replace("+00:00", "Z")


def _parse_iso(value: str) -> datetime:
    return datetime.fromisoformat(value.replace("Z", "+00:00"))
