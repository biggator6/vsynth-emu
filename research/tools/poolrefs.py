#!/usr/bin/env python3
"""poolrefs.py [--16] <hex value>...: code in rom.2 that loads each value from a
literal pool (mov.l @(disp,PC),Rn; with --16, mov.w for 16-bit values in the
voice code, 0x894d0000..0x89540000), and any raw words equal to it (tables,
vtables).  Values built from immediates (mov #k; shll8; add) are not found."""
import struct, sys, os
rom = open(os.environ.get('ROM', os.path.join(os.path.dirname(__file__), '../../firmware/unpacked/V-SynthXT.rom.2.bin')), 'rb').read()
B = 0x89300000
args = sys.argv[1:]
w16 = '--16' in args
for t in (int(a, 16) for a in args if a != '--16'):
    hits = []
    if w16:
        for pc in range(0x894d0000 - B, 0x89540000 - B, 2):
            op = struct.unpack_from('<H', rom, pc)[0]
            if op >> 12 == 0x9 and struct.unpack_from('<H', rom, pc + 4 + (op & 0xff) * 2)[0] == t: hits.append(B + pc)
        print(hex(t), 'mov.w at', ' '.join(hex(h) for h in hits)); continue
    pat, words, i = struct.pack('<I', t), [], rom.find(struct.pack('<I', t))
    while i >= 0:
        if i % 4 == 0:
            words.append(B + i)
            for pc in range(max(B, B + i - 1020), B + i, 2):
                op = struct.unpack_from('<H', rom, pc - B)[0]
                if op >> 12 == 0xD and (pc & ~3) + 4 + (op & 0xff) * 4 == B + i: hits.append(pc)
        i = rom.find(pat, i + 1)
    print(hex(t), 'mov.l at', ' '.join(hex(h) for h in hits), '| words at', ' '.join(hex(w) for w in words))
