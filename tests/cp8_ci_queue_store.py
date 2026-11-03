"""Durable CP8 CI queue ledger backed by a dedicated Git branch.

Writers build commits from the observed branch head and update the ref with
force=False. Competing writers based on the same parent cannot both advance the
ref: the loser receives a non-fast-forward conflict and retries from the new
head. This module does not start workflows or execute repository code.
"""
from __future__ import annotations

import base64
import json
import os
import subprocess
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timedelta, timezone
from typing import Any, Callable

from cp8_ci_queue import make_idempotency_key, validate_queue_record

LEDGER_PATH = "queue/ledger.json"
DEFAULT_BRANCH = "energun-ci-queue"
MAX_CAS_RETRIES = 24


class QueueStoreError(RuntimeError):
    pass


class QueueCASConflict(QueueStoreError):
    pass


class QueueRefNotFound(QueueStoreError):
    """The requested Git ref does not exist; other API errors are not absence."""
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
            if exc.code == 404 and method == "GET" and path.startswith("/git/ref/heads/"):
                raise QueueRefNotFound(f"Git ref not found: {path}") from exc
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
        tree = self.request("GET", "/git/trees/" + tree_sha + "?recursive=1")
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


class GitCliQueueClient:
    """Queue ledger transport using authenticated Git smart HTTP, not REST API quota."""

    def __init__(self, remote: str = "origin", repo_dir: str = ".") -> None:
        self.remote = remote
        self.repo_dir = repo_dir

    def _git(self, *args: str, env: dict[str, str] | None = None,
             check: bool = True) -> subprocess.CompletedProcess[str]:
        merged = os.environ.copy()
        if env:
            merged.update(env)
        result = subprocess.run(
            ["git", *args], cwd=self.repo_dir, env=merged,
            capture_output=True, text=True,
        )
        if check and result.returncode:
            raise QueueStoreError(
                f"git {' '.join(args[:3])} failed ({result.returncode}): "
                f"{result.stderr.strip()[:400]}"
            )
        return result

    def read_ref(self, branch: str) -> str:
        ref = f"refs/heads/{branch}"
        fetched = self._git("fetch", "--no-tags", self.remote,
                            f"+{ref}:refs/remotes/{self.remote}/{branch}", check=False)
        if fetched.returncode:
            detail = fetched.stderr.strip()
            if "couldn't find remote ref" in detail.lower() or "no such ref" in detail.lower():
                raise QueueRefNotFound(f"Git queue ref not found: {branch}")
            raise QueueStoreError(f"unable to fetch queue ref: {detail[:400]}")
        result = self._git("rev-parse", f"refs/remotes/{self.remote}/{branch}")
        sha = result.stdout.strip()
        if len(sha) != 40:
            raise QueueStoreError("Git returned an invalid queue branch SHA")
        return sha

    def read_ledger_at(self, commit_sha: str) -> dict[str, Any]:
        result = self._git("show", f"{commit_sha}:{LEDGER_PATH}", check=False)
        if result.returncode:
            if "does not exist" in result.stderr or "exists on disk, but not in" in result.stderr:
                return {"schema_version": 1, "records": []}
            raise QueueStoreError(f"unable to read queue ledger: {result.stderr.strip()[:400]}")
        try:
            ledger = json.loads(result.stdout)
        except json.JSONDecodeError as exc:
            raise QueueStoreError("queue ledger blob is invalid") from exc
        if not isinstance(ledger, dict) or ledger.get("schema_version") != 1 or not isinstance(ledger.get("records"), list):
            raise QueueStoreError("queue ledger schema is invalid")
        return ledger

    def commit_ledger(self, expected_head: str, ledger: dict[str, Any], branch: str) -> str:
        encoded = json.dumps(ledger, sort_keys=True, separators=(",", ":")) + "\\n"
        with tempfile.NamedTemporaryFile(prefix="cp8-index-", delete=True) as index:
            env = {"GIT_INDEX_FILE": index.name}
            self._git("read-tree", expected_head, env=env)
            # hash-object receives the ledger bytes on stdin.
            obj = subprocess.run(
                ["git", "hash-object", "-w", "--stdin"], cwd=self.repo_dir,
                env={**os.environ, **env}, input=encoded, capture_output=True, text=True,
            )
            if obj.returncode:
                raise QueueStoreError(f"unable to write queue ledger blob: {obj.stderr.strip()[:300]}")
            self._git("update-index", "--add", "--cacheinfo", f"100644,{obj.stdout.strip()},{LEDGER_PATH}", env=env)
            tree = self._git("write-tree", env=env).stdout.strip()
            commit = self._git("-c", "user.name=ENERGUN CP8 Queue",
                               "-c", "user.email=energum-cp8-queue@users.noreply.github.com",
                               "commit-tree", tree, "-p", expected_head, "-m",
                               "ci(queue): update durable CP8 request ledger").stdout.strip()
            pushed = self._git("push", self.remote, f"{commit}:refs/heads/{branch}", check=False)
            if pushed.returncode:
                detail = pushed.stderr.strip()
                if any(token in detail.lower() for token in ("non-fast-forward", "fetch first", "stale info", "rejected")):
                    raise QueueCASConflict(f"queue branch advanced during commit: {detail[:400]}")
                raise QueueStoreError(f"unable to push queue ledger: {detail[:400]}")
            return commit


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
            before = json.dumps(ledger, sort_keys=True, separators=(",", ":"))
            result = mutate(ledger)
            after = json.dumps(ledger, sort_keys=True, separators=(",", ":"))
            if after == before:
                return result
            try:
                self.client.commit_ledger(head, ledger, self.branch)
                return result
            except QueueCASConflict:
                if attempt + 1 == self.max_retries:
                    raise
                self.sleep(min(0.1 * (2 ** attempt), 2.0))
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
                   lease_seconds: int = 900, required_lane: str | None = None,
                   recover_owner_run_ids: set[int] | None = None) -> dict[str, Any] | None:
        current = now or datetime.now(timezone.utc)
        if lease_seconds < 30:
            raise ValueError("lease_seconds must be at least 30")
        if required_lane is not None and required_lane not in {"fast-regression", "baseline-build", "cp1-persistence", "cp2-dsp-vca", "cp3-sequencer"}:
            raise ValueError("required_lane is unsupported")
        timestamp = _iso(current)
        expiry = _iso(current + timedelta(seconds=lease_seconds))

        def mutate(ledger: dict[str, Any]) -> dict[str, Any] | None:
            candidates = []
            for item in ledger["records"]:
                if required_lane is not None and required_lane not in item.get("requested_lanes", []):
                    continue
                state = item.get("state")
                if state == "queued":
                    candidates.append(item)
                elif state in {"claimed", "running"}:
                    lease = item.get("claim", {}).get("lease_expires_at")
                    candidate_owner_run_id = item.get("claim", {}).get("owner_run_id")
                    lease_expired = bool(lease and _parse_iso(lease) <= current)
                    owner_finished = candidate_owner_run_id in (recover_owner_run_ids or set())
                    if lease_expired or owner_finished:
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
            _require_live_claim(item, owner_run_id, _parse_iso(timestamp))
            if item["state"] != "claimed":
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
            _require_live_claim(item, owner_run_id, _parse_iso(timestamp))
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


def _require_live_claim(item: dict[str, Any], owner_run_id: int, current: datetime) -> None:
    claim = item.get("claim") or {}
    if item.get("state") not in {"claimed", "running"} or claim.get("owner_run_id") != owner_run_id:
        raise QueueStoreError("claim ownership mismatch or invalid state")
    if _parse_iso(claim["lease_expires_at"]) <= current:
        raise QueueStoreError("claim lease has expired")


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
