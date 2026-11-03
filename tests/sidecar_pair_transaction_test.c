/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — host-only Sidecar/base pair transaction acceptance harness.
 *
 * Includes the real Felucca storage.c and Sidecar V1 serializer. The Sidecar
 * sectors below are a test-only backend, deliberately separate from the 1 MiB
 * base NOR image. This does NOT implement or authorize a target Flash map.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define __attribute__(x)
static uint8_t nor[0x100000];
static int fail_bytes = -1;
static int erase_error;

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off + n > sizeof nor) return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if ((off & 0xFFFu) || off + 4096u > sizeof nor || erase_error) return -1;
    memset(nor + off, 0xFF, 4096u);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *p = src;
    uint32_t i;
    if (off + n > sizeof nor || (off & 255u) + n > 256u) return -1;
    for (i = 0; i < n; ++i) {
        if (fail_bytes == 0) return -9;
        if (fail_bytes > 0) --fail_bytes;
        if ((nor[off + i] & p[i]) != p[i]) return -1;
        nor[off + i] &= p[i];
    }
    return 0;
}

#include "../firmware/src/storage.c"
#include "../firmware/src/energun_sidecar_v1.c"

#define SC_TEST_TYPE 0x7Eu
#define SC_TEST_PAYLOAD_OFF 256u
#define SC_TEST_MAX_RECORD ENERGUN_SC_V1_MAX_RECORD

typedef struct {
    uint8_t bytes[4096];
} sc_test_sector_t;

static sc_test_sector_t sc_nor[7][2];
static int sc_fail_after = -1;

static int sc_identity_slot(energun_sc_identity_kind_t kind, uint8_t index)
{
    if (kind == ENERGUN_SC_ID_PROJECT && index < 4u) return (int)index;
    if (kind == ENERGUN_SC_ID_LIVE_AUTOSAVE && index == 0u) return 4;
    if (kind == ENERGUN_SC_ID_PRESET_BANK && index < 2u) return 5 + (int)index;
    return -1;
}

static int sc_read_copy(energun_sc_identity_kind_t kind, uint8_t index,
                        unsigned copy, uint8_t *record, size_t cap,
                        size_t *record_len, uint32_t *sequence)
{
    st_hdr_t h;
    uint32_t off;
    int identity = sc_identity_slot(kind, index);
    if (identity < 0 || copy > 1u || record == NULL || record_len == NULL || sequence == NULL)
        return -1;
    off = 0u;
    memcpy(&h, sc_nor[identity][copy].bytes + off, sizeof h);
    if (h.magic != ST_MAGIC || h.type != SC_TEST_TYPE || h.slot != copy ||
        h.len > cap || h.len > ST_PAYLOAD_MAX ||
        h.hcrc != st_crc32(&h, sizeof h - 4u))
        return -1;
    if (st_crc32(sc_nor[identity][copy].bytes + SC_TEST_PAYLOAD_OFF, h.len) != h.crc)
        return -1;
    memcpy(record, sc_nor[copy].bytes + SC_TEST_PAYLOAD_OFF, h.len);
    *record_len = h.len;
    *sequence = h.seq;
    return 0;
}

static int sc_program_bytes(uint8_t *dst, const uint8_t *src, size_t n)
{
    size_t i;
    for (i = 0; i < n; ++i) {
        if (sc_fail_after == 0) return -1;
        if (sc_fail_after > 0) --sc_fail_after;
        if ((dst[i] & src[i]) != src[i]) return -1;
        dst[i] &= src[i];
    }
    return 0;
}

/* Test-only sector backend with the Felucca 32-byte header-last convention. */
static int sc_write_copy(energun_sc_identity_kind_t kind, uint8_t index,
                         unsigned copy, const uint8_t *record, size_t len,
                         uint32_t sequence)
{
    st_hdr_t h;
    uint8_t header[sizeof h];
    int identity = sc_identity_slot(kind, index);
    if (identity < 0 || copy > 1u || record == NULL || len > ST_PAYLOAD_MAX)
        return -1;
    memset(sc_nor[identity][copy].bytes, 0xFF, sizeof sc_nor[identity][copy].bytes);
    if (sc_program_bytes(sc_nor[identity][copy].bytes + SC_TEST_PAYLOAD_OFF, record, len))
        return -1;
    h.magic = ST_MAGIC;
    h.type = SC_TEST_TYPE;
    h.slot = (uint16_t)copy;
    h.seq = sequence;
    h.len = (uint32_t)len;
    h.crc = st_crc32(record, (uint32_t)len);
    h.rsv[0] = h.rsv[1] = 0xFFFFFFFFu;
    h.hcrc = st_crc32(&h, sizeof h - 4u);
    memcpy(header, &h, sizeof h);
    if (sc_program_bytes(sc_nor[identity][copy].bytes, header, sizeof header))
        return -1;
    return 0;
}

static void sc_candidates(energun_sc_identity_kind_t kind, uint8_t index,
                          energun_sc_candidate_t out[2],
                          uint8_t records[2][SC_TEST_MAX_RECORD])
{
    unsigned i;
    memset(out, 0, 2u * sizeof *out);
    for (i = 0; i < 2u; ++i) {
        size_t n = 0;
        uint32_t seq = 0;
        if (sc_read_copy(kind, index, i, records[i], SC_TEST_MAX_RECORD, &n, &seq) == 0) {
            out[i].record = records[i];
            out[i].record_length = n;
            out[i].storage_sequence = seq;
            out[i].storage_valid = 1u;
        }
    }
}

static int pair_load(uint32_t obj, energun_sc_identity_kind_t kind, uint8_t index,
                     uint8_t *base, size_t base_cap,
                     uint8_t *extension, size_t extension_cap,
                     uint32_t *base_seq, int *sidecar_copy)
{
    st_meta_t meta;
    st_meta_t read_meta;
    uint8_t records[2][SC_TEST_MAX_RECORD];
    energun_sc_candidate_t candidates[2];
    energun_sc_ab_selection_t selection;
    energun_sc_view_t view;
    size_t ext_len = 0;
    int rc;
    if (st_current_meta(obj, &meta) < 0 || meta.payload_length > base_cap)
        return -1;
    if (st_read_copy_meta(obj, meta.copy, base, (uint32_t)base_cap, &read_meta) < 0)
        return -1;
    sc_candidates(kind, index, candidates, records);
    if (energun_sc_v1_select_ab(candidates, kind, index,
            meta.sequence, meta.payload_crc, 0u, &selection) != ENERGUN_SC_OK ||
        selection.matching_copy < 0)
        return -2;
    rc = sc_read_copy(kind, index, (unsigned)selection.matching_copy,
                      extension, extension_cap, &ext_len, &meta.sequence);
    if (rc) return -3;
    rc = energun_sc_v1_validate(extension, ext_len, kind, index,
            read_meta.sequence, read_meta.payload_crc, 0u, &view);
    if (rc != ENERGUN_SC_OK) return -4;
    *base_seq = read_meta.sequence;
    *sidecar_copy = selection.matching_copy;
    return 0;
}

static int pair_commit(uint32_t obj, energun_sc_identity_kind_t kind, uint8_t index,
                       const uint8_t *base, size_t base_len,
                       const uint8_t *logical_payload, uint16_t payload_len,
                       int *sidecar_target_out)
{
    st_meta_t current;
    uint32_t current_seq = 0u, current_crc = 0u;
    uint32_t next_base_seq, next_base_crc;
    int current_copy = -1, base_target;
    uint8_t records[2][SC_TEST_MAX_RECORD];
    uint8_t encoded[SC_TEST_MAX_RECORD];
    size_t encoded_len = 0;
    energun_sc_candidate_t candidates[2];
    energun_sc_ab_selection_t selection;
    if (base == NULL || base_len > ST_PAYLOAD_MAX || logical_payload == NULL)
        return -1;
    if (st_current_meta(obj, &current) == 0) {
        current_seq = current.sequence;
        current_crc = current.payload_crc;
        current_copy = current.copy;
    }
    next_base_seq = current_seq + 1u;
    next_base_crc = st_crc32(base, (uint32_t)base_len);
    base_target = current_copy < 0 ? 0 : 1 - current_copy;
    sc_candidates(kind, index, candidates, records);
    if (energun_sc_v1_select_ab(candidates, kind, index,
            current_seq, current_crc, 0u, &selection) != ENERGUN_SC_OK)
        return -2;
    if (energun_sc_v1_encode(encoded, sizeof encoded, &encoded_len,
            kind, index, next_base_seq, next_base_crc,
            logical_payload, payload_len, 0u) != ENERGUN_SC_OK)
        return -3;
    if (sc_write_copy(kind, index, (unsigned)selection.write_copy, encoded, encoded_len,
                      selection.next_storage_sequence))
        return -4;
    if (sidecar_target_out != NULL) *sidecar_target_out = selection.write_copy;
    if (st_save_to(obj, base, (uint32_t)base_len, base_target) != 0)
        return -5;
    {
        st_meta_t committed;
        uint8_t verify_base[ST_PAYLOAD_MAX];
        uint8_t verify_sc[SC_TEST_MAX_RECORD];
        size_t verify_sc_len = 0;
        uint32_t verify_sc_seq = 0;
        if (st_current_meta(obj, &committed) < 0 ||
            committed.copy != (uint8_t)base_target ||
            committed.sequence != next_base_seq ||
            committed.payload_crc != next_base_crc ||
            st_read_copy_meta(obj, committed.copy, verify_base,
                              sizeof verify_base, &committed) < 0 ||
            committed.payload_length != base_len ||
            memcmp(verify_base, base, base_len) != 0 ||
            sc_read_copy(kind, index, (unsigned)selection.write_copy, verify_sc,
                         sizeof verify_sc, &verify_sc_len, &verify_sc_seq) != 0)
            return -6;
        if (energun_sc_v1_validate(verify_sc, verify_sc_len,
                kind, index, next_base_seq, next_base_crc,
                0u, &(energun_sc_view_t){0}) != ENERGUN_SC_OK)
            return -7;
    }
    return 0;
}

static int check(const char *label, int ok)
{
    printf("%-58s %s\n", label, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static void reset_media(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(sc_nor, 0xFF, sizeof sc_nor);
    fail_bytes = -1;
    sc_fail_after = -1;
    erase_error = 0;
}

int main(void)
{
    uint8_t payload[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
    uint8_t base1[64], base2[64], loaded_base[128], loaded_sc[SC_TEST_MAX_RECORD];
    uint32_t seq;
    int copy, bad = 0, target;
    unsigned cut;
    memset(payload, 0, sizeof payload); /* zero Motion; all parameter/private domains valid */
    memset(base1, 0x31, sizeof base1);
    memset(base2, 0x52, sizeof base2);

    reset_media();
    bad += check("first base+Sidecar pair commit", pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, &target) == 0);
    bad += check("first pair binds exact Base sequence/CRC",
        pair_load(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                  sizeof loaded_sc, &seq, &copy) == 0 &&
        seq == 1u && copy == target && !memcmp(loaded_base, base1, sizeof base1));

    bad += check("second pair replaces old pair", pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, &target) == 0);
    bad += check("second pair loads exact new Base and Sidecar",
        pair_load(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                  sizeof loaded_sc, &seq, &copy) == 0 &&
        seq == 2u && copy == target && !memcmp(loaded_base, base2, sizeof base2));

    /* Seven independent Base A/B histories: four projects, live autosave, two preset banks. */
    {
        static const struct {
            energun_sc_identity_kind_t kind;
            uint8_t index;
            uint32_t object;
            uint16_t payload_len;
        } ids[7] = {
            { ENERGUN_SC_ID_PROJECT, 0u, OBJ_PROJECT0 + 0u, 117u },
            { ENERGUN_SC_ID_PROJECT, 1u, OBJ_PROJECT0 + 1u, 117u },
            { ENERGUN_SC_ID_PROJECT, 2u, OBJ_PROJECT0 + 2u, 117u },
            { ENERGUN_SC_ID_PROJECT, 3u, OBJ_PROJECT0 + 3u, 117u },
            { ENERGUN_SC_ID_LIVE_AUTOSAVE, 0u, OBJ_AUTOSAVE, 117u },
            { ENERGUN_SC_ID_PRESET_BANK, 0u, OBJ_UPRESET0 + 0u, ENERGUN_SC_V1_PRESET_PAYLOAD },
            { ENERGUN_SC_ID_PRESET_BANK, 1u, OBJ_UPRESET0 + 1u, ENERGUN_SC_V1_PRESET_PAYLOAD }
        };
        unsigned i;
        int matrix_ok = 1;
        reset_media();
        for (i = 0; i < 7u; ++i) {
            uint8_t p1[64], p2[64], ext[SC_TEST_MAX_RECORD], got[128];
            uint8_t logical[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
            uint32_t got_seq = 0;
            int got_copy = -1;
            memset(logical, 0, sizeof logical);
            memset(p1, (int)(0x20u + i), sizeof p1);
            memset(p2, (int)(0x60u + i), sizeof p2);
            if (pair_commit(ids[i].object, ids[i].kind, ids[i].index,
                            p1, sizeof p1, logical, ids[i].payload_len, NULL) ||
                pair_commit(ids[i].object, ids[i].kind, ids[i].index,
                            p2, sizeof p2, logical, ids[i].payload_len, NULL) ||
                pair_load(ids[i].object, ids[i].kind, ids[i].index, got, sizeof got,
                          ext, sizeof ext, &got_seq, &got_copy) ||
                got_seq != 2u || memcmp(got, p2, sizeof p2) != 0)
                matrix_ok = 0;
        }
        bad += check("seven identities: independent Base A/B histories and exact pair roundtrip", matrix_ok);
    }

    /* Every torn Sidecar payload/header byte must leave the original pair readable. */
    for (cut = 0; cut < ENERGUN_SC_V1_HEADER_SIZE + 117u + sizeof(st_hdr_t); ++cut) {
        reset_media();
        if (pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, NULL) != 0) { bad++; break; }
        sc_fail_after = (int)cut;
        (void)pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, NULL);
        sc_fail_after = -1;
        bad += pair_load(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                         sizeof loaded_sc, &seq, &copy) == 0 &&
               seq == 1u && !memcmp(loaded_base, base1, sizeof base1) ? 0 : 1;
    }
    bad += check("Sidecar torn-write sweep preserves old pair", bad == 0);

    /* Every torn Base payload/header byte after a successful future-bound Sidecar write leaves old pair. */
    for (cut = 0; cut < sizeof base2 + sizeof(st_hdr_t); ++cut) {
        reset_media();
        if (pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, NULL) != 0) { bad++; break; }
        fail_bytes = (int)cut;
        (void)pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, NULL);
        fail_bytes = -1;
        bad += pair_load(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                         sizeof loaded_sc, &seq, &copy) == 0 &&
               seq == 1u && !memcmp(loaded_base, base1, sizeof base1) ? 0 : 1;
    }
    bad += check("Base torn-write sweep preserves old pair", bad == 0);

    /* A newer orphan must not displace the old matching copy as the next write target. */
    reset_media();
    bad += check("seed matching pair", pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, NULL) == 0);
    fail_bytes = 0; /* Sidecar B is committed, Base A erase/write then fails */
    bad += check("failed Base commit leaves old pair selected",
        pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, NULL) != 0 &&
        (fail_bytes = -1, pair_load(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                                    sizeof loaded_sc, &seq, &copy) == 0) &&
        seq == 1u && copy == 0 && !memcmp(loaded_base, base1, sizeof base1));
    bad += check("retry protects old matching copy despite newer orphan",
        pair_commit(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, &target) == 0 &&
        target == 1 &&
        pair_load(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                  sizeof loaded_sc, &seq, &copy) == 0 &&
        seq == 2u && copy == 1 && !memcmp(loaded_base, base2, sizeof base2));

    /* Corrupt the newest Base body: Felucca's real storage reader must fall back,
       and the matching old Sidecar must be the only accepted extension. */
    {
        st_meta_t meta;
        if (st_current_meta(OBJ_PROJECT0, &meta) == 0) {
            uint32_t off = st_sector(OBJ_PROJECT0, meta.copy) + ST_PAYLOAD_OFF;
            nor[off] ^= 1u;
        }
        bad += check("corrupt newest Base falls back to old Base+Sidecar",
            pair_load(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                      sizeof loaded_sc, &seq, &copy) == 0 &&
            seq == 1u && copy == 0 && !memcmp(loaded_base, base1, sizeof base1));
    }

    printf("%s\n", bad ? "SIDECAR PAIR TEST FAILED" : "SIDECAR PAIR TEST PASSED");
    return bad ? 1 : 0;
}
