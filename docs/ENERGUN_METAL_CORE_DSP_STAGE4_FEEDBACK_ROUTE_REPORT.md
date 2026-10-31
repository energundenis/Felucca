# ENERGUN METAL CORE — DSP stage 4: v98 wcMod feedback route

The preserved v98 source routes wcMod through a 3 ms delay and a gain of FM_FEEDBACK * FREQ * 3 back to wcMod.frequency. The direct FM path from wcMod to wcOsc.frequency is separate. Feedback therefore changes the modulator frequency, not OSC1 directly.

Implementation remains pending until the isolated fixed-point route and its tests are committed and pass CI. No engine registration, firmware build, or hardware write is implied.
