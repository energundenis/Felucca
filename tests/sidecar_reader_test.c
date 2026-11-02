/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — exact-generation Sidecar reader tests.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../firmware/src/energun_sidecar_v1.c"

static unsigned int checks, failures;
static void check(const char *name, int ok)
{
    ++checks;
    printf("sidecar reader: %-72s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

static void make_payload(uint8_t payload[117], uint8_t motion_count)
{
    unsigned int part, i;
    memset(payload, 0, 117u);
    for (part = 0; part < 4u; ++part) {
        for (i = 0; i < 16u; ++i)
            payload[part * 16u + i] = i == 9u ? 0u : 64u;
    }
    payload[116] = motion_count;
}

static size_t make_record(uint8_t record[ENERGUN_SC_V1_MAX_RECORD],
                          uint32_t base_seq, uint32_t base_crc,
                          uint8_t motion_count)
{
    uint8_t payload[117];
    size_t length = 0u;
    make_payload(payload, motion_count);
    if (energun_sc_v1_encode(record, ENERGUN_SC_V1_MAX_RECORD, &length,
            ENERGUN_SC_ID_PROJECT, 0u, base_seq, base_crc,
            payload, sizeof payload, 0u) != ENERGUN_SC_OK)
        return 0u;
    return length;
}

static void set_candidate(energun_sc_candidate_t *candidate,
                          const uint8_t *record, size_t length,
                          uint32_t storage_seq, uint8_t storage_valid)
{
    candidate->record = record;
    candidate->record_length = length;
    candidate->storage_sequence = storage_seq;
    candidate->storage_valid = storage_valid;
}

int main(void)
{
    const uint32_t base_seq = 7u, base_crc = 0x12345678u;
    uint8_t a[ENERGUN_SC_V1_MAX_RECORD], b[ENERGUN_SC_V1_MAX_RECORD];
    uint8_t stale[ENERGUN_SC_V1_MAX_RECORD], malformed[ENERGUN_SC_V1_MAX_RECORD];
    size_t alen = make_record(a, base_seq, base_crc, 0u);
    size_t blen = make_record(b, base_seq, base_crc, 0u);
    size_t stale_len = make_record(stale, base_seq - 1u, base_crc ^ 1u, 0u);
    size_t malformed_len = make_record(malformed, base_seq, base_crc, 0u);
    energun_sc_candidate_t candidates[2];
    energun_sc_read_result_t result;

    check("fixture records encode", alen == 117u + ENERGUN_SC_V1_HEADER_SIZE &&
          blen == alen && stale_len == alen && malformed_len == alen);

    set_candidate(&candidates[0], a, alen, 8u, 1u);
    set_candidate(&candidates[1], b, blen, 9u, 1u);
    check("newest valid exact-generation copy wins",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_OK &&
        result.selected_copy == 1 && result.storage_sequence == 9u &&
        result.view.payload_length == 117u);

    set_candidate(&candidates[0], stale, stale_len, 100u, 1u);
    set_candidate(&candidates[1], b, blen, 9u, 1u);
    check("higher-sequence orphan cannot override exact Base match",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_OK &&
        result.selected_copy == 1);

    set_candidate(&candidates[0], b, blen, 10u, 0u);
    set_candidate(&candidates[1], a, alen, 9u, 1u);
    check("outer storage CRC failure excludes otherwise valid record",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_OK &&
        result.selected_copy == 1);

    malformed[ENERGUN_SC_V1_HEADER_SIZE + 64u] = 3u;
    set_candidate(&candidates[0], malformed, malformed_len, 12u, 1u);
    set_candidate(&candidates[1], b, blen, 11u, 1u);
    check("semantic-invalid newer record is rejected",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_OK &&
        result.selected_copy == 1);

    set_candidate(&candidates[0], a, alen, 20u, 1u);
    set_candidate(&candidates[1], b, blen, 20u, 1u);
    check("equal local sequence deterministically selects A",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_OK &&
        result.selected_copy == 0);

    set_candidate(&candidates[0], a, alen, 0xFFFFFFFEu, 1u);
    set_candidate(&candidates[1], b, blen, 1u, 1u);
    check("unsigned half-range sequence wrap selects newer B",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_OK &&
        result.selected_copy == 1);

    set_candidate(&candidates[0], stale, stale_len, 100u, 1u);
    set_candidate(&candidates[1], NULL, 0u, 0u, 0u);
    check("no exact-generation record returns NO_MATCH and clears selection",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_ERR_NO_MATCH &&
        result.selected_copy == -1);

    set_candidate(&candidates[0], a, alen, 8u, 1u);
    set_candidate(&candidates[1], NULL, 0u, 0u, 0u);
    check("shared Motion pool overflow invalidates matching candidate",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 0u,
            base_seq, base_crc, 129u, &result) == ENERGUN_SC_ERR_MOTION_COUNT);

    check("invalid identity is rejected before candidate selection",
        energun_sc_v1_read_matching(candidates, ENERGUN_SC_ID_PROJECT, 9u,
            base_seq, base_crc, 0u, &result) == ENERGUN_SC_ERR_IDENTITY);

    printf("sidecar reader: %s (%u checks, %u failures)\n",
        failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
