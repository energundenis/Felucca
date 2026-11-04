# ENERGUN METAL CORE — CP2.24 explicit LPG Gate-off and retrigger contract

Date: 2026-11-04
Branch: `work/cp2-lpg-cutoff-bounds-20261104`
Status: isolated helper and host regression implemented; not connected to the production render callback.

## Scope

This envelope is the LPG gain envelope. It is independent of the voice VCA envelope. The LPG low-pass filter frequency automation is also a separate state/trajectory and is not changed by this helper.

## Event contract

`eng_metal_lpg_env_gate_off(env)`:

- Null and IDLE states are no-ops.
- For an active ATTACK or DECAY, sets the target to the documented LPG floor and selects the previously prepared decay coefficient.
- Preserves the instantaneous Q15 level; no sample is processed by the event helper, so the event itself cannot create a level discontinuity.
- Clears the pending tail countdown and enters DECAY.
- Repeated calls during DECAY preserve the same target/phase and do not restart an attack.

`eng_metal_lpg_env_trigger(env, peak, timing)` after Gate-off:

- Retains cancel-and-hold behavior: the current level is not reset to zero.
- Replaces the target with the new peak and starts ATTACK using the new timing.
- Thus a new trigger before the decay completes cancels the pending decay and rises from the instantaneous held value.

The sample processor remains bounded fixed-point work: no allocation, floating point, table lookup or division in the per-sample path. Attack/decay coefficients are prepared by the event-time helpers.

## Host regression evidence

`tests/metal_core_lpg_env_test.c` covers:

1. Gate-off on IDLE is a no-op.
2. Gate-off during ATTACK changes phase/target but preserves the current level.
3. The following sample moves monotonically toward the LPG floor.
4. Repeated Gate-off is idempotent.
5. Retrigger during ATTACK and DECAY holds the instantaneous level.
6. Retrigger after explicit Gate-off cancels decay and holds the current level.
7. Zero-length Gate enters DECAY immediately; invalid/null inputs remain safe.

Exact tested CP2 branch head: `a818e115e27dea2b463cb29cdd08f2dda488c962`.
Cross-CP orchestrator run #6: https://github.com/energundenis/Felucca/actions/runs/41802864500 — SUCCESS. CP2 host lane passed, including the post-Gate-off retrigger assertion.

## Integration boundary

This helper is not yet called by the production ENERGUN engine because the engine remains unregistered and the complete render chain is not integrated. It does not establish sample-accurate sub-block event timing. Gate observation remains limited by the current Felucca callback/event interface and must be explicitly handled at the approved render boundary.

No engine ID, production registry, parameter ABI, preset format, Sidecar/Flash map, Factory V15, or physical device state is changed.
