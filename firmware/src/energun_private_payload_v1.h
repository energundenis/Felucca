/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Private runtime / Sidecar payload bridge, V1.
 */
#ifndef ENERGUN_PRIVATE_PAYLOAD_V1_H
#define ENERGUN_PRIVATE_PAYLOAD_V1_H

#include "energun_private_v1.h"
#include "energun_sidecar_payload_v1.h"

/* Project/live bridge: all four parts are validated before either side is
 * changed. A restored project becomes clean runtime state.
 */
energun_private_status_v1_t energun_private_payload_v1_project_capture(
    const energun_private_cache_v1_t *cache,
    energun_sc_project_payload_v1_t *payload);
energun_private_status_v1_t energun_private_payload_v1_project_restore(
    energun_private_cache_v1_t *cache,
    const energun_sc_project_payload_v1_t *payload,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS]);

/* Preset bridge: slot is 0..15 within one of the two independent banks.
 * Capture changes only that slot and its bitmap bit. Restore applies that
 * slot to the selected runtime part, or canonical defaults if it is absent.
 */
energun_private_status_v1_t energun_private_payload_v1_preset_capture(
    const energun_private_cache_v1_t *cache, uint8_t part, uint8_t slot,
    energun_sc_preset_payload_v1_t *payload);
energun_private_status_v1_t energun_private_payload_v1_preset_restore(
    energun_private_cache_v1_t *cache, uint8_t part, uint8_t slot,
    const energun_sc_preset_payload_v1_t *payload,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS]);

#endif
