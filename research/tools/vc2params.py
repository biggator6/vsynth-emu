#!/usr/bin/env python3
"""vc2params.py <lo> <hi> (hex), with ROM=firmware/unpacked/V-SynthXT.rom.4.bin: rom.4's (the VC-2's) parameters by ID,
each its name, its descriptor in rom.2's 12-byte form (byte offset, width, bit, centre, default, min, max) and its value
text (names split by 1, a range by 2, the unit after a 3). rom.4's IDs are its own: from 0x37 they are not rom.2's
(pnames.py). The tables (research/VC2_VOCAL_DESIGNER.md, "The parameter tables"): names from a pointer to " Vocoder Sw"'s
string walked both ways; descriptors where rom.2's first 48 occur byte for byte; text from the first pointer after the
names to ID 0's string."""
import struct
import sys

from romfile import BSS, B, rom, u16, u32

NAMES, DESCRIPTORS, TEXT = 0x898b5798, 0x8950f0b0, 0x898b8154
ROM4_BSS = 0x8994fc20   # rom.4's header word +0x1c, so another image is refused rather than misread
if BSS != ROM4_BSS:
    sys.exit('vc2params.py reads rom.4: set ROM=firmware/unpacked/V-SynthXT.rom.4.bin')


def raw(a):
    o = a - B
    return rom[o:rom.index(b'\0', o)]


def text(t):
    s, _, unit = t.decode('latin1').partition('\x03')
    if '\x02' in s:
        return f'range {s.replace(chr(2), "..")} {unit}'.rstrip()
    return ' | '.join(s.split('\x01')) + (f' [{unit}]' if unit else '')


for i in range(int(sys.argv[1], 16), int(sys.argv[2], 16) + 1):
    d = DESCRIPTORS + 12 * i
    width = struct.unpack_from('<b', rom, d + 2 - B)[0]
    centre, default, low, high = (u16(d + 4 + 2 * k) for k in range(4))
    name = raw(u32(NAMES + 4 * i)).decode('latin1')
    print(f'{i:03x} {name:28s} @{u16(d):#06x} w{width:<3d} b{rom[d + 3 - B]} centre {centre:<4d} default {default:<4d} '
          f'{low}..{high}   {text(raw(u32(TEXT + 4 * i)))}')
