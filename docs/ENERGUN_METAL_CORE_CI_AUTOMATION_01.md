# ENERGUN METAL CORE CI automation

The dedicated workflow `.github/workflows/energun-metal-core-fast.yml` runs on pushes and pull requests targeting `energun-metal-core`.

Runner: `tests/run_metal_core_fast.sh`.

The fast lane checks out pinned submodules, generates the repository headers with the existing build generator, then compiles and executes eleven focused host tests covering oscillator primitives, LPG, VCA, CP2.23 harness, CP3.0 real-track adapter, CP3.1 memory accounting, fractional feedback-delay lifecycle and closed-loop stability (0..0.8 gain, strict C11 `-Wall -Wextra -Werror`), and FOLLOW.

Both the fast lane and full baseline share the same pip download cache key, reducing repeated Python dependency downloads across workflows. It uploads a test log artifact for 14 days. It intentionally does not build the full firmware, compile the full Metal Core engine for AC79, register a production engine, alter persistence, access hardware, or flash the device.

The existing `.github/workflows/energun-fm1-baseline.yml` remains the full integration gate for standard firmware build, isolated AC79 target probes, and complete host/web regression. The baseline also compiles isolated VCA and feedback-delay arithmetic probes with the pinned AC79 pi32v2 compiler and archives their disassembly/symbol tables; this is code-generation inspection, not target execution or a full-engine build. The fast lane supplements rather than replaces it.

The runner uses the same hardware include paths as `tests/run_tests.sh` and the same generated headers as the full build. It fetches recursive pinned submodules because the existing table generator imports the engine submodule, and the host tests include Felucca BSP headers.

Next stages, gated on implementation evidence: target compiler probes for finalized DSP structures; exact per-part memory assertions after the complete typed state is frozen; and measured decomposition of the full regression suite.

## CI checkpoint — 2026-11-01

- Full baseline workflow: https://github.com/energundenis/Felucca/actions/runs/41322047870 — SUCCESS; all 16 principal steps completed.
- Exact tested source: 542126c0498f177f38f6f14525ac3042806cfdb7 on DSP work branch en erg un/cp3-1-biquad-prototype-01 (PR #1 head), not the coordination branch.
- Unmodified Felucca firmware build: PASS.
- Isolated ENERGUN VCA compile with pinned AC79 pi32v2 target compiler: PASS.
- Felucca Host Regression and Web Regression: PASS.
- Build output/size recording and experimental evidence upload: PASS.
- Companion fast regression: https://github.com/energundenis/Felucca/actions/runs/41322047771 — SUCCESS on the same SHA.
- Biquad candidate host regression + AC79 compile/disassembly probe: https://github.com/energundenis/Felucca/actions/runs/41322047859 — SUCCESS on the same SHA.
- AC79 biquad inspection is compile/code-generation evidence only, not runtime cycle timing or full-engine CPU proof.
- This verifies the tested DSP candidate commit only. It does not mean PR #1 is merged, METAL CORE is production-registered/playable, or the coordination branch has the same tested tree.
- Coordination branch HEAD at checkpoint start: e6877d47f6b5684efab1e2cf759ed37b2ea76cd0. Main remains untouched.
- No device write or firmware flash occurred; factory firmware remains unchanged.

Scope limitation: these runs verify the tested DSP commit and CI paths. AC79 biquad inspection is compile-only, not runtime timing. They do not prove full-engine resource budgets, production engine registration, FM-1 hardware operation, installability, or recovery. PR #1 remains unmerged.
