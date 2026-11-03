#!/usr/bin/env python3
"""Trusted CP8 queue worker. Executes only fixed, repository-owned lanes."""
from __future__ import annotations

import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tests"))
from cp8_ci_queue_store import (  # noqa: E402
    DEFAULT_BRANCH, GitCliQueueClient, GitRefCASQueueStore, QueueCASConflict,
    QueueStoreError, QueueRefNotFound,
)
from cp8_worker_runtime import (  # noqa: E402
    claim_next_for_worker, finish_worker_task, mark_worker_running, register_worker, release_worker,
)

ALLOWED_COMMANDS = {
    "fast-regression": ["sh", "tests/run_metal_core_fast.sh"],
    "cp1-persistence": ["python3", "/tmp/energun_cp8_worker_lanes.py", "cp1-persistence"],
    "cp2-dsp-vca": ["python3", "/tmp/energun_cp8_worker_lanes.py", "cp2-dsp-vca"],
    "cp3-sequencer": ["python3", "/tmp/energun_cp8_worker_lanes.py", "cp3-sequencer"],
}


def env(name: str) -> str:
    value = os.environ.get(name, "")
    if not value:
        raise RuntimeError(f"required environment variable is missing: {name}")
    return value


def make_store():
    client = GitCliQueueClient(remote="origin", repo_dir=str(ROOT))
    branch = os.environ.get("CP8_QUEUE_BRANCH", DEFAULT_BRANCH)
    try:
        client.read_ref(branch)
    except QueueRefNotFound:
        # Initialize once from the trusted source SHA; concurrent initializers are fenced by push.
        source_sha = env("GITHUB_SHA")
        try:
            client.commit_ledger(source_sha, {"schema_version": 1, "records": [], "workers": {}}, branch)
        except QueueCASConflict:
            client.read_ref(branch)
    return GitRefCASQueueStore(client, branch=branch), branch


def detect_lanes() -> list[str]:
    event = os.environ.get("GITHUB_EVENT_NAME", "push")
    all_lanes = ["cp1-persistence", "cp2-dsp-vca", "cp3-sequencer"]
    if event == "workflow_dispatch":
        return all_lanes
    if event == "schedule":
        return []
    event_path = os.environ.get("GITHUB_EVENT_PATH", "")
    before = ""
    if event_path and Path(event_path).is_file():
        import json
        payload = json.loads(Path(event_path).read_text(encoding="utf-8"))
        before = payload.get("before", "")
    head = env("GITHUB_SHA")
    if not before or before == "0" * 40:
        return all_lanes
    changed = subprocess.run(
        ["git", "diff", "--name-only", before, head],
        cwd=ROOT, check=True, capture_output=True, text=True,
    ).stdout.splitlines()
    selected: set[str] = set()
    known = False
    for path in changed:
        name = Path(path).name
        if path.startswith("docs/") or name.endswith(".md"):
            known = True
        elif path.startswith(("firmware/hal/", "firmware/bsp/")) or path in {".gitmodules", "tools/build.py"}:
            selected.update(all_lanes)
            known = True
        elif path.startswith("firmware/src/") and any(token in name for token in ("storage", "sidecar", "pair_txn", "fm6bank")):
            selected.add("cp1-persistence")
            known = True
        elif path.startswith("tests/") and (name in {"storage_test.c", "fm6bank_test.c"} or name.startswith(("pair_txn", "pair_storage", "cp1_"))):
            selected.add("cp1-persistence")
            known = True
        elif path.startswith(("firmware/src/", "tests/")) and (name.startswith(("eng_metal", "metal_core", "metalcore")) or name == "run_metal_core_fast.sh"):
            selected.add("cp2-dsp-vca")
            known = True
        elif path.startswith(("firmware/src/", "tests/")) and name.startswith(("chord", "ratchet", "step15", "seq", "swing", "sequencer")):
            selected.add("cp3-sequencer")
            known = True
        elif path.startswith(".github/workflows/"):
            selected.update(all_lanes)
            known = True
        else:
            # Unknown source paths are treated conservatively.
            selected.update(all_lanes)
    if changed and not known and not selected:
        selected.update(all_lanes)
    return [lane for lane in all_lanes if lane in selected]


def enqueue() -> None:
    store, _ = make_store()
    lanes = detect_lanes()
    print(f"detected_lanes={','.join(lanes) if lanes else 'none'}")
    for lane in lanes:
        result = store.enqueue(
            repository=env("GITHUB_REPOSITORY"),
            lanes=[lane],
            ref=env("GITHUB_REF"),
            commit_sha=env("GITHUB_SHA"),
            event=os.environ.get("GITHUB_EVENT_NAME", "push"),
            workflow_run_id=int(env("GITHUB_RUN_ID")),
        )
        record = result["record"]
        print(f"queue_lane={lane}")
        print(f"queue_request_id={record['request_id']}")
        print(f"queue_created={str(result['created']).lower()}")
        print(f"queue_state={record['state']}")


def checkout_sha(sha: str) -> None:
    subprocess.run(["git", "fetch", "--no-tags", "origin", sha], cwd=ROOT, check=True)
    subprocess.run(["git", "checkout", "--detach", sha], cwd=ROOT, check=True)
    subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=ROOT, check=True)


def completed_owner_workers(store) -> set[tuple[str, int]]:
    """Find completed lane jobs, even while sibling jobs in the run are active."""
    head = store.client.read_ref(store.branch)
    ledger = store.client.read_ledger_at(head)
    owners = {
        (item.get("claim", {}).get("owner_workflow"),
         item.get("claim", {}).get("owner_run_id"))
        for item in ledger.get("records", [])
        if item.get("state") in {"claimed", "running"}
    }
    expected_names = {
        "cp8-cp1-persistence-run-": "Queue Worker CP1 — persistence",
        "cp8-cp1-persistence-worker": "Queue Worker CP1 — persistence",
        "cp8-cp2-dsp-vca-run-": "Queue Worker CP2 — DSP/VCA",
        "cp8-cp2-dsp-vca-worker": "Queue Worker CP2 — DSP/VCA",
        "cp8-cp3-sequencer-run-": "Queue Worker CP3 — sequencer",
        "cp8-cp3-sequencer-worker": "Queue Worker CP3 — sequencer",
    }
    completed: set[tuple[str, int]] = set()
    for worker_id, owner_run_id in owners:
        job_name = next((name for prefix, name in expected_names.items()
                         if isinstance(worker_id, str) and worker_id.startswith(prefix)), None)
        if job_name is None or not isinstance(owner_run_id, int):
            continue
        try:
            payload = store.client.request("GET", f"/actions/runs/{owner_run_id}/jobs?per_page=100")
            for job in payload.get("jobs", []):
                if job.get("name") == job_name and job.get("status") == "completed":
                    completed.add((worker_id, owner_run_id))
                    break
        except QueueStoreError as exc:
            print(f"WARNING: unable to verify worker job {worker_id} in run {owner_run_id}: {exc}", flush=True)
    return completed

def load_worker_dag() -> dict[str, list[str]]:
    import json
    path = ROOT / "tests" / "cp8_worker_dag.json"
    payload = json.loads(path.read_text(encoding="utf-8"))
    tasks = payload.get("tasks")
    if payload.get("schema_version") != 1 or not isinstance(tasks, list):
        raise RuntimeError("worker DAG schema is invalid")
    graph = {}
    for item in tasks:
        task_id = item.get("task_id")
        deps = item.get("depends_on")
        if task_id not in ALLOWED_COMMANDS or not isinstance(deps, list):
            raise RuntimeError(f"invalid worker DAG task: {task_id!r}")
        if task_id in graph or any(dep not in ALLOWED_COMMANDS for dep in deps):
            raise RuntimeError(f"duplicate task or unknown DAG dependency: {task_id!r}")
        graph[task_id] = deps
    if set(graph) != {"cp1-persistence", "cp2-dsp-vca", "cp3-sequencer"} or not 1 <= payload.get("max_parallel_workers", 0) <= 3:
        raise RuntimeError("worker DAG must declare all lanes and a parallel limit from 1 to 3")
    remaining = {key: set(value) for key, value in graph.items()}
    while remaining:
        ready = [key for key, deps in remaining.items() if not deps]
        if not ready:
            raise RuntimeError(f"worker DAG contains a cycle: {sorted(remaining)}")
        for key in ready: del remaining[key]
        for deps in remaining.values(): deps.difference_update(ready)
    return graph


def drain(lane: str) -> None:
    store, _ = make_store()
    run_id = int(env("GITHUB_RUN_ID"))
    run_url = f"{env('GITHUB_SERVER_URL')}/{env('GITHUB_REPOSITORY')}/actions/runs/{run_id}"
    if lane not in ALLOWED_COMMANDS: raise RuntimeError(f"lane is not allow-listed: {lane}")
    max_items = int(os.environ.get("CP8_MAX_ITEMS_PER_RUN", "2"))
    dag = load_worker_dag()
    worker_id = f"cp8-{lane}-run-{run_id}"
    register_worker(store, worker_id, run_id, lane, ["test-automation", lane], max_concurrency=1)
    lane_runner_source = (ROOT / "tools" / "cp8_worker_lanes.py").read_text(encoding="utf-8")
    Path("/tmp/energun_cp8_worker_lanes.py").write_text(lane_runner_source, encoding="utf-8")
    processed = 0
    try:
        while processed < max_items:
            recover_owners = completed_owner_workers(store)
            record = claim_next_for_worker(store, worker_id, run_id, dag,
                completed_owner_workers=recover_owners, lease_seconds=5400)
            if record is None:
                print("queue_empty_or_waiting_dependency_or_capacity=true")
                break
            request_id, sha = record["request_id"], record["commit_sha"]
            lanes = record["requested_lanes"]
            if lanes != [lane]: raise RuntimeError(f"lane isolation violation: expected {[lane]}, got {lanes}")
            print(f"claimed request={request_id} sha={sha} lane={lane} attempt={record['attempts']}")
            try:
                mark_worker_running(store, request_id, worker_id, run_id)
                checkout_sha(sha)
                command = ALLOWED_COMMANDS[lane]
                child_env = os.environ.copy()
                child_env["CP8_WORKSPACE"] = str(ROOT)
                print(f"RUN lane={lane} command={' '.join(command)}")
                subprocess.run(command, cwd=ROOT, env=child_env, check=True)
                finish_worker_task(store, request_id, worker_id, run_id, "passed",
                    "Allow-listed lane passed on the exact requested SHA.",
                    evidence=[run_url], tested_sha=sha)
                print(f"PASS request={request_id} sha={sha}")
            except Exception as exc:
                try:
                    finish_worker_task(store, request_id, worker_id, run_id, "failed",
                        f"{type(exc).__name__}: {str(exc)[:350]}")
                except QueueStoreError as finish_error:
                    print(f"WARNING: task result could not be committed: {finish_error}", flush=True)
                print(f"FAIL request={request_id}: {type(exc).__name__}: {exc}")
                raise
            processed += 1
    finally:
        try: release_worker(store, worker_id, run_id)
        except QueueStoreError as exc:
            print(f"WORKER_RETAINED_FOR_RECOVERY={exc}", flush=True)
    print(f"processed_requests={processed}")

if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "enqueue":
        enqueue()
    elif len(sys.argv) == 3 and sys.argv[1] == "drain" and sys.argv[2] in ALLOWED_COMMANDS:
        drain(sys.argv[2])
    else:
        raise SystemExit("usage: cp8_queue_worker.py enqueue|drain LANE")
