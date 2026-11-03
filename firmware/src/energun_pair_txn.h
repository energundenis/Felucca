/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Base + Sidecar pair transaction coordinator.
 *
 * Backend-independent transaction core. A production adapter must bind the
 * callbacks to Felucca A/B storage and a separately qualified Sidecar backend.
 * This module deliberately assigns no physical Flash addresses.
 */
#ifndef ENERGUN_PAIR_TXN_H
#define ENERGUN_PAIR_TXN_H
#include <stddef.h>
#include <stdint.h>
#include "energun_sidecar_v1.h"

typedef struct {
    uint8_t copy;
    uint32_t sequence;
    uint32_t payload_length;
    uint32_t payload_crc32;
} energun_pair_base_meta_t;

typedef struct {
    uint8_t sidecar_record[ENERGUN_SC_V1_MAX_RECORD];
} energun_pair_txn_workspace_t;

typedef enum {
    ENERGUN_PAIR_OK = 0,
    ENERGUN_PAIR_ERR_ARGUMENT = -1,
    ENERGUN_PAIR_ERR_BASE_PLAN = -2,
    ENERGUN_PAIR_ERR_SIDECAR_TARGET = -3,
    ENERGUN_PAIR_ERR_SIDECAR_ENCODE = -4,
    ENERGUN_PAIR_ERR_SIDECAR_COMMIT = -5,
    ENERGUN_PAIR_ERR_BASE_COMMIT = -6,
    ENERGUN_PAIR_ERR_PAIR_VERIFY = -7
} energun_pair_status_t;

/*
 * Adapter contract:
 * - plan_base is read-only and predicts the exact inactive copy, next sequence,
 *   payload length and CRC that write_base will commit.
 * - select_sidecar_target preserves the currently matching Sidecar copy and
 *   chooses the other copy (or A for an empty store).
 * - both write callbacks use header-last commit and readback validation.
 * - verify_pair reads committed storage and checks exact Base metadata and
 *   Sidecar identity, semantics and Base-generation binding.
 * - Calls must be serialized by the storage owner; this API is not re-entrant.
 */
typedef struct {
    int (*plan_base)(void *, uint32_t, const uint8_t *, uint32_t,
                     energun_pair_base_meta_t *);
    int (*select_sidecar_target)(void *, energun_sc_identity_kind_t, uint8_t,
                     const energun_pair_base_meta_t *, uint8_t *);
    int (*write_sidecar)(void *, energun_sc_identity_kind_t, uint8_t, uint8_t,
                     const uint8_t *, size_t);
    int (*write_base)(void *, uint32_t, const uint8_t *, uint32_t,
                     const energun_pair_base_meta_t *);
    int (*verify_pair)(void *, uint32_t, energun_sc_identity_kind_t, uint8_t,
                     const energun_pair_base_meta_t *, const uint8_t *, size_t,
                     uint16_t);
} energun_pair_txn_ops_t;

/*
 * Sidecar is committed first, then Base. Until Base's header-last commit the
 * old matching pair remains intact; a newly staged Sidecar is an orphan and
 * must be ignored. A post-Base verification failure is never reported as
 * success; recovery must scan both A/B generations and select a complete
 * matching pair. Caller supplies workspace to avoid a 521-byte automatic
 * stack allocation in the firmware path.
 */
energun_pair_status_t energun_pair_txn_save(
    const energun_pair_txn_ops_t *, void *, energun_pair_txn_workspace_t *,
    uint32_t, const uint8_t *, uint32_t,
    energun_sc_identity_kind_t, uint8_t, const uint8_t *, uint16_t, uint16_t);

const char *energun_pair_status_string(energun_pair_status_t);
#endif
