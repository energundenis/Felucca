# FM-1 pattern dry-run converter

The converter reads JSON exported by the V0.3 FM-1 bridge copy and creates a reviewable SysEx plan.

## Run

    python3 tools/fm1_pattern_dryrun.py energun_fm1_pattern_v1.json --track 0 --accent-mode velocity --output fm1_dry_run.json

Choose **velocity** to preserve the V0.3 accent velocity values, or **flag** to set the FM-1 step accent bit (which forces velocity 127 in the current sequencer). The choice is required; neither is silently selected.

## Dry-run behavior

- Validates the JSON schema, all step indices, pitch calculation, root, scale, and gate values.
- Builds 64 candidate TRACK_STEP (30) frames. Steps beyond the requested 8 / 16 / 32 length become REST, so old data cannot leak back if the pattern is lengthened later.
- Produces a symbolic parameter plan for P_SLEN, P_ROOT and P_SCALE.
- Reports per-step Gate values and a median candidate, but deliberately does not create a P_SGATE write or MOTION / Parameter Lock message.
- Does not open MIDI ports, send SysEx, or access the FM-1.

## Safety / compatibility

This is an offline plan generator only. The frames are not an upload tool. The Felucca SysEx protocol is not established as compatible with factory M-VAVE firmware V1.5. Any future write path requires the full factory V1.5 dump, a verified recovery method, explicit approval, a complete before-image of all 64 target steps, and a reversible test plan.
