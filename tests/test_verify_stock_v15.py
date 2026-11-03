#!/usr/bin/env python3
"""Unit tests for the stock V15 reference verifier."""

from pathlib import Path
import hashlib
import tempfile
import unittest
from unittest.mock import patch

import verify_stock_v15


class StockV15VerifierTests(unittest.TestCase):
    def test_exact_reference_metadata_is_frozen(self):
        self.assertEqual(verify_stock_v15.EXPECTED_SIZE, 699_956)
        self.assertEqual(
            verify_stock_v15.EXPECTED_SHA256,
            "db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a",
        )

    def test_accepts_exact_fingerprint(self):
        fixture = b"test fixture"
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "FM-1.fwsc"
            path.write_bytes(fixture)
            with patch.object(verify_stock_v15, "EXPECTED_SIZE", len(fixture)), patch.object(
                verify_stock_v15,
                "EXPECTED_SHA256",
                hashlib.sha256(fixture).hexdigest(),
            ):
                ok, _ = verify_stock_v15.verify(path)
            self.assertTrue(ok)

    def test_rejects_wrong_size(self):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "wrong-size.fwsc"
            path.write_bytes(b"not the reference")
            ok, message = verify_stock_v15.verify(path)
            self.assertFalse(ok)
            self.assertIn("Size mismatch", message)

    def test_rejects_missing_file(self):
        path = Path("/definitely/not/a/real/FM-1.fwsc")
        ok, message = verify_stock_v15.verify(path)
        self.assertFalse(ok)
        self.assertIn("File not found", message)


if __name__ == "__main__":
    unittest.main()
