/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — backend-neutral Base + Sidecar V1 pair coordinator.
 *
 * This module defines transaction ordering only. It does not assign Flash
 * addresses or provide a target Sidecar backend.
 */
#ifndef ENERGUN_PAIR_V1_H
#define ENERGUN_PAIR_V1_H

#include "energun_sidecar_v1.h"

#define ENERGUN_PAIR_V1_RECORD_MAX ENERGUN_SC_V1_MAX_RECORD

typedef struct {
    uint8_t copy;
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t payload_crc32;
} energun_pair_v1_base_meta_t;

typedef struct {
    /* 0 = valid current Base, 1 = no valid Base, negative = I/O error. */
    int (*base_current)(void *context, uint32_t object,
                        energun_pair_v1_base_meta_t *meta);
    /* Read the exact selected Base copy and return matching metadata. */
    int (*base_load_current)(void *context, uint32_t object,
                             uint8_t *payload, size_t capacity,
                             energun_pair_v1_base_meta_t *meta);
    /* Commit to the explicitly selected inactive Base copy; must verify it. */
    int (*base_write_copy)(void *context, uint32_t object, uint8_t copy,
                           const uint8_t *payload, size_t length);
    /* 0 = valid copy, 1 = empty/invalid copy, negative = I/O error. */
    int (*sidecar_read_copy)(void *context, energun_sc_identity_kind_t kind,
                             uint8_t index, uint8_t copy, uint8_t *record,
                             size_t capacity, size_t *length,
                             uint32_t *storage_sequence);
    /* Must commit header last and verify the complete Sidecar record. */
    int (*sidecar_write_copy)(void *context, energun_sc_identity_kind_t kind,
                              uint8_t index, uint8_t copy,
                              const uint8_t *record, size_t length,
                              uint32_t storage_sequence);
} energun_pair_v1_ops_t;

typedef struct {
    uint8_t records[2][ENERGUN_PAIR_V1_RECORD_MAX];
    uint8_t verify_record[ENERGUN_PAIR_V1_RECORD_MAX];
    energun_sc_candidate_t candidates[2];
} energun_pair_v1_workspace_t;

typedef enum {
    ENERGUN_PAIR_V1_OK = 0,
    ENERGUN_PAIR_V1_ERR_ARGUMENT = -1,
    ENERGUN_PAIR_V1_ERR_BASE_IO = -2,
    ENERGUN_PAIR_V1_ERR_SIDECAR_IO = -3,
    ENERGUN_PAIR_V1_ERR_SIDECAR_FORMAT = -4,
    ENERGUN_PAIR_V1_ERR_NO_MATCH = -5,
    ENERGUN_PAIR_V1_ERR_BASE_COMMIT = -6,
    ENERGUN_PAIR_V1_ERR_BASE_VERIFY = -7,
    ENERGUN_PAIR_V1_ERR_CAPACITY = -8
} energun_pair_v1_status_t;

/* Sidecar is committed and read-back validated first. Only then is the
 * inactive Base copy committed. A failed Base commit leaves an orphan
 * Sidecar candidate while the previous matching pair remains protected.
 */
energun_pair_v1_status_t energun_pair_v1_commit(
    const energun_pair_v1_ops_t *ops, void *context,
    energun_pair_v1_workspace_t *workspace,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    const uint8_t *base_payload, size_t base_length,
    const uint8_t *sidecar_payload, uint16_t sidecar_length,
    uint16_t base_motion_count);

/* Commit a zero-length Base tombstone to the inactive copy. Existing Sidecar
 * records become harmless orphans because no record can match the new Base
 * sequence/CRC. A later save starts a fresh pair without requiring a risky
 * in-place Sidecar erase.
 */
energun_pair_v1_status_t energun_pair_v1_erase(
    const energun_pair_v1_ops_t *ops, void *context,
    uint32_t object);

/* Load Base and its exact identity/generation-bound Sidecar as one pair.
 * No Base or Sidecar payload is returned unless an exact matching record
 * validates successfully.
 */
energun_pair_v1_status_t energun_pair_v1_load(
    const energun_pair_v1_ops_t *ops, void *context,
    energun_pair_v1_workspace_t *workspace,
    uint32_t object, energun_sc_identity_kind_t kind, uint8_t index,
    uint8_t *base_payload, size_t base_capacity,
    size_t *base_length,
    uint8_t *sidecar_payload, size_t sidecar_capacity,
    size_t *sidecar_length,
    uint16_t base_motion_count,
    energun_pair_v1_base_meta_t *base_meta);

#endif
