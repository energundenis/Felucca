#!/usr/bin/env python3
"""Read-only identity check for the immutable M-VAVE FM-1 stock V15 artifact."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path
import sys

EXPECTED_SIZE = 699_956
EXPECTED_SHA256 = "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a"


def fingerprint(path: Path) -> tuple[int, str]:
    digest = hashlib.sha256()
    size = 0
    with path.open("rb") as stream:
        while chunk := stream.read(1024 * 1024):
            size += len(chunk)
            digest.update(chunk)
    return size, digest.hexdigest()


def verify(path: Path) -> tuple[bool, str]:
    if not path.is_file():
        return False, f"File not found: {path}"
    size, digest = fingerprint(path)
    if size != EXPECTED_SIZE:
        return False, f"Size mismatch: expected {EXPECTED_SIZE}, got {size}"
    if digest != EXPECTED_SHA256:
        return False, f"SHA-256 mismatch: expected {EXPECTED_SHA256}, got {digest}"
    return True, "Exact stock V15 reference match"


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Verify an FM-1 stock V15 file by exact size and SHA-256."
    )
    parser.add_argument("file", type=Path, help="Path to the locally held FM-1.fwsc")
    args = parser.parse_args()

    ok, message = verify(args.file)
    print(f"STOCK_V15_REFERENCE={'PASS' if ok else 'FAIL'}")
    print(message)
    if ok:
        print(f"size={EXPECTED_SIZE}")
        print(f"sha256={EXPECTED_SHA256}")
        return 0
    return 1


if __name__ == "__main__":
    sys.exit(main())
