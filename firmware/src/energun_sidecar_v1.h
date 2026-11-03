/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — Sidecar V1 envelope and payload contract.
 *
 * Host-testable serialization module. It is not wired to storage.c, OBJ_COUNT,
 * a Flash sector map, archive V1, or physical hardware.
 */
#ifndef ENERGUN_SIDECAR_V1_H
#define ENERGUN_SIDECAR_V1_H

#include <stddef.h>
#include <stdint.h>

#define ENERGUN_SC_V1_HEADER_SIZE       20u
#define ENERGUN_SC_V1_PROJECT_VALUES    116u
#define ENERGUN_SC_V1_PROJECT_MAX_MOTION 128u
#define ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD 501u
#define ENERGUN_SC_V1_PRESET_PAYLOAD    210u
#define ENERGUN_SC_V1_PMC_MOTION_ID_FIRST 103u
#define ENERGUN_SC_V1_PMC_MOTION_ID_LAST  118u
#define ENERGUN_SC_V1_MAX_RECORD        (ENERGUN_SC_V1_HEADER_SIZE + ENERGUN_SC_V1_PROJECT_MAX_PAYLOAD)

#define ENERGUN_SC_V1_FORMAT_VERSION 1u
#define ENERGUN_SC_V1_SCHEMA_VERSION 1u

typedef enum {
    ENERGUN_SC_ID_PROJECT = 1,
    ENERGUN_SC_ID_LIVE_AUTOSAVE = 2,
    ENERGUN_SC_ID_PRESET_BANK = 3
} energun_sc_identity_kind_t;

typedef enum {
    ENERGUN_SC_OK = 0,
    ENERGUN_SC_ERR_ARGUMENT,
    ENERGUN_SC_ERR_MAGIC,
    ENERGUN_SC_ERR_FORMAT_VERSION,
    ENERGUN_SC_ERR_SCHEMA_VERSION,
    ENERGUN_SC_ERR_IDENTITY,
    ENERGUN_SC_ERR_BASE_GENERATION,
    ENERGUN_SC_ERR_RESERVED,
    ENERGUN_SC_ERR_LENGTH,
    ENERGUN_SC_ERR_PARAMETER_VALUE,
    ENERGUN_SC_ERR_PRIVATE_VALUE,
    ENERGUN_SC_ERR_MOTION_COUNT,
    ENERGUN_SC_ERR_MOTION_EVENT,
    ENERGUN_SC_ERR_SLOT_BITMAP
} energun_sc_status_t;

typedef struct {
    energun_sc_identity_kind_t identity_kind;
    uint8_t identity_index;
    uint32_t base_sequence;
    uint32_t base_payload_crc32;
    const uint8_t *payload;
    uint16_t payload_length;
} energun_sc_view_t;

/* One storage-validated A/B candidate. storage_valid must be true only
 * after the enclosing Felucca object header and payload CRC have passed.
 * storage_sequence is the enclosing storage object's local A/B sequence;
 * it is distinct from the Base generation stored inside the Sidecar envelope.
 */
typedef struct {
    const uint8_t *record;
    size_t record_length;
    uint32_t storage_sequence;
    uint8_t storage_valid;
} energun_sc_candidate_t;

typedef struct {
    int8_t matching_copy;       /* newest candidate bound to current Base, or -1 */
    int8_t write_copy;           /* safe opposite copy to overwrite next */
    int8_t newest_valid_copy;    /* newest structurally/semantically valid record */
    uint32_t next_storage_sequence;
} energun_sc_ab_selection_t;

/* Inspect a record against its own embedded Base fingerprint. This validates
 * envelope, identity and payload semantics, but does not assert that the
 * embedded Base is the currently selected Base.
 */
energun_sc_status_t energun_sc_v1_inspect(
    const uint8_t *record, size_t record_length,
    energun_sc_identity_kind_t expected_kind, uint8_t expected_index,
    uint16_t base_motion_count, energun_sc_view_t *view);

/* Select the protected matching copy and the safe write target.
 * Newest matching Base binding determines the protected copy. The next
 * storage sequence is based on the newest fully valid Sidecar record even
 * when that record is an orphan for a different Base generation.
 */
energun_sc_status_t energun_sc_v1_select_ab(
    const energun_sc_candidate_t candidates[2],
    energun_sc_identity_kind_t expected_kind, uint8_t expected_index,
    uint32_t current_base_sequence, uint32_t current_base_crc32,
    uint16_t base_motion_count, energun_sc_ab_selection_t *selection);

/* Validate a complete record against the selected base object's identity,
 * exact committed sequence and exact base payload CRC. base_motion_count is the number of legacy FUN10
 * events already occupying the shared 128-event pool (0..128).
 */
energun_sc_status_t energun_sc_v1_validate(
    const uint8_t *record, size_t record_length,
    energun_sc_identity_kind_t expected_kind, uint8_t expected_index,
    uint32_t expected_base_sequence, uint32_t expected_base_crc32,
    uint16_t base_motion_count,
    energun_sc_view_t *view);

/* Encode the 20-byte little-endian envelope around a caller-owned logical
 * payload. The payload is fully validated before output is modified.
 */
energun_sc_status_t energun_sc_v1_encode(
    uint8_t *output, size_t output_capacity, size_t *output_length,
    energun_sc_identity_kind_t identity_kind, uint8_t identity_index,
    uint32_t base_sequence, uint32_t base_payload_crc32,
    const uint8_t *payload, uint16_t payload_length,
    uint16_t base_motion_count);

/* Compare a preset Sidecar bitmap with the actual occupied slots in a raw
 * upreset.c bank. engine_count must be the target build's NENGINES so legacy
 * and optional-engine records follow the exact up_valid() rule.
 */
energun_sc_status_t energun_sc_v1_validate_preset_bank_binding(
    const uint8_t *base_bank, size_t base_bank_length, uint8_t engine_count,
    const uint8_t *sidecar_payload, uint16_t sidecar_payload_length);

const char *energun_sc_v1_status_string(energun_sc_status_t status);

#endif
