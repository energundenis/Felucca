/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Base + Sidecar pair transaction coordinator.
 */
#include "energun_pair_txn.h"

energun_pair_status_t energun_pair_txn_save(
    const energun_pair_txn_ops_t *ops, void *context,
    energun_pair_txn_workspace_t *workspace,
    uint32_t base_object, const uint8_t *base_payload, uint32_t base_length,
    energun_sc_identity_kind_t identity_kind, uint8_t identity_index,
    const uint8_t *sidecar_payload, uint16_t sidecar_payload_length,
    uint16_t base_motion_count)
{
    energun_pair_base_meta_t target;
    size_t record_length = 0u;
    uint8_t sidecar_copy = 0u;

    if (ops == NULL || workspace == NULL || ops->plan_base == NULL ||
        ops->select_sidecar_target == NULL || ops->write_sidecar == NULL ||
        ops->write_base == NULL || ops->verify_pair == NULL ||
        base_payload == NULL || base_length == 0u || sidecar_payload == NULL)
        return ENERGUN_PAIR_ERR_ARGUMENT;

    /* No persistent state changes before the read-only plan is accepted. */
    if (ops->plan_base(context, base_object, base_payload, base_length,
                       &target) != 0 || target.copy > 1u ||
        target.payload_length != base_length)
        return ENERGUN_PAIR_ERR_BASE_PLAN;

    if (ops->select_sidecar_target(context, identity_kind, identity_index,
                                   &target, &sidecar_copy) != 0 ||
        sidecar_copy > 1u)
        return ENERGUN_PAIR_ERR_SIDECAR_TARGET;

    if (energun_sc_v1_encode(workspace->sidecar_record,
            sizeof workspace->sidecar_record, &record_length,
            identity_kind, identity_index, target.sequence,
            target.payload_crc32, sidecar_payload, sidecar_payload_length,
            base_motion_count) != ENERGUN_SC_OK)
        return ENERGUN_PAIR_ERR_SIDECAR_ENCODE;

    /* Preserve the old matching pair until the new Sidecar is committed. */
    if (ops->write_sidecar(context, identity_kind, identity_index,
            sidecar_copy, workspace->sidecar_record, record_length) != 0)
        return ENERGUN_PAIR_ERR_SIDECAR_COMMIT;

    /* Base header is its commit record and is written only after Sidecar. */
    if (ops->write_base(context, base_object, base_payload, base_length,
                        &target) != 0)
        return ENERGUN_PAIR_ERR_BASE_COMMIT;

    if (ops->verify_pair(context, base_object, identity_kind, identity_index,
            &target, workspace->sidecar_record, record_length,
            base_motion_count) != 0)
        return ENERGUN_PAIR_ERR_PAIR_VERIFY;

    return ENERGUN_PAIR_OK;
}

const char *energun_pair_status_string(energun_pair_status_t status)
{
    switch (status) {
    case ENERGUN_PAIR_OK: return "ok";
    case ENERGUN_PAIR_ERR_ARGUMENT: return "invalid argument or adapter";
    case ENERGUN_PAIR_ERR_BASE_PLAN: return "Base commit plan rejected";
    case ENERGUN_PAIR_ERR_SIDECAR_TARGET: return "Sidecar target selection failed";
    case ENERGUN_PAIR_ERR_SIDECAR_ENCODE: return "Sidecar payload validation failed";
    case ENERGUN_PAIR_ERR_SIDECAR_COMMIT: return "Sidecar staging/commit failed";
    case ENERGUN_PAIR_ERR_BASE_COMMIT: return "Base commit failed";
    case ENERGUN_PAIR_ERR_PAIR_VERIFY: return "committed pair readback failed";
    default: return "unknown pair transaction status";
    }
}
