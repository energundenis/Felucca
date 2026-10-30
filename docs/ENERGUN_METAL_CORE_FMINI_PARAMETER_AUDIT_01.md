# ENERGUN METAL CORE — FMini-1 / Felucca Parameter Architecture Audit

Date: 2026-10-30
Branch: `energun-metal-core`
Status: source audit; no firmware implementation or target build.

## Executive finding

The eight Felucca engine EDIT parameters are not the only possible persistent sound controls. They are the engine ABI's eight generic EDIT slots. The track model, project format and user-preset record each carry a larger parameter array. Felucca's own source explicitly supports adding common per-track parameters before `P_E0`, migrating older projects and user presets while keeping the last eight engine EDIT values aligned.

This gives ENERGUN a credible route to FMini-1-style dedicated parameter pages with independently stored controls. The earlier proposal to compress OSC, FM, FOLD and LPG into eight macros is therefore not required by storage limits and should remain only a fallback.

## 1. Verified source facts

### Engine interface and EDIT pages
- `felucca_engine.h` defines exactly `P_E0..P_E7` as the engine's eight EDIT parameters.
- `params.c` maps EDIT 1 to E0..E3 and EDIT 2 to E4..E7.
- `engine_t.desc` can provide mode-dependent labels for those eight parameters only.
- `track_desc()` dispatches E0..E7 to the engine descriptor; all earlier IDs use the shared `TP[]` descriptor table.
- The page table is static. `page_visible()` already filters special pages by engine (ANALOG FILTER, DRUM LANES, FM6 operator pages, SLICES), so the codebase has a precedent for engine-specific pages.

### Track parameter storage
- `track_t` contains `int16_t p[P_COUNT]`.
- The current source identifies the current format as 111 parameters (project.c format comment, 1.5), with `P_E0` marking the final eight values.
- `core.h` documents the migration rule: common parameters are inserted immediately before `P_E0`; old stores map the first `np - 8` common parameters and then copy the final eight E values to today's `P_E0..P_E7`.
- The project serializer stores every parameter in `P_COUNT` and rejects only when the total encoded record no longer fits its fixed store, or when `P_COUNT > 127`.
- The project loader fills parameters absent from older files from the current engine's defaults, then maps the stored final eight values to today's E0..E7.
- User presets store `np = P_COUNT`; the current record reserves 144 bytes for parameters (`UP_PMAX * 2`, UP_PMAX=72). The compile-time guard is `P_COUNT <= UP_PMAX * 2` and `P_COUNT < 128`.
- User preset migration uses the same final-eight-E-values convention.
- Motion/parameter-lock IDs are seven-bit parameter IDs; `core.h` documents `P_COUNT < 128`. Thus new IDs must stay below 128.

### Implication
Adding a bounded number of dedicated common parameter IDs before `P_E0` is an established Felucca extension mechanism. It preserves the E0..E7 positions in old projects/presets through the existing count-based migration. It does, however, add those parameter slots to every track's stored array, even though only Metal Core pages and DSP will use them.

## 2. Recommended integration strategy

Prefer dedicated Metal Core parameter IDs over compressing independent controls into eight macros.

1. Add a carefully bounded set of new `P_MC_*` IDs immediately before `P_E0` in `core.h`. Keep the eight E IDs last.
2. Add fixed descriptors to `TP[]` in `params.c` (labels, ranges, defaults and formatting). These IDs are common storage slots, not new engine EDIT ABI slots.
3. Add Metal Core-specific page entries to the static `PAGES[]` table.
4. Extend `page_visible()` to show those pages only when the selected engine is ENERGUN METAL CORE.
5. Keep the normal EDIT 1 / EDIT 2 pages for the eight engine EDIT parameters only if they have a real use; do not duplicate the same controls across pages.
6. Read the new values from `track_t.p[]` in the Metal Core engine. Keep DSP state per voice in existing `voice_t.ph[]` / `voice_t.s[]`; do not use track parameters as audio-rate state.
7. Confirm project and user-preset migration, parameter locks, automation and MIDI/editor protocol behavior for the new IDs.

This matches the FMini-1 idea of multiple purpose-specific pages while respecting Felucca's actual storage and engine boundaries.

## 3. Constraints and risks

- `P_COUNT` must remain below 128 because parameter IDs and motion-lock IDs are one byte with bit 7 reserved for the lock flag.
- User-preset parameter capacity is currently 144 bytes; the present count is well below that ceiling, but the compile-time guard must remain.
- Project records have a fixed storage size. Each added parameter costs one byte per track in the packed project. The project packer already checks the resulting position against its reserved tail; nevertheless, measure worst-case size with the intended parameter count and maximal motion/chain data.
- New common parameter IDs exist in the track structure for all engines. They need neutral defaults and must not affect other engines, their UI, or their sound.
- Additions must be append-only immediately before E0 according to Felucca's documented migration convention. Never insert them among existing common parameters or after E7.
- New parameter names and pages require careful page-family navigation, visibility and knob mapping review.
- Parameter locks and automation can address the new IDs, but this should be tested end-to-end; storage support alone does not prove the UI/recording path is correct.
- The eight engine EDIT slots remain useful for engine-specific values but are no longer a hard limit on the complete Metal Core control surface.

## 4. FMini-1 mapping implications

FMini-1's seven pages (CTRL, OSC, MIX, FILT, F.ENV, A.ENV, FX) are a functional/UI reference, not a requirement to reproduce its exact synthesis architecture. ENERGUN Metal Core has its own approved signal chain and should receive pages that expose its actual independent controls.

Candidate Metal Core page groups:
- OSC: OSC1 waveform, OSC2 waveform, OSC2 coarse tune, OSC2 fine tune.
- MIX: OSC1 level, OSC2 direct level, and any genuinely required balance control.
- FM: FM amount, ratio, feedback (if and when safely implemented).
- FOLD: amount, symmetry, drive, mode.
- LPG: cutoff, resonance, decay, response.
- FOLLOW / METAL: only where their real sequencer/engine semantics belong; do not relabel them as FOLD controls.

The exact page grouping and number of new IDs must be reconciled with the hardware's physical page buttons/knobs and the existing Felucca page-family navigation. Do not blindly copy FMini-1's CTRL or ADSR pages if Felucca already provides equivalent global/per-track controls.

## 5. Recommended next audit / implementation gate

Before editing `core.h`, `params.c` or the engine:
1. Count the exact independent Metal Core parameters needed for V1 and identify which existing Felucca parameters already provide equivalent controls.
2. Calculate remaining ID capacity (`P_COUNT < 128`), user-preset capacity and worst-case project bytes with the proposed new IDs.
3. Audit the editor protocol and parameter-lock/automation paths for assumptions about `P_E0`, `P_COUNT`, and the eight engine values.
4. Confirm the hardware's page navigation can reach the intended Metal Core pages without disrupting existing Felucca families.
5. Define a migration test: old project + old user preset -> new firmware -> values of old common params and E0..E7 unchanged; new Metal Core params receive defaults; save/reload preserves all new values.

No parameter IDs, pages, DSP, UI or storage formats were changed by this audit. No firmware build, hardware test or flash was performed.
