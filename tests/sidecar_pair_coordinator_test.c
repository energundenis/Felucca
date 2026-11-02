/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — pair coordinator test against real storage.c + NOR.
 * OBJ_SETTINGS is a test-only Sidecar container; no production ID is assigned.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define __attribute__(x)
static uint8_t nor[0x100000u];
static unsigned int flash_ops, fail_at, fail_mode;
#define CUT_PARTIAL 1u
#define CUT_AFTER 2u

static int st_read(uint32_t off, void *dst, uint32_t n)
{
    if (off > sizeof nor || n > sizeof nor - off) return -1;
    memcpy(dst, nor + off, n);
    return 0;
}
static int st_erase(uint32_t off)
{
    size_t n = 4096u;
    ++flash_ops;
    if (off > sizeof nor || n > sizeof nor - off) return -1;
    if (fail_at == flash_ops && fail_mode == CUT_PARTIAL) {
        memset(nor + off, 0xFF, n / 2u);
        return -1;
    }
    memset(nor + off, 0xFF, n);
    if (fail_at == flash_ops && fail_mode == CUT_AFTER) return -1;
    return 0;
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    const uint8_t *in = (const uint8_t *)src;
    uint32_t i, count = n;
    ++flash_ops;
    if (off > sizeof nor || n > sizeof nor - off ||
        (off & 0xFFu) + n > 256u) return -1;
    if (fail_at == flash_ops && fail_mode == CUT_PARTIAL) count = n / 2u;
    for (i = 0; i < count; ++i) {
        if ((nor[off + i] & in[i]) != in[i]) return -1;
        nor[off + i] &= in[i];
    }
    if (fail_at == flash_ops) return -1;
    return 0;
}

#include "../firmware/src/storage.c"
#include "../firmware/src/energun_sidecar_v1.c"
#include "../firmware/src/energun_sidecar_pair_coordinator.c"

static unsigned int checks, failures;
static void check(const char *name, int ok)
{
    ++checks;
    printf("sidecar pair: %-76s %s\n", name, ok ? "PASS" : "FAIL");
    if (!ok) ++failures;
}

typedef struct {
    uint32_t live_generation, cleared_generation;
    uint8_t dirty_cleared, try_reenter;
    energun_sc_pair_coordinator_t *coordinator;
    const energun_sc_pair_backend_t *backend;
    const energun_sc_pair_request_t *request;
    energun_sc_pair_workspace_t *workspace;
    energun_sc_pair_status_t nested_result;
} sim_context_t;

static int read_base(void *ctx, energun_sc_pair_base_meta_t *out)
{
    sim_context_t *s = (sim_context_t *)ctx;
    st_meta_t meta;
    if (s->try_reenter) {
        s->try_reenter = 0u;
        s->nested_result = energun_sc_pair_commit(s->coordinator, s->backend,
            ctx, s->request, s->workspace);
    }
    memset(out, 0, sizeof *out);
    if (st_current_meta(OBJ_PROJECT0, &meta) != 0) return 0;
    out->valid = 1u;
    out->copy = meta.copy;
    out->sequence = meta.sequence;
    out->payload_length = meta.payload_length;
    out->payload_crc32 = meta.payload_crc;
    return 0;
}
static int write_base(void *ctx, uint8_t copy, uint32_t seq,
                      const uint8_t *payload, uint32_t length)
{
    st_meta_t meta;
    uint32_t expected_seq = 1u;
    uint8_t expected_copy = 0u;
    (void)ctx;
    if (st_current_meta(OBJ_PROJECT0, &meta) == 0) {
        expected_seq = meta.sequence + 1u;
        expected_copy = (uint8_t)(1u - meta.copy);
    }
    if (copy != expected_copy || seq != expected_seq) return -1;
    return st_save_to(OBJ_PROJECT0, payload, length, copy);
}
static int read_sidecar(void *ctx, uint8_t copy, uint8_t *dst,
                        size_t capacity, energun_sc_pair_sidecar_meta_t *out)
{
    st_meta_t meta;
    (void)ctx;
    if (st_read_copy_meta(OBJ_SETTINGS, copy, dst, (uint32_t)capacity,
                          &meta) != 0) return 1;
    out->sequence = meta.sequence;
    out->record_length = meta.payload_length;
    return 0;
}
static int write_sidecar(void *ctx, uint8_t copy, uint32_t seq,
                         const uint8_t *record, size_t length)
{
    st_meta_t meta;
    uint32_t expected_seq = 1u;
    (void)ctx;
    if (st_current_meta(OBJ_SETTINGS, &meta) == 0)
        expected_seq = meta.sequence + 1u;
    if (seq != expected_seq) return -1;
    return st_save_to(OBJ_SETTINGS, record, (uint32_t)length, copy);
}
static void clear_dirty(void *ctx, uint32_t generation)
{
    sim_context_t *s = (sim_context_t *)ctx;
    if (s->live_generation == generation) {
        s->dirty_cleared = 1u;
        s->cleared_generation = generation;
    }
}
static const energun_sc_pair_backend_t backend = {
    read_base, write_base, read_sidecar, write_sidecar, clear_dirty
};

static void make_base(uint8_t out[300], uint8_t seed)
{
    unsigned int i;
    for (i = 0; i < 300u; ++i) out[i] = (uint8_t)(seed + i * 13u);
}
static void make_sidecar(uint8_t out[501])
{
    unsigned int part, i;
    memset(out, 0, 501u);
    for (part = 0; part < 4u; ++part) {
        for (i = 0; i < 16u; ++i) out[part * 16u + i] = i == 9u ? 0u : 64u;
        for (i = 0; i < 13u; ++i) out[64u + part * 13u + i] = 0u;
    }
    out[116] = 128u;
    for (i = 0; i < 128u; ++i) {
        out[117u + i * 3u] = (uint8_t)i;
        out[118u + i * 3u] = 0u;
        out[119u + i * 3u] = 0u;
    }
}
static size_t encode(uint8_t *out, uint32_t base_seq, uint32_t base_crc,
                     const uint8_t *payload, uint16_t length)
{
    size_t written = 0u;
    if (energun_sc_v1_encode(out, ENERGUN_SC_V1_MAX_RECORD, &written,
        ENERGUN_SC_ID_PROJECT, 0u, base_seq, base_crc, payload, length, 0u)
        != ENERGUN_SC_OK) return 0u;
    return written;
}
static int fixture(sim_context_t *ctx, uint8_t old_base[300],
                   uint8_t new_base[300], uint8_t old_sc[117],
                   uint8_t new_sc[501])
{
    uint8_t old_record[ENERGUN_SC_PAIR_MAX_RECORD];
    uint8_t orphan[ENERGUN_SC_PAIR_MAX_RECORD];
    size_t old_len, orphan_len;
    uint32_t old_crc, new_crc;
    memset(nor, 0xFF, sizeof nor);
    flash_ops = fail_at = fail_mode = 0u;
    memset(ctx, 0, sizeof *ctx);
    make_base(old_base, 0x21u);
    make_base(new_base, 0xA7u);
    memset(old_sc, 0, 117u);
    make_sidecar(new_sc);
    old_crc = st_crc32(old_base, 300u);
    new_crc = st_crc32(new_base, 300u);
    old_len = encode(old_record, 1u, old_crc, old_sc, 117u);
    orphan_len = encode(orphan, 2u, new_crc, new_sc, 501u);
    if (!old_len || !orphan_len || st_save(OBJ_PROJECT0, old_base, 300u) != 0)
        return -1;
    /* Sidecar B seq1 matches Base A; Sidecar A seq2 is a future orphan. */
    if (st_save_to(OBJ_SETTINGS, old_record, (uint32_t)old_len, 1) != 0 ||
        st_save_to(OBJ_SETTINGS, orphan, (uint32_t)orphan_len, 0) != 0)
        return -1;
    flash_ops = 0u;
    return 0;
}
static energun_sc_pair_status_t commit(sim_context_t *ctx,
        energun_sc_pair_coordinator_t *coordinator,
        energun_sc_pair_workspace_t *workspace,
        const uint8_t *base_payload, const uint8_t *sidecar_payload,
        uint16_t sidecar_length, uint32_t generation)
{
    energun_sc_pair_request_t request;
    memset(&request, 0, sizeof request);
    request.identity_kind = ENERGUN_SC_ID_PROJECT;
    request.identity_index = 0u;
    request.base_motion_count = 0u;
    request.base_payload = base_payload;
    request.base_payload_length = 300u;
    request.sidecar_payload = sidecar_payload;
    request.sidecar_payload_length = sidecar_length;
    request.snapshot_generation = generation;
    ctx->coordinator = coordinator;
    ctx->backend = &backend;
    ctx->request = &request;
    ctx->workspace = workspace;
    return energun_sc_pair_commit(coordinator, &backend, ctx, &request, workspace);
}
static int active_pair(uint8_t base_out[300], uint32_t *base_seq,
                       uint8_t *sidecar_out, size_t *sidecar_len,
                       uint8_t *base_copy, uint8_t *sidecar_copy)
{
    st_meta_t bm, sm[2];
    energun_sc_candidate_t c[2];
    energun_sc_ab_selection_t sel;
    energun_sc_view_t view;
    uint8_t records[2][ENERGUN_SC_PAIR_MAX_RECORD];
    unsigned int i;
    if (st_current_meta(OBJ_PROJECT0, &bm) != 0 ||
        st_load(OBJ_PROJECT0, base_out, 300u) != 300) return -1;
    for (i = 0; i < 2u; ++i) {
        c[i].storage_valid = st_read_copy_meta(OBJ_SETTINGS, i, records[i],
            sizeof records[i], &sm[i]) == 0;
        c[i].record = records[i];
        c[i].record_length = c[i].storage_valid ? sm[i].payload_length : 0u;
        c[i].storage_sequence = c[i].storage_valid ? sm[i].sequence : 0u;
    }
    if (energun_sc_v1_select_ab(c, ENERGUN_SC_ID_PROJECT, 0u,
        bm.sequence, bm.payload_crc, 0u, &sel) != ENERGUN_SC_OK ||
        sel.matching_copy < 0) return -1;
    i = (unsigned int)sel.matching_copy;
    if (energun_sc_v1_validate(c[i].record, c[i].record_length,
        ENERGUN_SC_ID_PROJECT, 0u, bm.sequence, bm.payload_crc, 0u,
        &view) != ENERGUN_SC_OK) return -1;
    memcpy(sidecar_out, view.payload, view.payload_length);
    *sidecar_len = view.payload_length;
    *base_seq = bm.sequence;
    *base_copy = bm.copy;
    *sidecar_copy = (uint8_t)i;
    return 0;
}

static void test_success(void)
{
    sim_context_t ctx;
    energun_sc_pair_coordinator_t coordinator = {0u};
    energun_sc_pair_workspace_t workspace;
    uint8_t old_base[300], new_base[300], old_sc[117], new_sc[501];
    uint8_t got_base[300], got_sc[501], bc, sc;
    uint32_t seq;
    size_t sc_len;
    int init = fixture(&ctx, old_base, new_base, old_sc, new_sc);
    ctx.live_generation = 42u;
    check("pair coordinator commits Sidecar A over orphan before Base B",
        init == 0 && commit(&ctx, &coordinator, &workspace, new_base, new_sc,
            501u, 42u) == ENERGUN_SC_PAIR_OK && flash_ops == 9u);
    check("post-commit load returns exact new Base and matching Sidecar",
        active_pair(got_base, &seq, got_sc, &sc_len, &bc, &sc) == 0 &&
        seq == 2u && bc == 1u && sc == 0u && !memcmp(got_base,new_base,300u) &&
        sc_len == 501u && !memcmp(got_sc,new_sc,501u));
    check("dirty generation clears only after full pair verification",
        ctx.dirty_cleared && ctx.cleared_generation == 42u);
}
static void test_power_cuts(void)
{
    unsigned int mode, cut, tested = 0u;
    for (mode = CUT_PARTIAL; mode <= CUT_AFTER; ++mode) {
        for (cut = 1u; cut <= 9u; ++cut) {
            sim_context_t ctx;
            energun_sc_pair_coordinator_t coordinator = {0u};
            energun_sc_pair_workspace_t workspace;
            uint8_t old_base[300], new_base[300], old_sc[117], new_sc[501];
            uint8_t got_base[300], got_sc[501], bc, sc;
            uint32_t seq;
            size_t sc_len;
            int init = fixture(&ctx, old_base, new_base, old_sc, new_sc);
            if (init != 0) { check("power-cut fixture initializes", 0); return; }
            ctx.live_generation = 77u;
            fail_at = cut;
            fail_mode = mode;
            if (commit(&ctx, &coordinator, &workspace, new_base, new_sc,
                       501u, 77u) == ENERGUN_SC_PAIR_OK) {
                check("injected power cut interrupts the transaction", 0); return;
            }
            memset(&coordinator, 0, sizeof coordinator); /* reboot */
            if (active_pair(got_base, &seq, got_sc, &sc_len, &bc, &sc) != 0) {
                check("power loss leaves a complete recoverable pair", 0); return;
            }
            if (seq == 1u)
                check("cut before Base commit preserves old matching pair",
                    bc == 0u && sc == 1u && !memcmp(got_base,old_base,300u) &&
                    sc_len == 117u && !memcmp(got_sc,old_sc,117u));
            else if (seq == 2u)
                check("cut after durable Base header exposes complete new pair",
                    bc == 1u && sc == 0u && !memcmp(got_base,new_base,300u) &&
                    sc_len == 501u && !memcmp(got_sc,new_sc,501u));
            else check("recovery selects old or new Base, never a mixed generation", 0);
            ++tested;
        }
    }
    check("all 18 partial/after-operation power-cut points were exercised",
          tested == 18u);
}
static void test_guards(void)
{
    sim_context_t ctx;
    energun_sc_pair_coordinator_t coordinator = {0u};
    energun_sc_pair_workspace_t workspace;
    uint8_t old_base[300], new_base[300], old_sc[117], new_sc[501];
    int init = fixture(&ctx, old_base, new_base, old_sc, new_sc);
    ctx.live_generation = 99u;
    check("changed snapshot generation leaves dirty state set",
        init == 0 && commit(&ctx,&coordinator,&workspace,new_base,new_sc,501u,98u)
            == ENERGUN_SC_PAIR_OK && !ctx.dirty_cleared);

    init = fixture(&ctx, old_base, new_base, old_sc, new_sc);
    memset(&coordinator, 0, sizeof coordinator);
    check("invalid Sidecar payload is rejected before NOR erase/program",
        init == 0 && commit(&ctx,&coordinator,&workspace,new_base,new_sc,1u,1u)
            == ENERGUN_SC_PAIR_ERR_ENCODE && flash_ops == 0u);

    init = fixture(&ctx, old_base, new_base, old_sc, new_sc);
    memset(&coordinator, 0, sizeof coordinator);
    ctx.live_generation = 5u;
    ctx.try_reenter = 1u;
    check("re-entrant pair transaction is rejected while outer commit succeeds",
        init == 0 && commit(&ctx,&coordinator,&workspace,new_base,new_sc,501u,5u)
            == ENERGUN_SC_PAIR_OK && ctx.nested_result == ENERGUN_SC_PAIR_ERR_BUSY);
}
int main(void)
{
    test_success();
    test_power_cuts();
    test_guards();
    printf("sidecar pair: %s (%u checks, %u failures)\n",
        failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
