#!/usr/bin/env python3
"""Deterministic, allow-listed CP8 host-test lanes."""
from __future__ import annotations
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(os.environ.get("CP8_WORKSPACE", Path(__file__).resolve().parents[1]))


def run(args: list[str]) -> None:
    print("+", " ".join(args), flush=True)
    subprocess.run(args, cwd=ROOT, check=True)


def cp1() -> None:
    run(["python3", "-c", 'import runpy; runpy.run_path("tools/build.py")["generate"]()'])
    out = ROOT / "build" / "cp8-cp1"
    out.mkdir(parents=True, exist_ok=True)
    evidence_dir = ROOT / "build" / "metal-core-fast"
    evidence_dir.mkdir(parents=True, exist_ok=True)
    evidence_file = evidence_dir / "results.txt"
    evidence_file.write_text("", encoding="utf-8")
    include = ["-Ibuild/gen", "-Ifirmware/hal", "-Ifirmware/bsp/chip",
               "-Ifirmware/bsp/boards/fm1", "-Ifirmware/src"]
    for name, source in (("storage", "tests/storage_test.c"),
                         ("fm6bank", "tests/fm6bank_test.c")):
        run(["cc", "-O2", "-w", *include, "-o", str(out / name), source, "-lm"])
        run([str(out / name)])
        with evidence_file.open("a", encoding="utf-8") as evidence:
            evidence.write(f"PASS: {name}\\n")
    run(["sh", "tests/run_pair_txn_test.sh"])
    run(["sh", "tests/run_pair_storage_adapter_test.sh"])
    run(["python3", "tests/run_cp1_test_queue.py"])


def cp2() -> None:
    run(["sh", "tests/run_metal_core_fast.sh"])


def cp3() -> None:
    run(["python3", "-c", 'import runpy; runpy.run_path("tools/build.py")["generate"]()'])
    out = ROOT / "build" / "cp8-cp3"
    out.mkdir(parents=True, exist_ok=True)
    include = ["-Ibuild/gen", "-Ifirmware/hal", "-Ifirmware/bsp/chip",
               "-Ifirmware/bsp/boards/fm1", "-Ifirmware/src"]
    for name in ("chord", "ratchet", "step15", "seq155", "swing"):
        run(["cc", "-O1", "-w", *include, "-o", str(out / f"{name}_test"),
             f"tests/{name}_test.c", "-lm"])
        run([str(out / f"{name}_test")])


if __name__ == "__main__":
    lanes = {"cp1-persistence": cp1, "cp2-dsp-vca": cp2, "cp3-sequencer": cp3}
    if len(sys.argv) != 2 or sys.argv[1] not in lanes:
        raise SystemExit("usage: cp8_worker_lanes.py cp1-persistence|cp2-dsp-vca|cp3-sequencer")
    lanes[sys.argv[1]]()
