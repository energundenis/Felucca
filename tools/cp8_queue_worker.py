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
    DEFAULT_BRANCH, GitHubGitDataClient, GitRefCASQueueStore, QueueCASConflict,
    QueueStoreError,
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
    client = GitHubGitDataClient(env("GITHUB_REPOSITORY"), env("GITHUB_TOKEN"))
    branch = os.environ.get("CP8_QUEUE_BRANCH", DEFAULT_BRANCH)
    try:
        client.read_ref(branch)
    except QueueStoreError:
        source_sha = env("GITHUB_SHA")
        try:
            client.request("POST", "/git/refs", {
                "ref": "refs/heads/" + branch,
                "sha": source_sha,
            })
        except QueueCASConflict:
            # Another enqueue run initialized it concurrently.
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


def drain(lane: str) -> None:
    store, _ = make_store()
    run_id = int(env("GITHUB_RUN_ID"))
    run_url = (
        f"{env('GITHUB_SERVER_URL')}/{env('GITHUB_REPOSITORY')}/actions/runs/{run_id}"
    )
    if lane not in ALLOWED_COMMANDS:
        raise RuntimeError(f"lane is not allow-listed: {lane}")
    max_items = int(os.environ.get("CP8_MAX_ITEMS_PER_RUN", "2"))
    lane_runner_source = (ROOT / "tools" / "cp8_worker_lanes.py").read_text(encoding="utf-8")
    lane_runner_path = Path("/tmp/energun_cp8_worker_lanes.py")
    lane_runner_path.write_text(lane_runner_source, encoding="utf-8")
    processed = 0
    while processed < max_items:
        record = store.claim_next(f"cp8-{lane}-worker", run_id, lease_seconds=3600, required_lane=lane)
        if record is None:
            print("queue_empty=true")
            break
        request_id = record["request_id"]
        sha = record["commit_sha"]
        lanes = record["requested_lanes"]
        if lanes != [lane]:
            raise RuntimeError(f"lane isolation violation: expected {[lane]}, got {lanes}")
        print(f"claimed request={request_id} sha={sha} lane={lane}")
        try:
            store.mark_running(request_id, run_id)
            checkout_sha(sha)
            # Keep the coordinator-approved harness outside the tested checkout.
            command = ALLOWED_COMMANDS[lane]
            child_env = os.environ.copy()
            child_env["CP8_WORKSPACE"] = str(ROOT)
            print(f"RUN lane={lane} command={' '.join(command)}")
            subprocess.run(command, cwd=ROOT, env=child_env, check=True)
            store.finish(
                request_id, run_id, "passed",
                "All allow-listed queue lanes passed.",
                evidence=[run_url], tested_sha=sha,
            )
            print(f"PASS request={request_id} sha={sha}")
        except Exception as exc:
            store.finish(
                request_id, run_id, "failed",
                f"{type(exc).__name__}: {str(exc)[:350]}",
                now=None,
            )
            print(f"FAIL request={request_id}: {type(exc).__name__}: {exc}")
            raise
        processed += 1
    print(f"processed_requests={processed}")


if __name__ == "__main__":
    if len(sys.argv) == 2 and sys.argv[1] == "enqueue":
        enqueue()
    elif len(sys.argv) == 3 and sys.argv[1] == "drain" and sys.argv[2] in ALLOWED_COMMANDS:
        drain(sys.argv[2])
    else:
        raise SystemExit("usage: cp8_queue_worker.py enqueue|drain LANE")
