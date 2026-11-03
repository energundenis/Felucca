/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — seven-identity Sidecar isolation matrix.
 *
 * Host-only serializer/identity test. This does not map Sidecar copies to
 * target Flash, implement the production pair coordinator, or prove Base
 * transaction behavior for these identities.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "energun_sidecar_v1.h"

#define IDENTITY_COUNT 7u
#define RECORD_CAP ENERGUN_SC_V1_MAX_RECORD

typedef struct {
    uint8_t record[RECORD_CAP];
    size_t length;
    uint32_t storage_sequence;
    uint8_t valid;
} copy_t;

typedef struct {
    energun_sc_identity_kind_t kind;
    uint8_t index;
    copy_t ab[2];
} identity_t;

static identity_t identities[IDENTITY_COUNT] = {
    { ENERGUN_SC_ID_PROJECT, 0u, {{{0},0,0,0},{{0},0,0,0}} },
    { ENERGUN_SC_ID_PROJECT, 1u, {{{0},0,0,0},{{0},0,0,0}} },
    { ENERGUN_SC_ID_PROJECT, 2u, {{{0},0,0,0},{{0},0,0,0}} },
    { ENERGUN_SC_ID_PROJECT, 3u, {{{0},0,0,0},{{0},0,0,0}} },
    { ENERGUN_SC_ID_LIVE_AUTOSAVE, 0u, {{{0},0,0,0},{{0},0,0,0}} },
    { ENERGUN_SC_ID_PRESET_BANK, 0u, {{{0},0,0,0},{{0},0,0,0}} },
    { ENERGUN_SC_ID_PRESET_BANK, 1u, {{{0},0,0,0},{{0},0,0,0}} }
};

static unsigned failures;

static void check(const char *label, int ok)
{
    printf("%-58s %s\n", label, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

static uint16_t payload_length(const identity_t *id)
{
    return id->kind == ENERGUN_SC_ID_PRESET_BANK ?
        ENERGUN_SC_V1_PRESET_PAYLOAD : 117u;
}

static int commit(identity_t *id, unsigned copy, uint32_t base_seq,
                  uint32_t base_crc, const uint8_t *payload)
{
    size_t n = 0u;
    if (copy > 1u || energun_sc_v1_encode(id->ab[copy].record,
            sizeof id->ab[copy].record, &n, id->kind, id->index,
            base_seq, base_crc, payload, payload_length(id), 0u) != ENERGUN_SC_OK)
        return -1;
    id->ab[copy].length = n;
    id->ab[copy].storage_sequence =
        id->ab[1u-copy].valid ? id->ab[1u-copy].storage_sequence + 1u : 1u;
    id->ab[copy].valid = 1u;
    return 0;
}

static int validate_copy(const identity_t *id, unsigned copy,
                         uint32_t base_seq, uint32_t base_crc)
{
    energun_sc_view_t view;
    const copy_t *c = &id->ab[copy];
    return c->valid && energun_sc_v1_validate(c->record, c->length,
        id->kind, id->index, base_seq, base_crc, 0u, &view) == ENERGUN_SC_OK;
}

static void preset_set(uint8_t *p, unsigned slot, uint8_t seed)
{
    static const uint8_t fixed[6] = { 2u, 1u, 1u, 1u, 3u, 2u };
    unsigned k;
    p[slot >> 3] |= (uint8_t)(1u << (slot & 7u));
    for (k = 0; k < 13u; ++k) {
        uint8_t value = k < 6u ? fixed[k] : seed;
        p[2u + slot * 13u + k] = value;
    }
}

static void preset_clear(uint8_t *p, unsigned slot)
{
    unsigned k;
    p[slot >> 3] &= (uint8_t)~(1u << (slot & 7u));
    for (k = 0; k < 13u; ++k)
        p[2u + slot * 13u + k] = 0u;
}

int main(void)
{
    uint8_t project_payload[117] = {0};
    uint8_t preset_payload[ENERGUN_SC_V1_PRESET_PAYLOAD] = {0};
    uint8_t snapshot[ENERGUN_SC_V1_PRESET_PAYLOAD];
    unsigned i, j;
    char label[96];

    for (i = 0; i < IDENTITY_COUNT; ++i) {
        identity_t *id = &identities[i];
        uint8_t *payload = id->kind == ENERGUN_SC_ID_PRESET_BANK ?
            preset_payload : project_payload;
        uint32_t base_seq = 10u + i;
        uint32_t base_crc = 0xA1000000u + i;
        check("identity accepts its own first A/B record",
            commit(id, 0u, base_seq, base_crc, payload) == 0 &&
            validate_copy(id, 0u, base_seq, base_crc));
        check("identity accepts independent second A/B generation",
            commit(id, 1u, base_seq + 1u, base_crc + 1u, payload) == 0 &&
            validate_copy(id, 1u, base_seq + 1u, base_crc + 1u));
    }

    for (i = 0; i < IDENTITY_COUNT; ++i) {
        for (j = 0; j < IDENTITY_COUNT; ++j) {
            if (i == j) continue;
            (void)snprintf(label, sizeof label, "identity %u rejects identity %u record", i, j);
            check(label, energun_sc_v1_validate(
                identities[i].ab[1].record, identities[i].ab[1].length,
                identities[j].kind, identities[j].index,
                11u + i, 0xA1000001u + i, 0u,
                &(energun_sc_view_t){0}) == ENERGUN_SC_ERR_IDENTITY);
        }
    }

    /* Two preset banks have independent copies and independent slot maps. */
    memset(preset_payload, 0, sizeof preset_payload);
    preset_set(preset_payload, 1u, 10u);
    preset_set(preset_payload, 7u, 40u);
    memcpy(snapshot, preset_payload, sizeof snapshot);
    preset_set(preset_payload, 3u, 70u);
    check("preset slot write preserves all existing slots",
        preset_payload[2u + 1u * 13u] == snapshot[2u + 1u * 13u] &&
        preset_payload[2u + 7u * 13u] == snapshot[2u + 7u * 13u] &&
        (preset_payload[0] & (1u << 1)) && (preset_payload[0] & (1u << 7)));
    preset_clear(preset_payload, 7u);
    check("preset slot erase clears only target slot payload and bit",
        !(preset_payload[0] & (1u << 7)) &&
        preset_payload[2u + 7u * 13u] == 0u &&
        (preset_payload[0] & (1u << 1)) &&
        (preset_payload[0] & (1u << 3)) &&
        preset_payload[2u + 1u * 13u] == snapshot[2u + 1u * 13u]);

    memset(preset_payload, 0, sizeof preset_payload);
    preset_set(preset_payload, 12u, 90u);
    check("preset bank 0/1 identity records remain isolated",
        energun_sc_v1_encode(identities[5].ab[0].record, RECORD_CAP,
            &identities[5].ab[0].length, ENERGUN_SC_ID_PRESET_BANK, 0u,
            77u, 0x12345678u, preset_payload,
            ENERGUN_SC_V1_PRESET_PAYLOAD, 0u) == ENERGUN_SC_OK &&
        energun_sc_v1_validate(identities[5].ab[0].record,
            identities[5].ab[0].length, ENERGUN_SC_ID_PRESET_BANK, 1u,
            77u, 0x12345678u, 0u,
            &(energun_sc_view_t){0}) == ENERGUN_SC_ERR_IDENTITY);

    printf("%s\n", failures ? "SIDECAR IDENTITY MATRIX FAILED" :
        "SIDECAR IDENTITY MATRIX PASSED");
    return failures ? 1 : 0;
}
