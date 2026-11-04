# CP3 — combined OSC1 + VCA real-dispatch harness

Date: 2026-11-04

## Change

Added `tests/metal_core_cp3_osc_vca_real_dispatch_test.c` and wired it into
`tests/run_metal_core_fast.sh`.

The private test descriptor combines the existing per-voice OSC1 phase adapter
with the sample-domain VCA in one `engine_t` callback. The test compiles the
real Felucca host dispatcher and `track_render()` through `hostsim.c`.

## Contract exercised

- real note allocation invokes the combined descriptor;
- part state is declared through `engine_t.mem` and allocated through
  Felucca's `ENG_MEM` / `eng_mem_claim()`;
- OSC1 advances from the actual `vmod_t.inc`;
- VCA processing and oscillator rendering occur in the same per-voice callback;
- render output is additive and preserves a pre-existing mix value;
- sounding retrigger preserves oscillator phase and the instantaneous VCA level;
- real Note Off reaches Gate Off and `done()` retires the host voice after VCA
  release reaches idle.

## Scope limits

This is a CP3 integration harness, not production registration. It does not
change `engines.c`, engine IDs, parameter mapping, preset ABI, project storage,
Factory V15, or physical Flash. It does not establish target CPU timing or
hardware/audio qualification. OSC2, FM, METAL, FOLD, MAIN FILTER, LPG FILTER
and LPG remain subsequent integration stages.

## Verification

Exact-head Fast Regression and Full Baseline were queued by the branch update;
their final conclusions must be read from the corresponding Actions runs
before treating this harness as passed.
