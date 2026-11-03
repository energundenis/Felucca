/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — bounded Sidecar A/B Flash backend contract.
 *
 * This module is address-bounded and transport-injected. Runtime callsites
 * remain gated on a frozen payload/ownership contract and full target-link
 * qualification. The addresses below are candidate-only; this header does not
 * approve production sector allocation or physical writes.
 */
#ifndef ENERGUN_SIDECAR_FLASH_H
#define ENERGUN_SIDECAR_FLASH_H
#include <stddef.h>
#include <stdint.h>
#include "energun_pair_storage_adapter.h"

#define ENERGUN_SC_FLASH_SECTOR_SIZE  4096u
#define ENERGUN_SC_FLASH_PAGE_SIZE     256u
#define ENERGUN_SC_FLASH_PAYLOAD_OFF   256u
#define ENERGUN_SC_FLASH_TAIL_OFF     0xF00u
#define ENERGUN_SC_FLASH_MAX_RECORD   (ENERGUN_SC_FLASH_TAIL_OFF - ENERGUN_SC_FLASH_PAYLOAD_OFF)
#define ENERGUN_SC_FLASH_A            0x000E8000u
#define ENERGUN_SC_FLASH_B            0x000E9000u

typedef struct {
    int (*read)(void *, uint32_t, void *, size_t);
    int (*erase4k)(void *, uint32_t);
    int (*program_page)(void *, uint32_t, const void *, size_t);
} energun_sc_flash_io_t;

typedef struct {
    const energun_sc_flash_io_t *io;
    void *io_context;
} energun_sc_flash_t;

/* Initialize only; this does not probe, erase, allocate or write Flash. */
int energun_sc_flash_init(energun_sc_flash_t *,
                          const energun_sc_flash_io_t *, void *);

/* Callback-compatible with energun_sidecar_storage_ops_t. */
int energun_sc_flash_read_copy(void *, energun_sc_identity_kind_t, uint8_t,
                               uint8_t, uint8_t *, size_t, size_t *, uint32_t *);
int energun_sc_flash_write_copy(void *, energun_sc_identity_kind_t, uint8_t,
                                uint8_t, const uint8_t *, size_t);

extern const energun_sidecar_storage_ops_t energun_sc_flash_storage_ops;
int energun_sidecar_flash_hw_init(void);
/* Valid only after a successful init; static-lifetime context for runtime binding. */
energun_sc_flash_t *energun_sidecar_flash_hw_context(void);
#endif
