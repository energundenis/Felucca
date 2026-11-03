/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Felucca storage binding for Base + Sidecar transactions.
 *
 * Include the implementation from felucca.c immediately after storage.c.
 * The Sidecar sector backend is deliberately injected: no physical addresses
 * or XDATA allocation are defined here.
 */
#ifndef ENERGUN_PAIR_STORAGE_ADAPTER_H
#define ENERGUN_PAIR_STORAGE_ADAPTER_H
#include "energun_pair_txn.h"

typedef struct {
    /* Return only a fully committed outer A/B record after header and payload
     * CRC validation. Return nonzero for erased, torn or invalid copies.
     */
    int (*read_copy)(void *, energun_sc_identity_kind_t, uint8_t, uint8_t,
                     uint8_t *, size_t, size_t *, uint32_t *);
    /* Erase/program/header-last/readback one explicitly selected copy.
     * Must never touch the other copy. Return nonzero unless readback passes.
     */
    int (*write_copy)(void *, energun_sc_identity_kind_t, uint8_t, uint8_t,
                      const uint8_t *, size_t);
} energun_sidecar_storage_ops_t;

typedef struct {
    const energun_sidecar_storage_ops_t *sidecar_ops;
    void *sidecar_context;
    uint32_t base_object;
    energun_sc_identity_kind_t identity_kind;
    uint8_t identity_index;
    uint16_t base_motion_count;
    int8_t protected_base_copy;
    int8_t protected_sidecar_copy;
    uint8_t selected_sidecar_copy;
    uint32_t newest_base_sequence;
    uint8_t scratch_record[ENERGUN_SC_V1_MAX_RECORD];
} energun_pair_storage_adapter_t;

/* Bind a logical Base object to the existing Felucca A/B storage primitives
 * and a separately qualified Sidecar backend. No Flash address is accepted.
 */
int energun_pair_storage_adapter_init(
    energun_pair_storage_adapter_t *,
    const energun_sidecar_storage_ops_t *, void *, uint32_t);

/* Save uses pair-aware target selection: if a valid old pair exists, neither
 * its Base nor its Sidecar copy is overwritten before the new Base commits.
 */
energun_pair_status_t energun_pair_storage_adapter_save(
    energun_pair_storage_adapter_t *, energun_pair_txn_workspace_t *,
    const uint8_t *, uint32_t, energun_sc_identity_kind_t, uint8_t,
    const uint8_t *, uint16_t, uint16_t);

/* Pair-aware recovery scans both Base and Sidecar copies. It never returns a
 * Base generation without its exact matching Sidecar. Returns payload length
 * or -1 when no complete pair exists; caller applies defaults without writes.
 */
int energun_pair_storage_adapter_load(
    energun_pair_storage_adapter_t *, void *, uint32_t,
    void *, uint16_t, uint16_t *,
    energun_sc_identity_kind_t, uint8_t, uint16_t,
    energun_pair_base_meta_t *);
#endif
