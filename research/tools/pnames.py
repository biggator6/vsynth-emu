#!/usr/bin/env python3
"""pnames.py <lo> <hi>: the parameters with IDs lo…hi (hex): name (pointers from
0x89790338) and, for IDs ≥ 0x55d, the second descriptor bank's offset, default,
min and max (8 bytes per ID from 0x897a56d8).  A COSM type's patch values sit at
+0x12, +0x14, … in the converter's input, in ID order (the IDs per type: the
table at 0x897cebd8, or HARDWARE_FIRMWARE.md's type map)."""
import struct, sys, os
rom = open(os.environ.get('ROM', os.path.join(os.path.dirname(__file__), '../../firmware/unpacked/V-SynthXT.rom.2.bin')), 'rb').read()
B = 0x89300000
def u32(a): return struct.unpack_from('<I', rom, a - B)[0]
def s16(a): return struct.unpack_from('<h', rom, a - B)[0]
def cstr(a):
    o = a - B; return rom[o:rom.index(b'\0', o)].decode('latin1')
lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
for i in range(lo, hi + 1):
    p = u32(0x89790338 + 4 * i)
    name = cstr(p) if B <= p < B + len(rom) else '?%08x' % p
    if i >= 0x55d:
        d = [s16(0x897a56d8 + 8 * (i - 0x55d) + 2 * j) for j in range(4)]
        print('%03x  +0x%02x  %-30s default %4d  range %d…%d' % (i, 0x12 + 2 * (i - lo), name, d[1], d[2], d[3]))
    else:
        print('%03x  %s' % (i, name))
