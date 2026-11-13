#!/usr/bin/env python3
"""likeat.py <image A> <hex addr> <count> <image B> [hex lo] [hex hi]: where image B has code shaped like image A's
<count> instructions at addr, with what moves when code moves masked out: PC-relative loads' displacements (mov.w, mov.l,
mova), bra/bsr offsets and bt/bf targets. Both images map at 0x89300000. A miss says the code isn't a copy, not that the
routine isn't there (rom.4 compiles some of rom.2's routines anew; research/VC2_VOCAL_DESIGNER.md). shape() and shapes()
are codepairs.py's too."""
import struct
import sys

from romfile import B, image


def shape(op):
    if op >> 12 in (0x9, 0xD):
        return op & 0xff00
    if op >> 12 in (0xA, 0xB):
        return op & 0xf000
    if op >> 8 in (0x89, 0x8B, 0x8D, 0x8F, 0xC7):
        return op & 0xff00
    return op


def shapes(img, addr, count):
    """The shapes of count instructions at CPU address addr."""
    return [shape(struct.unpack_from('<H', img, addr - B + 2 * k)[0]) for k in range(count)]


def find(a, at, count, b, lo=0, hi=None):
    """CPU addresses in b where count instructions are shaped as a's at `at`, searched over file offsets [lo, hi)."""
    hi = len(b) - 2 * count if hi is None else hi
    want = shapes(a, at, count)
    return [B + o for o in range(lo, hi, 2)
            if all(shape(struct.unpack_from('<H', b, o + 2 * k)[0]) == want[k] for k in range(count))]


if __name__ == '__main__':
    a, at, count, b = image(sys.argv[1]), int(sys.argv[2], 16), int(sys.argv[3]), image(sys.argv[4])
    lo = int(sys.argv[5], 16) - B if len(sys.argv) > 5 else 0
    hi = int(sys.argv[6], 16) - B if len(sys.argv) > 6 else None
    hits = find(a, at, count, b, lo, hi)
    print(len(hits), 'hits:', ' '.join(hex(h) for h in hits[:20]))
