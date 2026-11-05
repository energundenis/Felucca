# ENERGUN FM-1 CORE — CP1 pair recovery verification checkpoint

Date: 2026-11-04  
Branch: `energun-metal-core`  
Implementation commit: `9875c4450bf02531aebfbe5fddbaf4bf99a8901f`

## Completed in this checkpoint

The host-only pair transaction acceptance harness now performs an exhaustive byte-cut sweep across the Sidecar record and its storage header while replacing an already valid Base+Sidecar pair.

For each injected cut point, the test:

1. Seeds a valid generation-1 pair.
2. Interrupts the next Sidecar write at the selected byte.
3. Requires the attempted pair commit to fail before Base commit.
4. Disables fault injection and reloads the pair.
5. Requires the original Base generation, Base payload, and matching Sidecar copy to remain selected.

This supplements the existing Base torn-write sweep, orphan/retry case, newest-Base corruption fallback, and seven-identity roundtrip matrix.

## Safety boundary

The test Sidecar sectors remain an isolated simulated backend. This checkpoint does not allocate physical Flash, change the target storage map, or connect production project/preset call sites. No Factory V15 or `main` changes are included.

## Acceptance still required

- Run `sh tests/run_sidecar_pair_transaction_test.sh` on the committed HEAD.
- Confirm Fast Regression and Full Baseline for the exact HEAD.
- Continue target-side Sidecar backend/map review before production pair ordering can be unblocked.
- Wire and verify actual project/live and preset call sites before declaring CP1 recovery complete.

## Exact-HEAD CI acceptance follow-up — 2026-11-04

The host fault-injection acceptance requested above is now verified on exact branch/test HEAD 78cc46c39b5b7599c894a2a144cea0f649105beb:

- Fast Regression: https://github.com/energundenis/Felucca/actions/runs/41734515051, run #445 — SUCCESS.
- The strict sidecar_pair_transaction compile and executable test passed inside the fast workflow's CP1 queue. The log explicitly reports PASS for the Sidecar torn-write sweep, Base torn-write sweep, and the added Sidecar record/header byte-cut sweep.
- The fast workflow reports all 24 focused test programs/suites passed. The CP1 queue itself correctly ends BLOCKED at the production pair-order/recovery gates; that is an intentional architecture gate, not a failing host test.
- Full Baseline: https://github.com/energundenis/Felucca/actions/runs/41734515025, run #572 — SUCCESS. The unchanged Felucca firmware build, full host/web regression, and AC79 pi32v2 compile-only probes for VCA, Sidecar V1, payload codecs, Private cache/bridge, pair coordinator and lifecycle adapters completed successfully.

This satisfies the previous checkpoint's exact-HEAD host-test and CI acceptance items. It does not close production recovery: no target Sidecar backend or approved physical Flash allocation exists, and the real project/live/autosave/preset call sites and runtime-cache restoration are not wired. The AC79 results are compile/code-generation evidence only; there is no target runtime or physical-device validation.

PR #5 remains open and unmerged. Its base is the isolated validation branch validation/cp1-pair-recovery-20261104; do not merge it into the project branch until the separate target-backend, Flash/recovery and runtime-integration gates are reviewed.
