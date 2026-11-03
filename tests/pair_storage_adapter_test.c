/* SPDX-License-Identifier: GPL-3.0-only
 * Host integration tests for the production storage adapter on simulated NOR.
 * All Sidecar addresses below are test-only RAM offsets, never firmware map data.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/energun_pair_storage_adapter.h"

#define NOR_SIZE 0x100000u
#define SC_A_TEST 0x1000u
#define SC_B_TEST 0x2000u
#define SC_MAGIC_TEST 0x31534345u
#define SC_HEADER_SIZE 32u
#define SC_PAYLOAD_OFF 256u

static uint8_t nor[NOR_SIZE];
static int base_erase_failure;
static int base_fail_after = -1;
static int sidecar_fault_stage;
static int sidecar_fail_after = -1;
static unsigned int checks, failures;

enum { SC_FAULT_NONE, SC_FAULT_ERASE, SC_FAULT_PAYLOAD,
       SC_FAULT_HEADER, SC_FAULT_READBACK };

typedef struct {
    uint32_t magic, sequence, length, crc, reserved[3], header_crc;
} test_sc_header_t;
_Static_assert(sizeof(test_sc_header_t) == SC_HEADER_SIZE,
               "simulated Sidecar header size");

#define CHECK(label, expression) do { \
    ++checks; if (!(expression)) { ++failures; printf("FAIL: %s\\n", label); } \
    else printf("PASS: %s\\n", label); \
} while (0)

static int sim_bounds(uint32_t off, uint32_t n)
{
    return off <= NOR_SIZE && n <= NOR_SIZE - off;
}
static int sim_erase(uint32_t off)
{
    if ((off & 0xFFFu) != 0u || !sim_bounds(off, 4096u))
        return -1;
    memset(nor + off, 0xFF, 4096u);
    return 0;
}
static int sim_program(uint32_t off, const void *source, uint32_t length,
                       int *fail_after)
{
    const uint8_t *src = (const uint8_t *)source;
    uint32_t i;
    if (!sim_bounds(off, length) || (off & 0xFFu) + length > 256u)
        return -1;
    for (i = 0; i < length; ++i) {
        if (fail_after != 0 && *fail_after == 0)
            return -9;
        if (fail_after != 0 && *fail_after > 0)
            --*fail_after;
        if ((uint8_t)(nor[off + i] | src[i]) != nor[off + i])
            return -2; /* NOR cannot change a programmed zero back to one. */
        nor[off + i] &= src[i];
    }
    return 0;
}

/* Existing Felucca storage hooks, now backed by a bounded NOR model. */
static int st_read(uint32_t off, void *destination, uint32_t length)
{
    if (!sim_bounds(off, length))
        return -1;
    memcpy(destination, nor + off, length);
    return 0;
}
static int st_erase(uint32_t off)
{
    if (base_erase_failure)
        return -8;
    return sim_erase(off);
}
static int st_prog(uint32_t off, const void *source, uint32_t length)
{
    return sim_program(off, source, length, &base_fail_after);
}
#include "../firmware/src/storage.c"
#include "../firmware/src/energun_sidecar_v1.c"
#include "../firmware/src/energun_pair_txn.c"
#include "../firmware/src/energun_pair_storage_adapter.c"

static uint32_t sc_test_sector(uint8_t copy)
{
    return copy == 0u ? SC_A_TEST : SC_B_TEST;
}
static int sc_test_read(void *opaque, energun_sc_identity_kind_t kind,
    uint8_t index, uint8_t copy, uint8_t *record, size_t capacity,
    size_t *record_length, uint32_t *sequence)
{
    test_sc_header_t h;
    uint32_t off = sc_test_sector(copy);
    (void)opaque; (void)kind; (void)index;
    if (copy > 1u || record == 0 || record_length == 0 || sequence == 0 ||
        st_read(off, &h, sizeof h) != 0 ||
        h.magic != SC_MAGIC_TEST || h.length > capacity ||
        h.length > ENERGUN_SC_V1_MAX_RECORD ||
        h.header_crc != st_crc32(&h, sizeof h - sizeof h.header_crc) ||
        st_read(off + SC_PAYLOAD_OFF, record, h.length) != 0 ||
        st_crc32(record, h.length) != h.crc)
        return -1;
    *record_length = h.length;
    *sequence = h.sequence;
    return 0;
}
static int sc_test_write(void *opaque, energun_sc_identity_kind_t kind,
    uint8_t index, uint8_t copy, const uint8_t *record, size_t length)
{
    test_sc_header_t h;
    uint32_t off = sc_test_sector(copy), pos = 0u;
    uint8_t verify[ENERGUN_SC_V1_MAX_RECORD];
    size_t verify_length = 0u;
    uint32_t sequence = 0u;
    (void)opaque; (void)kind; (void)index;
    if (copy > 1u || record == 0 || length > sizeof verify ||
        sidecar_fault_stage == SC_FAULT_ERASE || sim_erase(off) != 0)
        return -1;
    if (sidecar_fault_stage == SC_FAULT_PAYLOAD)
        return -1;
    while (pos < length) {
        uint32_t n = length - pos > 256u ? 256u : (uint32_t)(length - pos);
        if (sim_program(off + SC_PAYLOAD_OFF + pos, record + pos, n,
                        &sidecar_fail_after) != 0)
            return -1;
        pos += n;
    }
    if (sidecar_fault_stage == SC_FAULT_HEADER)
        return -1;
    memset(&h, 0xFF, sizeof h);
    h.magic = SC_MAGIC_TEST;
    h.sequence = 1u;
    h.length = (uint32_t)length;
    h.crc = st_crc32(record, (uint32_t)length);
    h.header_crc = st_crc32(&h, sizeof h - sizeof h.header_crc);
    if (sim_program(off, &h, sizeof h, &sidecar_fail_after) != 0)
        return -1;
    if (sidecar_fault_stage == SC_FAULT_READBACK)
        return -1;
    return sc_test_read(0, kind, index, copy, verify, sizeof verify,
                        &verify_length, &sequence) == 0 &&
           verify_length == length && memcmp(verify, record, length) == 0 ? 0 : -1;
}
static const energun_sidecar_storage_ops_t sc_test_ops = {
    sc_test_read, sc_test_write
};

static int recover_equals(energun_pair_storage_adapter_t *adapter,
    const uint8_t *expected, uint32_t length, uint32_t sequence,
    uint8_t sidecar_marker)
{
    uint8_t loaded[64], sidecar[117];
    uint16_t sidecar_length = 0u;
    energun_pair_base_meta_t meta;
    int n = energun_pair_storage_adapter_load(adapter, loaded,
        sizeof loaded, sidecar, sizeof sidecar, &sidecar_length,
        ENERGUN_SC_ID_PROJECT, 0u, 0u, &meta);
    if (!(n == (int)length && meta.sequence == sequence &&
          sidecar_length == sizeof sidecar &&
          sidecar[0] == (uint8_t)(sidecar_marker % 3u) &&
          memcmp(loaded, expected, length) == 0))
        printf("DEBUG recover n=%d expected=%u seq=%u expected_seq=%u sc_len=%u marker=%u\\n",
            n, (unsigned int)length, (unsigned int)meta.sequence,
            (unsigned int)sequence, (unsigned int)sidecar_length,
            (unsigned int)sidecar[0]);
    return n == (int)length && meta.sequence == sequence &&
           sidecar_length == sizeof sidecar &&
           sidecar[0] == (uint8_t)(sidecar_marker % 3u) &&
           memcmp(loaded, expected, length) == 0;
}
static void make_sidecar_payload(uint8_t payload[117], uint8_t marker)
{
    memset(payload, 0, 117u);
    payload[0] = (uint8_t)(marker % 3u); /* WAVE enum */
}
static int save_value(energun_pair_storage_adapter_t *adapter,
    energun_pair_txn_workspace_t *workspace, uint8_t marker)
{
    uint8_t base[16], sidecar[117];
    unsigned int i;
    for (i = 0; i < sizeof base; ++i)
        base[i] = (uint8_t)(marker + i);
    energun_pair_status_t status;
    make_sidecar_payload(sidecar, marker);
    status = energun_pair_storage_adapter_save(adapter, workspace, base,
        sizeof base, ENERGUN_SC_ID_PROJECT, 0u, sidecar,
        sizeof sidecar, 0u);
    if (status != ENERGUN_PAIR_OK)
        printf("DEBUG save marker=%u status=%d protected_base=%d protected_sidecar=%d target_sidecar=%u\\n",
            (unsigned int)marker, (int)status,
            (int)adapter->protected_base_copy,
            (int)adapter->protected_sidecar_copy,
            (unsigned int)adapter->selected_sidecar_copy);
    return status;
}
static int setup(energun_pair_storage_adapter_t *adapter)
{
    memset(nor, 0xFF, sizeof nor);
    base_erase_failure = 0;
    base_fail_after = -1;
    sidecar_fault_stage = SC_FAULT_NONE;
    sidecar_fail_after = -1;
    return energun_pair_storage_adapter_init(adapter, &sc_test_ops, 0,
                                               OBJ_PROJECT0);
}

int main(void)
{
    energun_pair_storage_adapter_t adapter;
    energun_pair_txn_workspace_t workspace;
    uint8_t old_base[16], orphan[16], probe[1] = {0u};
    unsigned int i;
    int ok;

    CHECK("adapter initializes against existing Base object",
          setup(&adapter) == 0);
    CHECK("legacy Base load/save entrypoints remain linked",
          st_save(OBJ_COUNT, probe, sizeof probe) != 0 &&
          st_load(OBJ_COUNT, probe, sizeof probe) < 0);
    CHECK("first save commits Base A + Sidecar A",
          save_value(&adapter, &workspace, 10u) == ENERGUN_PAIR_OK &&
          recover_equals(&adapter, (uint8_t[16]){10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25}, 16u, 1u, 10u));

    CHECK("second save commits matching B copies",
          save_value(&adapter, &workspace, 20u) == ENERGUN_PAIR_OK &&
          recover_equals(&adapter, (uint8_t[16]){20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35}, 16u, 2u, 20u));

    for (i = 0; i < sizeof old_base; ++i) {
        old_base[i] = (uint8_t)(20u + i);
        orphan[i] = (uint8_t)(90u + i);
    }
    /* Create a newer Base-only generation in A. B + Sidecar B remains the
     * last complete pair and must be protected by the adapter's planner.
     */
    CHECK("fixture creates newer orphan Base generation",
          st_save_to(OBJ_PROJECT0, orphan, sizeof orphan, 0) == 0);
    CHECK("recovery ignores orphan Base and returns previous complete pair",
          recover_equals(&adapter, old_base, sizeof old_base, 2u, 20u));
    CHECK("planner overwrites orphan A, not protected matching Base B",
          save_value(&adapter, &workspace, 30u) == ENERGUN_PAIR_OK &&
          recover_equals(&adapter, (uint8_t[16]){30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45}, 16u, 4u, 30u));

    sidecar_fault_stage = SC_FAULT_ERASE;
    CHECK("Sidecar erase failure preserves previous complete pair",
          save_value(&adapter, &workspace, 40u) == ENERGUN_PAIR_ERR_SIDECAR_COMMIT &&
          recover_equals(&adapter, (uint8_t[16]){30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45}, 16u, 4u, 30u));
    sidecar_fault_stage = SC_FAULT_PAYLOAD;
    CHECK("Sidecar payload failure preserves previous complete pair",
          save_value(&adapter, &workspace, 41u) == ENERGUN_PAIR_ERR_SIDECAR_COMMIT &&
          recover_equals(&adapter, (uint8_t[16]){30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45}, 16u, 3u, 30u));
    sidecar_fault_stage = SC_FAULT_HEADER;
    CHECK("Sidecar header failure preserves previous complete pair",
          save_value(&adapter, &workspace, 42u) == ENERGUN_PAIR_ERR_SIDECAR_COMMIT &&
          recover_equals(&adapter, (uint8_t[16]){30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45}, 16u, 3u, 30u));
    sidecar_fault_stage = SC_FAULT_READBACK;
    CHECK("Sidecar readback failure preserves previous complete pair",
          save_value(&adapter, &workspace, 43u) == ENERGUN_PAIR_ERR_SIDECAR_COMMIT &&
          recover_equals(&adapter, (uint8_t[16]){30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45}, 16u, 3u, 30u));
    sidecar_fault_stage = SC_FAULT_NONE;

    base_erase_failure = 1;
    CHECK("Base erase failure after Sidecar staging preserves old pair",
          save_value(&adapter, &workspace, 50u) == ENERGUN_PAIR_ERR_BASE_COMMIT &&
          recover_equals(&adapter, (uint8_t[16]){30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45}, 16u, 3u, 30u));
    base_erase_failure = 0;

    /* Sidecar is now a committed orphan in B. Base A + Sidecar A remains the
     * current complete pair. Inject Base payload and header cuts separately.
     */
    ok = 1;
    {
        static const int cuts[] = {0, 8, 16, 20, 31, 47};
        unsigned int k;
        for (k = 0; k < sizeof cuts / sizeof cuts[0]; ++k) {
            base_fail_after = cuts[k];
            if (save_value(&adapter, &workspace, (uint8_t)(80u + k)) !=
                    ENERGUN_PAIR_ERR_BASE_COMMIT ||
                !recover_equals(&adapter,
                    (uint8_t[16]){30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45},
                    16u, 3u, 30u)) {
                ok = 0;
                break;
            }
            base_fail_after = -1;
        }
    }
    base_fail_after = -1;
    CHECK("Base payload/header byte cuts preserve previous complete pair", ok);

    /* Verify NOR page boundaries and 1->0 programming are enforced. */
    {
        uint8_t page[257];
        memset(page, 0x00, sizeof page);
        CHECK("simulated NOR rejects a program crossing a 256-byte page",
              sim_program(0x4000u, page, sizeof page, 0) != 0);
        CHECK("simulated NOR rejects zero-to-one programming",
              sim_program(0x5000u, page, 1u, 0) == 0 &&
              sim_program(0x5000u, (uint8_t[]){0xFFu}, 1u, 0) != 0);
        CHECK("simulated NOR rejects out-of-range accesses",
              !sim_bounds(NOR_SIZE - 4u, 8u));
    }

    /* Byte-level power cuts during Sidecar payload/header programming. Each
     * trial starts from a valid old pair and must recover it unchanged.
     */
    ok = 1;
    for (i = 0; i < 169u; i += 13u) {
        setup(&adapter);
        if (save_value(&adapter, &workspace, 60u) != ENERGUN_PAIR_OK) {
            ok = 0; break;
        }
        sidecar_fail_after = (int)i;
        (void)save_value(&adapter, &workspace, 70u);
        sidecar_fail_after = -1;
        if (!recover_equals(&adapter,
                (uint8_t[16]){60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75},
                16u, 1u, 60u)) {
            ok = 0; break;
        }
    }
    CHECK("byte-cut sweep across Sidecar payload/header preserves old pair", ok);

    printf("PAIR STORAGE ADAPTER: %u checks, %u failures\\n", checks, failures);
    return failures != 0u;
}
