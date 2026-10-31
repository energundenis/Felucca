# ENERGUN FM-1 CORE — CP2.17 ownenv lifecycle hot-path optimization and regression evidence

Date: 2026-10-31
Branch: `energun-metal-core`
Implementation commit tested: `35768068d51130d83535207241bd46fac4147d3c`
Documentation milestone: CP2.17
CI run: [41229095524](https://github.com/energundenis/Felucca/actions/runs/41229095524)
Job: `124619315986`
Artifact: [13052638568](https://github.com/energundenis/Felucca/actions/runs/41229095524/artifacts/13052638568)

## 1. Why this optimization was required

The first own-envelope lifecycle change in `firmware/src/voice.c` correctly attempted to prevent the host ADSR from ending an engine-owned envelope, while preserving the shared-voice stage-4 steal fade. However, CI run 41227210331 reported a target static-cost regression:

- `mix_part`: cost 4351 against budget 3549 (over the allowed threshold).
- `fm1_alnk0_irq`: unchanged at 355 against budget 348.

The regression was localized to the shared `track_render()` path in `voice.c`; the test-only FM6 change did not affect target firmware cost.

## 2. Optimization applied

Implementation commit `35768068d51130d83535207241bd46fac4147d3c` streamlines the `ownenv` branch in `firmware/src/voice.c`:

- For engine-owned voices at ordinary stages, bypass the host ADSR and provide unity host envelope gain.
- Preserve host `env_tick()` specifically for stage 4, so the shared voice-steal fade remains host-controlled.
- Call the engine `done()` lifecycle callback for active ownenv voices and retire the voice when it reports completion.
- Keep the non-ownenv path on the original `env_tick()` behavior.
- Avoid repeated `ownenv` checks and unnecessary host envelope processing for ordinary ownenv voices.

This is a shared voice-core change, so its regression evidence is required before treating it as stable. The target static-cost budget was not raised.

## 3. CI verification

Run 41229095524 completed with conclusion **success**. All 15 primary workflow steps completed successfully; the pinned SDK cache-save step was skipped as expected because the cache was restored.

Key target-cost result from the AC79 target-budget analysis:

| Function | Cost | Budget | Result |
|---|---:|---:|---|
| `mix_part` | 3431 | 3549 | PASS |
| `fm1_alnk0_irq` | 355 | 348 | Existing +2% over baseline budget; unchanged from prior run |

Comparison:
- Failed run 41227210331: `mix_part` 4351.
- Successful optimized run 41229095524: `mix_part` 3431.
- Reduction from the failed run: 920 cost units (about 21.1%).
- Previous successful pre-lifecycle run 41225293878: `mix_part` 3649; the optimized result is 218 cost units lower.

The target-budget script reports a static code-cost estimate. It is **not** a cycle-accurate benchmark, runtime CPU measurement, interrupt-latency measurement, or execution on physical FM-1 hardware.

Additional checks from the same CI job:
- Unmodified Felucca firmware build: passed.
- Standalone ENERGUN VCA compile with pinned AC79 `pi32v2` compiler: passed.
- FM6 engine-owned envelope regression: passed, including the test that its internal Release outlives the fastest host ADSR Release.
- Isolated METAL CORE VCA suite: 20 checks, 0 failures.
- Host regression: `ALL HOST TESTS PASSED`.
- Web tests and backup web tests: passed.
- Existing golden-render / voice / routing / health checks were included in the host regression suite; the run completed successfully.

The log still contains non-fatal compiler warnings, including the existing sequence-point warning at `tests/input_test.c:47`, plus unused-variable and signedness warnings. They are not silently treated as resolved and remain separate cleanup items.

## 4. Artifact

Artifact ID: `13052638568`
Name: `energun-fm1-baseline-35768068d51130d83535207241bd46fac4147d3c`
Size: 949,224 bytes
SHA-256: `449a994f099b6f828028ede92646fb96008ccb4520b005746561087d0f1e2112`
Retention expiry: 2026-11-14T18:34:41Z

The artifact is a CI evidence package for the unmodified Felucca baseline build and standalone VCA target probe. It is not a firmware package containing an integrated ENERGUN engine.

## 5. Status and remaining gates

**Closed for this milestone**
- [x] Optimize ownenv lifecycle path in `voice.c`.
- [x] Preserve host stage-4 voice-steal fade in the optimized branch.
- [x] Keep the existing target-cost budget unchanged.
- [x] Pass AC79 target static-cost check for `mix_part`.
- [x] Pass FM6 ownenv lifecycle regression.
- [x] Pass isolated VCA, host and web regression suites.
- [x] Produce and upload CI evidence artifact.

**Still open**
- [ ] Integrate METAL CORE as a registered Felucca engine; this has not happened.
- [ ] Design and verify engine-owned per-voice VCA state placement/alignment and total per-part RAM.
- [ ] Measure actual AC79 cycle cost with a target-side benchmark or defensible timing harness.
- [ ] Add integration-level render and lifecycle tests for note-on, note-off, Gate, retrigger/ratchet, block-boundary completion and additive output.
- [ ] Resolve manual mode versus sequencer/performance Gate mapping.
- [ ] Complete parameter IDs, UI, persistence, storage/Flash layout, packaging and recovery gates.
- [ ] Perform device validation only after all safety gates and separate explicit user authorization.

## 6. Safety boundary

This milestone changes only the experimental `energun-metal-core` branch. The factory firmware and `main` branch were not changed. No Flash layout or device state was modified. No firmware was flashed and no physical FM-1 test is claimed.
