#!/usr/bin/env python3
"""Run the ENERGUN FM-1 CP1 persistence queue as a dependency-aware DAG.

Independent ready stages run concurrently. A stage starts only after every
dependency passes. A blocked/failed stage blocks its descendants, not unrelated
branches. The queue is host-only and never accesses a physical FM-1.
"""
from __future__ import annotations

from concurrent.futures import FIRST_COMPLETED, Future, ThreadPoolExecutor, wait
import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tests" / "cp1_test_queue.json"
OUT = ROOT / "build" / "cp1-test-queue"


def _evidence(stage: dict) -> tuple[str, str]:
    evidence_path = ROOT / stage.get("evidence_file", "")
    needle = stage.get("evidence_contains", "")
    if not evidence_path.is_file():
        return "FAIL", f"Required CI evidence file is missing: {stage.get('evidence_file')}"
    evidence = evidence_path.read_text(encoding="utf-8", errors="replace")
    if not needle or needle not in evidence:
        return "FAIL", f"Required passing evidence not found: {needle}"
    return "PASS", f"Verified from this run: {needle}"


def _run_command(stage: dict) -> tuple[str, str]:
    command = stage.get("command")
    if not isinstance(command, str) or not command.strip():
        return "FAIL", "Stage is ready but has no executable command."
    print(f"== CP1 parallel stage start: {stage['id']}", flush=True)
    proc = subprocess.run(command, cwd=ROOT, shell=True, check=False)
    if proc.returncode != 0:
        return "FAIL", f"Command failed with exit code {proc.returncode}: {command}"
    print(f"PASS: {stage['id']}", flush=True)
    return "PASS", f"Command passed: {command}"


def write_report(status: str, rows: list[dict], max_parallel: int) -> None:
    OUT.mkdir(parents=True, exist_ok=True)
    report = {
        "queue_id": "ENERGUN-FM1-CP1",
        "status": status,
        "execution_mode": "dependency-aware-parallel",
        "max_parallel": max_parallel,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "stages": rows,
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# ENERGUN FM-1 CORE — CP1 automated parallel test queue",
        "",
        f"**Queue result: {status}**",
        "",
        f"**Execution:** dependency-aware parallel; max workers: {max_parallel}.",
        "Independent ready stages run concurrently. Dependent stages wait for all prerequisites.",
        "A blocked/failed stage blocks its descendants but does not stop unrelated branches.",
        "",
        "| # | Stage | Result | Detail |",
        "|---:|---|---|---|",
    ]
    for i, row in enumerate(rows, 1):
        detail = (row.get("detail") or "").replace("|", "\\|").replace("\n", " ")
        lines.append(f"| {i} | {row['title']} | **{row['result']}** | {detail} |")
    lines += [
        "",
        "## Safety boundary",
        "",
        "Host-side tests and simulated Flash only. No physical FM-1 access/write, Flash allocation, bootloader, OTA or recovery action is performed or authorized.",
        "",
    ]
    markdown = "\n".join(lines)
    (OUT / "report.md").write_text(markdown, encoding="utf-8")
    summary = os.environ.get("GITHUB_STEP_SUMMARY")
    if summary:
        with open(summary, "a", encoding="utf-8") as f:
            f.write(markdown + "\n")
    output = os.environ.get("GITHUB_OUTPUT")
    if output:
        with open(output, "a", encoding="utf-8") as f:
            f.write(f"queue_status={status.lower()}\n")


def _validate(stages: list[dict]) -> tuple[dict[str, dict], list[str]]:
    by_id: dict[str, dict] = {}
    for stage in stages:
        sid = stage.get("id")
        if not isinstance(sid, str) or not sid or sid in by_id:
            raise ValueError(f"Missing or duplicate stage id: {sid}")
        by_id[sid] = stage
    for sid, stage in by_id.items():
        deps = stage.get("depends_on", [])
        if not isinstance(deps, list) or any(dep not in by_id for dep in deps):
            raise ValueError(f"Stage {sid} has an unknown dependency: {deps}")
        if sid in deps:
            raise ValueError(f"Stage {sid} depends on itself.")
        if stage.get("state") not in ("evidence", "ready", "blocked"):
            raise ValueError(f"Stage {sid} has invalid state: {stage.get('state')}")
    # Kahn cycle check; dependencies may appear later in the manifest.
    remaining = {sid: set(stage.get("depends_on", [])) for sid, stage in by_id.items()}
    order: list[str] = []
    while remaining:
        ready = [sid for sid, deps in remaining.items() if not deps]
        if not ready:
            raise ValueError(f"Dependency cycle detected among: {sorted(remaining)}")
        for sid in ready:
            order.append(sid)
            del remaining[sid]
        for deps in remaining.values():
            deps.difference_update(ready)
    return by_id, order


def main() -> int:
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
        stages = manifest.get("stages")
        if manifest.get("schema_version") != 1 or not isinstance(stages, list) or not stages:
            raise ValueError("Invalid CP1 queue manifest/schema.")
        by_id, topo_order = _validate(stages)
    except Exception as exc:
        print(f"::error::Cannot validate CP1 queue manifest: {exc}")
        return 1

    max_parallel = int(manifest.get("max_parallel", 3))
    if max_parallel < 1 or max_parallel > 8:
        print("::error::max_parallel must be between 1 and 8.")
        return 1

    results: dict[str, str] = {}
    details: dict[str, str] = {}
    running: dict[Future, str] = {}
    pending = set(by_id)
    stop_after_failure = bool(manifest.get("stop_on_first_failure", True))
    failure_seen = False

    with ThreadPoolExecutor(max_workers=max_parallel) as pool:
        while pending or running:
            progressed = False

            # Resolve dependency-blocked stages and synchronously verify evidence.
            for sid in topo_order:
                if sid not in pending:
                    continue
                stage = by_id[sid]
                deps = stage.get("depends_on", [])
                failed_dep = next((d for d in deps if results.get(d) == "FAIL"), None)
                blocked_dep = next((d for d in deps if results.get(d) == "BLOCKED"), None)
                if failed_dep or blocked_dep:
                    dep = failed_dep or blocked_dep
                    results[sid] = "BLOCKED"
                    details[sid] = f"Dependency {dep} did not pass."
                    pending.remove(sid)
                    progressed = True
                    continue
                if not all(results.get(d) == "PASS" for d in deps):
                    continue
                if failure_seen and stop_after_failure:
                    results[sid] = "BLOCKED"
                    details[sid] = "Not scheduled because stop_on_first_failure is enabled."
                    pending.remove(sid)
                    progressed = True
                    continue
                state = stage["state"]
                if state == "blocked":
                    results[sid] = "BLOCKED"
                    details[sid] = stage.get("blocker", "Implementation/test coverage is not available.")
                    pending.remove(sid)
                    print(f"::warning::CP1 stage blocked: {sid}: {details[sid]}", flush=True)
                    progressed = True
                elif state == "evidence":
                    result, detail = _evidence(stage)
                    results[sid], details[sid] = result, detail
                    pending.remove(sid)
                    if result == "FAIL":
                        failure_seen = True
                        print(f"::error::CP1 evidence failed: {sid}: {detail}", flush=True)
                    else:
                        print(f"PASS: {sid} — {detail}", flush=True)
                    progressed = True
                elif state == "ready":
                    pending.remove(sid)
                    running[pool.submit(_run_command, stage)] = sid
                    progressed = True

            if running:
                done, _ = wait(tuple(running), return_when=FIRST_COMPLETED)
                for future in done:
                    sid = running.pop(future)
                    try:
                        result, detail = future.result()
                    except Exception as exc:
                        result, detail = "FAIL", f"Runner exception: {exc}"
                    results[sid], details[sid] = result, detail
                    if result == "FAIL":
                        failure_seen = True
                        print(f"::error::CP1 stage failed: {sid}: {detail}", flush=True)
                continue

            if pending and not progressed:
                # All remaining nodes are waiting on a dependency state that
                # cannot be resolved; this is a defensive scheduler deadlock.
                for sid in list(pending):
                    results[sid] = "BLOCKED"
                    details[sid] = "Scheduler could not resolve dependencies."
                    pending.remove(sid)

    rows = [
        {
            "id": sid,
            "title": by_id[sid].get("title", sid),
            "result": results.get(sid, "BLOCKED"),
            "detail": details.get(sid, "No result recorded."),
        }
        for sid in by_id
    ]
    overall = "FAIL" if "FAIL" in results.values() else (
        "BLOCKED" if "BLOCKED" in results.values() else "PASS"
    )
    write_report(overall, rows, max_parallel)
    print(f"CP1_QUEUE_STATUS={overall}")
    print(f"CP1_QUEUE_MAX_PARALLEL={max_parallel}")
    return 1 if overall == "FAIL" else 0


if __name__ == "__main__":
    sys.exit(main())
