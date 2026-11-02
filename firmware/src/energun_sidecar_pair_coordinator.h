/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — backend-neutral Base + Sidecar pair coordinator.
 *
 * This module defines the transaction order only. It assigns no Felucca
 * storage object ID, Flash address, sector, or production call site.
 */
#ifndef ENERGUN_SIDECAR_PAIR_COORDINATOR_H
#define ENERGUN_SIDECAR_PAIR_COORDINATOR_H

#include "energun_sidecar_v1.h"

#include <stddef.h>
#include <stdint.h>

#define ENERGUN_SC_PAIR_COPY_COUNT 2u
#define ENERGUN_SC_PAIR_MAX_RECORD ENERGUN_SC_V1_MAX_RECORD

typedef struct {
    uint8_t valid;
    uint8_t copy;
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t payload_crc32;
} energun_sc_pair_base_meta_t;

typedef struct {
    uint32_t sequence;
    size_t record_length;
} energun_sc_pair_sidecar_meta_t;

/* Backend return rules:
 * - read_base_current: 0 = success (meta.valid may be 0), -1 = I/O error.
 * - read_sidecar_copy: 0 = fully storage-CRC-valid copy, 1 = absent/invalid,
 *   -1 = I/O error. It must copy bytes to caller-owned dst.
 * - write_*_copy: 0 only after the requested copy is committed and read-back
 *   verified; nonzero means failure/uncertain commit.
 *
 * The backend owns all physical layout and commit-record details.
 */
typedef struct {
    int (*read_base_current)(void *context,
                             energun_sc_pair_base_meta_t *meta);
    int (*write_base_copy)(void *context, uint8_t copy, uint32_t sequence,
                           const uint8_t *payload, uint32_t payload_length);
    int (*read_sidecar_copy)(void *context, uint8_t copy, uint8_t *dst,
                             size_t capacity,
                             energun_sc_pair_sidecar_meta_t *meta);
    int (*write_sidecar_copy)(void *context, uint8_t copy, uint32_t sequence,
                              const uint8_t *record, size_t record_length);
    void (*clear_dirty_if_generation)(void *context,
                                      uint32_t snapshot_generation);
} energun_sc_pair_backend_t;

typedef struct {
    energun_sc_identity_kind_t identity_kind;
    uint8_t identity_index;
    uint16_t current_base_motion_count; /* active Base pool occupancy; ignored if no Base */
    uint16_t base_motion_count;         /* new Base pool occupancy */
    const uint8_t *base_payload;
    uint32_t base_payload_length;
    const uint8_t *sidecar_payload;
    uint16_t sidecar_payload_length;
    uint32_t snapshot_generation;
} energun_sc_pair_request_t;

/* Caller-owned workspace; no heap allocation and no hidden static scratch. */
typedef struct {
    uint8_t sidecar_copy[ENERGUN_SC_PAIR_COPY_COUNT]
                        [ENERGUN_SC_PAIR_MAX_RECORD];
    uint8_t staged_record[ENERGUN_SC_PAIR_MAX_RECORD];
    uint8_t verify_record[ENERGUN_SC_PAIR_MAX_RECORD];
} energun_sc_pair_workspace_t;

typedef struct {
    uint8_t busy;
} energun_sc_pair_coordinator_t;

typedef enum {
    ENERGUN_SC_PAIR_OK = 0,
    ENERGUN_SC_PAIR_ERR_ARGUMENT,
    ENERGUN_SC_PAIR_ERR_BUSY,
    ENERGUN_SC_PAIR_ERR_BASE_READ,
    ENERGUN_SC_PAIR_ERR_SIDECAR_READ,
    ENERGUN_SC_PAIR_ERR_SELECT,
    ENERGUN_SC_PAIR_ERR_ENCODE,
    ENERGUN_SC_PAIR_ERR_SIDECAR_WRITE,
    ENERGUN_SC_PAIR_ERR_SIDECAR_VERIFY,
    ENERGUN_SC_PAIR_ERR_BASE_WRITE,
    ENERGUN_SC_PAIR_ERR_BASE_VERIFY,
    ENERGUN_SC_PAIR_ERR_PAIR_VERIFY
} energun_sc_pair_status_t;

/* Commit a coherent pair:
 * 1. snapshot Base and Sidecar are supplied by the caller and must remain
 *    immutable for the duration of this call;
 * 2. write and verify Sidecar bound to the next Base generation;
 * 3. commit Base to its inactive copy;
 * 4. re-read and verify the complete pair;
 * 5. clear dirty only through the generation-aware callback.
 *
 * A backend error may mean power was lost immediately after a commit record
 * became durable. Recovery must always re-read both objects; never infer the
 * active generation from this function's return code alone.
 */
energun_sc_pair_status_t energun_sc_pair_commit(
    energun_sc_pair_coordinator_t *coordinator,
    const energun_sc_pair_backend_t *backend,
    void *backend_context,
    const energun_sc_pair_request_t *request,
    energun_sc_pair_workspace_t *workspace);

uint32_t energun_sc_pair_crc32(const uint8_t *data, uint32_t length);

const char *energun_sc_pair_status_string(energun_sc_pair_status_t status);

#endif
