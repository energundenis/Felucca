/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Sidecar V1 host acceptance tests.
 * Compiles the exact firmware-side encoder/validator without Flash access.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/energun_sidecar_v1.h"

#define PROJECT_BASE 116u
#define MOTION_COUNT_OFF 116u
#define MOTION_OFF 117u
#define PROJECT_SEQ 0xFFFFFFFFu
#define PROJECT_CRC 0x12345678u
#define PRESET_SEQ 77u
#define PRESET_CRC 0x87654321u

static unsigned int failures;
static unsigned int checks;

static void check(const char *name, int ok)
{
    ++checks;
    printf("CP1.4.5 Sidecar V1: %-70s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

static void make_project(uint8_t *p)
{
    memset(p, 0, ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD);
    p[MOTION_COUNT_OFF] = 0u;
}

static void make_preset(uint8_t *p)
{
    memset(p, 0, ENERGUN_SC_V1_PRESET_PAYLOAD);
}

static size_t encode_project(uint8_t *record, const uint8_t *payload,
                             uint16_t length, uint16_t base_motion_count)
{
    size_t written = 0u;
    energun_sc_status_t rc = energun_sc_v1_encode(
        record, ENERGUN_SC_V1_MAX_RECORD, &written,
        ENERGUN_SC_ID_PROJECT, 2u, PROJECT_SEQ, PROJECT_CRC,
        payload, length, base_motion_count);
    return rc == ENERGUN_SC_OK ? written : 0u;
}

#define ENC_PROJECT(r,p,l,b) energun_sc_v1_encode((r), sizeof(r), &n, ENERGUN_SC_ID_PROJECT, 2u, 9u, PROJECT_CRC, (p), (l), (b))
#define ENC_PRESET(r,p,l) energun_sc_v1_encode((r), sizeof(r), &n, ENERGUN_SC_ID_PRESET_BANK, 1u, PRESET_SEQ, PRESET_CRC, (p), (l), 0u)
#define VAL_PROJECT(r,l,s,c,b,v) energun_sc_v1_validate((r), (l), ENERGUN_SC_ID_PROJECT, 2u, (s), (c), (b), (v))

static void test_project_roundtrip_and_binding(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    energun_sc_view_t view;
    size_t n;
    make_project(payload);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("project record encodes at sequence wrap boundary",
          n == ENERGUN_SC_V1_HEADER_SIZE + PROJECT_BASE + 1u &&
          VAL_PROJECT(record, n, PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_OK);
    check("decoded view exposes payload, exact sequence and CRC fingerprint",
          view.base_sequence == PROJECT_SEQ &&
          view.base_payload_crc32 == PROJECT_CRC &&
          view.payload_length == PROJECT_BASE + 1u &&
          view.payload == record + ENERGUN_SC_V1_HEADER_SIZE);
    check("wrong base sequence is rejected",
          VAL_PROJECT(record, n, 0u, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_BASE_GENERATION);
    check("same sequence with different base payload CRC is rejected",
          VAL_PROJECT(record, n, PROJECT_SEQ, PROJECT_CRC ^ 1u, 0u, &view) == ENERGUN_SC_ERR_BASE_GENERATION);
    check("wrong project slot is rejected",
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 1u,
              PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_IDENTITY);
}

static void test_pmc_ranges(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    size_t n;
    make_project(payload);
    payload[9] = 0xC0u;
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("FOLD SYMMETRY signed lower endpoint -64 accepted", n != 0u);
    payload[9] = 0x3Fu;
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("FOLD SYMMETRY signed upper endpoint +63 accepted", n != 0u);
    payload[9] = 0xBFu;
    check("FOLD SYMMETRY below -64 rejected", encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[9] = 0x40u;
    check("FOLD SYMMETRY above +63 rejected", encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[9] = 0u;
    payload[0] = 128u;
    check("unipolar P_MC code 128 rejected", encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[0] = 127u;
    check("unipolar P_MC upper endpoint 127 accepted", encode_project(record, payload, PROJECT_BASE + 1u, 0u) != 0u);
    payload[3u * 16u + 1u] = 128u;
    check("P_MC unipolar bounds are validated for part four too",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[3u * 16u + 1u] = 127u;
    payload[3u * 16u + 9u] = 0xBFu;
    check("signed FOLD SYMMETRY bounds are validated for every part",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
}

static void test_private_types(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    size_t n;
    make_project(payload);
    payload[64u + 5u] = 2u;
    check("FM CHARACTER ordinal 2 remains representable", encode_project(record, payload, PROJECT_BASE + 1u, 0u) != 0u);
    payload[64u + 1u] = 2u;
    check("FOLLOW rejects non-boolean value 2", encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[64u + 1u] = 1u;
    payload[64u + 4u] = 4u;
    check("MOD OSC WAVE rejects ordinal outside four values", encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[64u + 4u] = 3u;
    payload[64u + 6u] = 127u;
    check("private 7-bit continuous maximum accepted", encode_project(record, payload, PROJECT_BASE + 1u, 0u) != 0u);
    (void)n;
}

static void test_motion_payload(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    unsigned int i;
    size_t n = 0u;
    make_project(payload);
    payload[MOTION_COUNT_OFF] = 1u;
    payload[MOTION_OFF] = 0u;
    payload[MOTION_OFF + 1u] = 15u;
    payload[MOTION_OFF + 2u] = 127u;
    check("P_MC logical index 15 is accepted as the final Motion destination",
          ENC_PROJECT(record, payload, MOTION_OFF + 3u, 0u) == ENERGUN_SC_OK);
    payload[MOTION_OFF + 1u] = 0x8Fu;
    check("Motion Lock flag is preserved on P_MC logical index 15",
          ENC_PROJECT(record, payload, MOTION_OFF + 3u, 0u) == ENERGUN_SC_OK);
    payload[MOTION_OFF + 1u] = 16u;
    check("Private namespace index 16 is rejected from Sidecar Motion",
          ENC_PROJECT(record, payload, MOTION_OFF + 3u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
    payload[MOTION_OFF + 1u] = 0x90u;
    check("flagged Private namespace index 16 is also rejected",
          ENC_PROJECT(record, payload, MOTION_OFF + 3u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
    make_project(payload);
    payload[MOTION_COUNT_OFF] = 128u;
    for (i = 0; i < 128u; ++i) {
        uint8_t *event = payload + MOTION_OFF + i * 3u;
        event[0] = (uint8_t)i;
        event[1] = 1u;
        event[2] = 0u;
    }
    check("maximum 128 Sidecar events encode to 501-byte payload",
          encode_project(record, payload, ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD, 0u) == ENERGUN_SC_V1_MAX_RECORD);
    check("128 Sidecar events plus one base event exceed shared pool",
          ENC_PROJECT(record, payload, ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD, 1u) == ENERGUN_SC_ERR_MOTION_COUNT);
    payload[MOTION_COUNT_OFF] = 129u;
    check("event count above global 128 rejected",
          ENC_PROJECT(record, payload, ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD, 0u) == ENERGUN_SC_ERR_MOTION_COUNT);
    payload[MOTION_COUNT_OFF] = 0u;
    check("base Motion count above global 128 rejected",
          ENC_PROJECT(record, payload, MOTION_OFF, 129u) == ENERGUN_SC_ERR_MOTION_COUNT);
    payload[MOTION_COUNT_OFF] = 2u;
    payload[MOTION_OFF + 0u] = 7u;
    payload[MOTION_OFF + 1u] = 3u;
    payload[MOTION_OFF + 2u] = 0u;
    payload[MOTION_OFF + 3u] = 7u;
    payload[MOTION_OFF + 4u] = 0x83u;
    payload[MOTION_OFF + 5u] = 1u;
    check("duplicate place/parameter rejected even when lock bit differs",
          ENC_PROJECT(record, payload, MOTION_OFF + 6u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
    payload[MOTION_COUNT_OFF] = 1u;
    payload[MOTION_OFF] = 0u;
    payload[MOTION_OFF + 1u] = 1u;
    payload[MOTION_OFF + 2u] = 0xBFu;
    check("motion value below -64 rejected",
          ENC_PROJECT(record, payload, MOTION_OFF + 3u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
    payload[MOTION_OFF + 2u] = 0x80u;
    check("motion value below signed storage range rejected",
          ENC_PROJECT(record, payload, MOTION_OFF + 3u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
}

static void test_preset_bank(void)
{
    uint8_t payload[ENERGUN_SC_V1_PRESET_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    size_t n = 0u;
    make_preset(payload);
    payload[0] = 1u;
    payload[2u + 5u] = 2u;
    check("preset bank with one present slot validates",
          ENC_PRESET(record, payload, sizeof payload) == ENERGUN_SC_OK &&
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PRESET_BANK, 1u,
              PRESET_SEQ, PRESET_CRC, 0u, &(energun_sc_view_t){0}) == ENERGUN_SC_OK);
    payload[2u + 13u] = 1u;
    check("absent preset slot cannot retain stale Private values",
          ENC_PRESET(record, payload, sizeof payload) == ENERGUN_SC_ERR_SLOT_BITMAP);
    payload[2u + 13u] = 0u;
    payload[2u + 5u] = 3u;
    check("invalid Private enum in used preset slot rejected",
          ENC_PRESET(record, payload, sizeof payload) == ENERGUN_SC_ERR_PRIVATE_VALUE);
}

static void test_preset_bank_all_slots(void)
{
    static const uint8_t maximum[13] = {
        2u, 1u, 1u, 1u, 3u, 2u, 127u, 2u, 127u, 127u, 127u, 127u, 127u
    };
    uint8_t payload[ENERGUN_SC_V1_PRESET_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    uint8_t saved[ENERGUN_SC_V1_PRESET_PAYLOAD];
    energun_sc_view_t view;
    size_t n;
    unsigned int bank, slot, key;

    for (bank = 0u; bank < 2u; ++bank) {
        make_preset(payload);
        payload[0] = 0xFFu;
        payload[1] = 0xFFu;
        for (slot = 0u; slot < 16u; ++slot)
            for (key = 0u; key < 13u; ++key)
                payload[2u + slot * 13u + key] =
                    (uint8_t)((slot + key) % ((unsigned int)maximum[key] + 1u));
        n = 0u;
        check("all sixteen Private preset slots encode for each of two banks",
              energun_sc_v1_encode(record, sizeof record, &n,
                  ENERGUN_SC_ID_PRESET_BANK, (uint8_t)bank, bank + 1u,
                  PRESET_CRC, payload, sizeof payload, 0u) == ENERGUN_SC_OK &&
              energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PRESET_BANK,
                  (uint8_t)bank, bank + 1u, PRESET_CRC, 0u, &view) == ENERGUN_SC_OK &&
              memcmp(view.payload, payload, sizeof payload) == 0);
    }

    make_preset(payload);
    payload[0] = 0xFFu;
    payload[1] = 0xFFu;
    for (slot = 0u; slot < 16u; ++slot)
        for (key = 0u; key < 13u; ++key)
            payload[2u + slot * 13u + key] =
                (uint8_t)((slot + key) % ((unsigned int)maximum[key] + 1u));
    memcpy(saved, payload, sizeof saved);

    /* Delete slot 7: clear its presence bit and all thirteen Private bytes. */
    payload[0] &= (uint8_t)~(1u << 7);
    memset(payload + 2u + 7u * 13u, 0, 13u);
    n = 0u;
    check("deleting a preset clears its entire Private extension",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PRESET_BANK, 0u, 10u, PRESET_CRC,
              payload, sizeof payload, 0u) == ENERGUN_SC_OK &&
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PRESET_BANK,
              0u, 10u, PRESET_CRC, 0u, &view) == ENERGUN_SC_OK &&
          memcmp(view.payload, payload, sizeof payload) == 0);

    /* Reuse slot 7 with a new value set; all other slots must remain unchanged. */
    payload[0] |= (uint8_t)(1u << 7);
    for (key = 0u; key < 13u; ++key)
        payload[2u + 7u * 13u + key] = maximum[key];
    n = 0u;
    check("reusing a deleted preset slot cannot alter the other fifteen slots",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PRESET_BANK, 0u, 11u, PRESET_CRC,
              payload, sizeof payload, 0u) == ENERGUN_SC_OK &&
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PRESET_BANK,
              0u, 11u, PRESET_CRC, 0u, &view) == ENERGUN_SC_OK &&
          memcmp(view.payload + 2u, saved + 2u, 7u * 13u) == 0 &&
          memcmp(view.payload + 2u + 8u * 13u,
                 saved + 2u + 8u * 13u, 8u * 13u) == 0);
}

static void test_envelope_rejection_and_atomic_encode(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    uint8_t before[ENERGUN_SC_V1_MAX_RECORD];
    energun_sc_view_t view;
    size_t n = 0u;
    make_project(payload);
    check("unknown identity kind cannot encode",
          energun_sc_v1_encode(record, sizeof record, &n,
              (energun_sc_identity_kind_t)9, 0u, 1u, PROJECT_CRC, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_IDENTITY);
    check("live autosave accepts only logical index zero",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_LIVE_AUTOSAVE, 1u, 1u, PROJECT_CRC, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_IDENTITY);
    check("project payload with trailing byte rejected",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 0u, 1u, PROJECT_CRC, payload,
              PROJECT_BASE + 2u, 0u) == ENERGUN_SC_ERR_LENGTH);
    memset(record, 0xA5, sizeof record);
    memcpy(before, record, sizeof record);
    payload[0] = 200u;
    check("invalid payload leaves encoder output untouched",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 0u, 1u, PROJECT_CRC, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_PARAMETER_VALUE &&
          memcmp(record, before, sizeof record) == 0 && n == 0u);
    make_project(payload);
    check("short output capacity rejected",
          energun_sc_v1_encode(record, 20u, &n,
              ENERGUN_SC_ID_PROJECT, 0u, 1u, PROJECT_CRC, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_LENGTH);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[0] = 'X';
    check("bad magic rejected", VAL_PROJECT(record, n, PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_MAGIC);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[4] = 2u;
    check("unsupported format version rejected", VAL_PROJECT(record, n, PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_FORMAT_VERSION);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[5] = 2u;
    check("unsupported schema version rejected", VAL_PROJECT(record, n, PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_SCHEMA_VERSION);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[18] = 1u;
    check("nonzero reserved envelope field rejected", VAL_PROJECT(record, n, PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_RESERVED);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("truncated record rejected", VAL_PROJECT(record, n - 1u, PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_LENGTH);
    check("trailing record byte rejected", VAL_PROJECT(record, n + 1u, PROJECT_SEQ, PROJECT_CRC, 0u, &view) == ENERGUN_SC_ERR_LENGTH);
}

int main(void)
{
    test_project_roundtrip_and_binding();
    test_pmc_ranges();
    test_private_types();
    test_motion_payload();
    test_preset_bank();
    test_preset_bank_all_slots();
    test_envelope_rejection_and_atomic_encode();
    printf("CP1.4.5 Sidecar V1: %u checks, %u failures\n", checks, failures);
    return failures != 0u;
}
