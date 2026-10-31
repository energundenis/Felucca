# ENERGUN METAL CORE — CP2.8 host note-source provenance validation

Date: 2026-10-31
Branch: `energun-metal-core`

## CI evidence

- Workflow run: https://github.com/energundenis/Felucca/actions/runs/41208854407
- Tested source commit: `2b3d1484e7b69c7d0df81ff438e5a895cf01c91f`
- Conclusion: success
- Firmware build: success
- Host/web regression: `ALL HOST TESTS PASSED`

All 11 source-provenance cases passed:
1. live/performance note marks its poly voice;
2. sequencer note marks its own poly voice;
3. MONO stack keeps source beside each held note;
4. LEGATO fallback restores the older performance source;
5. same-pitch MONO retrigger updates active source;
6. real `seq_step()` path marks pattern note as sequencer;
7. keyboard/MIDI `input_on()` path marks performance note;
8. stopped-transport step preview is sequencer source;
9. arpeggiator output remains performance source;
10. engine-switch queue stores source with pending note;
11. queued note-off compares pitch without source bit.

## Status

CP2.8 host provenance implementation is validated. This does not mean FOLLOW is connected to the Metal Core render callback; render integration and pitch behavior remain later gates. The CI workflow does not flash the FM-1.
