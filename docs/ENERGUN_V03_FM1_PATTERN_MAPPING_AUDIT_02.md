# ENERGUN V0.3 → FM-1 pattern mapping audit 02

Date: 2026-10-31
References:
- \`energundenis/energun-generative-sequencer\`, \`main:versions/v0.3.html\`
- \`energundenis/Felucca\`, \`energun-metal-core\`
Status: source inspection and design notes only. No firmware/device writes.

## 1. V0.3 sequence representation

The accepted V0.3 browser generator keeps each step as:
- \`on\`: active / rest;
- \`note\`: scale-relative semitone interval;
- \`oct\`: octave offset;
- \`accent\`: boolean accent decision;
- \`gate\`: per-step gate percentage.

Pattern length is 8, 16, or 32. The absolute MIDI note is calculated by V0.3 as:

\`60 + ROOT_NOTE + note + 12 * octave\`

The note pool is generated from the selected scale's semitone intervals. Therefore the stored \`note\` is not a MIDI note by itself; the absolute MIDI note must be calculated before creating FM-1 step data.

## 2. Scale mapping — direct correspondence

FM-1's \`P_SCALE\` enum in \`firmware/src/params.c\` is:
0 CHR, 1 MAJ, 2 MIN, 3 DOR, 4 MIX, 5 PEN, 6 MPEN, 7 HARM, 8 PHRY, ...

V0.3's six scales map as follows:

| V0.3 scale | FM-1 P_SCALE |
|---|---:|
| MINOR | 2 (MIN) |
| PHRYGIAN | 8 (PHRY) |
| HARMONIC MINOR | 7 (HARM) |
| DORIAN | 3 (DOR) |
| MAJOR | 1 (MAJ) |
| CHROMATIC | 0 (CHR) |

V0.3 ROOT NOTE is a pitch class 0..11 and maps directly to FM-1 \`P_ROOT\`.

When FM-1's sequence quantizer is active, matching \`P_ROOT\` and \`P_SCALE\` to the generator's settings ensures that notes already generated in that scale are not unexpectedly snapped to another key. The integration must read the target track's quantizer first and must not silently change it.

## 3. Step mapping

Proposed mapping for a complete generated pattern:

| V0.3 | FM-1 step |
|---|---|
| active step | \`time = ST_NOTE\`, \`n = 1\` |
| inactive step | \`time = ST_REST\`, \`n = 0\` |
| absolute MIDI pitch | \`note[0]\` (0..127) |
| remaining note slots | zero |
| gate-independent velocity | \`vel\` |
| pattern length | \`P_SLEN\` |
| pattern root | \`P_ROOT\` |
| pattern scale | \`P_SCALE\` |

V0.3 syncopation changes the probability of activating offbeat positions; it does not move notes off the step grid. Therefore do not add FM-1 NUDGE values as an assumed syncopation translation. Preserve the generated ON/OFF positions.

For a full pattern replacement, the host-side adapter should construct a 64-step candidate: generated steps at the beginning, then REST steps to the end. This prevents stale notes beyond a short pattern from unexpectedly reappearing if the user later lengthens the FM-1 pattern. The adapter must preserve a complete before-image of all 64 target steps for rollback/undo.

## 4. Accent compatibility — not one-to-one

V0.3 accents are both a boolean decision and a velocity expression:
- unaccented note velocity is 82;
- accented velocity is \`round(80 + accent_amount * 47)\`.

FM-1's \`seq_step()\` currently does:
\`SF_ACCENT ? 127 : (step.vel ? step.vel : 96)\`.

Therefore setting FM-1's \`SF_ACCENT\` flag forces velocity 127 and ignores the V0.3 velocity value. There are two possible mappings, neither exactly preserves both properties:

1. Preserve V0.3's velocity dynamics: write the calculated velocity to \`vel\` and leave \`SF_ACCENT\` clear. This preserves the audible accent amount but does not set the device's accent flag/indicator.
2. Preserve the explicit accent flag: set \`SF_ACCENT\` for accented steps. The device plays those notes at velocity 127, so V0.3's graded accent strength is lost.

Do not modify \`seq_step()\` or the sound engine to resolve this. Choose the desired mapping after a listening test on the actual FM-1.

## 5. Gate compatibility — unresolved pending hardware validation

V0.3 has an independent \`gate\` percentage per step. FM-1's \`P_SGATE\` is track-wide and is used by the sequencer to calculate note duration.

The motion/Parameter Lock implementation can theoretically vary \`P_SGATE\` per step, but the current firmware deliberately excludes it from the allowed motion parameter set. The protocol documentation states that MOTION/Parameter Lock has host-side tests, not the same hardware verification status as the read/write track-step path.

Do not send MOTION commands or change the \`P_SGATE\` whitelist yet. First establish the basic read-only connection. A later, separately approved Gate experiment must:
- use a disposable/test pattern or track;
- preserve all current motion records;
- account for the shared 128-record capacity across four tracks;
- verify the gate lock is applied before note-on and restored at the next step;
- fail atomically if capacity is insufficient.

If per-step locks prove unsuitable, a simpler fallback is one track-wide \`P_SGATE\` derived from the generated gate values, with an explicit warning that per-step gate variation is collapsed. This fallback also requires user approval because it changes a track sequencer parameter.

## 6. Why the existing MIDI export is not the canonical bridge

V0.3's MIDI exporter writes note-on / note-off events on a 16th-note grid (120 ticks at PPQ 480) and derives note duration from each step's gate. It does not write metadata for:
- the requested pattern length (8 / 16 / 32);
- trailing REST steps after the last note;
- the V0.3 root and scale controls as separate fields;
- the boolean accent decision separately from its resulting velocity;
- the generation controls / settings.

Consequently a MIDI file can be useful for DAW playback, but it is not a lossless interchange format for the FM-1 sequencer.

Recommended next artifact: add a JSON pattern export to a *new versioned copy* of the generator, preserving V0.3 unchanged. The JSON should contain schema version, pattern length, root, scale, and every step's on/note/octave/accent/gate fields. A host-side adapter can validate this file and build a dry-run FM-1 SysEx plan without transmitting any writes.

## 7. Integration order

1. Run the read-only probe and verify INFO / TRACK / TRACK_STEP against the physical FM-1.
2. Add versioned JSON export to a copy of V0.3; do not edit the accepted V0.3 baseline.
3. Implement and test a pure offline converter: V0.3 JSON → 64 FM-1 step records + parameter plan.
4. Show a dry-run diff of current versus generated track data; no device writes.
5. Only after the user's explicit approval and the required factory V1.5 dump + verified recovery method, design a reversible write test on a disposable track.
6. Test Gate separately; do not assume MOTION hardware support from host tests.

## 8. Safety

This audit only reads source files and documents a proposed mapping. No physical FM-1 was queried, no SysEx was sent, no source firmware was changed, and no Flash/project/preset data was written.
