#!/usr/bin/env python3
"""Validate and plan the ENERGUN FM-1 CP0-CP7 development DAG.

This first scheduler increment is deliberately plan-only: it never launches
builds, edits source, merges branches, or accesses a physical FM-1.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tests" / "cp0_cp7_development_orchestrator.json"
OUT = ROOT / "build" / "development-orchestrator"
VALID_STATES = {"planned", "ready", "running", "checking", "passed", "blocked", "failed"}


def load_and_validate(path: Path = MANIFEST) -> tuple[dict, list[str]]:
    manifest = json.loads(path.read_text(encoding="utf-8"))
    if manifest.get("schema_version") != 1:
        raise ValueError("Unsupported orchestrator schema_version.")
    tasks = manifest.get("workstreams")
    if not isinstance(tasks, list) or not tasks:
        raise ValueError("workstreams must be a non-empty array.")
    by_id: dict[str, dict] = {}
    for task in tasks:
        task_id = task.get("id")
        if not isinstance(task_id, str) or not task_id or task_id in by_id:
            raise ValueError(f"Missing or duplicate workstream id: {task_id}")
        if task.get("state") not in VALID_STATES:
            raise ValueError(f"{task_id} has invalid state: {task.get('state')}")
        deps = task.get("depends_on", [])
        if not isinstance(deps, list) or any(not isinstance(dep, str) for dep in deps):
            raise ValueError(f"{task_id} has invalid depends_on list.")
        by_id[task_id] = task
    for task_id, task in by_id.items():
        unknown = [dep for dep in task.get("depends_on", []) if dep not in by_id]
        if unknown:
            raise ValueError(f"{task_id} references unknown dependencies: {unknown}")
        if task_id in task.get("depends_on", []):
            raise ValueError(f"{task_id} depends on itself.")
    remaining = {task_id: set(task.get("depends_on", [])) for task_id, task in by_id.items()}
    order: list[str] = []
    while remaining:
        ready = [task_id for task_id, deps in remaining.items() if not deps]
        if not ready:
            raise ValueError(f"Dependency cycle detected among: {sorted(remaining)}")
        for task_id in ready:
            order.append(task_id)
            del remaining[task_id]
        for deps in remaining.values():
            deps.difference_update(ready)
    return manifest, order


def read_cp1_status(report_path: Path | None) -> tuple[str | None, str | None]:
    if report_path is None or not report_path.is_file():
        return None, "CP1 queue report is not available in this run."
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as exc:
        return "FAIL", f"CP1 queue report could not be read: {exc}"
    if report.get("queue_id") != "ENERGUN-FM1-CP1":
        return "FAIL", "CP1 queue report has an unexpected queue_id."
    status = report.get("status")
    if status not in {"PASS", "BLOCKED", "FAIL"}:
        return "FAIL", f"CP1 queue report has an invalid status: {status!r}"
    return status, f"Imported explicit CP1 queue result: {status}."


def build_plan(manifest: dict, order: list[str], cp1_status: str | None = None, cp1_detail: str | None = None) -> dict:
    by_id = {task["id"]: task for task in manifest["workstreams"]}
    results: dict[str, str] = {}
    rows: list[dict] = []
    candidates: list[str] = []

    for task_id in order:
        task = by_id[task_id]
        state = task["state"]
        deps = task.get("depends_on", [])
        if task_id == "cp1-persistence" and cp1_status:
            state = {"PASS": "passed", "BLOCKED": "blocked", "FAIL": "failed"}[cp1_status]
        failed_dep = next((dep for dep in deps if results.get(dep) in {"FAILED", "BLOCKED"}), None)
        waiting_deps = [dep for dep in deps if results.get(dep) != "PASSED"]

        if state == "passed":
            status, detail = "PASSED", (cp1_detail if task_id == "cp1-persistence" and cp1_status == "PASS" else "Registry marks this gate passed; evidence must still be tied to the reported commit.")
        elif state == "failed":
            status, detail = "FAILED", (cp1_detail if task_id == "cp1-persistence" and cp1_status == "FAIL" else task.get("blocker", "Workstream is marked failed."))
        elif state == "blocked" or failed_dep:
            status = "BLOCKED"
            detail = (cp1_detail if task_id == "cp1-persistence" and cp1_status == "BLOCKED" else task.get("blocker") or f"Dependency {failed_dep} is not passed.")
        elif waiting_deps:
            status = "WAITING"
            detail = "Waiting for: " + ", ".join(waiting_deps)
        elif state == "planned":
            status, detail = "PLANNED", "Scope is registered but not ready for execution."
        elif state == "checking":
            status, detail = "CHECKING", "Evidence refresh or acceptance review is in progress."
        elif state == "running":
            status, detail = "RUNNING", "Workstream is already marked running by its owner."
        elif state == "ready" and not task.get("command"):
            status, detail = "READY_NO_EXECUTOR", "Dependencies passed, but no audited executable command is registered."
        else:
            status, detail = "READY", "Dependencies passed and an executable command is registered."
            candidates.append(task_id)

        results[task_id] = status
        rows.append({
            "id": task_id,
            "checkpoint": task["checkpoint"],
            "title": task["title"],
            "state": status,
            "depends_on": deps,
            "parallel_group": task.get("parallel_group"),
            "executor": task.get("executor"),
            "command_registered": bool(task.get("command")),
            "detail": detail,
        })

    max_parallel = int(manifest.get("execution_policy", {}).get("max_parallel", 1))
    if not 1 <= max_parallel <= 8:
        raise ValueError("execution_policy.max_parallel must be between 1 and 8.")
    dispatch = candidates[:max_parallel]
    return {
        "orchestrator_id": manifest.get("orchestrator_id"),
        "status": "BLOCKED" if any(row["state"] == "BLOCKED" for row in rows)
                  else "READY" if dispatch else "WAITING",
        "mode": "plan-only",
        "max_parallel": max_parallel,
        "dispatch_candidates": dispatch,
        "available_slots": max(0, max_parallel - len(dispatch)),
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "workstreams": rows,
    }


def write_report(plan: dict) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "report.json").write_text(json.dumps(plan, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# ENERGUN FM-1 CORE — CP0–CP7 Development Orchestrator",
        "",
        f"**Plan status: {plan['status']}**",
        "",
        f"**Mode:** {plan['mode']} · **Parallel limit:** {plan['max_parallel']}",
        f"**Dispatch candidates:** {', '.join(plan['dispatch_candidates']) or 'none'}",
        "",
        "| Checkpoint | Workstream | State | Executor | Detail |",
        "|---|---|---|---|---|",
    ]
    for row in plan["workstreams"]:
        detail = row["detail"].replace("|", "\\|").replace("\n", " ")
        lines.append(f"| {row['checkpoint']} | {row['title']} | **{row['state']}** | {row.get('executor') or '—'} | {detail} |")
    lines += [
        "",
        "## Execution boundary",
        "",
        "Plan-only mode. No task command was launched, no branch was merged, and no physical FM-1 or Flash was accessed.",
        "",
    ]
    report = "\n".join(lines)
    (OUT / "report.md").write_text(report, encoding="utf-8")
    summary = __import__("os").environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as stream:
            stream.write(report + "\n")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=MANIFEST)
    parser.add_argument("--cp1-report", type=Path, default=OUT.parent / "cp1-test-queue" / "report.json")
    args = parser.parse_args()
    try:
        manifest, order = load_and_validate(args.manifest)
        cp1_status, cp1_detail = read_cp1_status(args.cp1_report)
        plan = build_plan(manifest, order, cp1_status, cp1_detail)
        write_report(plan)
    except Exception as exc:
        print(f"::error::Development orchestrator validation failed: {exc}")
        return 1
    print(f"DEVELOPMENT_ORCHESTRATOR_STATUS={plan['status']}")
    print(f"DEVELOPMENT_ORCHESTRATOR_DISPATCH_CANDIDATES={len(plan['dispatch_candidates'])}")
    print(f"DEVELOPMENT_ORCHESTRATOR_MAX_PARALLEL={plan['max_parallel']}")
    print(f"DEVELOPMENT_ORCHESTRATOR_CP1_STATUS={cp1_status or 'NOT_AVAILABLE'}")
    return 1 if any(row['state'] == 'FAILED' for row in plan['workstreams']) else 0


if __name__ == "__main__":
    raise SystemExit(main())
