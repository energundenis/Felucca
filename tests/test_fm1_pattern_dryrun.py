# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).parents[1] / "tools" / "fm1_pattern_dryrun.py"
spec = importlib.util.spec_from_file_location("fm1_pattern_dryrun", MODULE_PATH)
dryrun = importlib.util.module_from_spec(spec)
spec.loader.exec_module(dryrun)


def pattern(length=8):
    steps = []
    for i in range(length):
        steps.append({
            "index": i, "on": i in (0, 3), "note": 0, "octave": 0,
            "midi": 60, "accent": i == 3, "gate": 65 + i
        })
    return {
        "schema": "energun.fm1.pattern", "schemaVersion": 1,
        "generatorVersion": "0.3", "patternLength": length,
        "rootNote": 0, "scale": "minor",
        "settings": {"accentAmount": 0.45},
        "steps": steps
    }


class DryRunTests(unittest.TestCase):
    def test_full_64_step_plan_and_tail_rests(self):
        plan = dryrun.build_dry_run(pattern(), 2, "velocity")
        frames = plan["commands"]["frames"]
        self.assertEqual(len(frames), 64)
        self.assertEqual(frames[0][4], 30)
        self.assertEqual(frames[1][7], 0)
        self.assertEqual(frames[8][7], 0)
        self.assertFalse(plan["transmissionEnabled"])

    def test_scale_mapping_and_gate_summary(self):
        p = pattern()
        p["scale"] = "phrygian"
        plan = dryrun.build_dry_run(p, 0, "velocity")
        self.assertEqual(plan["trackParameterPlan"][2]["value"], 8)
        self.assertEqual(plan["gate"]["minimum"], 65)
        self.assertEqual(plan["gate"]["maximum"], 68)
        self.assertEqual(plan["gate"]["medianCandidate"], 66.5)
        self.assertIsNone(plan["gate"]["trackParameterWrite"])

    def test_accent_velocity_mode(self):
        plan = dryrun.build_dry_run(pattern(), 0, "velocity")
        accented = plan["commands"]["frames"][3]
        self.assertEqual(accented[14], 101)
        self.assertEqual(accented[13], 0)

    def test_accent_flag_mode(self):
        plan = dryrun.build_dry_run(pattern(), 0, "flag")
        accented = plan["commands"]["frames"][3]
        self.assertEqual(accented[13], 1)

    def test_rejects_bad_midi_pitch(self):
        p = pattern()
        p["steps"][0]["midi"] = 128
        with self.assertRaises(ValueError):
            dryrun.build_dry_run(p, 0, "velocity")

    def test_rejects_unknown_scale(self):
        p = pattern()
        p["scale"] = "unknown"
        with self.assertRaises(ValueError):
            dryrun.build_dry_run(p, 0, "velocity")


if __name__ == "__main__":
    unittest.main()
