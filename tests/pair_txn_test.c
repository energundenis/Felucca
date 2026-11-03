/* SPDX-License-Identifier: GPL-3.0-only
 * Host fault-injection tests for the ENERGUN pair transaction coordinator.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/energun_pair_txn.h"
#define BASE_CAP 32u
#define RECORD_CAP ENERGUN_SC_V1_MAX_RECORD

typedef enum { FAULT_NONE, FAULT_SIDECAR_BEFORE_COMMIT, FAULT_BASE_ERASE,
    FAULT_BASE_PAYLOAD, FAULT_BASE_HEADER, FAULT_VERIFY } fault_t;
typedef struct {
    uint8_t base[2][BASE_CAP];
    uint32_t base_len[2], base_seq[2], base_crc[2];
    uint8_t base_valid[2];
    uint8_t sidecar[2][RECORD_CAP];
    size_t sidecar_len[2];
    uint8_t sidecar_valid[2], last_sidecar_copy;
    fault_t fault;
    unsigned int sidecar_writes, base_writes;
} model_t;

static uint32_t crc32_bytes(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu; size_t i; unsigned int b;
    for (i = 0; i < n; ++i) {
        c ^= p[i];
        for (b = 0; b < 8u; ++b)
            c = (c >> 1) ^ (0xEDB88320u & (uint32_t)-(int32_t)(c & 1u));
    }
    return ~c;
}
static int newer(uint32_t a, uint32_t b)
{ return a != b && (uint32_t)(a - b) < 0x80000000u; }
static int current_base(model_t *m)
{
    if (m->base_valid[0] && m->base_valid[1])
        return newer(m->base_seq[1], m->base_seq[0]) ? 1 : 0;
    if (m->base_valid[0]) return 0;
    if (m->base_valid[1]) return 1;
    return -1;
}
static int plan_base(void *ctx, uint32_t obj, const uint8_t *payload,
    uint32_t length, energun_pair_base_meta_t *target)
{
    model_t *m = ctx; int cur = current_base(m); (void)obj;
    if (!payload || !target || length > BASE_CAP) return -1;
    target->copy = (uint8_t)(cur == 0 ? 1 : 0);
    target->sequence = cur < 0 ? 0u : m->base_seq[cur] + 1u;
    target->payload_length = length;
    target->payload_crc32 = crc32_bytes(payload, length);
    return 0;
}
static int select_sidecar_target(void *ctx, energun_sc_identity_kind_t kind,
    uint8_t index, const energun_pair_base_meta_t *target_base, uint8_t *target)
{
    model_t *m = ctx; int cur = current_base(m), matching = -1;
    unsigned int i; energun_sc_view_t view; (void)target_base;
    if (!target) return -1;
    if (cur < 0) { *target = 0u; return 0; }
    for (i = 0; i < 2u; ++i)
        if (m->sidecar_valid[i] &&
            energun_sc_v1_validate(m->sidecar[i], m->sidecar_len[i], kind,
                index, m->base_seq[cur], m->base_crc[cur], 0u, &view) ==
                    ENERGUN_SC_OK) matching = (int)i;
    *target = (uint8_t)(matching == 0 ? 1 : 0);
    return 0;
}
static int write_sidecar(void *ctx, energun_sc_identity_kind_t kind,
    uint8_t index, uint8_t copy, const uint8_t *record, size_t length)
{
    model_t *m = ctx; (void)kind; (void)index;
    ++m->sidecar_writes; m->last_sidecar_copy = copy;
    if (m->fault == FAULT_SIDECAR_BEFORE_COMMIT || copy > 1u ||
        length > RECORD_CAP) return -1;
    memcpy(m->sidecar[copy], record, length);
    m->sidecar_len[copy] = length;
    m->sidecar_valid[copy] = 1u; /* outer header-last commit */
    return 0;
}
static int write_base(void *ctx, uint32_t obj, const uint8_t *payload,
    uint32_t length, const energun_pair_base_meta_t *target)
{
    model_t *m = ctx; ++m->base_writes; (void)obj;
    if (!target || target->copy > 1u || length > BASE_CAP ||
        m->fault == FAULT_BASE_ERASE || m->fault == FAULT_BASE_PAYLOAD ||
        m->fault == FAULT_BASE_HEADER) return -1;
    memcpy(m->base[target->copy], payload, length);
    m->base_len[target->copy] = length;
    m->base_seq[target->copy] = target->sequence;
    m->base_crc[target->copy] = target->payload_crc32;
    m->base_valid[target->copy] = 1u; /* commit header written last */
    return 0;
}
static int verify_pair(void *ctx, uint32_t obj, energun_sc_identity_kind_t kind,
    uint8_t index, const energun_pair_base_meta_t *expected,
    const uint8_t *record, size_t record_length, uint16_t motion_count)
{
    model_t *m = ctx; energun_sc_view_t view;
    unsigned int copy = m->last_sidecar_copy; (void)obj;
    if (m->fault == FAULT_VERIFY || !expected || copy > 1u) return -1;
    if (!m->base_valid[expected->copy] ||
        m->base_seq[expected->copy] != expected->sequence ||
        m->base_crc[expected->copy] != expected->payload_crc32 ||
        !m->sidecar_valid[copy] || m->sidecar_len[copy] != record_length ||
        memcmp(m->sidecar[copy], record, record_length) != 0) return -1;
    return energun_sc_v1_validate(m->sidecar[copy], m->sidecar_len[copy],
        kind, index, expected->sequence, expected->payload_crc32,
        motion_count, &view) == ENERGUN_SC_OK ? 0 : -1;
}
static const energun_pair_txn_ops_t ops = {
    plan_base, select_sidecar_target, write_sidecar, write_base, verify_pair
};
static unsigned int checks, failures;
#define CHECK(label, expression) do { \
    ++checks; if (!(expression)) { ++failures; printf("FAIL: %s\\n", label); } \
    else printf("PASS: %s\\n", label); \
} while (0)
static void seed_old_pair(model_t *m)
{
    uint8_t old_base[8] = {1,2,3,4,5,6,7,8}, payload[117] = {0};
    size_t n = 0u;
    (void)energun_sc_v1_encode(m->sidecar[0], sizeof m->sidecar[0], &n,
        ENERGUN_SC_ID_PROJECT, 0u, 5u, crc32_bytes(old_base, sizeof old_base),
        payload, sizeof payload, 0u);
    memcpy(m->base[0], old_base, sizeof old_base);
    m->base_len[0] = sizeof old_base; m->base_seq[0] = 5u;
    m->base_crc[0] = crc32_bytes(old_base, sizeof old_base);
    m->base_valid[0] = 1u; m->sidecar_len[0] = n; m->sidecar_valid[0] = 1u;
}
static int old_pair_intact(const model_t *m)
{
    energun_sc_view_t view;
    return m->base_valid[0] && m->base_seq[0] == 5u &&
        energun_sc_v1_validate(m->sidecar[0], m->sidecar_len[0],
            ENERGUN_SC_ID_PROJECT, 0u, 5u, m->base_crc[0], 0u,
            &view) == ENERGUN_SC_OK;
}
static energun_pair_status_t save_new(model_t *m, energun_pair_txn_workspace_t *w)
{
    uint8_t base[8] = {8,7,6,5,4,3,2,1}, payload[117] = {0};
    payload[0] = 12u;
    return energun_pair_txn_save(&ops, m, w, 1u, base, sizeof base,
        ENERGUN_SC_ID_PROJECT, 0u, payload, sizeof payload, 0u);
}
int main(void)
{
    model_t m; energun_pair_txn_workspace_t w;
    memset(&m, 0, sizeof m); seed_old_pair(&m);
    CHECK("successful transaction commits new Base + matching Sidecar",
        save_new(&m, &w) == ENERGUN_PAIR_OK && m.base_seq[1] == 6u &&
        m.sidecar_valid[1] && old_pair_intact(&m));
    memset(&m, 0, sizeof m); seed_old_pair(&m); m.fault = FAULT_SIDECAR_BEFORE_COMMIT;
    CHECK("Sidecar staging failure leaves old pair and skips Base write",
        save_new(&m, &w) == ENERGUN_PAIR_ERR_SIDECAR_COMMIT &&
        old_pair_intact(&m) && m.base_writes == 0u);
    memset(&m, 0, sizeof m); seed_old_pair(&m); m.fault = FAULT_BASE_ERASE;
    CHECK("Base erase failure leaves old pair; staged Sidecar is orphaned",
        save_new(&m, &w) == ENERGUN_PAIR_ERR_BASE_COMMIT &&
        old_pair_intact(&m) && m.sidecar_valid[1]);
    memset(&m, 0, sizeof m); seed_old_pair(&m); m.fault = FAULT_BASE_PAYLOAD;
    CHECK("Base payload failure leaves old pair",
        save_new(&m, &w) == ENERGUN_PAIR_ERR_BASE_COMMIT && old_pair_intact(&m));
    memset(&m, 0, sizeof m); seed_old_pair(&m); m.fault = FAULT_BASE_HEADER;
    CHECK("Base header failure leaves old pair",
        save_new(&m, &w) == ENERGUN_PAIR_ERR_BASE_COMMIT && old_pair_intact(&m));
    memset(&m, 0, sizeof m); seed_old_pair(&m); m.fault = FAULT_VERIFY;
    CHECK("post-commit verification failure is never reported as success",
        save_new(&m, &w) == ENERGUN_PAIR_ERR_PAIR_VERIFY);
    memset(&m, 0, sizeof m);
    CHECK("first save supports an empty Base/Sidecar store",
        save_new(&m, &w) == ENERGUN_PAIR_OK && m.base_valid[0] &&
        m.sidecar_valid[0] && m.base_seq[0] == 0u);
    memset(&m, 0, sizeof m); seed_old_pair(&m);
    CHECK("invalid arguments are rejected without writes",
        energun_pair_txn_save(NULL, &m, &w, 1u, m.base[0], 8u,
            ENERGUN_SC_ID_PROJECT, 0u, m.sidecar[0], 117u, 0u) ==
            ENERGUN_PAIR_ERR_ARGUMENT && m.base_writes == 0u &&
            m.sidecar_writes == 0u);
    printf("CP1 pair transaction core: %u checks, %u failures\\n", checks, failures);
    return failures != 0u;
}
