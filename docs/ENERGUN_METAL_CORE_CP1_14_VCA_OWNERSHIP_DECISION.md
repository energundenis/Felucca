# ENERGUN METAL CORE — CP1.14 VCA ownership decision

Date: 2026-10-31
Branch: energun-metal-core
Status: architecture decision accepted; implementation and numeric specification remain open.

## Accepted decision

The user approved preserving the original ENERGUN METAL CORE V98 amplitude-envelope behavior as the V1 target.

- Implement a dedicated engine-owned METAL CORE VCA envelope, based on the V98 trigger/gate behavior.
- Do not use the Felucca host ADSR as a second amplitude multiplier for METAL CORE voices.
- Do not multiply the host ADSR by the internal VCA. A two-envelope cascade is explicitly not the selected architecture.
- Felucca remains responsible for voice allocation, note/gate event delivery, shared voice budgeting and the overall audio path.
- The METAL CORE engine owns its amplitude contour and must coordinate voice completion with the host lifecycle contract (including `ownenv=1` / `done()`, subject to API verification and lifecycle tests).
- Preserve the independent LPG envelope and FM-index envelope. They must not be merged with VCA.
- Preserve V98 Velocity and Accent scaling, METAL-specific fast Attack behavior, retrigger semantics, Gate-off/Release behavior and supported note transitions, after exact source tracing and tests.

## Why this architecture

The V98 VCA is not behaviorally equivalent to the Felucca host ADSR. Felucca uses a block-ticked linear Attack and exponential Decay/Release; V98 schedules trigger-based gain segments, has its own peak scaling and special METAL Attack behavior. Using the host ADSR alone would change the reference contour. Cascading both envelopes would further alter peak, sustain and release.

The selected approach keeps the sound-shaping envelope inside the engine while retaining Felucca's voice manager and mixer responsibilities. This adds one per-voice envelope state and lifecycle integration, but does not require replacing the host's voice allocator or global audio path.

## Required implementation gates

1. Complete a line-by-line source trace of V98 VCA scheduling, including exact parameter units/ranges, defaults, time constants, velocity/accent scaling, METAL fast-attack override, Gate scheduling and cancellation/retrigger behavior.
2. Inspect Felucca engine API and host ADSR bypass/voice completion semantics for the exact pinned revision. Confirm whether `ownenv=1` and `done()` are the supported contract and document call timing.
3. Write an integer/sample-domain VCA specification, with explicit state transitions and fixed-point formats. No floating point, allocation or division in the sample loop.
4. Implement the VCA as an isolated DSP module with deterministic host tests before connecting it to the registered engine render callback.
5. Test attack/decay/sustain/release curves, short and long gates, retrigger during every phase, note stealing, legato/overlap, velocity/accent corners, METAL fast attack, zero times and voice completion.
6. Measure state memory and target CPU on the actual target build. The earlier 64-byte per-voice state estimate is provisional and must be updated to include VCA state.
7. Integrate only after tests and API/lifecycle gates pass; do not alter factory firmware or write to the physical device.

## Explicitly not authorized by this decision

This decision authorizes documentation and subsequent isolated specification/test work only. It does not authorize immediate engine registration, parameter-ID allocation, persistent-storage or Flash changes, firmware packaging, device writes, flashing or recovery operations.

## Relationship to prior CP1.9 note

This decision supersedes the earlier CP1.9 recommendation to compare host ADSR first and defer envelope ownership. That comparison has now established the selected direction: V98-compatible engine-owned VCA. The remaining work is exact specification and proof of the host lifecycle contract, not reconsideration of whether to cascade two envelopes.

## Status

CP1.14 architecture choice: RESOLVED.
CP1 overall: OPEN.
VCA source specification: OPEN.
Engine integration: NOT STARTED.
Physical device validation: NOT STARTED.
