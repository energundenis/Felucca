#include "energun_sidecar_flash.h"

/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments */
/* The firmware side of storage.c (FELUCCA_FLASH): flash reads in short IRQ-off windows so the
 * audio keeps up, erases and writes only inside the storage areas, the audio buffer silenced
 * first. The host tests (storage_test.c) provide their own st_* on a simulated NOR. */
static uint8_t flash_ok;                 /* JEDEC id matched at boot (persist_boot) */
static int st_read(uint32_t off, void *dst, uint32_t n)   /* 256-byte IRQ-off windows: audio keeps up */
{
    uint8_t *d = dst;
    while (n) {
        uint32_t k = n > 256u ? 256u : n, f = irq_save();
        int rc = FL_FAR(fl_read_ram)(off, d, k);
        irq_restore(f);
        if (rc)
            return rc;
        off += k;
        d += k;
        n -= k;
    }
    return 0;
}
static void audio_silence(void)                 /* IRQs off: the DMA would loop stale audio (buzz) */
{
    uint32_t i;
    for (i = 0; i < sizeof abuf / sizeof abuf[0]; i++)
        abuf[i] = 0;
}
/* the store: fm1_flash.h FL_STORE_OK, and the FM6 voice bank (fm6_vbank.c FVB_HOME, 1.4.1): one sector at 0xE7000, the first
 * of felucca-bsp's FL_XDATA1 (a firmware's own data), which Felucca uses for nothing else */
#define ST_VBANK_LO 0x000E7000u
#define ST_VBANK_HI 0x000E8000u
#define ST_OK(off, n) (FL_STORE_OK(off, n) || FL_IN(off, n, ST_VBANK_LO, ST_VBANK_HI))
static int st_erase(uint32_t off)
{
    uint32_t took, f;
    int rc;
    if (!ST_OK(off, 0x1000u))
        return -8;
    f = irq_save();
    audio_silence();
    fm1__led_lines(0);                          /* (#236) the erase stops the LED scan for ~17 ms (the IRQs are off and its code */
    rc = FL_FAR(fl_erase4k_ram)(off, &took);    /* is in the flash): one column would stay lit at full duty and the rest dark, a */
    irq_restore(f);                             /* flash of the whole panel. Dark rows: the panel just drops out; the tick lights */
    return rc;                                  /* the rows again at its next pass */
}
static int st_prog(uint32_t off, const void *src, uint32_t n)
{
    if (!ST_OK(off, n))                         /* only inside the storage areas */
        return -8;
    return fl_write(off, src, n);
}


/* Sidecar's deliberately narrower XDATA1 transport. The generic storage
 * st_erase/st_prog guards remain unchanged; only this backend may address
 * the two candidate sectors, and only below the loader-reserved tail.
 */
static int scf_hw_sector(uint32_t off)
{
    return off == ENERGUN_SC_FLASH_A || off == ENERGUN_SC_FLASH_B;
}
static int scf_hw_read(void *context, uint32_t off, void *dst, size_t n)
{
    (void)context;
    if (dst == 0 || n > 0xF00u ||
        !((off >= ENERGUN_SC_FLASH_A && off <= ENERGUN_SC_FLASH_A + 0xF00u &&
           n <= ENERGUN_SC_FLASH_A + 0xF00u - off) ||
          (off >= ENERGUN_SC_FLASH_B && off <= ENERGUN_SC_FLASH_B + 0xF00u &&
           n <= ENERGUN_SC_FLASH_B + 0xF00u - off)))
        return -8;
    return st_read(off, dst, (uint32_t)n);
}
static int scf_hw_erase4k(void *context, uint32_t off)
{
    uint32_t took, f;
    int rc;
    (void)context;
    if (!flash_ok || !scf_hw_sector(off))
        return -8;
    f = irq_save();
    audio_silence();
    fm1__led_lines(0);
    rc = FL_FAR(fl_erase4k_ram)(off, &took);
    irq_restore(f);
    return rc;
}
static int scf_hw_program_page(void *context, uint32_t off,
                               const void *src, size_t n)
{
    uint32_t sector_off;
    (void)context;
    if (!flash_ok || src == 0 || n == 0u || n > 256u ||
        (off & 0xFFu) + n > 256u)
        return -8;
    if (off >= ENERGUN_SC_FLASH_A && off < ENERGUN_SC_FLASH_A + 0x1000u)
        sector_off = off - ENERGUN_SC_FLASH_A;
    else if (off >= ENERGUN_SC_FLASH_B && off < ENERGUN_SC_FLASH_B + 0x1000u)
        sector_off = off - ENERGUN_SC_FLASH_B;
    else
        return -8;
    if (sector_off + n > 0xF00u)
        return -8;
    /* fl_write splits at page boundaries; this callback has already checked
     * that the caller supplied exactly one page-bounded fragment.
     */
    return fl_write(off, (const uint8_t *)src, (uint32_t)n);
}
static const energun_sc_flash_io_t scf_hw_io = {
    scf_hw_read, scf_hw_erase4k, scf_hw_program_page
};
static energun_sc_flash_t scf_hw_instance;
int energun_sidecar_flash_hw_init(void)
{
    return energun_sc_flash_init(&scf_hw_instance, &scf_hw_io, 0);
}
energun_sc_flash_t *energun_sidecar_flash_hw_context(void)
{
    return &scf_hw_instance;
}
