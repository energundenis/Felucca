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
