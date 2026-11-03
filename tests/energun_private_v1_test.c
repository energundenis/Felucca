/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Private runtime cache acceptance tests.
 */
#include "energun_private_v1.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #expr); } } while (0)

int main(void)
{
    energun_private_cache_v1_t cache;
    energun_private_part_v1_t snap;
    uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS] = {
        0u, 1u, 1u, 0u, 0u, 2u, 96u, 1u, 96u, 0u, 0u, 127u, 0u
    };
    uint8_t value = 0u;
    uint32_t generation;

    CHECK(energun_private_v1_init(&cache, defaults) == ENERGUN_PRIVATE_V1_OK);
    CHECK(cache.part[0].dirty_mask == 0u && cache.part[3].dirty_mask == 0u);
    CHECK(energun_private_v1_get(&cache, 0u, ENERGUN_PRIVATE_FM_CHARACTER, &value) == ENERGUN_PRIVATE_V1_OK && value == 2u);

    CHECK(energun_private_v1_set(&cache, 0u, ENERGUN_PRIVATE_FOLLOW, 0u) == ENERGUN_PRIVATE_V1_OK);
    CHECK(cache.part[0].dirty_mask == (1u << ENERGUN_PRIVATE_FOLLOW));
    generation = cache.part[0].generation;
    CHECK(energun_private_v1_set(&cache, 0u, ENERGUN_PRIVATE_FOLLOW, 0u) == ENERGUN_PRIVATE_V1_OK);
    CHECK(cache.part[0].generation == generation);
    CHECK(cache.part[0].dirty_mask == (1u << ENERGUN_PRIVATE_FOLLOW));
    CHECK(cache.part[1].value[ENERGUN_PRIVATE_FOLLOW] == 1u);
    CHECK(cache.part[0].generation == 1u && cache.part[1].generation == 0u);

    CHECK(energun_private_v1_set(&cache, 0u, ENERGUN_PRIVATE_FM_CHARACTER, 3u) == ENERGUN_PRIVATE_V1_ERR_VALUE);
    CHECK(cache.part[0].value[ENERGUN_PRIVATE_FM_CHARACTER] == 2u);
    CHECK(energun_private_v1_set(&cache, 4u, ENERGUN_PRIVATE_FOLLOW, 1u) == ENERGUN_PRIVATE_V1_ERR_PART);
    CHECK(energun_private_v1_set(&cache, 0u, 13u, 0u) == ENERGUN_PRIVATE_V1_ERR_KEY);

    CHECK(energun_private_v1_snapshot(&cache, 0u, &snap) == ENERGUN_PRIVATE_V1_OK);
    generation = snap.generation;
    snap.value[ENERGUN_PRIVATE_METAL] = 0u;
    snap.value[ENERGUN_PRIVATE_MODULATION] = 2u; /* invalid enum: whole replacement rejected */
    CHECK(energun_private_v1_replace(&cache, 0u, &snap, generation) == ENERGUN_PRIVATE_V1_ERR_VALUE);
    CHECK(cache.part[0].value[ENERGUN_PRIVATE_METAL] == 1u);
    CHECK(cache.part[0].value[ENERGUN_PRIVATE_MODULATION] == 0u);

    snap.value[ENERGUN_PRIVATE_MODULATION] = 1u;
    {
        energun_private_part_v1_t invalid = snap;
        invalid.valid = 0u;
        CHECK(energun_private_v1_replace(&cache, 0u, &invalid, generation) == ENERGUN_PRIVATE_V1_ERR_SCHEMA);
        CHECK(cache.part[0].value[ENERGUN_PRIVATE_METAL] == 1u);
    }
    CHECK(energun_private_v1_replace(&cache, 0u, &snap, generation + 1u) == ENERGUN_PRIVATE_V1_ERR_STALE);
    CHECK(cache.part[0].value[ENERGUN_PRIVATE_METAL] == 1u);
    CHECK(energun_private_v1_replace(&cache, 0u, &snap, generation) == ENERGUN_PRIVATE_V1_OK);
    CHECK(cache.part[0].value[ENERGUN_PRIVATE_METAL] == 0u);
    CHECK(cache.part[0].value[ENERGUN_PRIVATE_MODULATION] == 1u);
    CHECK((cache.part[0].dirty_mask & ((1u << ENERGUN_PRIVATE_METAL) | (1u << ENERGUN_PRIVATE_MODULATION))) != 0u);

    CHECK(energun_private_v1_mark_persisted(&cache, 0u, generation) == ENERGUN_PRIVATE_V1_ERR_STALE);
    generation = cache.part[0].generation;
    CHECK(energun_private_v1_mark_persisted(&cache, 0u, generation) == ENERGUN_PRIVATE_V1_OK);
    CHECK(cache.part[0].dirty_mask == 0u);

    CHECK(energun_private_v1_set(&cache, 2u, ENERGUN_PRIVATE_ATTACK, 80u) == ENERGUN_PRIVATE_V1_OK);
    CHECK(energun_private_v1_defaults(&cache, 2u, defaults) == ENERGUN_PRIVATE_V1_OK);
    CHECK(cache.part[2].value[ENERGUN_PRIVATE_ATTACK] == defaults[ENERGUN_PRIVATE_ATTACK]);
    CHECK(cache.part[2].dirty_mask == 0u);

    defaults[ENERGUN_PRIVATE_WAVE] = 3u;
    CHECK(energun_private_v1_init(&cache, defaults) == ENERGUN_PRIVATE_V1_ERR_VALUE);
    CHECK(energun_private_v1_get(&cache, 0u, ENERGUN_PRIVATE_FM_CHARACTER, &value) == ENERGUN_PRIVATE_V1_OK && value == 2u);

    printf("Private runtime cache: %u checks, %u failures\n", checks, failures);
    return failures == 0u ? 0 : 1;
}
