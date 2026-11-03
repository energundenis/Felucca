# CP1 Sidecar Flash region source audit — 2026-11-03

Repository submodule pin: `hugelton/felucca-bsp@5942e68b89ab9ccd23f2c541d712ca38fe635f11`.

## Source facts

- `chip/fm1_flash.h` defines `FL_XDATA1` as `[0xE7000, 0xFC000)` and exposes `FL_XDATA_OK` / `FL_RANGE_OK` for firmware-owned data.
- The same BSP defines main Felucca storage `[0x97000, 0xE0000)`, OTA staging `[0xE0000, 0xE5000)`, autosave `[0xE5000, 0xE7000)`, and globals `[0xFC000, 0xFF000)`.
- `firmware/src/fm6_vbank.c` and `storage_hw.c` reserve `[0xE7000, 0xE8000)` for the FM6 voice bank.
- `firmware/loader/ldr_core.c` scans update-record candidates from `0x93000` to `0xFC000`, skipping the main Felucca data interval. It checks 80 bytes at each sector's `+0xF00` for marker `0x5441` and a valid CRC.
- The BSP explicitly requires the update-record tail bytes in XDATA sectors to remain erased or not form a valid record.

## Candidate, not allocation

The aligned sectors at `0xE8000` and `0xE9000` are candidate Sidecar A/B sectors: they follow the FM6 voice-bank sector and lie before globals. A future backend must leave the entire final 256 bytes (`+0xF00..+0xFFF`) erased in each sector, so the loader cannot mistake Sidecar data for an update record.

The current `storage_hw.c` guard still rejects writes to these candidate sectors. No Sidecar sector is allocated, no production write path is enabled, and no physical FM-1 or Flash was accessed. This source audit does not replace build/link verification or simulated-NOR fault-injection tests.

## Remaining CP1 work

1. Implement bounded Sidecar A/B read/write callbacks with the loader-tail exclusion.
2. Add host tests for sector bounds, page programming, header-last commit, readback and power-cut recovery.
3. Integrate Sidecar serialization into project save/load, rename, preset-bank save/load and autosave/restore without changing legacy Base formats.
4. Coordinate Sidecar writes with FM6 staging ownership.
5. Run full focused host regression. Keep physical device writes disabled.
