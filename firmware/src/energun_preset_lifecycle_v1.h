/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — user-preset bank Sidecar lifecycle, V1.
 *
 * A preset bank is one Base object and one Sidecar object. Slot mutations
 * rewrite the complete bank pair while preserving all unrelated slots.
 */
#ifndef ENERGUN_PRESET_LIFECYCLE_V1_H
#define ENERGUN_PRESET_LIFECYCLE_V1_H

#include "energun_lifecycle_v1.h"
#include "energun_private_payload_v1.h"

energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_save_bank(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    const uint8_t *base_payload, size_t base_length,
    const energun_sc_preset_payload_v1_t *sidecar);

energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_load_bank(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    energun_sc_preset_payload_v1_t *sidecar,
    energun_pair_v1_base_meta_t *base_meta);

/* Strict variant for the native 16-slot upreset.c bank ABI. On a valid pair,
 * also requires the Sidecar slot bitmap to match records accepted by the
 * target build's up_valid() rules. Legacy Base-only fallback stays unchanged.
 */
energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_load_bank_checked(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t engine_count,
    uint8_t *base_payload, size_t base_capacity, size_t *base_length,
    energun_sc_preset_payload_v1_t *sidecar,
    energun_pair_v1_base_meta_t *base_meta);

/* Capture one part's Private state into one slot, then commit the complete
 * caller-prepared Base bank and updated Sidecar bank as a pair.
 */
energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_capture_slot(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t part, uint8_t slot,
    const uint8_t *new_base_bank, size_t new_base_bank_length,
    const energun_private_cache_v1_t *private_cache);

/* Clear one slot's presence bit and data without touching any other slot. */
energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_erase_slot(
    const energun_lifecycle_v1_t *lifecycle, uint32_t object, uint8_t bank,
    uint8_t slot,
    const uint8_t *new_base_bank, size_t new_base_bank_length);

/* Apply a decoded slot only after the caller has accepted/deserialized the
 * matching Base record. An absent slot applies canonical defaults atomically.
 */
energun_lifecycle_v1_status_t energun_preset_lifecycle_v1_apply_slot(
    const energun_sc_preset_payload_v1_t *sidecar,
    energun_private_cache_v1_t *private_cache,
    uint8_t part, uint8_t slot,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS]);

#endif
