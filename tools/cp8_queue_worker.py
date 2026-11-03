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
        # Initialize the ledger branch once from the exact trusted source SHA.
        # A competing initializer wins by non-fast-forward push; the loser fetches it.
        source_sha = env("GITHUB_SHA")
        try:
            client.commit_ledger(source_sha, {"schema_version": 1, "records": [], "workers": {}}, branch)
        except QueueCASConflict:
            client.read_ref(branch)
    return GitRefCASQueueStore(client, branch=branch), branch

