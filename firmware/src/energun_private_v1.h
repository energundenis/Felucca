/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — typed four-part Private runtime cache, V1.
 *
 * Host-testable lifecycle primitive. It owns logical Private values only;
 * it is not tied to engine_t.mem, Flash, editor commands or DSP snapshots.
 */
#ifndef ENERGUN_PRIVATE_V1_H
#define ENERGUN_PRIVATE_V1_H

#include <stdint.h>

#define ENERGUN_PRIVATE_V1_SCHEMA 1u
#define ENERGUN_PRIVATE_V1_PARTS 4u
#define ENERGUN_PRIVATE_V1_KEYS 13u
#define ENERGUN_PRIVATE_V1_ALL_DIRTY ((uint16_t)0x1FFFu)

typedef enum {
    ENERGUN_PRIVATE_WAVE = 0,
    ENERGUN_PRIVATE_FOLLOW,
    ENERGUN_PRIVATE_METAL,
    ENERGUN_PRIVATE_MODULATION,
    ENERGUN_PRIVATE_MOD_OSC_WAVE,
    ENERGUN_PRIVATE_FM_CHARACTER,
    ENERGUN_PRIVATE_COMPLEX_VOLUME,
    ENERGUN_PRIVATE_FOLD_MODE,
    ENERGUN_PRIVATE_LPG_VOLUME,
    ENERGUN_PRIVATE_ATTACK,
    ENERGUN_PRIVATE_DECAY,
    ENERGUN_PRIVATE_SUSTAIN,
    ENERGUN_PRIVATE_RELEASE
} energun_private_key_v1_t;

typedef struct {
    uint16_t schema_version;
    uint32_t generation;
    uint16_t dirty_mask;
    uint8_t valid;
    uint8_t value[ENERGUN_PRIVATE_V1_KEYS];
} energun_private_part_v1_t;

typedef struct {
    energun_private_part_v1_t part[ENERGUN_PRIVATE_V1_PARTS];
} energun_private_cache_v1_t;

typedef enum {
    ENERGUN_PRIVATE_V1_OK = 0,
    ENERGUN_PRIVATE_V1_ERR_ARGUMENT = -1,
    ENERGUN_PRIVATE_V1_ERR_PART = -2,
    ENERGUN_PRIVATE_V1_ERR_KEY = -3,
    ENERGUN_PRIVATE_V1_ERR_VALUE = -4,
    ENERGUN_PRIVATE_V1_ERR_SCHEMA = -5,
    ENERGUN_PRIVATE_V1_ERR_STALE = -6
} energun_private_status_v1_t;

/* Defaults are supplied by the canonical V1 descriptor owner. This module
 * validates their domains but deliberately does not invent startup values.
 */
energun_private_status_v1_t energun_private_v1_init(
    energun_private_cache_v1_t *cache,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS]);

/* Missing/invalid extension fallback: atomically restore the supplied V1
 * defaults for one part, clear dirty state and advance its generation.
 */
energun_private_status_v1_t energun_private_v1_defaults(
    energun_private_cache_v1_t *cache, uint8_t part,
    const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS]);

energun_private_status_v1_t energun_private_v1_get(
    const energun_private_cache_v1_t *cache, uint8_t part,
    uint8_t key, uint8_t *value);

energun_private_status_v1_t energun_private_v1_set(
    energun_private_cache_v1_t *cache, uint8_t part,
    uint8_t key, uint8_t value);

energun_private_status_v1_t energun_private_v1_snapshot(
    const energun_private_cache_v1_t *cache, uint8_t part,
    energun_private_part_v1_t *snapshot);

/* Compare-and-replace: expected_generation prevents an editor or persistence
 * callback from replacing a newer part snapshot. Validation is all-or-none.
 */
energun_private_status_v1_t energun_private_v1_replace(
    energun_private_cache_v1_t *cache, uint8_t part,
    const energun_private_part_v1_t *snapshot,
    uint32_t expected_generation);

/* Clear dirty bits only if no edit occurred since the saved snapshot. */
energun_private_status_v1_t energun_private_v1_mark_persisted(
    energun_private_cache_v1_t *cache, uint8_t part,
    uint32_t saved_generation);

#endif
