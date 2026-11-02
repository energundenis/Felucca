# ENERGUN FM-1 CORE — CP1.4.3 / CP1.4.4 Private access and Motion Lock implementation design

Date: 2026-11-02  
Branch: `energun-metal-core`  
Status: **DESIGN DRAFT — no implementation authorization implied**

## 1. Scope and hard boundaries

This draft translates the source audit into an implementation contract for:
- CP1.4.3: the 13 Private values owned independently by each of four parts;
- CP1.4.4: explicit Motion Lock eligibility for the 16 common P_MC controls.

This document assigns no numeric parameter IDs, editor command numbers, UI page IDs, sidecar byte offsets, Flash sectors or stable binary ABI. It does not alter production source or authorize firmware integration.

Dependencies:
- CP1.4.1 logical membership is now reconciled against the earlier accepted E0–E7 freeze; numeric IDs remain unassigned until CP1.4.2 encoding/compatibility and CP1.4.5 Sidecar gates are complete.
- CP1.4.2 must complete per-control curve review and legacy compatibility decisions.
- CP1.4.5 must define the shared Sidecar schema before persistent implementation.
- The current factory image and physical device remain untouched.

## 2. CP1.4.3 — Private runtime model

### 2.1 Ownership

The 13 Private values belong to a part, not to the currently selected engine instance:
- Four independent part banks.
- Each bank contains the same 13 logical controls and V1 defaults.
- Values survive engine changes, voice stealing, audio-engine memory reuse and track selection changes.
- They are not placed in `track_t.p[]` as fake common IDs and never placed in `engine_t.mem`.

Recommended logical runtime shape (illustrative only; not a serialized structure):

```c
typedef struct {
    uint16_t schema_version;
    uint16_t generation;
    uint16_t dirty_mask;
    uint8_t  valid;
    private_value_t value[PRIVATE_KEY_COUNT];
} private_part_state_t;

static private_part_state_t private_part[NPART];
```

The exact C representation, alignment and memory cost must be measured on the AC79 target before implementation. The type `private_value_t` is conceptual; continuous, enum and boolean values must be validated according to their descriptors.

### 2.2 Access contract

All reads/writes go through one typed API. UI, editor transport, preset handling and engine adapters must not write the cache directly.

Conceptual operations:
- `private_defaults(part)`: install deterministic V1 defaults and mark the bank as default-derived, not persisted.
- `private_get(part, logical_key, out)`: validate part/key and return the typed current value.
- `private_set(part, logical_key, value)`: validate type/range, update only the selected key, increment the in-memory generation and set the dirty bit.
- `private_snapshot(part, out)`: copy a consistent snapshot for save/editor operations.
- `private_replace(part, snapshot, expected_schema)`: validate the complete snapshot before atomic replacement; partial invalid snapshots are rejected.
- `private_mark_persisted(part, generation)`: clear dirty state only if the saved generation still matches the snapshot generation.

The API must distinguish invalid part, unknown key, wrong type, out-of-range value, unsupported schema and stale generation. No silent key aliasing or clamping of invalid enum/boolean values.

### 2.3 Lifecycle

1. Power-on: initialize all four banks to deterministic V1 defaults.
2. Project load: load the base FUN10 first; then load the matching Sidecar identity. If extension is absent, invalid or unsupported, keep defaults and still load the base project.
3. Part selection: change only the active part reference; do not copy another part's Private state.
4. Engine change: preserve Private state; engine-owned DSP memory may be reclaimed independently.
5. UI/editor edit: update the selected part via typed setter; mark the relevant project identity dirty. Never write Flash from the audio ISR.
6. Project save/autosave: take a generation-stamped snapshot and pass it to the CP1.4.5 transactional Sidecar layer.
7. User preset save: capture the selected part's Private values into the matching preset-slot extension record. User preset load restores that record to the selected part; if absent, apply defaults for that preset load without modifying other parts.
8. Backup/restore: include Private records for all project identities and all 32 user-preset slots; legacy archives restore defaults and must not attach stale extensions.

### 2.4 UI and editor transport

Source-specific protocol constraint:
- Firmware `ED_INFO` already uses tagged capability blocks (tag, version, payload). The current web parser recognizes a fixed sequence of tags and fixed offsets; it is not a general unknown-TLV iterator.
- Any future Private capability must therefore be appended after the current final known tag, never inserted between existing tags. The web parser must be updated to recognize the new tail while old clients continue ignoring trailing bytes.
- A new command must be capability-probed before use. Unknown command IDs receive no reply, so the editor must time out/fallback safely rather than assuming support.
- Update `web/EDITOR_PROTOCOL.md`, the real `web/editor.html` parser and `tests/editor_test.c` / `tests/test_web.mjs` together. Host-only parser tests do not establish physical SysEx interoperability.

- Private controls are displayed on a dedicated Metal Core page family, separate from the eight Engine EDIT destinations.
- The UI uses the typed access API; it does not use `param_desc_of()` for Private keys.
- Common P_MC controls remain in the common parameter transport after their IDs are approved. Private values do not enter `ED_DUMP` or pretend to be common `P_COUNT` entries.
- Add a version/capability-probed Private editor extension using new symbolic operations for INFO, GET, SET and full-part REPLACE. Numeric command values and payload encoding are deferred until the protocol version and Sidecar schema are frozen.
- GET replies include schema version and generation. SET includes the expected generation or an explicit compare-and-set policy so stale editor writes cannot overwrite a newer snapshot.
- Older editors continue using existing ED_INFO/ED_GET/ED_SET/ED_DUMP behavior. A client that does not advertise the Private capability simply does not expose the Private page.
- Full-part REPLACE validates every field before changing the runtime cache; no partially applied editor packet.

## 3. CP1.4.4 — P_MC Motion Lock policy

### 3.1 Whitelist

- Add one explicit symbolic whitelist for exactly the approved continuous P_MC controls.
- Do not use a broad `id < P_E0` rule: that would admit unrelated common controls such as transport, sequencer, mixer and routing parameters.
- Private values, booleans, enums and MCX0–MCX6 remain excluded from V1 Motion Lock.
- The final whitelist is generated from the approved logical inventory, not from numeric ranges guessed before CP1.4.1 closes.

### 3.2 Value representation

- Motion event storage remains signed int8 with the existing -64..127 container range.
- Every whitelisted P_MC descriptor must have an explicit reversible conversion between its 7-bit parameter code and motion value domain.
- For ordinary unipolar controls, the initial candidate is direct 0..127 mapping.
- For signed bipolar FOLD SYMMETRY, the candidate is the existing signed descriptor domain -64..63, not the generic project container range.
- The final conversion must be shared by capture, editor SET, project import/export, playback and lock restore. No path may use a different scaling.
- Validate the decoded motion value against the target descriptor before inserting or applying an event.

### 3.3 Reconciled symbolic P_MC whitelist and raw value domains

The earlier user-approved E0–E7 freeze is authoritative. The 16-control P_MC set is reconciled in `ENERGUN_FM1_CORE_CP1_4_1_E0_E7_RECONCILIATION.md`. OSC2 LEVEL and TIMBRE are E1/E2 and are not P_MC whitelist members; FM INDEX and LPG DECAY are P_MC members.

| Logical control | Candidate Motion value domain | Notes |
|---|---:|---|
| OSC2 DETUNE | 0..127 | Unipolar 7-bit code; decode through the same descriptor curve used by the parameter |
| FM INDEX | 0..127 | Unipolar 7-bit code |
| MOD RATIO | 0..127 | Unipolar 7-bit code; nonlinear display conversion is shared with the parameter |
| AM DEPTH | 0..127 | Unipolar 7-bit code |
| FM FEEDBACK | 0..127 | Unipolar 7-bit code |
| FM ENV AMT | 0..127 | Unipolar 7-bit code |
| FM ENV ATTACK | 0..127 | Unipolar 7-bit code; logarithmic display conversion is shared with the parameter |
| FM ENV DECAY | 0..127 | Unipolar 7-bit code; logarithmic display conversion is shared with the parameter |
| METAL DECAY | 0..127 | Unipolar 7-bit code; logarithmic display conversion is shared with the parameter |
| FOLD SYMMETRY | -64..63 | Centered signed 7-bit code; physical zero is raw 0 |
| WAVEFOLDER DRIVE | 0..127 | Unipolar 7-bit code |
| LPG RESPONSE | 0..127 | Unipolar 7-bit code |
| LPG DECAY | 0..127 | Unipolar 7-bit code; logarithmic display conversion is shared with the parameter |
| VOICE VCA LEVEL | 0..127 | Unipolar 7-bit code |
| WAVEFOLDER MODULE VOLUME | 0..127 | Unipolar 7-bit code |
| WEST COAST VOLUME | 0..127 | Unipolar 7-bit code |

The Motion event should store the same stored parameter code that `param_desc_of()` validates; physical display values are obtained through the single CP1.4.2 descriptor conversion. Do not introduce a second Motion-specific scale. The only bipolar exception in this candidate is FOLD SYMMETRY, whose legal signed descriptor range is -64..63.

### 3.4 Capacity and behavior


- Automation and Parameter Lock continue sharing one global pool of 128 records across all four tracks.
- Sidecar-stored P_MC motion records are not a second event pool. At runtime, the merged event count must never exceed 128.
- Duplicate key is (track, step, parameter). Update replaces the existing record; append at capacity fails without modifying prior records.
- Preserve the MOTION_LOCK high bit while translating the 7-bit parameter ID.
- Locks must restore the latest automation/base value using the existing motion lifecycle; chance-skipped steps, track clear, stop, retrigger and project reload require regression coverage.
- If a sidecar's P_MC motion records plus base FUN10 motion exceed 128, reject the extension atomically or apply the documented safe-default policy. Never truncate silently.

## 4. Required implementation sequence

1. CP1.4.1 logical ordering/overlap is reconciled; close the remaining CP1.4.2 encoding/compatibility gates.
2. Freeze the typed Private logical descriptor table (symbolic keys, types, defaults, ranges).
3. Freeze the Sidecar identity and generation model in CP1.4.5.
4. Implement the per-part runtime cache and typed access API.
5. Add capability-probed editor Private operations and the engine-specific UI page family.
6. Add the explicit P_MC whitelist and shared value conversion.
7. Integrate project/preset/archive persistence through the same Sidecar transaction layer.
8. Add regression tests before production engine registration.

## 5. Required test matrix

### Private state
- Four parts retain distinct sentinel values while selection and engine change repeatedly.
- DSP engine-memory ownership changes do not alter Private values.
- Defaults are deterministic on missing, old, malformed and unsupported Sidecars.
- Invalid part/key/type/schema and stale generation are rejected without partial mutation.
- Editor GET/SET/REPLACE agrees with UI and runtime cache.
- Project save/load and all 32 user-preset slot save/load paths round-trip all 13 Private values.
- Interrupted save/restore preserves an old or new matching generation, never a mixed record.

### Motion Lock
- Every approved P_MC key is accepted; every non-whitelisted common/Private/enum/boolean/MCX key is rejected.
- Boundary values, signed FOLD SYMMETRY zero, min/max and invalid values are covered.
- Motion capture, editor update, save/load, playback and lock restore use identical conversion.
- Mixed four-track automation/lock records share the 128-event cap.
- Duplicate replacement, full-pool refusal, deletion/reuse, chance skip, stop, clear and reload are tested.
- Malformed sidecar motion records, duplicate keys, invalid IDs and combined count >128 are rejected atomically.

## 6. Acceptance boundary

This draft is a design contract only. It does not:
- implement a runtime cache or new API;
- add editor commands or UI pages;
- modify `motion_param()` or motion event encoding;
- choose stable IDs or binary fields;
- choose a Sidecar format or Flash sectors;
- change factory firmware or access the physical FM-1.

**CP1.4.3 and CP1.4.4 remain OPEN until the implementation and test matrix pass.**
