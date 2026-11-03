"""Persistent CP8 worker registry and dependency-aware task claims.

Worker presence, capacity, leases and task assignment are committed atomically
in the Git-ref queue ledger. The runtime provides at-least-once execution with
fencing; task commands must remain idempotent.
"""
from __future__ import annotations

from datetime import datetime, timedelta, timezone
from typing import Any

from cp8_ci_queue import validate_queue_record
from cp8_ci_queue_store import (
    QueueStoreError, _find_record, _iso, _parse_iso,
)

ACTIVE_STATES = {"claimed", "running"}
TERMINAL_STATES = {"passed", "failed", "blocked"}


def register_worker(store: Any, worker_id: str, run_id: int, lane: str,
                    capabilities: list[str], max_concurrency: int = 1,
                    now: datetime | None = None) -> dict[str, Any]:
    if not worker_id or run_id <= 0 or not lane or not capabilities:
        raise ValueError("worker identity, run, lane and capabilities are required")
    if not 1 <= max_concurrency <= 3:
        raise ValueError("worker max_concurrency must be between 1 and 3")
    timestamp = _iso(now or datetime.now(timezone.utc))

    def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
        workers = ledger.setdefault("workers", {})
        current = workers.get(worker_id)
        if current and current.get("run_id") != run_id and current.get("state") in {"busy", "available"}:
            raise QueueStoreError("worker identity is already owned by another run")
        if current and current.get("run_id") == run_id and current.get("state") == "busy":
            raise QueueStoreError("worker is already busy in this run")
        item = {
            "worker_id": worker_id, "run_id": run_id, "lane": lane,
            "capabilities": sorted(set(capabilities)),
            "max_concurrency": max_concurrency, "state": "available",
            "registered_at": current.get("registered_at", timestamp) if current else timestamp,
            "heartbeat_at": timestamp, "active_request_id": None,
            "version": 1,
        }
        workers[worker_id] = item
        return item
    return store._transact(mutate)


def heartbeat_worker(store: Any, worker_id: str, run_id: int,
                     now: datetime | None = None) -> dict[str, Any]:
    timestamp = _iso(now or datetime.now(timezone.utc))

    def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
        item = ledger.setdefault("workers", {}).get(worker_id)
        if not item or item.get("run_id") != run_id:
            raise QueueStoreError("worker ownership mismatch")
        item["heartbeat_at"] = timestamp
        return item
    return store._transact(mutate)


def claim_next_for_worker(store: Any, worker_id: str, run_id: int,
                          dependencies: dict[str, list[str]],
                          completed_owner_run_ids: set[int] | None = None,
                          lease_seconds: int = 3600,
                          now: datetime | None = None) -> dict[str, Any] | None:
    current = now or datetime.now(timezone.utc)
    if lease_seconds < 30:
        raise ValueError("lease_seconds must be at least 30")
    timestamp = _iso(current)
    expiry = _iso(current + timedelta(seconds=lease_seconds))
    completed_owner_run_ids = completed_owner_run_ids or set()

    def mutate(ledger: dict[str, Any]) -> dict[str, Any] | None:
        workers = ledger.setdefault("workers", {})
        worker = workers.get(worker_id)
        if not worker or worker.get("run_id") != run_id:
            raise QueueStoreError("worker must be registered to this run before claiming")
        if worker.get("state") not in {"available", "busy"}:
            raise QueueStoreError("worker is not available")
        records = ledger.setdefault("records", [])
        completed = set(ledger.setdefault("completed_task_ids", []))
        # A completed queue record is authoritative evidence for its lane task.
        for record in records:
            if record.get("state") == "passed":
                completed.update(record.get("requested_lanes", []))
        active_global = sum(1 for item in records if item.get("state") in ACTIVE_STATES and (item.get("claim") or {}).get("owner_run_id") not in completed_owner_run_ids)
        if active_global >= 3:
            return None
        active_for_worker = [
            item for item in records
            if item.get("state") in ACTIVE_STATES
            and (item.get("claim") or {}).get("owner_run_id") == run_id
            and (item.get("claim") or {}).get("owner_workflow") == worker_id
        ]
        if len(active_for_worker) >= worker["max_concurrency"]:
            return None

        candidates = []
        for item in records:
            lane_list = item.get("requested_lanes", [])
            if worker.get("lane") not in lane_list:
                continue
            lane = worker["lane"]
            required = set(dependencies.get(lane, []))
            if not required.issubset(completed):
                continue
            state = item.get("state")
            if state == "queued":
                candidates.append(item)
                continue
            if state in ACTIVE_STATES:
                claim = item.get("claim") or {}
                lease = claim.get("lease_expires_at")
                owner = claim.get("owner_run_id")
                expired = bool(lease and _parse_iso(lease) <= current)
                owner_finished = owner in completed_owner_run_ids
                if owner_finished:
                    candidates.append(item)
        if not candidates:
            worker["state"] = "available"
            worker["heartbeat_at"] = timestamp
            return None
        selected = min(candidates, key=lambda item: (item["created_at"], item["request_id"]))
        previous = selected.get("claim") or {}
        selected["state"] = "claimed"
        selected["attempts"] += 1
        selected["updated_at"] = timestamp
        selected["claim"] = {
            "owner_workflow": worker_id,
            "owner_run_id": run_id,
            "claimed_at": timestamp,
            "heartbeat_at": timestamp,
            "lease_expires_at": expiry,
            "fencing_attempt": selected["attempts"],
            "previous_owner_run_id": previous.get("owner_run_id"),
        }
        selected["result"] = None
        validate_queue_record(selected)
        worker["state"] = "busy"
        worker["heartbeat_at"] = timestamp
        worker["active_request_id"] = selected["request_id"]
        return selected
    return store._transact(mutate)


def mark_worker_running(store: Any, request_id: str, worker_id: str, run_id: int,
                        now: datetime | None = None) -> dict[str, Any]:
    timestamp = _iso(now or datetime.now(timezone.utc))

    def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
        item = _find_record(ledger, request_id)
        claim = item.get("claim") or {}
        if (item.get("state") != "claimed" or claim.get("owner_run_id") != run_id
                or claim.get("owner_workflow") != worker_id
                or claim.get("fencing_attempt") != item.get("attempts")):
            raise QueueStoreError("worker claim lost or fenced by another worker")
        if _parse_iso(claim["lease_expires_at"]) <= _parse_iso(timestamp):
            raise QueueStoreError("claim lease has expired")
        item["state"] = "running"
        item["updated_at"] = timestamp
        validate_queue_record(item)
        return item
    return store._transact(mutate)


def finish_worker_task(store: Any, request_id: str, worker_id: str, run_id: int,
                       outcome: str, summary: str, evidence: list[str] | None = None,
                       tested_sha: str | None = None,
                       now: datetime | None = None) -> dict[str, Any]:
    if outcome not in TERMINAL_STATES:
        raise ValueError("outcome must be passed, failed or blocked")
    timestamp = _iso(now or datetime.now(timezone.utc))

    def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
        item = _find_record(ledger, request_id)
        claim = item.get("claim") or {}
        if (item.get("state") not in ACTIVE_STATES
                or claim.get("owner_run_id") != run_id
                or claim.get("owner_workflow") != worker_id
                or claim.get("fencing_attempt") != item.get("attempts")):
            raise QueueStoreError("worker claim lost or fenced by a newer attempt")
        if _parse_iso(claim["lease_expires_at"]) <= _parse_iso(timestamp):
            raise QueueStoreError("claim lease has expired")
        if outcome == "passed" and tested_sha != item.get("commit_sha"):
            raise ValueError("PASS evidence must match the exact requested SHA")
        item["state"] = outcome
        item["updated_at"] = timestamp
        item["claim"] = None
        result = {"outcome": outcome, "finished_at": timestamp, "summary": summary}
        if outcome == "passed":
            result.update({"tested_sha": tested_sha, "evidence": evidence or []})
            ledger.setdefault("completed_task_ids", [])
            for lane in item.get("requested_lanes", []):
                if lane not in ledger["completed_task_ids"]:
                    ledger["completed_task_ids"].append(lane)
        item["result"] = result
        worker = ledger.setdefault("workers", {}).get(worker_id)
        if worker and worker.get("run_id") == run_id:
            worker["state"] = "available"
            worker["active_request_id"] = None
            worker["heartbeat_at"] = timestamp
        validate_queue_record(item)
        return item
    return store._transact(mutate)


def release_worker(store: Any, worker_id: str, run_id: int,
                   now: datetime | None = None) -> dict[str, Any]:
    timestamp = _iso(now or datetime.now(timezone.utc))

    def mutate(ledger: dict[str, Any]) -> dict[str, Any]:
        worker = ledger.setdefault("workers", {}).get(worker_id)
        if not worker or worker.get("run_id") != run_id:
            raise QueueStoreError("worker ownership mismatch")
        if worker.get("state") == "busy":
            # Do not release a live claim. Recovery is performed by a new owner.
            raise QueueStoreError("cannot release worker with an active task")
        worker["state"] = "offline"
        worker["heartbeat_at"] = timestamp
        worker["active_request_id"] = None
        return worker
    return store._transact(mutate)
