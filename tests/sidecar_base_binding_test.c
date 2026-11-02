/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Base generation metadata + Sidecar binding host tests.
 * Uses the real storage.c and Sidecar V1 source against simulated NOR.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define __attribute__(x)
static uint8_t nor[0x100000u];
static uint32_t flash_calls;

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    ++flash_calls;
    if (off > sizeof nor || n > sizeof nor - off) return -1;
    memcpy(dst, nor + off, n);
    return 0;
}

static int st_erase(uint32_t off)
{
    ++flash_calls;
    if (off > sizeof nor || 4096u > sizeof nor - off) return -1;
    memset(nor + off, 0xFF, 4096u);
    return 0;
}

static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *in = (const uint8_t *)src;
    uint32_t i;
    ++flash_calls;
    if (off > sizeof nor || n > sizeof nor - off || (off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; ++i) {
        if ((nor[off + i] & in[i]) != in[i])
            return -1;
        nor[off + i] &= in[i];
    }
    return 0;
}

#include "../firmware/src/storage.c"
#include "../firmware/src/energun_sidecar_v1.c"

static unsigned int failures;
static unsigned int checks;

static void check(const char *name, int ok)
{
    ++checks;
    printf("CP1.4.5 Base binding: %-68s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

int main(void)
{
    uint8_t base_a[64], base_b[64], base_out[64], base_copy[64];
    uint8_t sidecar_payload[ENERGUN_SC_V1_PROJECT_VALUES + 1u];
    uint8_t sidecar_a[ENERGUN_SC_V1_MAX_RECORD];
    uint8_t sidecar_b[ENERGUN_SC_V1_MAX_RECORD];
    size_t sidecar_a_len = 0u, sidecar_b_len = 0u;
    st_meta_t meta_a, meta_b, meta_fallback, copy_meta;
    energun_sc_view_t view;
    uint32_t calls_before;

    memset(nor, 0xFF, sizeof nor);
    memset(base_a, 0xA5, sizeof base_a);
    memset(base_b, 0x5A, sizeof base_b);
    memset(sidecar_payload, 0, sizeof sidecar_payload);
    sidecar_payload[ENERGUN_SC_V1_PROJECT_VALUES] = 0u;

    check("empty Base object has no generation metadata",
          st_current_meta(OBJ_PROJECT0, &meta_a) != 0);
    check("first Base save commits to A",
          st_save(OBJ_PROJECT0, base_a, sizeof base_a) == 0);
    check("metadata exposes selected copy, sequence, length and CRC",
          st_current_meta(OBJ_PROJECT0, &meta_a) == 0 &&
          meta_a.copy == 0u && meta_a.sequence == 1u &&
          meta_a.payload_length == sizeof base_a &&
          meta_a.payload_crc == st_crc32(base_a, sizeof base_a));
    check("explicit A/B copy reader returns the exact CRC-validated payload",
          st_read_copy_meta(OBJ_PROJECT0, 0u, base_copy, sizeof base_copy,
                            &copy_meta) == 0 &&
          copy_meta.copy == 0u && copy_meta.sequence == meta_a.sequence &&
          copy_meta.payload_crc == meta_a.payload_crc &&
          memcmp(base_copy, base_a, sizeof base_a) == 0);
    check("Sidecar encodes against the exact selected Base fingerprint",
          energun_sc_v1_encode(sidecar_a, sizeof sidecar_a, &sidecar_a_len,
              ENERGUN_SC_ID_PROJECT, 0u, meta_a.sequence, meta_a.payload_crc,
              sidecar_payload, sizeof sidecar_payload, 0u) == ENERGUN_SC_OK);
    check("Sidecar validates against selected Base metadata",
          energun_sc_v1_validate(sidecar_a, sidecar_a_len,
              ENERGUN_SC_ID_PROJECT, 0u, meta_a.sequence, meta_a.payload_crc,
              0u, &view) == ENERGUN_SC_OK);

    check("second Base save commits to B",
          st_save(OBJ_PROJECT0, base_b, sizeof base_b) == 0);
    check("metadata follows the newer valid B generation",
          st_current_meta(OBJ_PROJECT0, &meta_b) == 0 &&
          meta_b.copy == 1u && meta_b.sequence == 2u &&
          meta_b.payload_crc == st_crc32(base_b, sizeof base_b));
    check("old Sidecar is rejected against new Base generation",
          energun_sc_v1_validate(sidecar_a, sidecar_a_len,
              ENERGUN_SC_ID_PROJECT, 0u, meta_b.sequence, meta_b.payload_crc,
              0u, &view) == ENERGUN_SC_ERR_BASE_GENERATION);
    check("new Sidecar encodes against second Base generation",
          energun_sc_v1_encode(sidecar_b, sizeof sidecar_b, &sidecar_b_len,
              ENERGUN_SC_ID_PROJECT, 0u, meta_b.sequence, meta_b.payload_crc,
              sidecar_payload, sizeof sidecar_payload, 0u) == ENERGUN_SC_OK);
    check("new Sidecar validates against second Base generation",
          energun_sc_v1_validate(sidecar_b, sidecar_b_len,
              ENERGUN_SC_ID_PROJECT, 0u, meta_b.sequence, meta_b.payload_crc,
              0u, &view) == ENERGUN_SC_OK);

    /* Damage the newest Base payload. st_current() must fall back to A, and
     * metadata must describe that fallback copy rather than the bad B header.
     */
    nor[st_sector(OBJ_PROJECT0, 1u) + ST_PAYLOAD_OFF + 7u] ^= 0x01u;
    check("metadata follows valid A fallback after newer B payload corruption",
          st_current_meta(OBJ_PROJECT0, &meta_fallback) == 0 &&
          meta_fallback.copy == 0u && meta_fallback.sequence == meta_a.sequence &&
          meta_fallback.payload_crc == meta_a.payload_crc);
    check("old Sidecar matches the actual fallback Base generation",
          energun_sc_v1_validate(sidecar_a, sidecar_a_len,
              ENERGUN_SC_ID_PROJECT, 0u, meta_fallback.sequence,
              meta_fallback.payload_crc, 0u, &view) == ENERGUN_SC_OK);
    check("Base load still returns the fallback payload",
          st_load(OBJ_PROJECT0, base_out, sizeof base_out) == (int)sizeof base_a &&
          memcmp(base_out, base_a, sizeof base_a) == 0);

    calls_before = flash_calls;
    check("invalid object metadata request performs no Flash reads",
          st_current_meta(OBJ_COUNT, &meta_a) != 0 && flash_calls == calls_before);
    check("null metadata output is rejected",
          st_current_meta(OBJ_PROJECT0, 0) != 0);

    printf("CP1.4.5 Base binding: %u checks, %u failures\n", checks, failures);
    return failures != 0u;
}
