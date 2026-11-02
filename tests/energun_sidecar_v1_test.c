/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Sidecar V1 host acceptance tests.
 *
 * Compiles the exact firmware-side encoder/validator with no Flash backend.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../firmware/src/energun_sidecar_v1.h"

#define PROJECT_BASE 116u
#define MOTION_COUNT_OFF 116u
#define MOTION_OFF 117u

static unsigned int failures;
static unsigned int checks;

static void check(const char *name, int ok)
{
    ++checks;
    printf("CP1.4.5 Sidecar V1: %-70s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok)
        ++failures;
}

static void make_project(uint8_t *payload)
{
    memset(payload, 0, ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD);
    payload[MOTION_COUNT_OFF] = 0u;
}

static void make_preset(uint8_t *payload)
{
    memset(payload, 0, ENERGUN_SC_V1_PRESET_PAYLOAD);
}

static size_t encode_project(uint8_t *record, const uint8_t *payload,
                             uint16_t payload_length, uint16_t base_motion_count)
{
    size_t written = 0u;
    energun_sc_status_t status = energun_sc_v1_encode(
        record, ENERGUN_SC_V1_MAX_RECORD, &written,
        ENERGUN_SC_ID_PROJECT, 2u, 0xFFFFFFFFu, 0x12345678u,
        payload, payload_length, base_motion_count);
    return status == ENERGUN_SC_OK ? written : 0u;
}

static void test_project_roundtrip_and_binding(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    energun_sc_view_t view;
    size_t n;
    make_project(payload);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("project identity encodes and validates at sequence wrap boundary",
          n == ENERGUN_SC_V1_HEADER_SIZE + PROJECT_BASE + 1u &&
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 2u,
              0xFFFFFFFFu, 0x12345678u, 0u, &view) == ENERGUN_SC_OK);
    check("decoded view exposes exact payload and generation",
          view.base_sequence == 0xFFFFFFFFu &&
          view.payload_length == PROJECT_BASE + 1u &&
          view.payload == record + ENERGUN_SC_V1_HEADER_SIZE);
    check("wrong base generation is rejected",
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 2u,
              0u, 0x12345678u, 0u, &view) == ENERGUN_SC_ERR_BASE_GENERATION);
    check("wrong project slot is rejected",
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 1u,
              0xFFFFFFFFu, 0u, &view) == ENERGUN_SC_ERR_IDENTITY);
}

static void test_pmc_ranges(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    size_t n;
    make_project(payload);
    payload[9] = 0xC0u; /* signed -64 */
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("FOLD SYMMETRY signed lower endpoint -64 is accepted", n != 0u);
    payload[9] = 0x3Fu; /* +63 */
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("FOLD SYMMETRY signed upper endpoint +63 is accepted", n != 0u);
    payload[9] = 0xBFu; /* -65 */
    check("FOLD SYMMETRY below -64 is rejected",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[9] = 0x40u; /* +64 */
    check("FOLD SYMMETRY above +63 is rejected",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[9] = 0u;
    payload[0] = 128u;
    check("unipolar P_MC code 128 is rejected",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[0] = 127u;
    check("unipolar P_MC upper endpoint 127 is accepted",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) != 0u);
}

static void test_private_types(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    make_project(payload);
    payload[64u + 5u] = 2u; /* FM CHARACTER: SMOOTH/CLASSIC/METALLIC */
    check("FM CHARACTER ordinal 2 remains representable",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) != 0u);
    payload[64u + 1u] = 2u; /* FOLLOW boolean */
    check("FOLLOW rejects non-boolean value 2",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[64u + 1u] = 1u;
    payload[64u + 4u] = 4u; /* MOD OSC WAVE has four values */
    check("MOD OSC WAVE rejects ordinal outside four values",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) == 0u);
    payload[64u + 4u] = 3u;
    payload[64u + 6u] = 127u; /* module volume */
    check("private 7-bit continuous maximum is accepted",
          encode_project(record, payload, PROJECT_BASE + 1u, 0u) != 0u);
}

static void test_motion_payload(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    unsigned int i;
    size_t n;
    make_project(payload);
    payload[MOTION_COUNT_OFF] = 128u;
    for (i = 0; i < 128u; ++i) {
        uint8_t *event = payload + MOTION_OFF + i * 3u;
        event[0] = (uint8_t)i; /* unique track/step place */
        event[1] = 1u;
        event[2] = 0u;
    }
    n = encode_project(record, payload, ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD, 0u);
    check("maximum 128 Sidecar events encode to exact 501-byte payload",
          n == ENERGUN_SC_V1_MAX_RECORD);
    check("shared pool rejects 128 Sidecar events plus one base event",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 2u, 9u, 0x12345678u, payload,
              ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD, 1u) == ENERGUN_SC_ERR_MOTION_COUNT);
    payload[MOTION_COUNT_OFF] = 129u;
    check("event count above global 128 is rejected",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 2u, 9u, payload,
              ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD, 0u) == ENERGUN_SC_ERR_MOTION_COUNT);
    payload[MOTION_COUNT_OFF] = 2u;
    payload[MOTION_OFF + 0u] = 7u;
    payload[MOTION_OFF + 1u] = 3u;
    payload[MOTION_OFF + 2u] = 0u;
    payload[MOTION_OFF + 3u] = 7u;
    payload[MOTION_OFF + 4u] = 0x83u; /* same parameter, lock bit differs */
    payload[MOTION_OFF + 5u] = 1u;
    check("duplicate place/parameter event rejected regardless of lock bit",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 2u, 9u, payload,
              MOTION_OFF + 6u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
    payload[MOTION_COUNT_OFF] = 1u;
    payload[MOTION_OFF] = 0u;
    payload[MOTION_OFF + 1u] = 1u;
    payload[MOTION_OFF + 2u] = 0xBFu; /* -65 */
    check("motion value below -64 rejected",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 2u, 9u, payload,
              MOTION_OFF + 3u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
    payload[MOTION_OFF + 2u] = 0x80u; /* -128 */
    check("motion value below signed storage range rejected",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 2u, 9u, payload,
              MOTION_OFF + 3u, 0u) == ENERGUN_SC_ERR_MOTION_EVENT);
}

static void test_preset_bank(void)
{
    uint8_t payload[ENERGUN_SC_V1_PRESET_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    size_t n = 0u;
    make_preset(payload);
    payload[0] = 1u; /* slot 0 present */
    payload[2u + 5u] = 2u;
    check("preset bank with one present slot validates",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PRESET_BANK, 1u, 77u, 0x87654321u, 0x12345678u, payload,
              sizeof payload, 0u) == ENERGUN_SC_OK &&
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PRESET_BANK, 1u,
              77u, 0x87654321u, 0u, &(energun_sc_view_t){0}) == ENERGUN_SC_OK);
    payload[2u + 13u] = 1u; /* slot 1 absent but stale data remains */
    check("absent preset slot cannot retain stale Private values",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PRESET_BANK, 1u, 77u, payload,
              sizeof payload, 0u) == ENERGUN_SC_ERR_SLOT_BITMAP);
    payload[2u + 13u] = 0u;
    payload[2u + 5u] = 3u;
    check("invalid Private enum in used preset slot is rejected",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PRESET_BANK, 1u, 77u, payload,
              sizeof payload, 0u) == ENERGUN_SC_ERR_PRIVATE_VALUE);
}

static void test_envelope_rejection_and_atomic_encode(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t record[ENERGUN_SC_V1_MAX_RECORD];
    uint8_t before[ENERGUN_SC_V1_MAX_RECORD];
    energun_sc_view_t view;
    size_t n = 0u;
    make_project(payload);
    check("wrong identity kind cannot encode",
          energun_sc_v1_encode(record, sizeof record, &n,
              (energun_sc_identity_kind_t)9, 0u, 1u, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_IDENTITY);
    check("live autosave accepts only logical index zero",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_LIVE_AUTOSAVE, 1u, 0x12345678u, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_IDENTITY);
    check("project payload with trailing byte is rejected",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 0u, 1u, payload,
              PROJECT_BASE + 2u, 0u) == ENERGUN_SC_ERR_LENGTH);
    memset(record, 0xA5, sizeof record);
    memcpy(before, record, sizeof record);
    payload[0] = 200u;
    check("invalid payload does not modify encoder output",
          energun_sc_v1_encode(record, sizeof record, &n,
              ENERGUN_SC_ID_PROJECT, 0u, 1u, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_PARAMETER_VALUE &&
          memcmp(record, before, sizeof record) == 0 && n == 0u);
    make_project(payload);
    check("short output capacity rejected without write",
          energun_sc_v1_encode(record, 20u, &n,
              ENERGUN_SC_ID_PROJECT, 0u, 1u, payload,
              PROJECT_BASE + 1u, 0u) == ENERGUN_SC_ERR_LENGTH);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[0] = 'X';
    check("bad magic rejected",
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 2u,
              0xFFFFFFFFu, 0u, &view) == ENERGUN_SC_ERR_MAGIC);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[4] = 2u;
    check("unsupported format version rejected",
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 2u,
              0xFFFFFFFFu, 0u, &view) == ENERGUN_SC_ERR_FORMAT_VERSION);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[5] = 2u;
    check("unsupported logical schema version rejected",
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 2u,
              0xFFFFFFFFu, 0u, &view) == ENERGUN_SC_ERR_SCHEMA_VERSION);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    record[18] = 1u;
    check("nonzero reserved envelope field rejected",
          energun_sc_v1_validate(record, n, ENERGUN_SC_ID_PROJECT, 2u,
              0xFFFFFFFFu, 0u, &view) == ENERGUN_SC_ERR_RESERVED);
    n = encode_project(record, payload, PROJECT_BASE + 1u, 0u);
    check("truncated record rejected",
          energun_sc_v1_validate(record, n - 1u, ENERGUN_SC_ID_PROJECT, 2u,
              0xFFFFFFFFu, 0u, &view) == ENERGUN_SC_ERR_LENGTH);
    check("trailing record byte rejected",
          energun_sc_v1_validate(record, n + 1u, ENERGUN_SC_ID_PROJECT, 2u,
              0xFFFFFFFFu, 0u, &view) == ENERGUN_SC_ERR_LENGTH);
}

int main(void)
{
    test_project_roundtrip_and_binding();
    test_pmc_ranges();
    test_private_types();
    test_motion_payload();
    test_preset_bank();
    test_envelope_rejection_and_atomic_encode();
    printf("CP1.4.5 Sidecar V1: %u checks, %u failures\n", checks, failures);
    return failures != 0u;
}
