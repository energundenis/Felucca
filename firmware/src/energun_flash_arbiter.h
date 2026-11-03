/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — bounded lease for shared Flash staging ownership.
 */
#ifndef ENERGUN_FLASH_ARBITER_H
#define ENERGUN_FLASH_ARBITER_H
#include <stdint.h>

#define ENERGUN_FLASH_OWNER_NONE       0u
#define ENERGUN_FLASH_OWNER_FM6_STAGE  1u
#define ENERGUN_FLASH_OWNER_AUTOSAVE   2u

typedef struct {
    uint32_t last_activity_ms;
    uint32_t lease_ms;
    uint8_t owner;
} energun_flash_arbiter_t;

/* All time comparisons are wrap-safe for leases shorter than 2^31 ms. */
int energun_flash_arbiter_acquire(energun_flash_arbiter_t *, uint8_t,
                                  uint32_t, uint32_t);
int energun_flash_arbiter_renew(energun_flash_arbiter_t *, uint8_t, uint32_t);
int energun_flash_arbiter_held(energun_flash_arbiter_t *, uint8_t, uint32_t);
int energun_flash_arbiter_release(energun_flash_arbiter_t *, uint8_t);
#endif
