"""Validation primitives for the CP8.1 durable CI request ledger.

This module is deliberately storage- and workflow-agnostic. It validates queue
records; it does not claim leases, mutate GitHub, execute CI, or run commands.
"""
from __future__ import annotations

import hashlib
import json
import re
from datetime import datetime, timezone
from typing import Any

SCHEMA_VERSION = 1
ALLOWED_LANES = frozenset({"fast-regression", "baseline-build"})
ALLOWED_EVENTS = frozenset({"push", "pull_request", "workflow_dispatch", "schedule", "reconcile"})
ALLOWED_STATES = frozenset({
    "queued", "claimed", "running", "passed", "blocked", "failed", "superseded"
})
TRANSITIONS = {
    "queued": frozenset({"claimed", "superseded"}),
    "claimed": frozenset({"running", "queued", "blocked", "failed"}),
    "running": frozenset({"passed", "blocked", "failed"}),
    "passed": frozenset(),
    "blocked": frozenset(),
    "failed": frozenset(),
    "superseded": frozenset(),
}
_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
_REPO_RE = re.compile(r"^[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+$")


def _require(condition: bool, message: str) -> None:
    if not condition:
        raise ValueError(message)


def _parse_time(value: Any, field: str) -> datetime:
    _require(isinstance(value, str) and bool(value), f"{field} must be an ISO-8601 timestamp")
    normalized = value[:-1] + "+00:00" if value.endswith("Z") else value
    try:
        parsed = datetime.fromisoformat(normalized)
    except ValueError as exc:
        raise ValueError(f"{field} must be an ISO-8601 timestamp") from exc
    _require(parsed.tzinfo is not None, f"{field} must include a timezone")
    return parsed.astimezone(timezone.utc)


def make_idempotency_key(repository: str, lanes: list[str], ref: str, commit_sha: str) -> str:
    """Return a stable key for the same repo/lane-set/ref/exact-SHA request."""
    _require(isinstance(repository, str) and _REPO_RE.fullmatch(repository) is not None,
             "repository must use owner/name form")
    _require(isinstance(lanes, list) and lanes, "lanes must be a non-empty list")
    _require(all(isinstance(item, str) and item in ALLOWED_LANES for item in lanes),
             "lanes contains an unsupported lane")
    _require(len(set(lanes)) == len(lanes), "lanes must not contain duplicates")
    _require(isinstance(ref, str) and bool(ref.strip()), "ref must be a non-empty string")
    _require(isinstance(commit_sha, str) and _SHA_RE.fullmatch(commit_sha) is not None,
             "commit_sha must be a full 40-character lowercase SHA")
    payload = {
        "repository": repository,
        "lanes": sorted(lanes),
        "ref": ref,
        "commit_sha": commit_sha,
    }
    encoded = json.dumps(payload, sort_keys=True, separators=(",", ":")).encode("utf-8")
    return hashlib.sha256(encoded).hexdigest()


def validate_queue_record(record: dict[str, Any]) -> dict[str, Any]:
    """Validate one durable queue record and return it unchanged."""
    _require(isinstance(record, dict), "queue record must be an object")
    _require(record.get("schema_version") == SCHEMA_VERSION, "unsupported schema_version")
    _require(isinstance(record.get("request_id"), str) and bool(record["request_id"].strip()),
             "request_id is required")
    repository = record.get("repository")
    lanes = record.get("requested_lanes")
    ref = record.get("ref")
    commit_sha = record.get("commit_sha")
    expected_key = make_idempotency_key(repository, lanes, ref, commit_sha)
    _require(record.get("idempotency_key") == expected_key, "idempotency_key does not match request identity")

    source = record.get("source")
    _require(isinstance(source, dict), "source must be an object")
    _require(isinstance(source.get("event"), str) and source["event"] in ALLOWED_EVENTS, "source.event is unsupported")
    for optional_id in ("pull_request_number", "workflow_run_id"):
        value = source.get(optional_id)
        _require(value is None or (isinstance(value, int) and not isinstance(value, bool) and value > 0),
                 f"source.{optional_id} must be a positive integer or null")

    state = record.get("state")
    _require(isinstance(state, str) and state in ALLOWED_STATES, "state is unsupported")
    attempts = record.get("attempts")
    _require(isinstance(attempts, int) and not isinstance(attempts, bool) and attempts >= 0,
             "attempts must be a non-negative integer")
    created = _parse_time(record.get("created_at"), "created_at")
    updated = _parse_time(record.get("updated_at"), "updated_at")
    _require(updated >= created, "updated_at must not precede created_at")

    claim = record.get("claim")
    result = record.get("result")
    if state in {"queued", "superseded"}:
        _require(claim is None, f"{state} records must not retain an active claim")
    if state in {"claimed", "running"}:
        _require(isinstance(claim, dict), f"{state} records require a claim")
        _require(isinstance(claim.get("owner_workflow"), str) and bool(claim["owner_workflow"]),
                 "claim.owner_workflow is required")
        run_id = claim.get("owner_run_id")
        _require(isinstance(run_id, int) and not isinstance(run_id, bool) and run_id > 0,
                 "claim.owner_run_id must be a positive integer")
        claimed = _parse_time(claim.get("claimed_at"), "claim.claimed_at")
        heartbeat = _parse_time(claim.get("heartbeat_at"), "claim.heartbeat_at")
        expires = _parse_time(claim.get("lease_expires_at"), "claim.lease_expires_at")
        _require(claimed <= heartbeat < expires, "claim timestamps must satisfy claimed_at <= heartbeat_at < lease_expires_at")
        _require(claimed >= created, "claim.claimed_at must not precede created_at")
        _require(attempts >= 1, "claimed/running records must have attempts >= 1")
    if state in {"passed", "blocked", "failed"}:
        _require(claim is None, "terminal records must release their claim")
        _require(isinstance(result, dict), f"{state} records require a result")
        _require(result.get("outcome") == state, "result.outcome must match record state")
        _parse_time(result.get("finished_at"), "result.finished_at")
        _require(isinstance(result.get("summary"), str) and bool(result["summary"].strip()),
                 "result.summary is required")
        if state == "passed":
            _require(result.get("tested_sha") == commit_sha,
                     "passed result must identify the exact requested commit_sha")
            evidence = result.get("evidence")
            _require(isinstance(evidence, list) and len(evidence) > 0,
                     "passed result requires at least one evidence reference")
            _require(all(isinstance(item, str) and item.startswith("https://") for item in evidence),
                     "evidence references must be HTTPS URLs")
    if state == "superseded":
        _require(isinstance(result, dict) and result.get("outcome") == "superseded",
                 "superseded records require a superseded result")
        _parse_time(result.get("finished_at"), "result.finished_at")
        _require(isinstance(result.get("summary"), str) and bool(result["summary"].strip()),
                 "result.summary is required")
    return record


def validate_transition(current_state: str, next_state: str) -> None:
    """Reject illegal state changes; storage must still perform an atomic CAS."""
    _require(isinstance(current_state, str) and current_state in ALLOWED_STATES, "current_state is unsupported")
    _require(isinstance(next_state, str) and next_state in TRANSITIONS[current_state],
             f"illegal queue transition: {current_state} -> {next_state}")
