/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Sidecar A/B selection host acceptance.
 * Real storage.c + Sidecar V1 code against simulated NOR; no device/sector map.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define __attribute__(x)
static uint8_t nor[0x100000u];

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off > sizeof nor || n > sizeof nor - off) return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (off > sizeof nor || 4096u > sizeof nor - off) return -1;
    memset(nor + off, 0xFF, 4096u);
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *in = (const uint8_t *)src;
    uint32_t i;
    if (off > sizeof nor || n > sizeof nor - off || (off & 0xFFu) + n > 256u)
        return -1;
    for (i = 0; i < n; ++i) {
        if ((nor[off + i] & in[i]) != in[i]) return -1;
        nor[off + i] &= in[i];
    }
    return 0;
}

#include "../firmware/src/storage.c"
#include "../firmware/src/energun_sidecar_v1.c"

static unsigned int checks, failures;
static uint8_t payload[ENERGUN_SC_V1_PROJECT_VALUES + 1u];

static void check(const char *name, int ok)
{
    ++checks;
    printf("CP1.4.5 Sidecar A/B: %-68s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

static size_t make_record(uint8_t *out, uint32_t base_seq, uint32_t base_crc)
{
    size_t length = 0u;
    if (energun_sc_v1_encode(out, ENERGUN_SC_V1_MAX_RECORD, &length,
            ENERGUN_SC_ID_PROJECT, 0u, base_seq, base_crc,
            payload, sizeof payload, 0u) != ENERGUN_SC_OK)
        return 0u;
    return length;
}

static void candidate(energun_sc_candidate_t *c, const uint8_t *record,
                      size_t length, uint32_t seq, int storage_valid)
{
    c->record = record;
    c->record_length = length;
    c->storage_sequence = seq;
    c->storage_valid = (uint8_t)(storage_valid != 0);
}

int main(void)
{
    uint8_t base[32];
    uint8_t rec_a[ENERGUN_SC_V1_MAX_RECORD], rec_b[ENERGUN_SC_V1_MAX_RECORD];
    uint8_t read_a[ENERGUN_SC_V1_MAX_RECORD], read_b[ENERGUN_SC_V1_MAX_RECORD];
    size_t len_a, len_b;
    st_meta_t base_meta, side_meta_a, side_meta_b;
    energun_sc_candidate_t c[2];
    energun_sc_ab_selection_t sel;
    uint32_t current_crc = 0x12345678u;
    int rc;

    memset(nor, 0xFF, sizeof nor);
    memset(payload, 0, sizeof payload);
    memset(base, 0xA5, sizeof base);

    check("real storage saves and reports selected Base metadata",
          st_save(OBJ_PROJECT0, base, sizeof base) == 0 &&
          st_current_meta(OBJ_PROJECT0, &base_meta) == 0 &&
          base_meta.copy == 0u && base_meta.sequence == 1u &&
          base_meta.payload_crc == st_crc32(base, sizeof base));

    /* Test fixture only: OBJ_SETTINGS is reused as an isolated simulated
     * A/B container. It is NOT an assigned production Sidecar object ID.
     */
    len_a = make_record(rec_a, base_meta.sequence, base_meta.payload_crc);
    len_b = make_record(rec_b, base_meta.sequence + 1u, current_crc);
    check("matching and orphan fixture records encode",
          len_a != 0u && len_b != 0u);
    check("simulated storage commits candidate A then candidate B",
          st_save_to(OBJ_SETTINGS, rec_a, (uint32_t)len_a, 0) == 0 &&
          st_save_to(OBJ_SETTINGS, rec_b, (uint32_t)len_b, 1) == 0);
    check("explicit A/B reader returns independently CRC-validated copies",
          st_read_copy_meta(OBJ_SETTINGS, 0u, read_a, sizeof read_a, &side_meta_a) == 0 &&
          st_read_copy_meta(OBJ_SETTINGS, 1u, read_b, sizeof read_b, &side_meta_b) == 0 &&
          side_meta_a.sequence == 1u && side_meta_b.sequence == 2u &&
          side_meta_a.payload_length == len_a && side_meta_b.payload_length == len_b);

    candidate(&c[0], read_a, side_meta_a.payload_length, side_meta_a.sequence, 1);
    candidate(&c[1], read_b, side_meta_b.payload_length, side_meta_b.sequence, 1);
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("older matching pair is protected from newer orphan",
          rc == ENERGUN_SC_OK && sel.matching_copy == 0 &&
          sel.newest_valid_copy == 1 && sel.write_copy == 1 &&
          sel.next_storage_sequence == 3u);

    /* The current Base has advanced: neither Sidecar matches. The newest
     * valid record is preserved while the older copy becomes the write target.
     */
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence + 9u, 0xCAFEBABEu, 0u, &sel);
    check("when neither candidate matches, newest valid orphan is preserved",
          rc == ENERGUN_SC_OK && sel.matching_copy == -1 &&
          sel.newest_valid_copy == 1 && sel.write_copy == 0 &&
          sel.next_storage_sequence == 3u);

    /* Matching candidate B must remain protected even if A has a newer local
     * sequence but belongs to a different Base generation.
     */
    candidate(&c[0], read_b, side_meta_b.payload_length, 50u, 1);
    candidate(&c[1], read_a, side_meta_a.payload_length, 40u, 1);
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("matching B is protected even when orphan A has newer local sequence",
          rc == ENERGUN_SC_OK && sel.matching_copy == 1 &&
          sel.newest_valid_copy == 0 && sel.write_copy == 0 &&
          sel.next_storage_sequence == 51u);

    /* Both matching: protect the newest matching record. */
    candidate(&c[0], read_a, side_meta_a.payload_length, 7u, 1);
    candidate(&c[1], read_a, side_meta_a.payload_length, 8u, 1);
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("when both copies match, newest matching copy is protected",
          rc == ENERGUN_SC_OK && sel.matching_copy == 1 &&
          sel.write_copy == 0 && sel.next_storage_sequence == 9u);

    /* Sequence wrap: 1 is newer than 0xFFFFFFFE under half-range ordering. */
    candidate(&c[0], read_a, side_meta_a.payload_length, 0xFFFFFFFEu, 1);
    candidate(&c[1], read_a, side_meta_a.payload_length, 1u, 1);
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("storage sequence wrap selects 1 as newer than FFFFFFFE",
          rc == ENERGUN_SC_OK && sel.matching_copy == 1 &&
          sel.newest_valid_copy == 1 && sel.write_copy == 0 &&
          sel.next_storage_sequence == 2u);

    candidate(&c[0], read_a, side_meta_a.payload_length, 12u, 1);
    candidate(&c[1], read_a, side_meta_a.payload_length, 12u, 1);
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("equal storage sequence deterministically chooses A",
          rc == ENERGUN_SC_OK && sel.matching_copy == 0 &&
          sel.newest_valid_copy == 0 && sel.write_copy == 1);

    candidate(&c[0], read_a, side_meta_a.payload_length, 12u, 0);
    candidate(&c[1], read_b, side_meta_b.payload_length, 13u, 1);
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("outer storage-invalid candidate is ignored",
          rc == ENERGUN_SC_OK && sel.matching_copy == -1 &&
          sel.newest_valid_copy == 1 && sel.write_copy == 0);

    candidate(&c[0], read_a, side_meta_a.payload_length, 12u, 1);
    candidate(&c[1], read_b, side_meta_b.payload_length, 13u, 1);
    read_b[0] ^= 0x01u;
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("malformed Sidecar envelope is ignored without hiding valid A",
          rc == ENERGUN_SC_OK && sel.matching_copy == 0 &&
          sel.newest_valid_copy == 0 && sel.write_copy == 1);
    read_b[0] ^= 0x01u;

    candidate(&c[0], read_a, side_meta_a.payload_length, 1u, 0);
    candidate(&c[1], read_b, side_meta_b.payload_length, 2u, 0);
    rc = energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
            base_meta.sequence, base_meta.payload_crc, 0u, &sel);
    check("no valid candidates initializes A and sequence one",
          rc == ENERGUN_SC_OK && sel.matching_copy == -1 &&
          sel.newest_valid_copy == -1 && sel.write_copy == 0 &&
          sel.next_storage_sequence == 1u);

    check("null candidate array is rejected",
          energun_sc_v1_select_ab(NULL, ENERGUN_SC_ID_PROJECT, 0u,
              base_meta.sequence, base_meta.payload_crc, 0u, &sel) ==
              ENERGUN_SC_ERR_ARGUMENT);
    check("null selection output is rejected",
          energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
              base_meta.sequence, base_meta.payload_crc, 0u, NULL) ==
              ENERGUN_SC_ERR_ARGUMENT);

    /* Damage the actual simulated NOR copy: explicit storage reader rejects
     * it by the outer storage CRC before Sidecar selection sees its bytes.
     */
    nor[st_sector(OBJ_SETTINGS, 1u) + ST_PAYLOAD_OFF + 2u] ^= 0x01u;
    check("real storage A/B reader rejects a corrupted outer payload CRC",
          st_read_copy_meta(OBJ_SETTINGS, 1u, read_b, sizeof read_b, &side_meta_b) != 0);

    printf("CP1.4.5 Sidecar A/B: %u checks, %u failures\n", checks, failures);
    return failures != 0u;
}
