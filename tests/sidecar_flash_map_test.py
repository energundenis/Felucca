#!/usr/bin/env python3
"""Source-level qualification of the pinned FM-1 XDATA1 candidate window."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]

def read(path):
    p = ROOT / path
    if not p.is_file():
        raise SystemExit(f"FAIL: missing source: {path}")
    return p.read_text(encoding="utf-8", errors="replace")

def define(source, name):
    m = re.search(rf"^#define\s+{re.escape(name)}\s+(0x[0-9A-Fa-f]+|[0-9]+)u?\b", source, re.M)
    if not m:
        raise SystemExit(f"FAIL: missing numeric definition {name}")
    return int(m.group(1), 0)

def check(ok, label):
    if not ok:
        raise SystemExit(f"FAIL: {label}")
    print(f"PASS: {label}")

bsp = read("firmware/bsp/chip/fm1_flash.h")
loader = read("firmware/loader/ldr_core.c")
storage_hw = read("firmware/src/storage_hw.c")
vbank = read("firmware/src/fm6_vbank.c")

xlo, xhi = define(bsp, "FL_XDATA1_LO"), define(bsp, "FL_XDATA1_HI")
check((xlo, xhi) == (0xE7000, 0xFC000), "BSP XDATA1 range is [0xE7000, 0xFC000)")
check("FL_XDATA_OK" in bsp and "FL_RANGE_OK" in bsp, "BSP provides bounded firmware-data range checks")
check((define(bsp, "FL_DATA_LO"), define(bsp, "FL_DATA_HI")) == (0x97000, 0xE0000), "main data store range is confirmed")
check((define(bsp, "FL_OTA_LO"), define(bsp, "FL_OTA_HI")) == (0xE0000, 0xE5000), "OTA staging range is confirmed")
check((define(bsp, "FL_AUTO_LO"), define(bsp, "FL_AUTO_HI")) == (0xE5000, 0xE7000), "autosave range is confirmed")
check((define(bsp, "FL_GLOB_LO"), define(bsp, "FL_GLOB_HI")) == (0xFC000, 0xFF000), "global storage starts beyond XDATA1")

vbank_lo, vbank_hi = define(storage_hw, "ST_VBANK_LO"), define(storage_hw, "ST_VBANK_HI")
check((vbank_lo, vbank_hi, define(vbank, "FVB_HOME")) == (0xE7000, 0xE8000, 0xE7000), "FM6 voice bank owns [0xE7000, 0xE8000)")

check((define(loader, "LDR_REC_LO"), define(loader, "LDR_REC_HI")) == (0x93000, 0xFC000), "loader record scan range is confirmed")
check((define(loader, "LDR_DATA_LO"), define(loader, "LDR_DATA_HI")) == (0x97000, 0xE0000), "loader excludes the main Felucca data store")
check("ldr_fread(s - 0x100u, r, 80)" in loader and "ota_rd16(r + 6) == 0x5441u" in loader and "ota_crc16(r + 2, 78, 0)" in loader, "loader checks marker and CRC at each sector +0xF00")

a, b, sector = 0xE8000, 0xE9000, 0x1000
check(a % sector == 0 and b % sector == 0 and a + sector <= b, "candidate A/B offsets are aligned and non-overlapping")
check(xlo <= vbank_hi <= a and b + sector <= xhi, "candidate A/B fit after FM6 and before global storage")
check(a >= 0xE5000 and b + sector <= 0xFC000, "candidate A/B avoid autosave and global storage")
check(sector - 0x100 == 0xF00 and "0xF00..0xF07" in bsp, "reserve the full final 256 bytes against update-record scanning")
guard_line = next((line for line in storage_hw.splitlines() if line.startswith("#define ST_OK")), "")
check("FL_IN(off, n, ST_VBANK_LO, ST_VBANK_HI)" in guard_line and "FL_XDATA_OK" not in guard_line and "FL_XDATA1" not in guard_line, "production storage guard still blocks candidate writes")

print("SIDECAR FLASH MAP SOURCE AUDIT: PASS — candidate only, no allocation or device writes")
