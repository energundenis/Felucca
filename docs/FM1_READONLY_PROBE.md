# FM-1 read-only SysEx probe

This is the first computer-side integration step for ENERGUN GENERATIVE SEQUENCER V0.3.

## Safety scope

`tools/fm1_readonly_probe.py` sends only:
- `INFO` (1);
- `TRACK` (27) with no arguments, to read current selection and track state;
- `TRACK_STEP` (30) with track and step indices, to read step data.

The tool has an explicit command allowlist. It does not send `SET`, `STEP_SET`, `TRACK_PARAM`, `MOTION`, project save/load, backup restore, sample writes, or firmware operations. It does not change the selected track.

This is a read-only diagnostic tool. It is not a generator uploader and does not yet write patterns to the FM-1.

## Requirements (macOS)

Python 3.10+ and a USB connection to the FM-1.

Install the MIDI libraries:

```sh
python3 -m pip install mido python-rtmidi
```

Run from the repository root:

```sh
python3 tools/fm1_readonly_probe.py
```

If automatic port selection is ambiguous, list names in the error and pass a unique fragment:

```sh
python3 tools/fm1_readonly_probe.py --input "FM-1" --output "FM-1"
```

By default it reads all 64 steps of the currently selected track. To read every track:

```sh
python3 tools/fm1_readonly_probe.py --all-tracks --json fm1_readonly_capture.json
```

To read a specific track (zero-based):

```sh
python3 tools/fm1_readonly_probe.py --track 0 --json fm1_track_1.json
```

The JSON capture includes firmware version, advertised sequencer dimensions, track state, and step data. Keep the capture private; it may contain personal pattern content.

## Validation

Run the pure protocol/codec tests (no FM-1 connection needed):

```sh
python3 tests/test_fm1_readonly_probe.py -v
```

These tests validate parsing and the read-only command guard. They do not establish that a particular physical device/firmware build responds correctly; that must be confirmed by a real read-only capture.

## Gate / Parameter Lock status

The firmware protocol document says MOTION / Parameter Lock has host-side handler tests, but is not in the set of commands documented as checked on hardware. Do not use it for the generator's Gate transfer until a separate real-device test is designed and explicitly approved. The current probe intentionally does not send MOTION.
