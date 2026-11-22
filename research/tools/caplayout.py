#!/usr/bin/env python3
"""caplayout.py: the test wave's layout, which every cutter reads a take by (fxprobe.wav, and vc2probe.wav's first 16.8
s): where its mark, its one-sided clicks, its sweep, its 1 kHz steps and its noise are, and the levels a check calls the
same.  Split from capbase.py as it stood; capbase.py gives them on."""
import fxcapture

WAVE_FS = fxcapture.FS
CUT_LEAD = 0.25                       # seconds kept before a take's note-on
MARK, CLICK_L, CLICK_R = 0.5, 2.0, 4.0
SWEEP, STEPS, NOISE = (6.0, 11.0), (12.5, 13.8), (14.8, 15.8)
STEP_DB = [-72 + 6 * i for i in range(13)]
LOW_STEPS = (-48, -42)                # the 1 kHz steps a take's gain is read on: clear of clipping in `hotter` (+24 dB) even at a path of +9.4 dB
SAME_DB = 0.05                        # below this, two levels are the same level: a setting did nothing
