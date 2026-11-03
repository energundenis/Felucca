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
#include "../firmware/src/energun_pair_v1.c"
#include "../firmware/src/energun_sidecar_payload_v1.c"
#include "../firmware/src/energun_private_v1.c"
#include "../firmware/src/energun_private_payload_v1.c"
#include "../firmware/src/energun_lifecycle_v1.c"
#include "../firmware/src/energun_preset_lifecycle_v1.c"

#define SC_TEST_TYPE 0x7Eu
#define SC_TEST_PAYLOAD_OFF 256u
#define SC_TEST_MAX_RECORD ENERGUN_SC_V1_MAX_RECORD

typedef struct {
    uint8_t bytes[4096];
} sc_test_sector_t;

static sc_test_sector_t sc_nor[7][2];
static int sc_fail_after = -1;
static int sc_read_error;

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
    memcpy(record, sc_nor[identity][copy].bytes + SC_TEST_PAYLOAD_OFF, h.len);
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

static int pair_test_base_current(void *context, uint32_t obj,
                                   energun_pair_v1_base_meta_t *meta)
{
    st_meta_t m;
    (void)context;
    if (meta == NULL) return -1;
    if (st_current_meta(obj, &m) < 0) return 1;
    meta->copy = m.copy;
    meta->sequence = m.sequence;
    meta->payload_length = m.payload_length;
    meta->payload_crc32 = m.payload_crc;
    return 0;
}

static int pair_test_base_load(void *context, uint32_t obj, uint8_t *payload,
                               size_t capacity, energun_pair_v1_base_meta_t *meta)
{
    st_meta_t current, loaded;
    (void)context;
    if (payload == NULL || meta == NULL || st_current_meta(obj, &current) < 0 ||
        current.payload_length > capacity ||
        st_read_copy_meta(obj, current.copy, payload, (uint32_t)capacity, &loaded) < 0)
        return -1;
    meta->copy = loaded.copy;
    meta->sequence = loaded.sequence;
    meta->payload_length = loaded.payload_length;
    meta->payload_crc32 = loaded.payload_crc;
    return 0;
}

static int pair_test_base_write(void *context, uint32_t obj, uint8_t copy,
                                const uint8_t *payload, size_t length)
{
    (void)context;
    return st_save_to(obj, payload, (uint32_t)length, (int)copy);
}

static int pair_test_sidecar_read(void *context, energun_sc_identity_kind_t kind,
                                  uint8_t index, uint8_t copy, uint8_t *record,
                                  size_t capacity, size_t *length, uint32_t *sequence)
{
    int rc;
    (void)context;
    if (sc_read_error) return -1;
    rc = sc_read_copy(kind, index, copy, record, capacity, length, sequence);
    return rc == 0 ? 0 : 1; /* simulated NOR: invalid/empty is not an I/O fault */
}

static int pair_test_sidecar_write(void *context, energun_sc_identity_kind_t kind,
                                   uint8_t index, uint8_t copy,
                                   const uint8_t *record, size_t length,
                                   uint32_t sequence)
{
    (void)context;
    return sc_write_copy(kind, index, copy, record, length, sequence);
}

static const energun_pair_v1_ops_t pair_test_ops = {
    pair_test_base_current,
    pair_test_base_load,
    pair_test_base_write,
    pair_test_sidecar_read,
    pair_test_sidecar_write
};

static energun_pair_v1_workspace_t coordinator_workspace;

static int coordinator_matching_copy(energun_sc_identity_kind_t kind, uint8_t index,
                                     const energun_pair_v1_base_meta_t *base,
                                     int *copy)
{
    energun_sc_candidate_t candidates[2];
    uint8_t records[2][SC_TEST_MAX_RECORD];
    energun_sc_ab_selection_t selection;
    sc_candidates(kind, index, candidates, records);
    if (energun_sc_v1_select_ab(candidates, kind, index,
            base->sequence, base->payload_crc32, 0u, &selection) != ENERGUN_SC_OK ||
        selection.matching_copy < 0)
        return -1;
    *copy = selection.matching_copy;
    return 0;
}

static int coordinator_commit_test(uint32_t obj, energun_sc_identity_kind_t kind,
                                   uint8_t index, const uint8_t *base, size_t base_len,
                                   const uint8_t *logical, uint16_t logical_len,
                                   int *sidecar_target_out)
{
    energun_pair_v1_base_meta_t current;
    int rc = energun_pair_v1_commit(&pair_test_ops, NULL, &coordinator_workspace,
        obj, kind, index, base, base_len, logical, logical_len, 0u);
    if (rc != ENERGUN_PAIR_V1_OK)
        return -1;
    if (sidecar_target_out != NULL) {
        if (pair_test_base_current(NULL, obj, &current) != 0 ||
            coordinator_matching_copy(kind, index, &current, sidecar_target_out) != 0)
            return -1;
    }
    return 0;
}

static int coordinator_load_test(uint32_t obj, energun_sc_identity_kind_t kind,
                                 uint8_t index, uint8_t *base, size_t base_cap,
                                 uint8_t *extension, size_t extension_cap,
                                 uint32_t *base_seq, int *sidecar_copy)
{
    energun_pair_v1_base_meta_t meta;
    size_t base_len = 0u, extension_len = 0u;
    int rc = energun_pair_v1_load(&pair_test_ops, NULL, &coordinator_workspace,
        obj, kind, index, base, base_cap, &base_len, extension, extension_cap,
        &extension_len, 0u, &meta);
    if (rc != ENERGUN_PAIR_V1_OK)
        return -1;
    if (base_seq != NULL) *base_seq = meta.sequence;
    if (sidecar_copy != NULL &&
        coordinator_matching_copy(kind, index, &meta, sidecar_copy) != 0)
        return -1;
    return 0;
}

static int check(const char *label, int ok)
{
    printf("%-58s %s\n", label, ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static int lifecycle_guard_value;
static int lifecycle_test_guard(void *context)
{
    (void)context;
    return lifecycle_guard_value;
}

static void reset_media(void)
{
    memset(nor, 0xFF, sizeof nor);
    memset(sc_nor, 0xFF, sizeof sc_nor);
    fail_bytes = -1;
    sc_fail_after = -1;
    sc_read_error = 0;
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
    bad += check("first base+Sidecar pair commit", coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, &target) == 0);
    bad += check("first pair binds exact Base sequence/CRC",
        coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                  sizeof loaded_sc, &seq, &copy) == 0 &&
        seq == 1u && copy == target && !memcmp(loaded_base, base1, sizeof base1));

    bad += check("second pair replaces old pair", coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, &target) == 0);
    bad += check("second pair loads exact new Base and Sidecar",
        coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                  sizeof loaded_sc, &seq, &copy) == 0 &&
        seq == 2u && copy == target && !memcmp(loaded_base, base2, sizeof base2));

    /* Lifecycle semantics: rename rebinds the same extension to a new Base
       generation; Save As creates an independent destination identity. */
    {
        uint8_t renamed_base[64], saved_as_base[64], got_base[128], got_sc[SC_TEST_MAX_RECORD];
        uint32_t got_seq = 0u;
        int lifecycle_ok = 1;
        memcpy(renamed_base, base2, sizeof renamed_base);
        renamed_base[0] = 0x7Au; /* project name/header change; sound extension is unchanged */
        lifecycle_ok &= coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            renamed_base, sizeof renamed_base, payload, 117u, NULL) == 0;
        lifecycle_ok &= coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            got_base, sizeof got_base, got_sc, sizeof got_sc, &got_seq, NULL) == 0 &&
            got_seq == 3u && !memcmp(got_base, renamed_base, sizeof renamed_base) &&
            !memcmp(got_sc, payload, 117u);

        memcpy(saved_as_base, renamed_base, sizeof saved_as_base);
        saved_as_base[1] = 0x6Bu;
        lifecycle_ok &= coordinator_commit_test(OBJ_PROJECT0 + 1u, ENERGUN_SC_ID_PROJECT, 1u,
            saved_as_base, sizeof saved_as_base, payload, 117u, NULL) == 0;
        lifecycle_ok &= coordinator_load_test(OBJ_PROJECT0 + 1u, ENERGUN_SC_ID_PROJECT, 1u,
            got_base, sizeof got_base, got_sc, sizeof got_sc, &got_seq, NULL) == 0 &&
            got_seq == 1u && !memcmp(got_base, saved_as_base, sizeof saved_as_base) &&
            !memcmp(got_sc, payload, 117u);
        lifecycle_ok &= coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            got_base, sizeof got_base, got_sc, sizeof got_sc, &got_seq, NULL) == 0 &&
            got_seq == 3u && !memcmp(got_base, renamed_base, sizeof renamed_base);
        bad += check("lifecycle: Rename preserves extension; Save As isolates destination", lifecycle_ok);
    }

    /* Seven identities exercise the reusable coordinator, not a test-local ordering model. */
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
        static energun_pair_v1_workspace_t workspace;
        unsigned i;
        int matrix_ok = 1;
        reset_media();
        for (i = 0; i < 7u; ++i) {
            uint8_t p1[64], p2[64], got[128], ext[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
            uint8_t logical[ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD];
            size_t got_base_len = 0u, got_ext_len = 0u;
            energun_pair_v1_base_meta_t meta;
            memset(logical, 0, sizeof logical);
            memset(p1, (int)(0x20u + i), sizeof p1);
            memset(p2, (int)(0x60u + i), sizeof p2);
            if (energun_pair_v1_commit(&pair_test_ops, NULL, &workspace,
                    ids[i].object, ids[i].kind, ids[i].index,
                    p1, sizeof p1, logical, ids[i].payload_len, 0u) != ENERGUN_PAIR_V1_OK ||
                energun_pair_v1_commit(&pair_test_ops, NULL, &workspace,
                    ids[i].object, ids[i].kind, ids[i].index,
                    p2, sizeof p2, logical, ids[i].payload_len, 0u) != ENERGUN_PAIR_V1_OK ||
                energun_pair_v1_load(&pair_test_ops, NULL, &workspace,
                    ids[i].object, ids[i].kind, ids[i].index,
                    got, sizeof got, &got_base_len, ext, sizeof ext, &got_ext_len,
                    0u, &meta) != ENERGUN_PAIR_V1_OK ||
                meta.sequence != 2u || got_base_len != sizeof p2 ||
                got_ext_len != ids[i].payload_len ||
                memcmp(got, p2, sizeof p2) != 0 ||
                memcmp(ext, logical, ids[i].payload_len) != 0)
                matrix_ok = 0;

            /* A Base object must never load a Sidecar through a different identity. */
            if (ids[i].kind == ENERGUN_SC_ID_PROJECT) {
                uint8_t wrong_index = (uint8_t)((ids[i].index + 1u) & 3u);
                if (energun_pair_v1_load(&pair_test_ops, NULL, &workspace,
                        ids[i].object, ENERGUN_SC_ID_PROJECT, wrong_index,
                        got, sizeof got, &got_base_len, ext, sizeof ext, &got_ext_len,
                        0u, &meta) == ENERGUN_PAIR_V1_OK)
                    matrix_ok = 0;
            } else if (energun_pair_v1_load(&pair_test_ops, NULL, &workspace,
                    ids[i].object, ENERGUN_SC_ID_PROJECT, 0u,
                    got, sizeof got, &got_base_len, ext, sizeof ext, &got_ext_len,
                    0u, &meta) == ENERGUN_PAIR_V1_OK) {
                matrix_ok = 0;
            }

            /* Erase is a Base tombstone: stale Sidecar A/B data must not load. */
            if (energun_pair_v1_erase(&pair_test_ops, NULL, ids[i].object) != ENERGUN_PAIR_V1_OK ||
                energun_pair_v1_load(&pair_test_ops, NULL, &workspace,
                    ids[i].object, ids[i].kind, ids[i].index,
                    got, sizeof got, &got_base_len, ext, sizeof ext, &got_ext_len,
                    0u, &meta) != ENERGUN_PAIR_V1_ERR_NO_MATCH ||
                got_base_len != 0u || got_ext_len != 0u ||
                energun_pair_v1_commit(&pair_test_ops, NULL, &workspace,
                    ids[i].object, ids[i].kind, ids[i].index,
                    p1, sizeof p1, logical, ids[i].payload_len, 0u) != ENERGUN_PAIR_V1_OK ||
                energun_pair_v1_load(&pair_test_ops, NULL, &workspace,
                    ids[i].object, ids[i].kind, ids[i].index,
                    got, sizeof got, &got_base_len, ext, sizeof ext, &got_ext_len,
                    0u, &meta) != ENERGUN_PAIR_V1_OK ||
                meta.sequence != 4u || got_base_len != sizeof p1 ||
                memcmp(got, p1, sizeof p1) != 0)
                matrix_ok = 0;
        }
        bad += check("coordinator: seven identities, A/B histories and exact pair roundtrip", matrix_ok);
    }

    /* A failed tombstone cannot displace the previous Base + Sidecar pair. */
    reset_media();
    bad += check("seed pair before erase fault", coordinator_commit_test(
        OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, NULL) == 0);
    fail_bytes = 0;
    bad += check("failed erase preserves previous pair",
        energun_pair_v1_erase(&pair_test_ops, NULL, OBJ_PROJECT0) == ENERGUN_PAIR_V1_ERR_BASE_COMMIT &&
        (fail_bytes = -1, coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            loaded_base, sizeof loaded_base, loaded_sc, sizeof loaded_sc, &seq, &copy) == 0) &&
        seq == 1u && !memcmp(loaded_base, base1, sizeof base1));
    bad += check("successful erase tombstone hides stale Sidecar",
        energun_pair_v1_erase(&pair_test_ops, NULL, OBJ_PROJECT0) == ENERGUN_PAIR_V1_OK &&
        energun_pair_v1_load(&pair_test_ops, NULL, &coordinator_workspace,
            OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            loaded_base, sizeof loaded_base, &(size_t){0u},
            loaded_sc, sizeof loaded_sc, &(size_t){0u}, 0u,
            &(energun_pair_v1_base_meta_t){0}) == ENERGUN_PAIR_V1_ERR_NO_MATCH);

    /* Every torn Sidecar payload/header byte must leave the original pair readable. */
    for (cut = 0; cut < ENERGUN_SC_V1_HEADER_SIZE + 117u + sizeof(st_hdr_t); ++cut) {
        reset_media();
        if (coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, NULL) != 0) { bad++; break; }
        sc_fail_after = (int)cut;
        (void)coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, NULL);
        sc_fail_after = -1;
        bad += coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                         sizeof loaded_sc, &seq, &copy) == 0 &&
               seq == 1u && !memcmp(loaded_base, base1, sizeof base1) ? 0 : 1;
    }
    bad += check("Sidecar torn-write sweep preserves old pair", bad == 0);

    /* Every torn Base payload/header byte after a successful future-bound Sidecar write leaves old pair. */
    for (cut = 0; cut < sizeof base2 + sizeof(st_hdr_t); ++cut) {
        reset_media();
        if (coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, NULL) != 0) { bad++; break; }
        fail_bytes = (int)cut;
        (void)coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, NULL);
        fail_bytes = -1;
        bad += coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                         sizeof loaded_sc, &seq, &copy) == 0 &&
               seq == 1u && !memcmp(loaded_base, base1, sizeof base1) ? 0 : 1;
    }
    bad += check("Base torn-write sweep preserves old pair", bad == 0);

    /* A newer orphan must not displace the old matching copy as the next write target. */
    reset_media();
    bad += check("seed matching pair", coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u, NULL) == 0);
    fail_bytes = 0; /* Sidecar B is committed, Base A erase/write then fails */
    bad += check("failed Base commit leaves old pair selected",
        coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, NULL) != 0 &&
        (fail_bytes = -1, coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                                    sizeof loaded_sc, &seq, &copy) == 0) &&
        seq == 1u && copy == 0 && !memcmp(loaded_base, base1, sizeof base1));
    bad += check("retry protects old matching copy despite newer orphan",
        coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, base2, sizeof base2, payload, 117u, &target) == 0 &&
        target == 1 &&
        coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
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
            coordinator_load_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u, loaded_base, sizeof loaded_base, loaded_sc,
                      sizeof loaded_sc, &seq, &copy) == 0 &&
            seq == 1u && copy == 0 && !memcmp(loaded_base, base1, sizeof base1));
    }

    /* Legacy compatibility: missing/invalid Sidecar restores Base and asks
       the runtime owner to apply canonical defaults. I/O errors do not fallback. */
    {
        size_t base_len = 0u, ext_len = 99u;
        energun_pair_v1_base_meta_t meta;
        uint8_t bad_record[117];
        int legacy_ok = 1;

        reset_media();
        legacy_ok &= coordinator_commit_test(OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            base1, sizeof base1, payload, 117u, NULL) == 0;
        legacy_ok &= energun_pair_v1_load_compat(&pair_test_ops, NULL,
            &coordinator_workspace, OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            loaded_base, sizeof loaded_base, &base_len,
            loaded_sc, sizeof loaded_sc, &ext_len, 0u, &meta) == ENERGUN_PAIR_V1_OK &&
            base_len == sizeof base1 && ext_len == 117u &&
            !memcmp(loaded_base, base1, sizeof base1);

        memset(sc_nor[0][0].bytes, 0xFF, sizeof sc_nor[0][0].bytes);
        memset(sc_nor[0][1].bytes, 0xFF, sizeof sc_nor[0][1].bytes);
        memset(loaded_sc, 0xA5, sizeof loaded_sc);
        base_len = 0u; ext_len = 99u;
        legacy_ok &= energun_pair_v1_load_compat(&pair_test_ops, NULL,
            &coordinator_workspace, OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            loaded_base, sizeof loaded_base, &base_len,
            loaded_sc, sizeof loaded_sc, &ext_len, 0u, &meta) ==
            ENERGUN_PAIR_V1_LEGACY_FALLBACK &&
            base_len == sizeof base1 && ext_len == 0u &&
            meta.sequence == 1u && meta.payload_length == sizeof base1 &&
            !memcmp(loaded_base, base1, sizeof base1) && loaded_sc[0] == 0xA5;

        memset(bad_record, 0, sizeof bad_record);
        legacy_ok &= sc_write_copy(ENERGUN_SC_ID_PROJECT, 0u, 0u,
            bad_record, sizeof bad_record, 7u) == 0;
        base_len = 0u; ext_len = 99u;
        legacy_ok &= energun_pair_v1_load_compat(&pair_test_ops, NULL,
            &coordinator_workspace, OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            loaded_base, sizeof loaded_base, &base_len,
            loaded_sc, sizeof loaded_sc, &ext_len, 0u, &meta) ==
            ENERGUN_PAIR_V1_LEGACY_FALLBACK &&
            base_len == sizeof base1 && ext_len == 0u &&
            meta.sequence == 1u && !memcmp(loaded_base, base1, sizeof base1);

        sc_read_error = 1;
        base_len = 0u; ext_len = 99u;
        legacy_ok &= energun_pair_v1_load_compat(&pair_test_ops, NULL,
            &coordinator_workspace, OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            loaded_base, sizeof loaded_base, &base_len,
            loaded_sc, sizeof loaded_sc, &ext_len, 0u, &meta) ==
            ENERGUN_PAIR_V1_ERR_SIDECAR_IO &&
            base_len == 0u && ext_len == 0u;
        sc_read_error = 0;

        reset_media();
        base_len = 0u; ext_len = 99u;
        legacy_ok &= energun_pair_v1_load_compat(&pair_test_ops, NULL,
            &coordinator_workspace, OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            loaded_base, sizeof loaded_base, &base_len,
            loaded_sc, sizeof loaded_sc, &ext_len, 0u, &meta) ==
            ENERGUN_PAIR_V1_ERR_NO_MATCH &&
            base_len == 0u && ext_len == 0u;
        bad += check("legacy Base fallback: missing/invalid Sidecar, I/O guard and no-Base", legacy_ok);
    }


    /* CP1.6 lifecycle adapter acceptance. */
    {
        static energun_pair_v1_workspace_t lifecycle_workspace;
        static uint8_t base_scratch[4096];
        static uint8_t sidecar_scratch[ENERGUN_PAIR_V1_RECORD_MAX];
        static uint8_t canonical_defaults[117];
        energun_lifecycle_v1_t lifecycle;
        energun_pair_v1_base_meta_t meta;
        uint8_t renamed[64], copied[64], got_base[128], got_sidecar[128];
        size_t got_base_len = 0u, got_sidecar_len = 0u;
        int lifecycle_ok = 1;
        memset(&lifecycle, 0, sizeof lifecycle);
        memset(canonical_defaults, 0, sizeof canonical_defaults);
        lifecycle.ops = &pair_test_ops;
        lifecycle.workspace = &lifecycle_workspace;
        lifecycle.base_scratch = base_scratch;
        lifecycle.base_scratch_capacity = sizeof base_scratch;
        lifecycle.sidecar_scratch = sidecar_scratch;
        lifecycle.sidecar_scratch_capacity = sizeof sidecar_scratch;
        lifecycle.canonical_sidecar_defaults = canonical_defaults;
        lifecycle.canonical_sidecar_defaults_length = sizeof canonical_defaults;
        lifecycle.write_guard = lifecycle_test_guard;
        lifecycle.base_motion_count = 0u;

        {
            energun_sc_project_payload_v1_t typed, typed_loaded;
            memset(&typed, 0, sizeof typed);
            memset(&typed_loaded, 0xA5, sizeof typed_loaded);
            typed.p_mc[0][0] = 1u;
            typed.private_values[3][5] = 2u;
            lifecycle_ok &= energun_lifecycle_v1_save_project(&lifecycle,
                OBJ_PROJECT0 + 3u, 3u, base1, sizeof base1, &typed) ==
                ENERGUN_LIFECYCLE_V1_OK;
            lifecycle_ok &= energun_lifecycle_v1_load_project(&lifecycle,
                OBJ_PROJECT0 + 3u, 3u, got_base, sizeof got_base, &got_base_len,
                &typed_loaded, &meta) == ENERGUN_LIFECYCLE_V1_OK &&
                typed_loaded.p_mc[0][0] == 1u &&
                typed_loaded.private_values[3][5] == 2u &&
                typed_loaded.motion_count == 0u &&
                got_base_len == sizeof base1 && !memcmp(got_base, base1, sizeof base1);
        }

        reset_media();
        lifecycle_ok &= energun_lifecycle_v1_save(&lifecycle, OBJ_PROJECT0,
            ENERGUN_SC_ID_PROJECT, 0u, base1, sizeof base1, payload, 117u) ==
            ENERGUN_LIFECYCLE_V1_OK;
        memcpy(renamed, base1, sizeof renamed);
        renamed[0] = 0x7Au;
        lifecycle_ok &= energun_lifecycle_v1_copy_stored(&lifecycle,
            OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            renamed, sizeof renamed) == ENERGUN_LIFECYCLE_V1_OK;
        lifecycle_ok &= energun_lifecycle_v1_load(&lifecycle, OBJ_PROJECT0,
            ENERGUN_SC_ID_PROJECT, 0u, got_base, sizeof got_base, &got_base_len,
            got_sidecar, sizeof got_sidecar, &got_sidecar_len, &meta) ==
            ENERGUN_LIFECYCLE_V1_OK && got_base_len == sizeof renamed &&
            got_sidecar_len == 117u && !memcmp(got_base, renamed, sizeof renamed) &&
            !memcmp(got_sidecar, payload, 117u) && meta.sequence == 2u;

        memcpy(copied, renamed, sizeof copied);
        copied[1] = 0x6Bu;
        lifecycle_ok &= energun_lifecycle_v1_copy_stored(&lifecycle,
            OBJ_PROJECT0, ENERGUN_SC_ID_PROJECT, 0u,
            OBJ_PROJECT0 + 1u, ENERGUN_SC_ID_PROJECT, 1u,
            copied, sizeof copied) == ENERGUN_LIFECYCLE_V1_OK;
        lifecycle_ok &= energun_lifecycle_v1_load(&lifecycle, OBJ_PROJECT0 + 1u,
            ENERGUN_SC_ID_PROJECT, 1u, got_base, sizeof got_base, &got_base_len,
            got_sidecar, sizeof got_sidecar, &got_sidecar_len, &meta) ==
            ENERGUN_LIFECYCLE_V1_OK && meta.sequence == 1u &&
            !memcmp(got_base, copied, sizeof copied) &&
            !memcmp(got_sidecar, payload, 117u);

        reset_media();
        lifecycle_ok &= st_save(OBJ_PROJECT0, base1, sizeof base1) == 0;
        lifecycle_ok &= energun_lifecycle_v1_load(&lifecycle, OBJ_PROJECT0,
            ENERGUN_SC_ID_PROJECT, 0u, got_base, sizeof got_base, &got_base_len,
            got_sidecar, sizeof got_sidecar, &got_sidecar_len, &meta) ==
            ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS && got_base_len == sizeof base1 &&
            got_sidecar_len == sizeof canonical_defaults &&
            !memcmp(got_sidecar, canonical_defaults, sizeof canonical_defaults);

        lifecycle_guard_value = 1;
        {
            energun_sc_project_payload_v1_t typed_live;
            memset(&typed_live, 0, sizeof typed_live);
            lifecycle_ok &= energun_lifecycle_v1_save_live(&lifecycle,
                OBJ_AUTOSAVE, base1, sizeof base1, &typed_live) ==
                ENERGUN_LIFECYCLE_V1_ERR_GUARD;
        }
        lifecycle_ok &= energun_lifecycle_v1_autosave(&lifecycle, OBJ_AUTOSAVE,
            base1, sizeof base1, payload, 117u) == ENERGUN_LIFECYCLE_V1_ERR_GUARD;
        /* FM6 reservation is scoped to the autosave copy; project saves remain usable. */
        lifecycle_ok &= energun_lifecycle_v1_save(&lifecycle, OBJ_PROJECT0 + 2u,
            ENERGUN_SC_ID_PROJECT, 2u, base1, sizeof base1, payload, 117u) ==
            ENERGUN_LIFECYCLE_V1_OK;
        lifecycle_guard_value = 0;
        lifecycle_ok &= energun_lifecycle_v1_autosave(&lifecycle, OBJ_AUTOSAVE,
            base1, sizeof base1, payload, 117u) == ENERGUN_LIFECYCLE_V1_OK;
        lifecycle_ok &= energun_lifecycle_v1_erase(&lifecycle, OBJ_PROJECT0) ==
            ENERGUN_LIFECYCLE_V1_OK;
        got_base_len = got_sidecar_len = 0u;
        lifecycle_ok &= energun_lifecycle_v1_load(&lifecycle, OBJ_PROJECT0,
            ENERGUN_SC_ID_PROJECT, 0u, got_base, sizeof got_base, &got_base_len,
            got_sidecar, sizeof got_sidecar, &got_sidecar_len, &meta) ==
            ENERGUN_LIFECYCLE_V1_ERR_PAIR && got_base_len == 0u;
        lifecycle_guard_value = 0;
        bad += check("CP1.6 lifecycle adapter: save/rename/Save As/legacy/autosave guard/erase", lifecycle_ok);
    }


    /* CP1.7 preset-bank lifecycle: bank pair, slot preservation, defaults and restore. */
    {
        static energun_pair_v1_workspace_t preset_workspace;
        static uint8_t preset_base_scratch[4096];
        static uint8_t preset_sidecar_scratch[ENERGUN_PAIR_V1_RECORD_MAX];
        static uint8_t preset_defaults[ENERGUN_SC_V1_PRESET_PAYLOAD];
        static uint8_t private_defaults[ENERGUN_PRIVATE_V1_KEYS];
        energun_lifecycle_v1_t preset_lifecycle;
        energun_private_cache_v1_t private_cache;
        energun_sc_preset_payload_v1_t bank_payload;
        energun_pair_v1_base_meta_t meta;
        uint8_t bank_base[64], changed_base[64], loaded_base[128];
        size_t loaded_base_length = 0u;
        uint8_t value = 0u;
        int preset_ok = 1;
        memset(&preset_lifecycle, 0, sizeof preset_lifecycle);
        memset(preset_defaults, 0, sizeof preset_defaults);
        memset(private_defaults, 0, sizeof private_defaults);
        preset_lifecycle.ops = &pair_test_ops;
        preset_lifecycle.workspace = &preset_workspace;
        preset_lifecycle.base_scratch = preset_base_scratch;
        preset_lifecycle.base_scratch_capacity = sizeof preset_base_scratch;
        preset_lifecycle.sidecar_scratch = preset_sidecar_scratch;
        preset_lifecycle.sidecar_scratch_capacity = sizeof preset_sidecar_scratch;
        preset_lifecycle.canonical_sidecar_defaults = preset_defaults;
        preset_lifecycle.canonical_sidecar_defaults_length = sizeof preset_defaults;
        preset_lifecycle.base_motion_count = 0u;
        preset_ok &= energun_private_v1_init(&private_cache, private_defaults) ==
            ENERGUN_PRIVATE_V1_OK;
        preset_ok &= energun_private_v1_set(&private_cache, 1u,
            ENERGUN_PRIVATE_WAVE, 2u) == ENERGUN_PRIVATE_V1_OK;
        preset_ok &= energun_private_v1_set(&private_cache, 1u,
            ENERGUN_PRIVATE_FM_CHARACTER, 2u) == ENERGUN_PRIVATE_V1_OK;
        preset_ok &= energun_private_v1_set(&private_cache, 2u,
            ENERGUN_PRIVATE_FOLLOW, 1u) == ENERGUN_PRIVATE_V1_OK;

        reset_media();
        memset(&bank_payload, 0, sizeof bank_payload);
        memset(bank_base, 0x33, sizeof bank_base);
        preset_ok &= energun_preset_lifecycle_v1_save_bank(&preset_lifecycle,
            OBJ_UPRESET0, 0u, bank_base, sizeof bank_base, &bank_payload) ==
            ENERGUN_LIFECYCLE_V1_OK;

        memcpy(changed_base, bank_base, sizeof changed_base);
        changed_base[0] = 0x44u;
        preset_ok &= energun_preset_lifecycle_v1_capture_slot(&preset_lifecycle,
            OBJ_UPRESET0, 0u, 1u, 5u, changed_base, sizeof changed_base,
            &private_cache) == ENERGUN_LIFECYCLE_V1_OK;
        memcpy(changed_base, bank_base, sizeof changed_base);
        changed_base[1] = 0x55u;
        preset_ok &= energun_preset_lifecycle_v1_capture_slot(&preset_lifecycle,
            OBJ_UPRESET0, 0u, 2u, 6u, changed_base, sizeof changed_base,
            &private_cache) == ENERGUN_LIFECYCLE_V1_OK;

        memset(&bank_payload, 0, sizeof bank_payload);
        preset_ok &= energun_preset_lifecycle_v1_load_bank(&preset_lifecycle,
            OBJ_UPRESET0, 0u, loaded_base, sizeof loaded_base,
            &loaded_base_length, &bank_payload, &meta) ==
            ENERGUN_LIFECYCLE_V1_OK && loaded_base_length == sizeof bank_base;
        preset_ok &= (bank_payload.slot_bitmap[0] & (1u << 5)) != 0u &&
            (bank_payload.slot_bitmap[0] & (1u << 6)) != 0u &&
            bank_payload.private_values[5][ENERGUN_PRIVATE_WAVE] == 2u &&
            bank_payload.private_values[5][ENERGUN_PRIVATE_FM_CHARACTER] == 2u &&
            bank_payload.private_values[6][ENERGUN_PRIVATE_FOLLOW] == 1u;

        preset_ok &= energun_private_v1_defaults(&private_cache, 3u,
            private_defaults) == ENERGUN_PRIVATE_V1_OK;
        preset_ok &= energun_preset_lifecycle_v1_apply_slot(&bank_payload,
            &private_cache, 3u, 5u, private_defaults) == ENERGUN_LIFECYCLE_V1_OK;
        preset_ok &= energun_private_v1_get(&private_cache, 3u,
            ENERGUN_PRIVATE_WAVE, &value) == ENERGUN_PRIVATE_V1_OK && value == 2u;

        memcpy(changed_base, bank_base, sizeof changed_base);
        changed_base[2] = 0x66u;
        preset_ok &= energun_preset_lifecycle_v1_erase_slot(&preset_lifecycle,
            OBJ_UPRESET0, 0u, 5u, changed_base, sizeof changed_base) ==
            ENERGUN_LIFECYCLE_V1_OK;
        memset(&bank_payload, 0, sizeof bank_payload);
        preset_ok &= energun_preset_lifecycle_v1_load_bank(&preset_lifecycle,
            OBJ_UPRESET0, 0u, loaded_base, sizeof loaded_base,
            &loaded_base_length, &bank_payload, &meta) ==
            ENERGUN_LIFECYCLE_V1_OK &&
            (bank_payload.slot_bitmap[0] & (1u << 5)) == 0u &&
            (bank_payload.slot_bitmap[0] & (1u << 6)) != 0u &&
            bank_payload.private_values[5][ENERGUN_PRIVATE_WAVE] == 0u &&
            bank_payload.private_values[6][ENERGUN_PRIVATE_FOLLOW] == 1u;

        preset_ok &= energun_preset_lifecycle_v1_apply_slot(&bank_payload,
            &private_cache, 3u, 5u, private_defaults) == ENERGUN_LIFECYCLE_V1_OK;
        preset_ok &= energun_private_v1_get(&private_cache, 3u,
            ENERGUN_PRIVATE_WAVE, &value) == ENERGUN_PRIVATE_V1_OK && value == 0u;

        memcpy(changed_base, bank_base, sizeof changed_base);
        changed_base[3] = 0x77u;
        preset_ok &= energun_lifecycle_v1_copy_stored(&preset_lifecycle,
            OBJ_UPRESET0, ENERGUN_SC_ID_PRESET_BANK, 0u,
            OBJ_UPRESET0 + 1u, ENERGUN_SC_ID_PRESET_BANK, 1u,
            changed_base, sizeof changed_base) == ENERGUN_LIFECYCLE_V1_OK;
        memset(&bank_payload, 0, sizeof bank_payload);
        preset_ok &= energun_preset_lifecycle_v1_load_bank(&preset_lifecycle,
            OBJ_UPRESET0 + 1u, 1u, loaded_base, sizeof loaded_base,
            &loaded_base_length, &bank_payload, &meta) ==
            ENERGUN_LIFECYCLE_V1_OK &&
            (bank_payload.slot_bitmap[0] & (1u << 6)) != 0u &&
            bank_payload.private_values[6][ENERGUN_PRIVATE_FOLLOW] == 1u;

        reset_media();
        preset_ok &= st_save(OBJ_UPRESET0 + 1u, bank_base, sizeof bank_base) == 0;
        memset(&bank_payload, 0xA5, sizeof bank_payload);
        preset_ok &= energun_preset_lifecycle_v1_load_bank(&preset_lifecycle,
            OBJ_UPRESET0 + 1u, 1u, loaded_base, sizeof loaded_base,
            &loaded_base_length, &bank_payload, &meta) ==
            ENERGUN_LIFECYCLE_V1_LEGACY_DEFAULTS &&
            loaded_base_length == sizeof bank_base &&
            bank_payload.slot_bitmap[0] == 0u && bank_payload.slot_bitmap[1] == 0u;
        bad += check("CP1.7 preset lifecycle: bank pair, slot isolation, erase, rename, legacy defaults and restore", preset_ok);
    }

    printf("%s\n", bad ? "SIDECAR PAIR TEST FAILED" : "SIDECAR PAIR TEST PASSED");
    return bad ? 1 : 0;
}
