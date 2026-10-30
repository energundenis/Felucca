/* SPDX-License-Identifier: GPL-3.0-only
 * ENERGUN METAL CORE — isolated integration scaffold.
 *
 * This file is intentionally NOT included by engines.c yet.
 * It defines the implementation boundary before engine IDs, UI pages, or DSP
 * are changed. Do not register a silent placeholder as a playable engine.
 *
 * Integration contract:
 *   - Use Felucca's track_t / voice_t / vmod_t / engine_t and allocator.
 *   - Initial voice mode: monophonic, selected through the existing track mode.
 *   - Render adds to out[]; it must never overwrite the host mix.
 *   - Stereo is supplied by the host's existing part/mix path.
 *   - Any persistent per-part state must be declared in engine_t.mem and fit
 *     ENG_MEM_PART (checked at registration with ENG_MEM_FITS).
 *   - Any per-voice state must use only the documented voice_t state slots,
 *     respecting note_on retrigger/keep semantics.
 *   - DSP must be fixed-point, bounded per CTL block, and measured on target.
 *
 * Parameter mapping is deliberately pending. P_E0..P_E7 are the eight
 * engine edit values; do not assign FOLLOW/METAL switches to these slots
 * until the control/value-retention design is finalized.
 *
 * FOLLOW and METAL requirements:
 *   - independent ON/OFF controls on the FOLD page;
 *   - OFF bypasses only the corresponding processing stage;
 *   - OFF preserves the associated amount/value;
 *   - ON restores processing using the retained amount/value.
 *
 * Implementation stages:
 *   1. agree parameter map and signal-flow diagram;
 *   2. implement oscillator / FM / fold / LPG in isolated functions;
 *   3. implement note lifecycle and block rendering;
 *   4. add host regression tests and CPU baseline;
 *   5. register at the append-only engine ID and add UI pages.
 *
 * No functions or symbols are exported here yet: this prevents an incomplete
 * engine from accidentally entering a firmware build.
 */
