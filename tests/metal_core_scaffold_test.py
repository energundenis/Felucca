#!/usr/bin/env python3
"""Guard test for the intentionally unregistered ENERGUN METAL CORE scaffold.

Run from the repository root:
    python3 tests/metal_core_scaffold_test.py

This is an integration-boundary test, not an audio/DSP test. It ensures the
unfinished scaffold cannot silently enter the firmware engine table.
"""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
ENGINE = ROOT / "firmware/src/eng_metal_core.c"
TABLE = ROOT / "firmware/src/engines.c"

def fail(message: str) -> None:
    print(f"FAIL: {message}")
    raise SystemExit(1)

if not ENGINE.is_file():
    fail("firmware/src/eng_metal_core.c is missing")

source = ENGINE.read_text(encoding="utf-8")
table = TABLE.read_text(encoding="utf-8")

required = (
    "isolated integration scaffold",
    "FOLLOW and METAL requirements",
    "OFF preserves the associated amount/value",
    "No functions or symbols are exported here yet",
)
for marker in required:
    if marker not in source:
        fail(f"scaffold contract marker missing: {marker}")

if re.search(r'^\s*#include\s+["<].*eng_metal_core\.c[">]', table, re.M):
    fail("unfinished scaffold is included by engines.c")

if re.search(r'\bENG_METAL_CORE\b|\bENGI_METAL_CORE\b', table):
    fail("unfinished scaffold has been registered in the engine table")

print("PASS: METAL CORE scaffold exists and remains isolated/unregistered.")
print("NOTE: no DSP, audio output, UI, engine ID, or firmware behavior is tested.")
