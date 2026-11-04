/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — shared Felucca engine-memory budget constants.
 *
 * Keep these limits in one place so production allocation and CP3.1
 * accounting cannot silently drift apart. This is a pool-size contract,
 * not a claim that the provisional METAL CORE state is production-ready.
 */
#ifndef ENG_METAL_CORE_MEMORY_BUDGET_H
#define ENG_METAL_CORE_MEMORY_BUDGET_H

#ifndef FELUCCA_ADPCM
#define FELUCCA_ADPCM 0
#endif

#define ENG_METAL_CORE_MEM_PART_NO_ADPCM 12888u
#define ENG_METAL_CORE_MEM_PART_ADPCM    13344u

#if FELUCCA_ADPCM
#define ENG_METAL_CORE_MEM_PART ENG_METAL_CORE_MEM_PART_ADPCM
#else
#define ENG_METAL_CORE_MEM_PART ENG_METAL_CORE_MEM_PART_NO_ADPCM
#endif

#endif
