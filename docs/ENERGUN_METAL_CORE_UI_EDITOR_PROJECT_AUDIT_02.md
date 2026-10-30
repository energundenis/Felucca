# ENERGUN METAL CORE — UI / Editor Protocol / Project Capacity Audit 02

Date: 2026-10-30
Branch: `energun-metal-core`
Status: source audit only. No firmware code or storage format changed.

## Executive result

The previous parameter audit correctly found that Felucca's eight engine EDIT values are not the only persistent track parameters. However, a critical additional constraint has now been verified: **the current project format has room for only four more per-track parameter IDs**, not the 16 IDs suggested by the global 7-bit parameter-ID ceiling.

Therefore, do not add the full OSC/FM/FOLD/LPG control set to `P_COUNT` yet. A project-storage strategy is required first.

## 1. Confirmed control/editor protocol behavior

Inspected `firmware/src/editor.c`:

- `ED_INFO` reports `P_COUNT` and `P_E0` dynamically.
- `ED_DUMP` iterates all `P_COUNT` track parameters.
- `ED_GET`, `ED_SET` and `ED_DESC` accept parameter IDs below `P_COUNT`; descriptors are obtained through `param_desc_of()`.
- `ED_TRACK_PARAM` validates IDs against `P_COUNT` and uses the same descriptor/range mechanism.
- Live-sync shadows allocate `P_COUNT + G_COUNT` values and compare the whole selected track parameter array.
- User-preset get/put and track dump paths iterate `P_COUNT`.

Conclusion: the editor protocol itself is already count-driven. Adding IDs before `P_E0` can flow through the protocol without inventing a new command, but any companion web editor must use the updated device-reported count and descriptor list rather than hard-code the old count.

## 2. Parameter locks and automation

Inspected `core.h`, `editor.c`, `project.c`:

- Motion event parameter IDs are seven-bit values; bit 7 is reserved as `MOTION_LOCK`.
- `P_COUNT` must remain below 128.
- `ED_MOTION` uses the same parameter ID model.
- `ED_SET` and `ED_TRACK_PARAM` call `motion_capture()`.
- UI lock-page selection uses `motion_param(id)`, so newly added parameter IDs can be lockable if their descriptors and motion eligibility allow it.
- Project motion records store IDs in one byte and the project loader remaps old IDs at/after the old E0 boundary when the parameter count grows.

Conclusion: automation/locks are structurally compatible with new IDs before E0, subject to explicit tests of recording, lock capture, playback, clear, and old-project migration. The ID ceiling remains a hard limit.

## 3. UI page architecture

Inspected `params.c`, `ui.c`, `ui_input.c`:

- Pages are held in a static `PAGES[]` table. Each page has a title, family, scope, graph type and four parameter IDs.
- The UI already supports engine-specific page visibility. Existing examples include ANALOG FILTER, DRUM LANES, FM6 operator pages and SLICES.
- `page_visible()` is the natural place to restrict Metal Core pages to the Metal Core engine.
- `page_desc()` resolves the four page controls through `track_desc()`, which obtains value pointers from `TSEL->p[]`.
- The four physical page knobs are therefore naturally suited to pages containing up to four independently stored parameters.
- Page family navigation cycles pages belonging to a family; page entries do not need new physical encoders.
- Parameter-lock source selection also examines the page's parameter IDs.

Conclusion: the existing page system provides a plausible route for Metal Core-specific pages. A new generic page framework is not yet justified. The next UI audit should verify exact family cycling, button shortcuts, and display/graph behavior for the proposed page set.

## 4. Critical project-storage ceiling

Inspected the format accounting comment at the top of `project.c` and the serializer `proj_pack()`:

- Current project format FUN10 is fixed at 3840 bytes (`PROJ_STORE_SIZE` / storage payload).
- Current `P_COUNT = 111`, with `P_E0 = 103`.
- The current format's packed content is accounted as 3268 bytes before the FM6 patch area at offset 3312, leaving 44 bytes.
- The source explicitly states those 44 bytes permit only 11 more track parameters in raw byte terms, but 1.5 already consumed seven of them, leaving **four** additional track parameter bytes per track.
- The current project packer writes one byte per track parameter for each of four tracks. Therefore each new common parameter costs four bytes in a project.
- The project format is a fixed-size storage object; merely raising `P_COUNT` does not enlarge the available payload.
- The loader validates the stored count and rejects a project with `np > P_COUNT`; old firmware also rejects a newer FUN10 with more parameters than it knows.

This is the blocking constraint. While the global parameter-ID ceiling leaves 16 theoretical IDs (111 -> 127), the current project layout only has capacity for four more common IDs before its existing reserved space is exhausted.

## 5. User-preset capacity

Inspected `upreset.c`:

- User-preset records reserve 144 bytes for parameter values (`UP_PMAX * 2`).
- The current record stores `P_COUNT` values and has a compile-time capacity guard.
- It therefore has much more headroom than the current project format.
- This does not solve project capacity: a sound may save to a user preset but fail to fit in a project unless both persistence paths are designed consistently.

## 6. Architecture options to investigate (not yet approved)

A. **Use at most four new common IDs.**
- Lowest-risk immediate path.
- Insufficient for every independently controllable Metal Core parameter.
- Remaining controls would need to use existing global/per-track Felucca parameters or a carefully justified engine-specific representation.

B. **Version and redesign project storage.**
- Requires a new project magic/version and explicit backward reader for FUN10.
- Must determine whether the 3840-byte storage payload can be increased safely across flash layout, RAM slots, backup protocol, and editor protocol.
- Increasing the record size may affect the storage allocator, slot offsets, backup/restore, checksum, retained RAM cache, and compatibility with old firmware.
- Do not assume this is possible before auditing those layers.

C. **Add a compact engine-specific extension block.**
- Could store extra Metal Core values only for tracks using that engine, reducing overhead compared with adding many common IDs to all four tracks.
- Still needs a versioned on-disk format and an explicit load/save path for project files and user presets.
- Must define defaults, validation, migration, engine-switch behavior and motion/automation semantics for extension parameters.
- This is potentially the best scalable solution, but needs a dedicated design audit.

D. **Use the existing eight E slots for a limited set and a project extension for the rest.**
- Could reduce the number of new common IDs.
- Must not collapse independent controls into macros merely to avoid storage work.
- Any E-slot assignments must be designed only after the full FMini-1 page/control matrix is complete.

## 7. Required next audit before implementation

1. Inspect `storage.c`, `storage.h`, flash layout/linker constants and backup/restore paths to establish whether project payload size can grow.
2. Inspect project slot/cache sizing and all `PROJ_STORE_SIZE` consumers.
3. Inspect editor backup protocol and web editor assumptions about project size and parameter counts.
4. Inspect user-preset save/load migration in full and decide whether extension parameters belong in the existing record or a versioned patch payload.
5. Build a complete parameter matrix for the approved Metal Core controls, distinguishing:
   - existing Felucca common parameters,
   - the eight engine EDIT slots,
   - new common IDs (maximum four under current FUN10),
   - possible engine-specific extension values.
6. Only after the storage strategy is selected should page IDs and final page layout be frozen.

## 8. Implementation status

No source files, parameter IDs, UI pages, DSP, project format, or user-preset format were modified in this audit. No firmware build, hardware test or flash was performed.
