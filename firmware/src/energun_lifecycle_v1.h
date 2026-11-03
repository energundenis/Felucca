/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — project/live lifecycle adapter for Base + Sidecar V1.
 *
 * Orchestrates project save/load/rename/Save As/erase and live autosave on top
 * of the backend-neutral pair coordinator. Storage callbacks remain injected;
 * this module does not assign target Flash addresses or provide a backend.
 */
#ifndef ENERGUN_LIFECYCLE_V1_H
#define ENERGUN_LIFECYCLE_V1_H

#include "energun_pair_v1.h"

typedef int (*energun_lifecycle_v1_guard_fn)(void *context);

typedef struct {
    const energun_pair_v1_ops_t *ops;
    void *storage_context;
    energun_pair_v1_workspace_t *workspace;
    const uint8_t *canonical_sidecar_defaults;
    uint16_t canonical_sidecar_defaults_length;
    uint16_t base_motion_count;
    energun_lifecycle_v1_guard_fn write_guard;
    void *guard_context;
} energun_lifecycle_v1_t;

typedef enum {
    ENERGUN_LIFECYCLE_V1_OK = 0,
    ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS = 1,
    ENERGUN_LIFECYCLE_V1_ERR_ARGUMENT = -20,
    ENERGUN_LIFECYCLE_V1_ERR_GUARD = -21,
    ENERGUN_LIFECYCLE_V1_ERR_CAPACITY = -22,
    ENERGUN_LIFECYCLE_V1_ERR_PAIR = -23
} energun_lifecycle_v1_status_t;

/* Save a complete project/live snapshot as one Base+Sidecar pair. */
energun_lifecycle_v1_status_t energun_lifecycle_v1_save(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    const uint8_t *base_payload, size_t base_length,
    const uint8_t *sidecar_payload, uint16_t sidecar_length);

/* Load a project/live pair. Legacy Base-only objects return LEGACY_DEFAULTS
 * and receive the caller-supplied canonical Sidecar payload in the output.
 * No runtime cache is changed here; the caller applies the typed bridge only
 * after Base deserialization and engine validation have succeeded.
 */
energun_lifecycle_v1_status_t energun_lifecycle_v1_load(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    uint8_t *sidecar_payload, size_t sidecar_capacity, size_t *sidecar_length,
    energun_pair_v1_base_meta_t *base_meta);

/* Rename and Save As share one safe primitive: load the STORED source pair,
 * preserve its stored Sidecar (or canonical defaults for legacy Base-only),
 * and commit the caller-provided destination Base under a distinct identity.
 * The caller must construct renamed_base from the stored Base with only the
 * name changed; unsaved live performance state must not be substituted.
 */
energun_lifecycle_v1_status_t energun_lifecycle_v1_copy_stored(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t source_object, energun_sc_identity_kind_t source_kind,
    uint8_t source_index,
    uint32_t destination_object, energun_sc_identity_kind_t destination_kind,
    uint8_t destination_index,
    const uint8_t *destination_base, size_t destination_base_length);

/* Erase is a verified Base tombstone; Sidecar sectors are not erased. */
energun_lifecycle_v1_status_t energun_lifecycle_v1_erase(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object);

/* Live autosave uses the same pair transaction and refuses to begin while
 * the injected FM6 staging guard reports the autosave copy is reserved.
 */
energun_lifecycle_v1_status_t energun_lifecycle_v1_autosave(
    const energun_lifecycle_v1_t *lifecycle,
    uint32_t object, const uint8_t *base_payload, size_t base_length,
    const uint8_t *sidecar_payload, uint16_t sidecar_length);

#endif
