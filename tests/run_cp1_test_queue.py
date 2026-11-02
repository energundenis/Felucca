#!/usr/bin/env python3
"""Run the ENERGUN FM-1 CP1 persistence test queue in dependency order.

Existing Felucca tests are consumed from the preceding fast-regression result
file. New stages are enabled only when their real executable test command is
added to tests/cp1_test_queue.json in the same change as implementation.
A missing/unimplemented stage is reported BLOCKED, never PASS.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import sys
from datetime import datetime, timezone

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "tests" / "cp1_test_queue.json"
OUT = ROOT / "build" / "cp1-test-queue"
OUT.mkdir(parents=True, exist_ok=True)


def write_report(status: str, rows: list[dict], blocked_at: str | None = None) -> None:
    report = {
        "queue_id": "ENERGUN-FM1-CP1",
        "status": status,
        "blocked_at": blocked_at,
        "generated_utc": datetime.now(timezone.utc).isoformat(),
        "stages": rows,
    }
    (OUT / "report.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    lines = [
        "# ENERGUN FM-1 CORE — CP1 automated test queue",
        "",
        f"**Queue result: {status}**",
        "",
        "Stages execute sequentially. A stage is PASS only when its test command or required CI evidence succeeds.",
        "BLOCKED means implementation/test coverage is not available yet; it is not a passing test.",
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
        "This queue uses host-side simulated Flash and CI evidence only. It does not connect to, write, flash, erase, or modify a physical FM-1. It does not authorize bootloader, OTA, recovery, or physical Flash-layout changes.",
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
            f.write(f"blocked_at={blocked_at or ''}\n")


def main() -> int:
    try:
        manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    except Exception as exc:
        print(f"::error::Cannot read CP1 queue manifest: {exc}")
        return 1

    stages = manifest.get("stages")
    if manifest.get("schema_version") != 1 or not isinstance(stages, list) or not stages:
        print("::error::Invalid CP1 queue manifest/schema.")
        return 1

    known: set[str] = set()
    results: dict[str, str] = {}
    rows: list[dict] = []
    blocked_at = None
    overall = "PASS"

    for stage in stages:
        sid = stage.get("id")
        title = stage.get("title", sid or "unnamed stage")
        if not sid or sid in known:
            print(f"::error::Missing or duplicate stage id: {sid}")
            return 1
        deps = stage.get("depends_on", [])
        if any(dep not in known for dep in deps):
            print(f"::error::Stage {sid} has a missing or forward dependency: {deps}")
            return 1
        known.add(sid)

        failed_dep = next((dep for dep in deps if results.get(dep) != "PASS"), None)
        if failed_dep:
            row = {"id": sid, "title": title, "result": "BLOCKED", "detail": f"Dependency {failed_dep} did not pass."}
            rows.append(row)
            results[sid] = "BLOCKED"
            blocked_at = blocked_at or sid
            overall = "BLOCKED"
            break

        state = stage.get("state")
        if state == "evidence":
            evidence_path = ROOT / stage.get("evidence_file", "")
            needle = stage.get("evidence_contains", "")
            if not evidence_path.is_file():
                detail = f"Required CI evidence file is missing: {stage.get('evidence_file')}"
                row = {"id": sid, "title": title, "result": "FAIL", "detail": detail}
                rows.append(row)
                results[sid] = "FAIL"
                overall = "FAIL"
                blocked_at = sid
                break
            evidence = evidence_path.read_text(encoding="utf-8", errors="replace")
            if not needle or needle not in evidence:
                detail = f"Required passing evidence not found: {needle}"
                row = {"id": sid, "title": title, "result": "FAIL", "detail": detail}
                rows.append(row)
                results[sid] = "FAIL"
                overall = "FAIL"
                break
            detail = f"Verified from this run: {needle}"
            row = {"id": sid, "title": title, "result": "PASS", "detail": detail}
            rows.append(row)
            results[sid] = "PASS"
            print(f"PASS: {sid} — {detail}")
            continue

        if state != "ready":
            detail = stage.get("blocker", "Executable acceptance test is not enabled.")
            row = {"id": sid, "title": title, "result": "BLOCKED", "detail": detail}
            rows.append(row)
            results[sid] = "BLOCKED"
            blocked_at = sid
            overall = "BLOCKED"
            print(f"::warning::CP1 queue blocked at {sid}: {detail}")
            break

        command = stage.get("command")
        if not isinstance(command, str) or not command.strip():
            print(f"::error::Stage {sid} is marked ready but has no executable command.")
            return 1
        print(f"== CP1 queue stage: {sid}")
        proc = subprocess.run(command, cwd=ROOT, shell=True, check=False)
        if proc.returncode != 0:
            detail = f"Command failed with exit code {proc.returncode}: {command}"
            row = {"id": sid, "title": title, "result": "FAIL", "detail": detail}
            rows.append(row)
            results[sid] = "FAIL"
            overall = "FAIL"
            blocked_at = sid
            print(f"::error::CP1 queue stage {sid} failed.")
            break
        row = {"id": sid, "title": title, "result": "PASS", "detail": f"Command passed: {command}"}
        rows.append(row)
        results[sid] = "PASS"
        print(f"PASS: {sid}")

    # Include every later stage in the report as BLOCKED after the first
    # missing prerequisite/test or failure; never silently omit queued work.
    if len(rows) < len(stages):
        halt = blocked_at or (rows[-1]["id"] if rows else "manifest-validation")
        for stage in stages[len(rows):]:
            rows.append({
                "id": stage.get("id", "unknown"),
                "title": stage.get("title", "unnamed stage"),
                "result": "BLOCKED",
                "detail": f"Queue stopped at {halt}; this dependent stage was not executed.",
            })

    write_report(overall, rows, blocked_at)
    print(f"CP1_QUEUE_STATUS={overall}")
    if overall == "BLOCKED":
        print("Queue orchestration completed; later dependent stages were intentionally not run.")
        return 0
    return 0 if overall == "PASS" else 1


if __name__ == "__main__":
    sys.exit(main())
