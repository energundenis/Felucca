#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Offline ENERGUN V0.3 JSON -> FM-1 SysEx dry-run planner.

This program never opens MIDI ports and never transmits anything.
It produces candidate TRACK_STEP (30) frames for review only.
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path
from statistics import median
from typing import Any

HEADER = [0x7D, 0x46, 0x4C]
CMD_TRACK_STEP = 30
NTRACK = 4
NSTEP = 64
SCALE_MAP = {
    "chromatic": 0,       # CHR
    "major": 1,            # MAJ
    "minor": 2,            # MIN
    "dorian": 3,           # DOR
    "harmonic": 7,         # HARM
    "phrygian": 8,         # PHRY
}


def js_round(value: float) -> int:
    """Match JavaScript Math.round for non-negative values."""
    return int(math.floor(value + 0.5))


def validate_pattern(data: dict[str, Any]) -> tuple[int, int, str, list[dict[str, Any]], float]:
    if data.get("schema") != "energun.fm1.pattern" or data.get("schemaVersion") != 1:
        raise ValueError("unsupported JSON schema; expected energun.fm1.pattern version 1")
    length = data.get("patternLength")
    if length not in (8, 16, 32):
        raise ValueError("patternLength must be 8, 16 or 32")
    root = data.get("rootNote")
    if not isinstance(root, int) or isinstance(root, bool) or not 0 <= root <= 11:
        raise ValueError("rootNote must be an integer from 0 to 11")
    scale = data.get("scale")
    if scale not in SCALE_MAP:
        raise ValueError(f"unsupported V0.3 scale: {scale!r}")
    steps = data.get("steps")
    if not isinstance(steps, list) or len(steps) != length:
        raise ValueError("steps must contain exactly patternLength entries")
    settings = data.get("settings")
    if not isinstance(settings, dict):
        raise ValueError("settings object is missing")
    accent_amount = settings.get("accentAmount")
    if not isinstance(accent_amount, (int, float)) or isinstance(accent_amount, bool) or not 0 <= accent_amount <= 1:
        raise ValueError("settings.accentAmount must be between 0 and 1")
    for i, step in enumerate(steps):
        if not isinstance(step, dict) or step.get("index") != i:
            raise ValueError(f"step {i}: missing or incorrect index")
        if not isinstance(step.get("on"), bool) or not isinstance(step.get("accent"), bool):
            raise ValueError(f"step {i}: on and accent must be booleans")
        note, octave, midi, gate = (step.get(k) for k in ("note", "octave", "midi", "gate"))
        if any(not isinstance(v, int) or isinstance(v, bool) for v in (note, octave, midi, gate)):
            raise ValueError(f"step {i}: note, octave, midi and gate must be integers")
        expected = 60 + root + note + 12 * octave
        if midi != expected or not 0 <= midi <= 127:
            raise ValueError(f"step {i}: MIDI note is inconsistent or outside 0..127")
        if not 10 <= gate <= 100:
            raise ValueError(f"step {i}: gate must be in the range 10..100")
    return length, root, scale, steps, float(accent_amount)


def make_step_args(track: int, index: int, step: dict[str, Any] | None,
                   accent_mode: str, accent_amount: float) -> list[int]:
    if step is None or not step["on"]:
        n, notes, step_time, flags, velocity = 0, [0, 0, 0, 0], 2, 0, 82
    else:
        n = 1
        notes = [step["midi"], 0, 0, 0]
        step_time = 0
        if accent_mode == "flag":
            flags, velocity = (1 if step["accent"] else 0), 82
        else:
            flags = 0
            velocity = js_round(80 + accent_amount * 47) if step["accent"] else 82
    # Full 1.5.5 TRACK_STEP write: track,index + step data + grid/chance/ratchet/nudge/condition.
    return [
        track, index, n, *notes, step_time, flags, velocity,
        0, 0, 0,       # no drum-lane hits or accents
        100,           # chance 100%, no conditions
        1,             # ratchet x1
        8,             # nudge 0
        0,             # no condition
    ]


def build_dry_run(data: dict[str, Any], track: int, accent_mode: str) -> dict[str, Any]:
    if not 0 <= track < NTRACK:
        raise ValueError("track must be 0..3")
    if accent_mode not in ("velocity", "flag"):
        raise ValueError("accent_mode must be 'velocity' or 'flag'")
    length, root, scale, steps, accent_amount = validate_pattern(data)
    frames = []
    for index in range(NSTEP):
        source_step = steps[index] if index < length else None
        args = make_step_args(track, index, source_step, accent_mode, accent_amount)
        frames.append([0xF0, *HEADER, CMD_TRACK_STEP, *args, 0xF7])
    active_gates = [s["gate"] for s in steps if s["on"]]
    return {
        "schema": "energun.fm1.dry-run",
        "schemaVersion": 1,
        "sourceGenerator": data.get("generatorVersion", "unknown"),
        "targetFirmwareProtocol": "Felucca SysEx editor protocol; factory M-VAVE V1.5 compatibility not established",
        "targetTrack": track,
        "transmissionEnabled": False,
        "flashOperations": False,
        "commands": {
            "stepWrite": "TRACK_STEP (30)",
            "frameCount": len(frames),
            "frames": frames,
        },
        "trackParameterPlan": [
            {"label": "P_SLEN", "value": length},
            {"label": "P_ROOT", "value": root},
            {"label": "P_SCALE", "value": SCALE_MAP[scale], "scaleName": scale},
        ],
        "accentMapping": accent_mode,
        "gate": {
            "status": "not mapped: FM-1 has track-wide P_SGATE; per-step Parameter Lock is not hardware-validated",
            "perStepValues": [s["gate"] for s in steps],
            "activeStepCount": len(active_gates),
            "minimum": min(active_gates) if active_gates else None,
            "maximum": max(active_gates) if active_gates else None,
            "medianCandidate": median(active_gates) if active_gates else None,
            "trackParameterWrite": None,
            "motionCommandsIncluded": False,
        },
        "warnings": [
            "DRY RUN ONLY: no MIDI ports are opened and no SysEx is transmitted.",
            "The 64-frame plan clears unused steps and drum-lane hit masks; target must be a note-based synth track, never DRUM or SLICE.",
            "Review the full target-track before-image, engine identity and existing motion records before any future write.",
            "P_SLEN, P_ROOT and P_SCALE are symbolic labels here; a future bridge must resolve IDs via INFO/DESC.",
            "The existing track quantizer must be read and checked before a future write.",
            "The accent mapping is a choice: velocity mode preserves V0.3 velocity but not the device accent flag; flag mode sets SF_ACCENT and forces velocity 127.",
            "Gate remains unresolved and is deliberately not written.",
        ],
    }


def main() -> int:
    parser = argparse.ArgumentParser(description="Build an offline FM-1 SysEx dry-run from V0.3 JSON")
    parser.add_argument("pattern_json", help="JSON exported by the V0.3 FM-1 bridge copy")
    parser.add_argument("--track", type=int, default=0, help="target track index 0..3 (default: 0)")
    parser.add_argument("--accent-mode", choices=("velocity", "flag"), required=True,
                        help="explicitly choose velocity-preserving or FM-1 accent-flag mapping")
    parser.add_argument("--output", help="save dry-run plan JSON to this path")
    args = parser.parse_args()
    try:
        data = json.loads(Path(args.pattern_json).expanduser().read_text(encoding="utf-8"))
        plan = build_dry_run(data, args.track, args.accent_mode)
        rendered = json.dumps(plan, ensure_ascii=False, indent=2)
        print(rendered)
        if args.output:
            path = Path(args.output).expanduser()
            path.write_text(rendered + "\n", encoding="utf-8")
            print(f"Saved dry-run plan: {path}", file=sys.stderr)
        return 0
    except Exception as exc:
        print(f"Dry-run failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
