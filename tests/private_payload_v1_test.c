/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Private Sidecar bridge acceptance.
 */
#include "energun_private_payload_v1.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
static unsigned failures;
#define CHECK(expr) do { ++checks; if (!(expr)) { ++failures; printf("FAIL line %d: %s\n", __LINE__, #expr); } } while (0)

static const uint8_t defaults[ENERGUN_PRIVATE_V1_KEYS] = {
    0u, 1u, 1u, 0u, 0u, 2u, 96u, 1u, 96u, 0u, 0u, 127u, 0u
};

int main(void)
{
    energun_private_cache_v1_t source, restored, before;
    energun_sc_project_payload_v1_t project;
    energun_sc_preset_payload_v1_t preset;
    unsigned part, key;

    memset(&project, 0xA5, sizeof project);
    memset(&preset, 0, sizeof preset);
    CHECK(energun_private_v1_init(&source, defaults) == ENERGUN_PRIVATE_V1_OK);
    for (part = 0; part < ENERGUN_PRIVATE_V1_PARTS; ++part) {
        for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key) {
            uint8_t v = defaults[key];
            if (key == ENERGUN_PRIVATE_FOLLOW || key == ENERGUN_PRIVATE_METAL)
                v = (uint8_t)((part + key) & 1u);
            if (key == ENERGUN_PRIVATE_FM_CHARACTER)
                v = (uint8_t)(part % 3u);
            CHECK(energun_private_v1_set(&source, (uint8_t)part,
                (uint8_t)key, v) == ENERGUN_PRIVATE_V1_OK);
        }
    }
    CHECK(energun_private_payload_v1_project_capture(&source, &project) == ENERGUN_PRIVATE_V1_OK);
    for (part = 0; part < ENERGUN_PRIVATE_V1_PARTS; ++part)
        for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
            CHECK(project.private_values[part][key] == source.part[part].value[key]);

    CHECK(energun_private_v1_init(&restored, defaults) == ENERGUN_PRIVATE_V1_OK);
    CHECK(energun_private_payload_v1_project_restore(&restored, &project, defaults) == ENERGUN_PRIVATE_V1_OK);
    for (part = 0; part < ENERGUN_PRIVATE_V1_PARTS; ++part) {
        CHECK(restored.part[part].dirty_mask == 0u);
        CHECK(restored.part[part].generation == 1u);
        for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
            CHECK(restored.part[part].value[key] == source.part[part].value[key]);
    }

    before = restored;
    project.private_values[3][ENERGUN_PRIVATE_FM_CHARACTER] = 3u;
    CHECK(energun_private_payload_v1_project_restore(&restored, &project, defaults) == ENERGUN_PRIVATE_V1_ERR_VALUE);
    CHECK(memcmp(&restored, &before, sizeof restored) == 0);
    project.private_values[3][ENERGUN_PRIVATE_FM_CHARACTER] = source.part[3].value[ENERGUN_PRIVATE_FM_CHARACTER];

    /* Preset slot updates must preserve every unrelated slot and bitmap bit. */
    memset(preset.private_values[5], 0x5A, ENERGUN_PRIVATE_V1_KEYS);
    memset(preset.private_values[6], 0x33, ENERGUN_PRIVATE_V1_KEYS);
    preset.slot_bitmap[0] = (uint8_t)(1u << 5);
    CHECK(energun_private_payload_v1_preset_capture(&source, 2u, 6u, &preset) == ENERGUN_PRIVATE_V1_OK);
    CHECK((preset.slot_bitmap[0] & (1u << 5)) != 0u);
    CHECK((preset.slot_bitmap[0] & (1u << 6)) != 0u);
    for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key) {
        CHECK(preset.private_values[6][key] == source.part[2].value[key]);
        CHECK(preset.private_values[5][key] == 0x5Au);
    }
    CHECK(energun_private_payload_v1_preset_restore(&restored, 0u, 6u, &preset, defaults) == ENERGUN_PRIVATE_V1_OK);
    CHECK(restored.part[0].dirty_mask == 0u);
    for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
        CHECK(restored.part[0].value[key] == source.part[2].value[key]);

    /* An unset slot deterministically restores canonical defaults. */
    CHECK(energun_private_payload_v1_preset_restore(&restored, 1u, 7u, &preset, defaults) == ENERGUN_PRIVATE_V1_OK);
    CHECK(restored.part[1].dirty_mask == 0u);
    for (key = 0; key < ENERGUN_PRIVATE_V1_KEYS; ++key)
        CHECK(restored.part[1].value[key] == defaults[key]);

    before = restored;
    preset.private_values[8][ENERGUN_PRIVATE_FM_CHARACTER] = 3u;
    preset.slot_bitmap[1] |= (uint8_t)(1u << 0); /* slot 8 */
    CHECK(energun_private_payload_v1_preset_restore(&restored, 1u, 8u, &preset, defaults) == ENERGUN_PRIVATE_V1_ERR_VALUE);
    CHECK(memcmp(&restored, &before, sizeof restored) == 0);

    CHECK(energun_private_payload_v1_preset_capture(&source, 0u, 16u, &preset) == ENERGUN_PRIVATE_V1_ERR_PART);
    CHECK(energun_private_payload_v1_preset_restore(&restored, 4u, 0u, &preset, defaults) == ENERGUN_PRIVATE_V1_ERR_PART);

    printf("Private Sidecar bridge: %u checks, %u failures\n", checks, failures);
    return failures == 0u ? 0 : 1;
}
