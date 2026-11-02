/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — backend-neutral Base + Sidecar pair coordinator.
 */
#include "energun_sidecar_pair_coordinator.h"

#include <string.h>

static uint32_t sc_pair_crc32_add(uint32_t crc, const uint8_t *data,
                                  uint32_t length)
{
    uint32_t i;
    while (length-- != 0u) {
        crc ^= *data++;
        for (i = 0; i < 8u; ++i)
            crc = (crc >> 1) ^ ((crc & 1u) ? 0xEDB88320u : 0u);
    }
    return crc;
}

uint32_t energun_sc_pair_crc32(const uint8_t *data, uint32_t length)
{
    if (data == 0 && length != 0u)
        return 0u;
    return ~sc_pair_crc32_add(0xFFFFFFFFu, data, length);
}

static int sc_pair_read_candidates(
    const energun_sc_pair_backend_t *backend, void *context,
    energun_sc_pair_workspace_t *workspace,
    energun_sc_candidate_t candidates[2])
{
    unsigned int copy;
    for (copy = 0; copy < 2u; ++copy) {
        energun_sc_pair_sidecar_meta_t meta;
        int rc;
        meta.sequence = 0u;
        meta.record_length = 0u;
        rc = backend->read_sidecar_copy(context, (uint8_t)copy,
                                        workspace->sidecar_copy[copy],
                                        ENERGUN_SC_PAIR_MAX_RECORD, &meta);
        if (rc < 0)
            return -1;
        candidates[copy].record = workspace->sidecar_copy[copy];
        candidates[copy].record_length = rc == 0 ? meta.record_length : 0u;
        candidates[copy].storage_sequence = meta.sequence;
        candidates[copy].storage_valid = (uint8_t)(rc == 0);
        if (rc == 0 && meta.record_length > ENERGUN_SC_PAIR_MAX_RECORD)
            return -1;
    }
    return 0;
}

static energun_sc_pair_status_t sc_pair_finish(
    energun_sc_pair_coordinator_t *coordinator,
    energun_sc_pair_status_t status)
{
    coordinator->busy = 0u;
    return status;
}

energun_sc_pair_status_t energun_sc_pair_commit(
    energun_sc_pair_coordinator_t *coordinator,
    const energun_sc_pair_backend_t *backend,
    void *backend_context,
    const energun_sc_pair_request_t *request,
    energun_sc_pair_workspace_t *workspace)
{
    energun_sc_pair_base_meta_t base;
    energun_sc_pair_base_meta_t verified_base;
    energun_sc_candidate_t candidates[2];
    energun_sc_ab_selection_t selection;
    energun_sc_view_t sidecar_view;
    energun_sc_pair_sidecar_meta_t sidecar_meta;
    uint32_t next_base_sequence;
    uint32_t next_base_crc;
    uint8_t base_write_copy;
    uint8_t sidecar_write_copy;
    size_t staged_length = 0u;
    int rc;

    if (coordinator == 0 || backend == 0 || request == 0 ||
        workspace == 0 || backend->read_base_current == 0 ||
        backend->write_base_copy == 0 || backend->read_sidecar_copy == 0 ||
        backend->write_sidecar_copy == 0 ||
        (request->base_payload == 0 && request->base_payload_length != 0u) ||
        (request->sidecar_payload == 0 &&
         request->sidecar_payload_length != 0u))
        return ENERGUN_SC_PAIR_ERR_ARGUMENT;
    if (coordinator->busy != 0u)
        return ENERGUN_SC_PAIR_ERR_BUSY;

    coordinator->busy = 1u;
    memset(&base, 0, sizeof base);
    rc = backend->read_base_current(backend_context, &base);
    if (rc != 0 || (base.valid != 0u && base.copy > 1u))
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_BASE_READ);

    if (sc_pair_read_candidates(backend, backend_context, workspace,
                                candidates) != 0)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_SIDECAR_READ);

    rc = energun_sc_v1_select_ab(
        candidates, request->identity_kind, request->identity_index,
        base.valid ? base.sequence : 0u,
        base.valid ? base.payload_crc32 : 0u,
        request->base_motion_count, &selection);
    if (rc != ENERGUN_SC_OK)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_SELECT);

    /* With no valid Base generation, no old Sidecar can be a matching pair,
     * even if its envelope happens to contain the zero/zero sentinel.
     */
    if (!base.valid) {
        selection.matching_copy = -1;
        selection.write_copy = selection.newest_valid_copy >= 0
            ? (int8_t)(1 - selection.newest_valid_copy) : 0;
    }
    if (selection.write_copy < 0 || selection.write_copy > 1)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_SELECT);

    next_base_sequence = base.valid ? base.sequence + 1u : 1u;
    base_write_copy = base.valid ? (uint8_t)(1u - base.copy) : 0u;
    sidecar_write_copy = (uint8_t)selection.write_copy;
    next_base_crc = energun_sc_pair_crc32(request->base_payload,
                                          request->base_payload_length);

    rc = energun_sc_v1_encode(
        workspace->staged_record, sizeof workspace->staged_record,
        &staged_length, request->identity_kind, request->identity_index,
        next_base_sequence, next_base_crc, request->sidecar_payload,
        request->sidecar_payload_length, request->base_motion_count);
    if (rc != ENERGUN_SC_OK)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_ENCODE);

    /* Stage the future-bound Sidecar before touching the active Base copy.
     * The selector protects the old matching copy even if an orphan has a
     * higher local storage sequence.
     */
    rc = backend->write_sidecar_copy(
        backend_context, sidecar_write_copy, selection.next_storage_sequence,
        workspace->staged_record, staged_length);
    if (rc != 0)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_SIDECAR_WRITE);

    sidecar_meta.sequence = 0u;
    sidecar_meta.record_length = 0u;
    rc = backend->read_sidecar_copy(
        backend_context, sidecar_write_copy, workspace->verify_record,
        sizeof workspace->verify_record, &sidecar_meta);
    if (rc != 0 || sidecar_meta.sequence != selection.next_storage_sequence ||
        sidecar_meta.record_length != staged_length ||
        energun_sc_v1_validate(
            workspace->verify_record, sidecar_meta.record_length,
            request->identity_kind, request->identity_index,
            next_base_sequence, next_base_crc, request->base_motion_count,
            &sidecar_view) != ENERGUN_SC_OK)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_SIDECAR_VERIFY);

    /* Only a completely committed and read-back-validated Sidecar permits
     * the Base generation to advance.
     */
    rc = backend->write_base_copy(
        backend_context, base_write_copy, next_base_sequence,
        request->base_payload, request->base_payload_length);
    if (rc != 0)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_BASE_WRITE);

    memset(&verified_base, 0, sizeof verified_base);
    rc = backend->read_base_current(backend_context, &verified_base);
    if (rc != 0 || !verified_base.valid ||
        verified_base.copy != base_write_copy ||
        verified_base.sequence != next_base_sequence ||
        verified_base.payload_length != request->base_payload_length ||
        verified_base.payload_crc32 != next_base_crc)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_BASE_VERIFY);

    if (sc_pair_read_candidates(backend, backend_context, workspace,
                                candidates) != 0)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_SIDECAR_READ);
    rc = energun_sc_v1_select_ab(
        candidates, request->identity_kind, request->identity_index,
        verified_base.sequence, verified_base.payload_crc32,
        request->base_motion_count, &selection);
    if (rc != ENERGUN_SC_OK || selection.matching_copy != sidecar_write_copy ||
        !candidates[sidecar_write_copy].storage_valid ||
        candidates[sidecar_write_copy].storage_sequence !=
            sidecar_meta.sequence)
        return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_ERR_PAIR_VERIFY);

    if (backend->clear_dirty_if_generation != 0)
        backend->clear_dirty_if_generation(backend_context,
                                           request->snapshot_generation);
    return sc_pair_finish(coordinator, ENERGUN_SC_PAIR_OK);
}

const char *energun_sc_pair_status_string(energun_sc_pair_status_t status)
{
    switch (status) {
    case ENERGUN_SC_PAIR_OK: return "ok";
    case ENERGUN_SC_PAIR_ERR_ARGUMENT: return "argument";
    case ENERGUN_SC_PAIR_ERR_BUSY: return "busy";
    case ENERGUN_SC_PAIR_ERR_BASE_READ: return "base-read";
    case ENERGUN_SC_PAIR_ERR_SIDECAR_READ: return "sidecar-read";
    case ENERGUN_SC_PAIR_ERR_SELECT: return "select";
    case ENERGUN_SC_PAIR_ERR_ENCODE: return "encode";
    case ENERGUN_SC_PAIR_ERR_SIDECAR_WRITE: return "sidecar-write";
    case ENERGUN_SC_PAIR_ERR_SIDECAR_VERIFY: return "sidecar-verify";
    case ENERGUN_SC_PAIR_ERR_BASE_WRITE: return "base-write";
    case ENERGUN_SC_PAIR_ERR_BASE_VERIFY: return "base-verify";
    case ENERGUN_SC_PAIR_ERR_PAIR_VERIFY: return "pair-verify";
    default: return "unknown";
    }
}
