# ENERGUN FM-1 CORE — CP1.4.9 Archive V2 logical schema proposal

Date: 2026-11-03  
Branch: `energun-metal-core`  
Status: **SCHEMA PROPOSAL — NOT IMPLEMENTED; NO ARCHIVE V1 CHANGE**

## 1. Compatibility contract

Archive V1 remains byte-for-byte and behaviorally unchanged:

- `format` remains `felucca-backup`, `version: 1`.
- Its 13-, 12- and 11-object manifests remain accepted exactly as today.
- V1 restore remains the existing independent-object restore path.
- A V1 archive contains no Sidecar data. Restoring V1 must apply canonical Private/P_MC defaults for any destination pair whose Sidecar is absent; it must never retain an unrelated Sidecar from an older generation.

Archive V2 is a distinct manifest version. It reuses the existing `objects` array and adds a required `sidecars` array. It must not renumber or reinterpret V1 object IDs.

## 2. V2 top-level shape

```json
{
  "format": "felucca-backup",
  "version": 2,
  "firmware": "…",
  "created": "…",
  "objects": [ /* same object records and CRC rules as V1 */ ],
  "sidecars": [
    {
      "kind": "project",
      "index": 0,
      "base_id": 2,
      "base_crc32": 0,
      "schema": 1,
      "size": 117,
      "crc": 0,
      "data": "…"
    }
  ]
}
```

The example is illustrative, not a complete seven-entry archive. JSON integers for CRC fields are unsigned 32-bit values. Base64 must be canonical and decode to exactly `size` bytes. The `crc` is CRC-32 of the decoded logical Sidecar payload, not of its storage envelope.

The Sidecar payload is the explicit byte-codec output from `energun_sidecar_payload_v1.c`; C struct padding and the storage envelope are never archived.

## 3. Frozen identity mapping

Exactly seven logical Sidecar entries are represented, in this order:

| Entry | kind | index | base_id | Payload |
|---|---|---:|---:|---|
| Project A | `project` | 0 | 2 | Project/live payload codec |
| Project B | `project` | 1 | 3 | Project/live payload codec |
| Project C | `project` | 2 | 4 | Project/live payload codec |
| Project D | `project` | 3 | 5 | Project/live payload codec |
| Live runtime snapshot | `live` | 0 | 0 | Project/live payload codec |
| User preset bank A | `preset_bank` | 0 | 6 | 16-slot preset payload codec |
| User preset bank B | `preset_bank` | 1 | 7 | 16-slot preset payload codec |

The archive IDs above are existing logical backup protocol IDs. They are not storage object IDs, Sidecar sector addresses, or authorization to allocate physical Flash.

The live entry is intentionally named `live`, not `live_autosave`: archive object 0 is a snapshot of the current runtime project. It is not the persistent autosave storage object. The restore coordinator must explicitly convert this archived live snapshot into the destination's current autosave Base+Sidecar pair before restoring the runtime from that committed pair.

## 4. Binding and restore rules

Each entry's `base_crc32` must equal the CRC of the decoded V2 `objects` record whose ID equals `base_id`. This proves that the logical extension in the archive belongs to the exact Base bytes packaged beside it.

The archive must not preserve or replay a source device's Base storage sequence. That sequence is local to the source A/B store and is invalid on the destination. During restore:

1. Validate the complete V2 manifest, all Base objects, all seven Sidecar entries, decoded lengths, CRCs, identity uniqueness/order, payload codecs, and Base CRC bindings before the first destructive request.
2. Stage the Base and its logical Sidecar payload together.
3. For each stored project slot, use the pair coordinator to commit Sidecar first, then Base, read back and verify the exact new destination generation.
4. For each preset bank, do the same pair operation. The preset Sidecar's 16-bit slot bitmap must agree with the restored Base bank's occupied-slot map. Empty/deleted slots must have zero Private payload so a reused slot cannot inherit stale data.
5. Restore the separate user-preset FM6 patch object (archive ID 9) according to the existing compatibility order. Do not claim that this third object is atomic with the Base+Sidecar pair.
6. Restore settings as today.
7. For the live entry, pair-commit the archived live Base snapshot to the destination's live-autosave Base object with Sidecar identity `LIVE_AUTOSAVE`, verify the committed pair, then import that committed Base into runtime and atomically apply the typed Private cache. Do not import live Base first and leave its Sidecar only in volatile RAM.
8. On any failure, stop. Never report full restore success after a partial pair or partial archive restore.

The pair coordinator must rebind the logical payload to the destination's newly allocated Base sequence and CRC. It must not copy a source envelope or source sequence.

## 5. Validation constraints

A V2 reader rejects, before any write:

- unknown top-level version or format;
- missing or extra Sidecar entries (the seven-entry set is exact);
- duplicate `(kind,index)`, duplicate `base_id`, invalid kind/index pairs, or incorrect canonical order;
- a `base_id` not present exactly once in `objects`;
- mismatch between `base_crc32` and the decoded Base object's CRC;
- malformed Base64, decoded size mismatch, payload CRC mismatch, or unsupported Sidecar schema;
- project payloads that fail the 117-byte fixed prefix / 3-byte Motion-event length rule or the Sidecar semantic validator;
- preset payloads not exactly 210 bytes or with invalid slot bitmap / Private values;
- a preset Sidecar slot bitmap that differs from the actual occupied slots in its Base bank.

A missing Sidecar in V2 is not silently treated as V1. Only a V1 archive gets the V1 legacy-default policy.

## 6. Browser and firmware protocol boundary

The browser's V2 parser can validate the complete archive offline, but capture/restore must not be enabled until firmware supports a staged pair-aware protocol. Reusing the existing one-object PUT loop for Sidecar IDs is prohibited: it cannot atomically stage and commit a Base+Sidecar pair or rebind the Base generation.

Required protocol capability (symbolic commands/identities only; numeric command IDs remain unassigned):

- LIST/GET returns the seven logical Sidecar payloads and their Base association metadata.
- Pair PUT BEGIN identifies one logical Base identity and declares both Base and Sidecar lengths/CRCs.
- Pair PUT DATA stages both payloads without destructive writes.
- Pair PUT COMMIT validates both payloads and executes the Sidecar-first/Base-second coordinator.
- Pair PUT ABORT clears staging and leaves the previous matching pair intact.
- Pair operations return the destination Base generation/fingerprint for read-back verification.

The protocol must use symbolic identity kinds and indices. No raw Flash address, sector number, storage enum value, or unapproved object ID is exposed to the browser.

## 7. Failure and recovery matrix

| Failure point | Required outcome |
|---|---|
| Offline archive validation | No device request and no write |
| Staging Base or Sidecar interrupted | Abort staging; previous pair unchanged |
| Sidecar write torn | Previous matching Sidecar remains selectable; Base not committed |
| Sidecar verified, Base commit fails | New Sidecar is an orphan; old matching pair remains selectable |
| Base commit succeeds, readback fails | Return failure; recovery re-reads both A/B copies and selects only exact Base-bound Sidecar |
| Destination Base sequence differs from source | Expected; rebind Sidecar to destination sequence |
| Preset slot absent in Base bank | Corresponding Sidecar bitmap bit and Private values must be clear |
| Live pair committed, runtime import fails | Persistent pair remains valid; report runtime import failure, do not claim success |
| Archive restore interrupted between logical objects | Stop and report partial restore; already committed objects remain individually valid |

## 8. Implementation sequence and acceptance

1. Freeze this logical V2 schema and add browser-only fixtures for valid/invalid V2 manifests without enabling device writes.
2. Add a firmware-side archive staging descriptor that holds Base + Sidecar in RAM and validates all sizes before any storage operation.
3. Add pair-aware symbolic LIST/GET/PUT handlers backed by an approved Sidecar storage adapter.
4. Add browser capture and restore V2 paths; keep V1 parser/fixtures untouched.
5. Add host tests for all seven identities, every failure point in the table, V1 compatibility, wrong Base binding, all 32 preset slots, live restore, and separate FM6 patch ordering.
6. Run focused fast regression and full baseline CI on the exact code HEAD.

CP1.4.9 is not closed by this proposal. It remains open until firmware and browser V2 paths, pair recovery tests and exact-HEAD CI all pass.

## 9. Explicit non-goals

- No Sidecar physical Flash allocation or storage object ID assignment.
- No changes to Archive V1 or its accepted 11/12/13-object manifests.
- No claim of whole-archive atomicity across projects, banks, settings, samples and FM6 patch data.
- No production call-site integration, firmware release, factory image change or physical FM-1 write.
