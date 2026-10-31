/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — CP3.1 lower-bound memory accounting probe.
 * This is a host compile-time accounting test, not a production state layout.
 */
#include "../firmware/src/eng_metal_core_vca.h"
#include "../firmware/src/eng_metal_core_lpg.h"

#include <stdint.h>
#include <stdio.h>

#define TEST_VOICES 8u
#define TEST_POOL_NO_ADPCM 12888u
#define TEST_POOL_ADPCM 13344u
#define TEST_DELAY_MS 3u
#define TEST_DELAY_SAMPLES(sr) (((sr) * TEST_DELAY_MS + 999u) / 1000u)
#define TEST_DELAY_BYTES(sr) (TEST_DELAY_SAMPLES(sr) * 4u * TEST_VOICES)

typedef struct {
    uint32_t metal_fm_phase;
    uint32_t metal_ring_phase;
    uint32_t feedback_write_index;
    eng_metal_vca_t vca;
    eng_metal_lpg_env_t lpg;
} test_known_voice_state_t;

typedef struct {
    test_known_voice_state_t voice[TEST_VOICES];
} test_known_part_state_t;

_Static_assert(sizeof(eng_metal_vca_t) == 36u,
               "VCA state size changed; revisit CP3.1 memory budget");
_Static_assert(sizeof(eng_metal_lpg_env_t) == 24u,
               "LPG envelope size changed; revisit CP3.1 memory budget");
_Static_assert(sizeof(test_known_voice_state_t) == 72u,
               "known voice-state subtotal changed");
_Static_assert(sizeof(test_known_part_state_t) == 576u,
               "eight known voice-state records must occupy 576 bytes");
_Static_assert(TEST_DELAY_SAMPLES(44100u) == 133u, "44.1 kHz ring length");
_Static_assert(TEST_DELAY_SAMPLES(48000u) == 144u, "48 kHz ring length");
_Static_assert(TEST_DELAY_SAMPLES(96000u) == 288u, "96 kHz ring length");
_Static_assert(TEST_DELAY_SAMPLES(192000u) == 576u, "192 kHz ring length");
_Static_assert(sizeof(test_known_part_state_t) + TEST_DELAY_BYTES(44100u)
                   <= TEST_POOL_NO_ADPCM,
               "known 44.1 kHz subtotal must fit non-ADPCM pool");
_Static_assert(sizeof(test_known_part_state_t) + TEST_DELAY_BYTES(96000u)
                   <= TEST_POOL_NO_ADPCM,
               "known 96 kHz subtotal must fit non-ADPCM pool");
_Static_assert(sizeof(test_known_part_state_t) + TEST_DELAY_BYTES(192000u)
                   > TEST_POOL_ADPCM,
               "192 kHz delay design must be rejected by both pool sizes");

int main(void)
{
    const uint32_t bytes_44100 =
        (uint32_t)sizeof(test_known_part_state_t) + TEST_DELAY_BYTES(44100u);
    const uint32_t bytes_48000 =
        (uint32_t)sizeof(test_known_part_state_t) + TEST_DELAY_BYTES(48000u);
    const uint32_t bytes_96000 =
        (uint32_t)sizeof(test_known_part_state_t) + TEST_DELAY_BYTES(96000u);
    const uint32_t bytes_192000 =
        (uint32_t)sizeof(test_known_part_state_t) + TEST_DELAY_BYTES(192000u);

    printf("CP3.1 known state: VCA=%u LPG=%u voice=%u part=%u bytes\n",
           (unsigned)sizeof(eng_metal_vca_t),
           (unsigned)sizeof(eng_metal_lpg_env_t),
           (unsigned)sizeof(test_known_voice_state_t),
           (unsigned)sizeof(test_known_part_state_t));
    printf("CP3.1 known subtotal: 44.1k=%u 48k=%u 96k=%u 192k=%u bytes\n",
           bytes_44100, bytes_48000, bytes_96000, bytes_192000);

    if (bytes_44100 != 4832u || bytes_48000 != 5184u ||
        bytes_96000 != 9792u || bytes_192000 != 19008u)
        return 1;
    if (TEST_POOL_NO_ADPCM - bytes_44100 != 8056u ||
        TEST_POOL_NO_ADPCM - bytes_96000 != 3096u)
        return 1;

    puts("CP3.1 memory accounting: PASS (0 failures; lower-bound only)");
    return 0;
}
