#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Read-only M-VAVE FM-1 SysEx probe for macOS / Python 3.10+.

This tool sends only INFO (1), TRACK (27, query only), and TRACK_STEP
(30, query only). It contains no SET, STEP_SET, MOTION, project-save,
backup-restore, sample, firmware-update, or flash-write operation.
"""
from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

HEADER = (0x7D, 0x46, 0x4C)
CMD_INFO = 1
CMD_TRACK = 27
CMD_TRACK_STEP = 30
NSTEP_MAX = 64
NTRACK_MAX = 4


def decode_v14(lo: int, hi: int) -> int:
    if not (0 <= lo <= 127 and 0 <= hi <= 127):
        raise ValueError("v14 bytes must be 7-bit values")
    return (lo | (hi << 7)) - 8192


def split_c_string(data: list[int], pos: int) -> tuple[str, int]:
    try:
        end = data.index(0, pos)
    except ValueError as exc:
        raise ValueError("unterminated SysEx string") from exc
    return bytes(data[pos:end]).decode("ascii", errors="replace"), end + 1


def parse_info(payload: list[int]) -> dict[str, Any]:
    version, p = split_c_string(payload, 0)
    if len(payload) < p + 5:
        raise ValueError("INFO reply is shorter than its fixed header")
    nengines, pcount, gcount, nstep, pe0 = payload[p:p + 5]
    p += 5
    engines = []
    for _ in range(nengines):
        name, p = split_c_string(payload, p)
        engines.append(name)
    ntracks = payload[p] if p < len(payload) else None
    return {
        "version": version,
        "engine_count": nengines,
        "track_parameter_count": pcount,
        "global_parameter_count": gcount,
        "steps_per_track": nstep,
        "engine_parameter_base": pe0,
        "engines": engines,
        "track_count": ntracks,
        "raw_info_payload": payload,
    }


def parse_track(payload: list[int]) -> dict[str, Any]:
    if len(payload) < 2:
        raise ValueError("TRACK reply is too short")
    selected, count = payload[:2]
    if count > NTRACK_MAX or len(payload) < 2 + count * 6:
        raise ValueError("TRACK reply has an invalid track list")
    tracks = []
    p = 2
    for index in range(count):
        engine, preset = payload[p:p + 2]
        level = decode_v14(payload[p + 2], payload[p + 3])
        mute, armed = payload[p + 4:p + 6]
        tracks.append({
            "track": index,
            "engine": engine,
            "preset": preset,
            "level_raw": level,
            "mute": bool(mute),
            "armed": bool(armed),
        })
        p += 6
    if selected >= count:
        raise ValueError("TRACK selected index is outside the track list")
    return {"selected_track": selected, "track_count": count, "tracks": tracks}


def parse_step(payload: list[int]) -> dict[str, Any]:
    # TRACK_STEP reply: track, index, then the 15-byte v1.5.5 step record.
    if len(payload) != 17:
        raise ValueError(f"expected 17 TRACK_STEP payload bytes, received {len(payload)}")
    track, index, n = payload[:3]
    if track >= NTRACK_MAX or index >= NSTEP_MAX or n > 4:
        raise ValueError("TRACK_STEP reply has invalid track/index/note count")
    notes = payload[3:7]
    step_time, flags, velocity = payload[7:10]
    hit7, acc7, high = payload[10:13]
    hit = hit7 | ((high & 1) << 7)
    accent_lanes = acc7 | (((high >> 1) & 1) << 7)
    chance, ratchet, nudge_encoded, condition = payload[13:17]
    if step_time > 2 or ratchet < 1 or ratchet > 4 or nudge_encoded > 15:
        raise ValueError("TRACK_STEP reply contains invalid step fields")
    return {
        "track": track,
        "index": index,
        "note_count": n,
        "notes": notes[:n],
        "all_note_slots": notes,
        "time": ("NOTE", "TIE", "REST")[step_time],
        "flags": flags,
        "accent": bool(flags & 1),
        "slide": bool(flags & 2),
        "velocity": velocity,
        "drum_hit_mask": hit,
        "drum_accent_mask": accent_lanes,
        "chance_percent": chance,
        "ratchet": ratchet,
        "nudge_sixteenths": nudge_encoded - 8,
        "condition": condition,
    }


class ReadOnlyProbe:
    def __init__(self, input_port: Any, output_port: Any, timeout: float):
        self.input = input_port
        self.output = output_port
        self.timeout = timeout

    def request(self, command: int, args: list[int] | None = None) -> list[int]:
        if command not in (CMD_INFO, CMD_TRACK, CMD_TRACK_STEP):
            raise ValueError("safety guard: this probe permits read-only commands only")
        args = args or []
        if any(not isinstance(v, int) or v < 0 or v > 127 for v in args):
            raise ValueError("SysEx arguments must be 7-bit integers")
        # Discard stale input before issuing a new request.
        for _ in self.input.iter_pending():
            pass
        self.output.send(__import__("mido").Message(
            "sysex", data=(*HEADER, command, *args)
        ))
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            for message in self.input.iter_pending():
                if message.type != "sysex":
                    continue
                data = list(message.data)
                if len(data) >= 4 and tuple(data[:3]) == HEADER and data[3] == command:
                    return data[4:]
            time.sleep(0.002)
        raise TimeoutError(f"no reply for read-only SysEx command {command}")

    def capture(self, track: int | None, all_tracks: bool) -> dict[str, Any]:
        info_payload = self.request(CMD_INFO)
        track_payload = self.request(CMD_TRACK)
        info = parse_info(info_payload)
        if not info["version"].startswith("FELUCCA "):
            raise RuntimeError(
                "This probe targets the Felucca SysEx protocol only. "
                "Factory M-VAVE firmware V1.5 compatibility is not established; stopping before TRACK/step queries."
            )
        tracks = parse_track(track_payload)
        nstep = min(int(info["steps_per_track"]), NSTEP_MAX)
        ntracks = min(int(tracks["track_count"]), NTRACK_MAX)
        if nstep < 1 or ntracks < 1:
            raise ValueError("device reported an unsupported sequencer size")
        if all_tracks:
            selected_tracks = list(range(ntracks))
        else:
            selected_tracks = [tracks["selected_track"] if track is None else track]
        if any(t < 0 or t >= ntracks for t in selected_tracks):
            raise ValueError(f"track must be in the range 0..{ntracks - 1}")

        patterns: dict[str, list[dict[str, Any]]] = {}
        for t in selected_tracks:
            steps = []
            for index in range(nstep):
                payload = self.request(CMD_TRACK_STEP, [t, index])
                step = parse_step(payload)
                if step["track"] != t or step["index"] != index:
                    raise ValueError("device replied with a different track or step index")
                steps.append(step)
            patterns[str(t)] = steps
        return {
            "capture_utc": datetime.now(timezone.utc).isoformat(),
            "safety": {
                "mode": "read-only",
                "commands_sent": [CMD_INFO, CMD_TRACK, CMD_TRACK_STEP],
                "writes_sent": False,
                "flash_operations": False,
                "firmware_operations": False,
            },
            "device": info,
            "track_state": tracks,
            "captured_tracks": selected_tracks,
            "steps_per_track": nstep,
            "patterns": patterns,
        }


def choose_port(names: list[str], requested: str | None, direction: str) -> str:
    if requested:
        matches = [name for name in names if requested.casefold() in name.casefold()]
        if len(matches) == 1:
            return matches[0]
        if not matches:
            raise RuntimeError(f"{direction} MIDI port matching {requested!r} was not found")
        raise RuntimeError(f"{direction} MIDI port selector {requested!r} is ambiguous: {matches}")
    fm_matches = [name for name in names if "fm-1" in name.casefold() or "fm1" in name.casefold()]
    if len(fm_matches) == 1:
        return fm_matches[0]
    if len(names) == 1:
        return names[0]
    raise RuntimeError(
        f"Could not select {direction} MIDI port automatically.\n"
        f"Available {direction} ports:\n  " + "\n  ".join(names or ["(none)"]) +
        f"\nPass --{direction} with a unique name fragment."
    )


def main() -> int:
    parser = argparse.ArgumentParser(description="Read-only M-VAVE FM-1 sequencer probe")
    parser.add_argument("--input", help="unique fragment of the FM-1 MIDI input port name")
    parser.add_argument("--output", help="unique fragment of the FM-1 MIDI output port name")
    parser.add_argument("--track", type=int, help="track index 0..3; default: currently selected track")
    parser.add_argument("--all-tracks", action="store_true", help="read all tracks (up to 4 x 64 steps)")
    parser.add_argument("--timeout", type=float, default=1.0, help="reply timeout in seconds (default: 1.0)")
    parser.add_argument("--json", dest="json_path", help="also save the read-only capture as JSON")
    args = parser.parse_args()

    if args.track is not None and args.all_tracks:
        parser.error("--track and --all-tracks cannot be used together")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")

    try:
        import mido
    except ImportError:
        print("Missing dependency. Install with: python3 -m pip install mido python-rtmidi", file=sys.stderr)
        return 2

    try:
        in_name = choose_port(mido.get_input_names(), args.input, "input")
        out_name = choose_port(mido.get_output_names(), args.output, "output")
        print(f"FM-1 MIDI input:  {in_name}", file=sys.stderr)
        print(f"FM-1 MIDI output: {out_name}", file=sys.stderr)
        with mido.open_input(in_name) as midi_in, mido.open_output(out_name) as midi_out:
            result = ReadOnlyProbe(midi_in, midi_out, args.timeout).capture(args.track, args.all_tracks)
        rendered = json.dumps(result, ensure_ascii=False, indent=2)
        print(rendered)
        if args.json_path:
            path = Path(args.json_path).expanduser()
            path.write_text(rendered + "\n", encoding="utf-8")
            print(f"Saved read-only capture: {path}", file=sys.stderr)
        return 0
    except Exception as exc:
        print(f"Probe failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
