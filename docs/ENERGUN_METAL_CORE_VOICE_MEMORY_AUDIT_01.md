# ENERGUN METAL CORE — Voice State and Memory Allocation Audit 01

Status: source audit + proposed allocation. No DSP implementation, build, hardware test, or flash is claimed.

## 1. Audited source revisions

- Felucca-Engines: `d0260074ce100a47675814b7f38d61f11176c926`
- Felucca integration branch: `energun-metal-core`
- Felucca engine interface: `src/felucca_engine.h`
- Shared DSP primitives: `src/dsp.c`
- Reference engine: `src/eng_analog.c`
- Felucca host memory pool: `firmware/src/engines.c`

The audit is pinned to the revisions above. Re-check these assumptions if either source revision changes.

## 2. Confirmed engine/voice contracts

From `felucca_engine.h`:

- `voice_t.ph[3]`: three 32-bit phase accumulators.
- `voice_t.s[8]`: eight signed 32-bit engine state words (32 bytes per voice).
- `voice_t` also carries note/gate/activity, envelope, pitch/glide and other host-owned fields. Metal Core must not repurpose host-owned fields.
- `engine_t.keep` is a bit mask controlling which `s[k]` words survive a retrigger of an already sounding voice. The interface comment specifically notes that phase retention is separately handled by the host.
- `note_on` runs when a voice starts or is retriggered; `render` adds up to `CTL` samples into the output buffer.
- `CTL = 32` samples.
- Optional `block` runs once per block and part; optional `post` processes the summed part output.
- `engine_t.mem` requests part-scoped engine memory. The host owns allocation and clears the region when switching from another engine.
- `preset_t.e[8]` stores the eight EDIT values; envelope, filter envelope, mono/poly and FX sends are stored separately.

Implication: keep all per-voice state in the provided voice fields; use the part memory region only for genuinely shared/per-part state. Do not allocate dynamically or retain pointers to transient host data.

## 3. Confirmed DSP primitive

`src/dsp.c` contains a trapezoidal State Variable Filter (Simper SVF):

- Coefficients are calculated per block.
- Cutoff input is clamped to `0..127 << 8`.
- Resonance maps to damping Q12 using `8192 - reso * 7600 / 127`.
- The low-pass integrator has two state words (`ic1`, `ic2`).
- Internal state is clamped to ±150000.
- The source documents the filter as unconditionally stable, with signal bounds selected so intermediate products fit in 32-bit arithmetic.

`eng_analog.c` demonstrates the intended use: two filter state words in `voice_t.s[0..1]`, coefficients prepared for the block, and `engine_t.keep = 0x03` to retain those two filter states over a retrigger.

This is a reusable primitive, not a complete LPG. Metal Core still needs its own LPG envelope/control and a distinct post-FOLD filter stage.

## 4. Proposed per-voice state map (draft for implementation)

The current interface gives 8 × int32 state words and 3 × uint32 phase accumulators per voice. Proposed map:

| Field | Proposed use | Reason |
|---|---|---|
| `ph[0]` | OSC1 phase | Main audible carrier |
| `ph[1]` | OSC2 phase | FM modulator / optional direct source |
| `ph[2]` | Reserved | Keep available; do not assume a third oscillator is approved |
| `s[0]`, `s[1]` | FOLD-stage memory only if the selected fold algorithm requires it | Keep algorithm state separate from LPG |
| `s[2]`, `s[3]` | LPG SVF integrator `ic1`, `ic2` | Dedicated LPG filter state |
| `s[4]` | LPG envelope level / one-pole state | Block-smoothed control state |
| `s[5]` | LPG envelope stage or remaining-time state | Only if it cannot be represented by host gate/stage safely |
| `s[6]` | FM feedback state | Bounded, normalized, smoothed feedback memory |
| `s[7]` | Reserved / future bounded state | Avoid overcommitting before FOLD and feedback algorithms are finalized |

Important: this is a capacity map, not a final ABI commitment. FOLD may be memoryless in its first implementation; if so, `s[0..1]` can remain reserved. Do not use `s[5]` for a private note lifecycle until the voice gate/retrigger/release behavior has been audited. If a state is required to survive retrigger, set its `keep` bit deliberately; otherwise clear/reinitialize it in `note_on`.

### Retrigger policy

- Oscillator phase continuity is governed by the host phase-retention contract; do not use `keep` as a substitute for it.
- LPG filter integrators should initially reset on a fresh voice start, but the desired behavior on legato/retrigger must be checked against the host's retrigger path.
- FM feedback state should reset or decay deterministically on retrigger; never allow stale feedback to cause a burst.
- The LPG control envelope must respect host note-off and voice shutdown. It must not keep a voice alive beyond the host lifecycle unless a separately reviewed engine-owned-envelope design is approved.

## 5. Part memory: do not reserve it prematurely

The Felucca host defines `ENG_MEM_PART` in `firmware/src/engines.c` and provides one region per part through `ENG_MEM(p)`. The source has build-configuration-dependent values (the inspected file contains conditional definitions, including 13344 and 12888 bytes). The region is sized for the largest enabled engine and shared by the active engine of that part.

For the first Metal Core implementation, prefer **zero part-memory allocation** unless the DSP design proves it needs shared tables, delay lines, or per-part state. The two oscillators, FM feedback and LPG integrators are naturally per-voice and fit the existing voice fields. This avoids growing the global pool and avoids competing with optional ADPCM/PHYS configurations.

If later required, declare the exact `engine_t.mem` byte count, add a compile-time fit assertion against the active `ENG_MEM_PART`, and document the maximum enabled build configuration. Never hardcode the pool's current conditional size into the engine.

## 6. Eight EDIT parameters: proposed page grouping

The engine interface provides exactly eight stored EDIT parameters per engine. Suggested initial grouping for the first playable version:

- EDIT 1 — OSC: OSC1 waveform, OSC2 waveform, OSC2 coarse/fine tune and direct level must be mapped carefully; not every subparameter can be independently exposed in the first eight controls.
- EDIT 2 — FM: ratio, amount, feedback, with bounded ranges and zero-FM default.
- EDIT 3 — FOLD: primary fold amount, symmetry, drive and mode require a considered compact mapping.
- EDIT 4 — LPG: cutoff, resonance, decay and response.

This is a UI/page concept, not a final parameter-index assignment. Do not silently drop agreed controls or encode multiple unrelated values into one parameter without a documented mapping. Before coding, inspect Felucca's parameter descriptors, UI EDIT pages and preset serialization/load paths; confirm whether additional non-EDIT engine-specific parameters can be persisted (currently no such mechanism has been established).

## 7. Risks and constraints

- `voice_t.s[8]` is a strict 32-byte per-voice budget unless the shared engine ABI is intentionally changed; changing it would affect every engine and is not justified for this project.
- Avoid 64-bit multiplies/divides and expensive per-sample coefficient calculation in the hot render loop.
- Calculate filter coefficients at block/control rate; smooth parameter transitions.
- Keep all fixed-point intermediate ranges proven, especially FM feedback and FOLD gain.
- Do not use heap allocation, blocking calls, unbounded loops, or floating point in the real-time render path.
- Host tests do not prove target CPU timing. Measure on the actual FM-1 target before enabling more expensive modes.
- Preserve the approved signal order: OSCILLATORS → FM → FOLD → LPG → OUTPUT.
- FOLLOW remains sequencer pitch tracking; METAL remains its separate metal-ratio behavior. Neither is an LPG envelope or a Wavefolder control.

## 8. Next implementation gate

Before writing DSP code:

1. Audit Felucca's voice start/retrigger/note-off/voice-free path and document exact phase/state reset semantics.
2. Audit EDIT parameter descriptors, page rendering, preset save/load and project serialization.
3. Decide final eight-parameter mapping and defaults without losing agreed controls.
4. Define the exact fixed-point ranges and test vectors for OSC/FM/FOLD/LPG.
5. Only then implement a small standalone DSP test harness for the SVF/LPG stage, followed by engine integration.

No DSP code has been added by this document. No firmware build, target timing measurement, hardware test or flash has been performed.
