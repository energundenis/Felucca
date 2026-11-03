/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN FM-1 CORE — bounded lease for shared Flash staging ownership.
 */
#include "energun_flash_arbiter.h"

static void energun_flash_arbiter_expire(energun_flash_arbiter_t *arbiter,
                                         uint32_t now_ms)
{
    if (arbiter->owner != ENERGUN_FLASH_OWNER_NONE &&
        arbiter->lease_ms != 0u &&
        (uint32_t)(now_ms - arbiter->last_activity_ms) >= arbiter->lease_ms) {
        arbiter->owner = ENERGUN_FLASH_OWNER_NONE;
        arbiter->lease_ms = 0u;
        arbiter->last_activity_ms = 0u;
    }
}

int energun_flash_arbiter_acquire(energun_flash_arbiter_t *arbiter,
                                  uint8_t owner, uint32_t now_ms,
                                  uint32_t lease_ms)
{
    if (arbiter == 0 || owner == ENERGUN_FLASH_OWNER_NONE || lease_ms == 0u ||
        lease_ms >= 0x80000000u)
        return -1;
    energun_flash_arbiter_expire(arbiter, now_ms);
    if (arbiter->owner != ENERGUN_FLASH_OWNER_NONE && arbiter->owner != owner)
        return -1;
    arbiter->owner = owner;
    arbiter->last_activity_ms = now_ms;
    arbiter->lease_ms = lease_ms;
    return 0;
}

int energun_flash_arbiter_renew(energun_flash_arbiter_t *arbiter,
                                uint8_t owner, uint32_t now_ms)
{
    if (arbiter == 0 || owner == ENERGUN_FLASH_OWNER_NONE)
        return -1;
    energun_flash_arbiter_expire(arbiter, now_ms);
    if (arbiter->owner != owner)
        return -1;
    arbiter->last_activity_ms = now_ms;
    return 0;
}

int energun_flash_arbiter_held(energun_flash_arbiter_t *arbiter,
                               uint8_t owner, uint32_t now_ms)
{
    if (arbiter == 0 || owner == ENERGUN_FLASH_OWNER_NONE)
        return 0;
    energun_flash_arbiter_expire(arbiter, now_ms);
    return arbiter->owner == owner;
}

int energun_flash_arbiter_release(energun_flash_arbiter_t *arbiter,
                                  uint8_t owner)
{
    if (arbiter == 0 || owner == ENERGUN_FLASH_OWNER_NONE ||
        arbiter->owner != owner)
        return -1;
    arbiter->owner = ENERGUN_FLASH_OWNER_NONE;
    arbiter->lease_ms = 0u;
    arbiter->last_activity_ms = 0u;
    return 0;
}
