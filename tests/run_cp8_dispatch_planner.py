#!/usr/bin/env python3
"""Dry-run CP8 task router. It never launches agents, edits source or creates PRs."""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
REGISTRY = ROOT / "tests" / "cp8_worker_registry.json"
TASKS = ROOT / "tests" / "cp8_task_catalog.json"
OUT = ROOT / "build" / "cp8-dispatch" / "report.json"


def read_json(path: Path) -> dict:
    return json.loads(path.read_text(encoding="utf-8"))


def _safe_paths(paths: list[str], owner: str) -> None:
    if not isinstance(paths, list) or not paths:
        raise ValueError(f"{owner}: allowed_paths must be a non-empty list")
    for value in paths:
        if not isinstance(value, str) or not value or value.startswith("/") or "\\" in value:
            raise ValueError(f"{owner}: invalid allowed path {value!r}")
        parts = value.rstrip("/").split("/")
        if any(part in {"", ".", ".."} for part in parts):
            raise ValueError(f"{owner}: unsafe allowed path {value!r}")


def validate_registry(data: dict) -> dict[str, dict]:
    if data.get("schema_version") != 1:
        raise ValueError("Unsupported worker registry schema_version")
    policy = data.get("policy", {})
    required_safety = {
        "allow_main_write": False,
        "allow_auto_merge": False,
        "allow_physical_device_access": False,
        "allow_flash_write": False,
        "require_isolated_branch": True,
        "require_review": True,
    }
    for key, expected in required_safety.items():
        if policy.get(key) is not expected:
            raise ValueError(f"Unsafe or missing registry policy: {key} must be {expected}")
    max_parallel = policy.get("max_parallel_assignments")
    if not isinstance(max_parallel, int) or isinstance(max_parallel, bool) or not 1 <= max_parallel <= 8:
        raise ValueError("max_parallel_assignments must be an integer from 1 to 8")
    catalog = data.get("capability_catalog")
    if not isinstance(catalog, list) or not catalog or len(set(catalog)) != len(catalog):
        raise ValueError("capability_catalog must be a non-empty unique list")
    workers = data.get("workers")
    if not isinstance(workers, list):
        raise ValueError("workers must be a list")
    by_id: dict[str, dict] = {}
    for worker in workers:
        worker_id = worker.get("worker_id")
        if not isinstance(worker_id, str) or not worker_id or worker_id in by_id:
            raise ValueError(f"Missing or duplicate worker_id: {worker_id!r}")
        if worker.get("state") not in {"active", "disabled"}:
            raise ValueError(f"{worker_id}: state must be active or disabled")
        caps = worker.get("capabilities")
        if not isinstance(caps, list) or not caps or not set(caps).issubset(set(catalog)):
            raise ValueError(f"{worker_id}: invalid capabilities")
        limit = worker.get("max_concurrency")
        if not isinstance(limit, int) or isinstance(limit, bool) or not 1 <= limit <= max_parallel:
            raise ValueError(f"{worker_id}: max_concurrency exceeds policy")
        if not isinstance(worker.get("version"), str) or not worker["version"]:
            raise ValueError(f"{worker_id}: version is required")
        _safe_paths(worker.get("allowed_paths"), worker_id)
        by_id[worker_id] = worker
    return by_id


def validate_tasks(data: dict, capabilities: set[str]) -> list[dict]:
    if data.get("schema_version") != 1:
        raise ValueError("Unsupported task catalog schema_version")
    tasks = data.get("tasks")
    if not isinstance(tasks, list):
        raise ValueError("tasks must be a list")
    by_id: dict[str, dict] = {}
    for task in tasks:
        task_id = task.get("task_id")
        if not isinstance(task_id, str) or not task_id or task_id in by_id:
            raise ValueError(f"Missing or duplicate task_id: {task_id!r}")
        required = task.get("required_capabilities")
        if not isinstance(required, list) or not required or not set(required).issubset(capabilities):
            raise ValueError(f"{task_id}: unknown or missing required capabilities")
        deps = task.get("depends_on", [])
        if not isinstance(deps, list) or any(not isinstance(dep, str) for dep in deps):
            raise ValueError(f"{task_id}: invalid depends_on")
        if task.get("state") not in {"ready", "blocked", "completed", "disabled"}:
            raise ValueError(f"{task_id}: invalid state")
        priority = task.get("priority")
        if not isinstance(priority, int) or isinstance(priority, bool):
            raise ValueError(f"{task_id}: priority must be an integer")
        _safe_paths(task.get("allowed_paths"), task_id)
        by_id[task_id] = task
    for task_id, task in by_id.items():
        unknown = set(task.get("depends_on", [])) - set(by_id)
        if unknown:
            raise ValueError(f"{task_id}: unknown dependencies {sorted(unknown)}")
    remaining = {key: set(value.get("depends_on", [])) for key, value in by_id.items()}
    while remaining:
        ready = [key for key, deps in remaining.items() if not deps]
        if not ready:
            raise ValueError(f"Task dependency cycle: {sorted(remaining)}")
        for key in ready:
            del remaining[key]
        for deps in remaining.values():
            deps.difference_update(ready)
    return tasks


def build_dispatch_plan(registry: dict, catalog: dict) -> dict:
    workers = validate_registry(registry)
    capability_set = set(registry["capability_catalog"])
    tasks = validate_tasks(catalog, capability_set)
    policy_limit = registry["policy"]["max_parallel_assignments"]
    completed = set(catalog.get("completed_task_ids", []))
    if any(not isinstance(item, str) for item in completed):
        raise ValueError("completed_task_ids must contain strings")
    known_ids = {task["task_id"] for task in tasks}
    if not completed.issubset(known_ids):
        raise ValueError("completed_task_ids contains unknown tasks")

    active = sorted((w for w in workers.values() if w["state"] == "active"), key=lambda w: w["worker_id"])
    active_counts = {w["worker_id"]: 0 for w in active}
    assignments = []
    rows = []
    ordered = sorted(tasks, key=lambda t: (-t["priority"], t["task_id"]))

    for task in ordered:
        task_id = task["task_id"]
        if task_id in completed or task["state"] == "completed":
            rows.append({"task_id": task_id, "state": "COMPLETED", "worker_id": None, "detail": "Explicit completion record."})
            continue
        if task["state"] != "ready":
            rows.append({"task_id": task_id, "state": task["state"].upper(), "worker_id": None, "detail": "Task is not ready."})
            continue
        missing = sorted(set(task.get("depends_on", [])) - completed)
        if missing:
            rows.append({"task_id": task_id, "state": "WAITING_DEPENDENCIES", "worker_id": None, "detail": "Waiting for: " + ", ".join(missing)})
            continue
        eligible = [
            worker for worker in active
            if set(task["required_capabilities"]).issubset(set(worker["capabilities"]))
            and active_counts[worker["worker_id"]] < worker["max_concurrency"]
            and set(task["allowed_paths"]).issubset(set(worker["allowed_paths"]))
        ]
        if len(assignments) >= policy_limit:
            rows.append({"task_id": task_id, "state": "WAITING_CAPACITY", "worker_id": None, "detail": "Global assignment limit reached."})
            continue
        if not eligible:
            rows.append({"task_id": task_id, "state": "WAITING_NO_ELIGIBLE_WORKER", "worker_id": None, "detail": "No active worker satisfies capabilities, path scope and concurrency limits."})
            continue
        worker = min(eligible, key=lambda w: (active_counts[w["worker_id"]], w["worker_id"]))
        worker_id = worker["worker_id"]
        active_counts[worker_id] += 1
        assignment = {
            "task_id": task_id,
            "worker_id": worker_id,
            "required_capabilities": task["required_capabilities"],
            "allowed_paths": task["allowed_paths"],
            "isolated_branch_required": True,
            "base_ref": "energun-metal-core",
            "auto_merge": False,
            "physical_device_access": False,
        }
        assignments.append(assignment)
        rows.append({"task_id": task_id, "state": "ASSIGNMENT_PROPOSED", "worker_id": worker_id, "detail": "Dry-run proposal only; no worker was invoked."})

    if not active:
        status = "NO_WORKERS_REGISTERED"
    elif assignments:
        status = "DRY_RUN_ASSIGNMENTS_PROPOSED"
    else:
        status = "NO_ELIGIBLE_ASSIGNMENTS"
    return {
        "planner_id": "ENERGUN-FM1-CP8-DISPATCH",
        "mode": "dry-run",
        "status": status,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "registered_workers": len(workers),
        "active_workers": len(active),
        "max_parallel_assignments": policy_limit,
        "assignments": assignments,
        "tasks": rows,
        "side_effects": {"workers_invoked": False, "branches_created": False, "files_modified": False, "pull_requests_created": False},
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--registry", type=Path, default=REGISTRY)
    parser.add_argument("--tasks", type=Path, default=TASKS)
    parser.add_argument("--output", type=Path, default=OUT)
    args = parser.parse_args()
    try:
        plan = build_dispatch_plan(read_json(args.registry), read_json(args.tasks))
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(plan, indent=2) + "\\n", encoding="utf-8")
    except (OSError, json.JSONDecodeError, ValueError) as exc:
        print(f"CP8_DISPATCH_VALIDATION_FAILED={exc}")
        return 1
    print(f"CP8_DISPATCH_STATUS={plan['status']}")
    print(f"CP8_DISPATCH_REGISTERED_WORKERS={plan['registered_workers']}")
    print(f"CP8_DISPATCH_PROPOSED_ASSIGNMENTS={len(plan['assignments'])}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
