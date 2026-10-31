# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).parents[1] / "tools" / "fm1_readonly_probe.py"
spec = importlib.util.spec_from_file_location("fm1_readonly_probe", MODULE_PATH)
probe = importlib.util.module_from_spec(spec)
spec.loader.exec_module(probe)


class ProbeCodecTests(unittest.TestCase):
    def test_v14_signed_values(self):
        for value in (-8192, -1, 0, 1, 8191):
            encoded = value + 8192
            self.assertEqual(probe.decode_v14(encoded & 127, encoded >> 7), value)

    def test_info_reply(self):
        payload = [ord(c) for c in "FELUCCA 1.7.0"] + [0, 14, 111, 13, 64, 103]
        for name in ["ANALOG", "FM6"]:
            payload.extend(ord(c) for c in name)
            payload.append(0)
        payload.append(4)
        info = probe.parse_info(payload)
        self.assertEqual(info["version"], "FELUCCA 1.7.0")
        self.assertEqual(info["steps_per_track"], 64)
        self.assertEqual(info["track_count"], 4)

    def test_track_reply(self):
        # selected=1, two tracks; each track has engine, preset, signed level, mute, armed
        payload = [1, 2, 0, 3, 0, 64, 0, 0, 4, 5, 0, 64, 1, 1]
        result = probe.parse_track(payload)
        self.assertEqual(result["selected_track"], 1)
        self.assertEqual(result["tracks"][1]["mute"], True)
        self.assertEqual(result["tracks"][1]["armed"], True)

    def test_step_reply(self):
        # track 0, step 7, one note C3 (48), NOTE, accent, velocity 127,
        # no drum hits, chance 100, ratchet 1, nudge 0, no condition.
        payload = [0, 7, 1, 48, 0, 0, 0, 0, 1, 127, 0, 0, 0, 100, 1, 8, 0]
        step = probe.parse_step(payload)
        self.assertEqual(step["notes"], [48])
        self.assertTrue(step["accent"])
        self.assertEqual(step["nudge_sixteenths"], 0)
        self.assertEqual(step["condition"], 0)

    def test_step_rejects_bad_length(self):
        with self.assertRaises(ValueError):
            probe.parse_step([0, 0, 0])

    def test_safety_guard(self):
        class NeverUsed:
            def send(self, _message):
                raise AssertionError("must not send a message")
        p = probe.ReadOnlyProbe(None, NeverUsed(), 1.0)
        with self.assertRaises(ValueError):
            p.request(3, [0, 0, 0, 0])


if __name__ == "__main__":
    unittest.main()
