#!/usr/bin/env python3
# mutate_midi.py [cases] [name ...]: every deliberate error in midi_muts.txt (or the named ones), each planted by
# mutate.sh in XtMidiEdits.cpp (PORT=midi, v462) and run through xt_midi on [cases] cases (200), each to its first case
# that differs (XT_MIDI_STOP).  Its runner is mutlist's, as mutate_msm.py's.
import sys
import mutlist
XT_MIDI = mutlist.Harness('mutate_midi', 'xt_midi', 'midi_muts.txt', ('midi', '2024'), 'XT_MIDI_STOP')
sys.exit(mutlist.main(XT_MIDI, '200', sys.argv[1:]))
