/* SPDX-License-Identifier: GPL-3.0-only
 * Host acceptance for FM6/autosave staging reservation leases.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/energun_flash_arbiter.h"

static unsigned int checks, failures;
#define CHECK(label, expression) do { \
    ++checks; if (!(expression)) { ++failures; printf("FAIL: %s\\n", label); } \
    else printf("PASS: %s\\n", label); \
} while (0)

int main(void)
{
    energun_flash_arbiter_t arbiter;
    memset(&arbiter, 0, sizeof arbiter);

    CHECK("FM6 staging lease acquired", energun_flash_arbiter_acquire(
        &arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE, 100u, 1000u) == 0);
    CHECK("autosave is blocked while FM6 owns staging", energun_flash_arbiter_acquire(
        &arbiter, ENERGUN_FLASH_OWNER_AUTOSAVE, 200u, 100u) != 0);
    CHECK("FM6 lease is reported held before expiry", energun_flash_arbiter_held(
        &arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE, 500u));
    CHECK("FM6 activity renews the lease", energun_flash_arbiter_renew(
        &arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE, 900u) == 0);
    CHECK("renewal extends ownership beyond original deadline", energun_flash_arbiter_held(
        &arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE, 1800u));
    CHECK("idle lease expires at its exact deadline", !energun_flash_arbiter_held(
        &arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE, 1900u));
    CHECK("autosave may acquire after FM6 lease expiry", energun_flash_arbiter_acquire(
        &arbiter, ENERGUN_FLASH_OWNER_AUTOSAVE, 1900u, 100u) == 0);
    CHECK("stale FM6 release cannot clear autosave ownership", energun_flash_arbiter_release(
        &arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE) != 0 &&
        energun_flash_arbiter_held(&arbiter, ENERGUN_FLASH_OWNER_AUTOSAVE, 1901u));
    CHECK("current owner releases its reservation", energun_flash_arbiter_release(
        &arbiter, ENERGUN_FLASH_OWNER_AUTOSAVE) == 0);

    CHECK("lease acquisition is wrap-safe across millisecond rollover",
        energun_flash_arbiter_acquire(&arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE,
            0xFFFFFFF0u, 32u) == 0 &&
        energun_flash_arbiter_held(&arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE,
            0x0000000Fu));
    CHECK("rollover lease expires at the exact elapsed duration",
        !energun_flash_arbiter_held(&arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE,
            0x00000010u));
    CHECK("zero owner and zero lease are rejected",
        energun_flash_arbiter_acquire(&arbiter, ENERGUN_FLASH_OWNER_NONE,
            0u, 10u) != 0 &&
        energun_flash_arbiter_acquire(&arbiter, ENERGUN_FLASH_OWNER_FM6_STAGE,
            0u, 0u) != 0);

    printf("FLASH ARBITER: %u checks, %u failures\\n", checks, failures);
    return failures != 0u;
}
