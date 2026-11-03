/* SPDX-License-Identifier: GPL-3.0-only
 * Host acceptance for bounded Sidecar Flash backend; simulated NOR only.
 */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "../firmware/src/energun_sidecar_flash.h"

#define NOR_BASE ENERGUN_SC_FLASH_A
#define NOR_SIZE 0x2000u
static uint8_t nor[NOR_SIZE];
static unsigned int checks, failures, erases;
static int bounds(uint32_t off, size_t n)
{
    return off >= NOR_BASE && off - NOR_BASE <= NOR_SIZE &&
           n <= NOR_SIZE - (off - NOR_BASE);
}
static int sim_read(void *ctx, uint32_t off, void *dst, size_t n)
{
    (void)ctx;
    if (!bounds(off,n)) return -1;
    memcpy(dst,nor+(off-NOR_BASE),n); return 0;
}
static int sim_erase(void *ctx, uint32_t off)
{
    (void)ctx;
    if ((off != ENERGUN_SC_FLASH_A && off != ENERGUN_SC_FLASH_B) ||
        !bounds(off,4096u)) return -1;
    memset(nor+(off-NOR_BASE),0xFF,4096u); ++erases; return 0;
}
static int sim_program(void *ctx, uint32_t off, const void *src, size_t n)
{
    const uint8_t *p=src; size_t i; (void)ctx;
    if (!bounds(off,n) || n==0u || n>256u ||
        (off&255u)+n>256u || off%4096u>=0xF00u ||
        n>0xF00u-(off%4096u)) return -1;
    for(i=0;i<n;i++) if ((nor[off-NOR_BASE+i]&p[i])!=p[i]) return -1;
    for(i=0;i<n;i++) nor[off-NOR_BASE+i]&=p[i];
    return 0;
}
#define CHECK(label,expr) do { ++checks; if (!(expr)) { ++failures; printf("FAIL: %s\n",label); } } while(0)
int main(void)
{
    static const energun_sc_flash_io_t io={sim_read,sim_erase,sim_program};
    energun_sc_flash_t flash;
    uint8_t input[521], output[521];
    size_t n=0; uint32_t seq=0; unsigned int i;
    memset(nor,0xFF,sizeof nor);
    for(i=0;i<sizeof input;i++) input[i]=(uint8_t)(i*17u+3u);
    CHECK("init accepts complete IO contract",energun_sc_flash_init(&flash,&io,0)==0);
    CHECK("A write commits and reads back",energun_sc_flash_write_copy(&flash,ENERGUN_SC_ID_PROJECT,2,0,input,sizeof input)==0 &&
        energun_sc_flash_read_copy(&flash,ENERGUN_SC_ID_PROJECT,2,0,output,sizeof output,&n,&seq)==0 &&
        n==sizeof input && seq==1u && memcmp(input,output,n)==0);
    CHECK("B write uses only its exact sector",energun_sc_flash_write_copy(&flash,ENERGUN_SC_ID_PROJECT,2,1,input,117u)==0);
    CHECK("wrong identity rejected",energun_sc_flash_read_copy(&flash,ENERGUN_SC_ID_PROJECT,3,0,output,sizeof output,&n,&seq)!=0);
    CHECK("invalid copy rejected",energun_sc_flash_read_copy(&flash,ENERGUN_SC_ID_PROJECT,2,2,output,sizeof output,&n,&seq)!=0);
    nor[256] ^= 1u;
    CHECK("payload corruption rejected by CRC",energun_sc_flash_read_copy(&flash,ENERGUN_SC_ID_PROJECT,2,0,output,sizeof output,&n,&seq)!=0);
    CHECK("reserved update-loader tail is never programmed",sim_program(ENERGUN_SC_FLASH_A+0xF00u,input,1u)!=0);
    CHECK("erase rejects non-candidate sector",sim_erase(ENERGUN_SC_FLASH_A+4096u)!=0);
    CHECK("program cannot cross page",sim_program(ENERGUN_SC_FLASH_A+0x2FFu,input,2u)!=0);
    printf("SIDECAR FLASH BACKEND: %u checks, %u failures; erase calls=%u\n",checks,failures,erases);
    return failures!=0u;
}
