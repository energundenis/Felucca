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


def enqueue() -> None:
    store, _ = make_store()
    result = store.enqueue(
        repository=env("GITHUB_REPOSITORY"),
        lanes=["fast-regression"],
        ref=env("GITHUB_REF"),
        commit_sha=env("GITHUB_SHA"),
        event=os.environ.get("GITHUB_EVENT_NAME", "push"),
        workflow_run_id=int(env("GITHUB_RUN_ID")),
    )
    record = result["record"]
    print(f"queue_request_id={record['request_id']}")
    print(f"queue_created={str(result['created']).lower()}")
    print(f"queue_state={record['state']}")


def checkout_sha(sha: str) -> None:
    subprocess.run(["git", "fetch", "--no-tags", "origin", sha], cwd=ROOT, check=True)
    subprocess.run(["git", "checkout", "--detach", sha], cwd=ROOT, check=True)
    subprocess.run(["git", "submodule", "update", "--init", "--recursive"], cwd=ROOT, check=True)


def drain() -> None:
    store, _ = make_store()
    run_id = int(env("GITHUB_RUN_ID"))
    run_url = (
        f"{env('GITHUB_SERVER_URL')}/{env('GITHUB_REPOSITORY')}/actions/runs/{run_id}"
    )
    max_items = int(os.environ.get("CP8_MAX_ITEMS_PER_RUN", "4"))
    processed = 0
    while processed < max_items:
        record = store.claim_next("cp8-trusted-coordinator", run_id, lease_seconds=3600)
        if record is None:
            print("queue_empty=true")
            break
        request_id = record["request_id"]
        sha = record["commit_sha"]
        lanes = record["requested_lanes"]
        print(f"claimed request={request_id} sha={sha} lanes={','.join(lanes)}")
        try:
            store.mark_running(request_id, run_id)
            checkout_sha(sha)
            for lane in lanes:
                command = ALLOWED_COMMANDS.get(lane)
                if command is None:
                    raise RuntimeError(f"lane is not allow-listed: {lane}")
                print(f"RUN lane={lane} command={' '.join(command)}")
                subprocess.run(command, cwd=ROOT, check=True)
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
    if len(sys.argv) != 2 or sys.argv[1] not in {"enqueue", "drain"}:
        raise SystemExit("usage: cp8_queue_worker.py enqueue|drain")
    {"enqueue": enqueue, "drain": drain}[sys.argv[1]]()
