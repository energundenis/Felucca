# ENERGUN METAL CORE — CP1.13 FM CHARACTER startup decision

Date: 2026-10-31
Branch: energun-metal-core
Status: user-approved verification procedure; final default remains pending source/runtime evidence.

## Decision accepted

The user approved the proposed approach for resolving the V98 FM CHARACTER initialization discrepancy.

Do not choose CLASSIC or METALLIC based on preference or on one source declaration alone. Preserve the original V98 behavior until the actual startup state is established.

## Required verification

Inspect the protected, unchanged V98 reference and trace:

1. All initialization sites for FM CHARACTER.
2. Whether startup state is taken from the JavaScript parameter object or the HTML select element.
3. The final selected UI value after complete initialization.
4. The value actually read by the FM-index calculation/render path.
5. Whether a later initialization or event handler overwrites either state.

Test/record the three available modes (SMOOTH, CLASSIC, METALLIC) sufficiently to identify the effective startup mode and its corresponding FM-index response.

## Acceptance rule

- If the original V98 runtime starts and renders METALLIC, retain METALLIC as the V1 default.
- If it starts and renders CLASSIC, retain CLASSIC as the V1 default.
- If UI state and DSP state disagree, document the mismatch and obtain an explicit resolution before freezing the V1 default.
- Do not silently “fix” the reference discrepancy while claiming exact V98 parity.

## Scope and safety

- Parameter #12, FM CHARACTER, remains open pending this verification.
- The other 36 controls are unaffected by this decision.
- This is a source/reference verification task, not an authorization to modify V98, firmware, parameter IDs, editor pages, storage, Flash, factory firmware, or the physical FM-1.
- main remains untouched. The protected V98 reference and known-good project files must remain unchanged; any diagnostic copy must be separate.

## Status

The verification procedure is approved. The effective startup default has **not** yet been established, so CP1 is not closed.
