# ENERGUN METAL CORE CI automation

The dedicated workflow `.github/workflows/energun-metal-core-fast.yml` runs on pushes and pull requests targeting `energun-metal-core`.

Runner: `tests/run_metal_core_fast.sh`.

The fast lane checks out pinned submodules, generates the repository headers with the existing build generator, then compiles and executes ten focused host tests covering oscillator primitives, LPG, VCA, CP2.23 harness, CP3.0 real-track adapter, CP3.1 memory accounting, and FOLLOW.

It uploads a test log artifact for 14 days. It intentionally does not build the full firmware, compile the full Metal Core engine for AC79, register a production engine, alter persistence, access hardware, or flash the device.

The existing `.github/workflows/energun-fm1-baseline.yml` remains the full integration gate for standard firmware build, isolated AC79 target probes, and complete host/web regression. The fast lane supplements rather than replaces it.

The runner uses the same hardware include paths as `tests/run_tests.sh` and the same generated headers as the full build. It fetches recursive pinned submodules because the existing table generator imports the engine submodule, and the host tests include Felucca BSP headers.

Next stages, gated on implementation evidence: target compiler probes for finalized DSP structures; exact per-part memory assertions after the complete typed state is frozen; and measured decomposition of the full regression suite.