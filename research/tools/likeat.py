#!/usr/bin/env python3
"""likeat.py <image A> <hex addr> <count> <image B> [hex lo] [hex hi]: where image B has code shaped like image A's
<count> instructions at addr, with what moves when code moves masked out: PC-relative loads' displacements (mov.w, mov.l,
mova), bra/bsr offsets and bt/bf targets. Both images map at 0x89300000. rom.4 is compiled differently from rom.2, and
rom.2's tone routines find no match in it (research/VC2_VOCAL_DESIGNER.md), so a miss here says the code isn't a copy,
not that the routine isn't there."""
import struct
import sys

from romfile import B


def load(path):
    with open(path, 'rb') as f:
        return f.read()


def shape(op):
    if op >> 12 in (0x9, 0xD):
        return op & 0xff00
    if op >> 12 in (0xA, 0xB):
        return op & 0xf000
    if op >> 8 in (0x89, 0x8B, 0x8D, 0x8F, 0xC7):
        return op & 0xff00
    return op


a, at, count, b = load(sys.argv[1]), int(sys.argv[2], 16), int(sys.argv[3]), load(sys.argv[4])
lo = int(sys.argv[5], 16) - B if len(sys.argv) > 5 else 0
hi = int(sys.argv[6], 16) - B if len(sys.argv) > 6 else len(b) - 2 * count
want = [shape(struct.unpack_from('<H', a, at - B + 2 * k)[0]) for k in range(count)]
hits = [B + o for o in range(lo, hi, 2)
        if all(shape(struct.unpack_from('<H', b, o + 2 * k)[0]) == want[k] for k in range(count))]
print(len(hits), 'hits:', ' '.join(hex(h) for h in hits[:20]))
