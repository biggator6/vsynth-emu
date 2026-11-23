#!/usr/bin/env python3
"""vc2vsynth.py: the slot question asked of rom.2, in V-Synth mode, beside the VC-2 runs (HARDWARE_FIRMWARE.md, "rom.4's
outside input": whether an EXT IN OSC follows External Input Type at MONO-L and MONO-R, as v204's mapping of the
returned words says).  The same input as the VC-2 runs, into an EXT IN OSC on fxcapture.py's patch and map, out at
MAIN.  vc2capture.py writes it as vc2_vsynth.mid, its takes in vc2_takes.csv; the XT must be in V-Synth mode for it."""
import fxcapture
from capsysex import addr_add
from vc2takes import MONO_L, MONO_LR, MONO_R, STEREO, take

EXT_TYPE, EXT_SOURCE = 0x3f, 0x43     # rom.2's External In Type (0x78) and Source (0x7c) in the system block, from 0x39
EXTIN = 2                             # rom.2's OSC Type EXTIN
TYPES = {STEREO: 'STEREO', MONO_LR: 'MONO-L+R', MONO_L: 'MONO-L', MONO_R: 'MONO-R'}


def takes():
    t = [take('silence', 'the floor', key=None, probe=None)]
    for name, ext in (('main', STEREO), ('main_again', STEREO), ('vs_monolr', MONO_LR), ('vs_monol', MONO_L),
                      ('vs_monor', MONO_R)):
        t.append(take(name, 'the EXT IN OSC at External In Type %s: which input side reaches each output' % TYPES[ext],
                      {'External In Type': ext}, probe='fx'))
    return t


class VsRun(fxcapture.Writer):
    """fxcapture's patch with OSC 1 an EXT IN OSC out at MAIN, the system's input Analog."""
    def apply_take(self, t):
        if t is None:
            self.setup()
            self.sx(addr_add(fxcapture.ZONE, fxcapture.Z_TYPE), [EXTIN])
            self.sx(addr_add(fxcapture.ZONE, fxcapture.Z_ASSIGN), [fxcapture.ASSIGN['MAIN']])
            self.sx(addr_add(fxcapture.SYSTEM, EXT_SOURCE), [0])
            return True
        ext = t['set'].get('External In Type', MONO_LR)
        if self.state.get('ext') == ext:
            return False
        self.sx(addr_add(fxcapture.SYSTEM, EXT_TYPE), [ext])
        self.state['ext'] = ext
        return False


def settings(t):
    """A take in the panel's words: V-Synth mode, OSC 1 Type EXT IN, Output Assign MAIN, External In Source Analog."""
    if t['key'] is None:
        return 'V-Synth mode; play nothing; the input silent'
    return 'V-Synth mode; OSC 1 Type EXT IN; Output Assign MAIN; External In Type %s; hold C4' % TYPES[
        t['set']['External In Type']]
