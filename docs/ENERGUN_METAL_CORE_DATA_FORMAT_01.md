

## 10. Follow-up source audit — findings (2026-10-30)

The following facts were confirmed by reading the pinned Felucca/BSP sources. They narrow the candidate design but do not authorize writes.

### Confirmed address ownership and current access policy

- BSP `chip/fm1_flash.h` defines XDATA0 as `0x93000–0x97000` (16 KiB) and XDATA1 as `0xE7000–0xFC000` (84 KiB).
- BSP comments describe these ranges as extra flash for firmware-owned data, outside the normal store. They explicitly warn that the update loader checks a special update record at offset `0xF00–0xF07` in each 4 KiB sector. Those eight bytes must remain erased or otherwise not form a valid update record.
- Felucca `firmware/src/storage_hw.c` currently defines `ST_OK` as `FL_STORE_OK` plus only the single FM6 voice-bank sector `0xE7000–0xE8000`. It does **not** permit general XDATA writes.
- Felucca `firmware/src/storage.c` identifies `0xE7000` as the FM6 voice bank. Do not allocate that sector.
- The candidate region `0xE8000–0xF2000` is therefore inside the BSP's nominal XDATA1 range and outside the currently documented Felucca storage map, but it remains only a candidate. We have not yet proven that no other firmware/BSP/loader component owns any of its sectors.
- `storage.c`'s normal A/B object format writes payload from sector offset 256 and can use the full sector. It cannot be reused unchanged in XDATA because it does not preserve the loader-reserved `0xF00–0xF07` bytes. A dedicated writer must skip those bytes and validate all bounds.
- Existing `st_crc32()` in `storage.c` is the zlib-compatible CRC-32 variant: reflected polynomial `0xEDB88320`, initial state `0xFFFFFFFF`, final complement. This matches CRC-32/ISO-HDLC proposed above; implementation should reuse this helper rather than add a second CRC implementation.

### Emulator persistence gap

- Pinned upstream `web/emu/worklet.js` exports `STORE_AREAS = [[0x97000, 0xE0000], [0xE5000, 0xE8000], [0xFC000, 0x100000]]`.
- Its sector capture/restore logic only persists sectors inside those ranges. XDATA0 and XDATA1 above `0xE8000` are not included.
- Consequently, a new XDATA sidecar backend would not survive a browser-emulator reload unless the emulator's flash persistence allowlist is extended in a matching, separately tested change. This is an emulator limitation, not evidence that the hardware XDATA region is unavailable.
- The emulator currently includes `0xE7000` because that sector is FM6 Voice Bank storage; this does not imply the remaining XDATA1 range is persisted.

### Revised status of the proposed 40 KiB reservation

The proposed `0xE8000–0xF2000` reservation remains **unverified**. Before assigning it:
1. Search the complete pinned Felucca, BSP and loader trees for every XDATA address, XDATA helper, direct flash offset and sector erase/program operation.
2. Inspect the exact loader source around its update-record scan and erase behavior.
3. Verify the physical FM-1 flash JEDEC ID/capacity and board revision on the target.
4. Extend and test the firmware storage whitelist only after the ownership audit.
5. Extend emulator persistence for XDATA if browser-based testing is part of acceptance.
6. Add a test proving each XDATA write preserves bytes `0xF00–0xF07` and all neighboring sectors.

**No sector in XDATA is approved for allocation yet.**
