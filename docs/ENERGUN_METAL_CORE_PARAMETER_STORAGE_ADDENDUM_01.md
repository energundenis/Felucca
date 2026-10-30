# METAL CORE parameter sidecar — motion-event addendum

This addendum supplements PARAMETER_STORAGE_PLAN_01.

The project sidecar must store more than the new P_MC parameter values. It must also store every project motion / parameter-lock event whose parameter ID refers to a P_MC parameter.

Reason: the base FUN10 project must retain its legacy parameter count and existing size. During old-project migration, the count-based loader interprets the final eight stored values as the old E0–E7 values and moves them to the current E0–E7 positions. A new P_MC ID cannot be placed in that same base motion block without risking it being interpreted as an old E parameter.

Therefore:
- Main FUN10 serialization contains legacy common parameters, E0–E7, and motion events for those IDs only.
- The sidecar contains the new P_MC values for all four tracks and motion events whose IDs refer to P_MC parameters.
- On load, restore the base project first, then load the matching sidecar and merge its P_MC values and events into RAM.
- On save, split P_MC events out before packing FUN10; commit the sidecar first, then commit the main project.
- The combined in-memory motion count must still satisfy the existing MOTION_MAX and motion_valid checks.
- A missing or invalid sidecar gives P_MC defaults and no P_MC motion events; the main project remains loadable.

The sidecar's project-generation link must be validated before merging any values or events. No code changes or storage allocation are made by this addendum.
