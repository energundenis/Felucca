# ENERGUN METAL CORE — CP2.19 VCA performance-mode decision

Date: 2026-10-31
Branch: energun-metal-core
Status: user-approved product behavior decision; implementation not yet connected.

## Decision

The user selected the full-envelope option:

**Every performance note, including live/manual FM-1 MIDI playing, uses the full METAL CORE VCA contour.** The VCA is not held at a constant LEVEL in manual/performance mode.

The same VCA contour is used by the integrated sequencer. There is one consistent amplitude-envelope behavior across live MIDI and sequencer playback; the Gate source and Gate duration differ by note source.

## Behavior by note source

### Live/manual performance (MIDI)

- MIDI Note On starts the internal VCA Attack → Decay → Sustain contour.
- MIDI Note Off starts Release from the instantaneous current VCA level.
- A new Note On retrigger while sounding holds the current level and starts a fresh contour; it must not jump to zero.
- LEVEL sets the VCA peak; SUSTAIN, ATTACK, DECAY and RELEASE retain their approved V98 parameter meanings and ranges.
- The contour must not be multiplied by Felucca's host ADSR. ownenv=1 remains the intended engine contract.
- Current host render callback exposes Gate state at block entry, not an exact note-off sample offset. Until a host event-offset extension is implemented and tested, a Note Off is applied at the first sample of the next render block. Do not claim sample-accurate MIDI release.

### Integrated sequencer

- Each sequencer step triggers the same Attack → Decay → Sustain VCA contour.
- Step Gate-off / Gate duration controls when Release begins, retaining the V98 short-Gate Option B rule: if Gate-off occurs during Attack or before Decay begins, Release starts immediately from the current level and pending Decay is suppressed.
- Ratchet/retrigger events hold the current VCA level and start the new contour without a reset-to-zero discontinuity.
- Sequencer timing and ratchet subdivision must not be silently conflated: the exact Gate duration follows the sequencer's defined step/gate event, to be specified in its integration contract.

## Preserved distinctions

- The envelope shape is common to live MIDI and sequencer.
- The Gate source is different: MIDI key state for live performance; sequencer step Gate for sequenced notes.
- Explicit Accent provenance remains unresolved. Do not infer sequencer ACCENT from expressive MIDI velocity without an approved mapping.
- Sample-accurate sub-block Gate timing remains unresolved at the current host API boundary. First implementation may release at the next render-block boundary, clearly documented and tested.

## Implementation consequences

1. Remove the V98 manual-mode constant-level bypass from the FM-1 product behavior.
2. Route every live MIDI Note On/Off through the internal VCA lifecycle.
3. Route sequencer step Gate and ratchet/retrigger events through the same internal VCA model.
4. Keep host ADSR bypassed for METAL CORE (ownenv=1); keep host LFO-to-AMP and engine-switch fade as independent multiplicative controls.
5. Add host integration tests for MIDI Gate, sequencer Gate, retrigger, short Gate, release completion and block-boundary timing.
6. Do not register the engine or change firmware/device state until Accent and Gate-event handling, memory, target-cost and regression gates are resolved.

## Safety boundary

This is a product behavior decision only. No DSP source, engine registration, parameter map, UI, persistence, Flash layout, firmware package or physical device state is changed by this decision. No device flashing. main and factory firmware remain untouched.