#!/usr/bin/env python3
"""poolrefs.py [--16] <hex value>...: code in rom.2 that loads each value from a
literal pool (mov.l @(disp,PC),Rn; with --16, mov.w for 16-bit values in the
voice code, 0x894d0000..0x89540000), and any raw words equal to it (tables,
vtables).  Values built from immediates (mov #k; shll8; add) are not found.
loads(img, value) is the same search on any image (romtables.py's)."""
import struct
import sys

from romfile import rom, B


def loads(img, t):
    """(code addresses that mov.l t from a literal pool, addresses of the aligned words equal to t) in image img."""
    pat, hits, words, i = struct.pack('<I', t), [], [], img.find(struct.pack('<I', t))
    while i >= 0:
        if i % 4 == 0:
            words.append(B + i)
            for pc in range(max(B, B + i - 1020), B + i, 2):
                op = struct.unpack_from('<H', img, pc - B)[0]
                if op >> 12 == 0xD and (pc & ~3) + 4 + (op & 0xff) * 4 == B + i:
                    hits.append(pc)
        i = img.find(pat, i + 1)
    return hits, words


def loads16(img, t):
    """Voice code (0x894d0000..0x89540000) that mov.w's the 16-bit t from a literal pool."""
    hits = []
    for pc in range(0x894d0000 - B, 0x89540000 - B, 2):
        op = struct.unpack_from('<H', img, pc)[0]
        if op >> 12 == 0x9 and struct.unpack_from('<H', img, pc + 4 + (op & 0xff) * 2)[0] == t:
            hits.append(B + pc)
    return hits


if __name__ == '__main__':
    args = sys.argv[1:]
    w16 = '--16' in args
    for t in (int(a, 16) for a in args if a != '--16'):
        if w16:
            print(hex(t), 'mov.w at', ' '.join(hex(h) for h in loads16(rom, t)))
            continue
        hits, words = loads(rom, t)
        print(hex(t), 'mov.l at', ' '.join(hex(h) for h in hits), '| words at', ' '.join(hex(w) for w in words))
