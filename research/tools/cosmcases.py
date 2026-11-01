#!/usr/bin/env python3
"""cosmcases.py <code>…: for a COSM block code (the converter's +0, 0…20), where
the voice's COSM object goes for it: the DSP routine (the mono table's initial
image at 0x89858278, the stereo table's at 0x898582cc; .data 0x8987c7ec and
0x8987c840), and the cases of the per-code switches:
  binding   0x894f7330 (block offsets of slot 1's and 2's envelopes, then LFOs)
  note-on   0x894edf70 (after the common part and the block copy)
  start     0x894ec340 (start values, indexed by code − 1)
  block     0x894ee4a0 (the block copy, also a new block while the note sounds)
  update    0x894ee6a0 (the 10-ms update, indexed by code − 1)
  alloc     0x894ecb00 (delay buffers: codes 7…18 that reach 0x894ecb78 get four)
A case at the switch's shared exit has none of its own.  Block copies that jump
on (mov.l/jmp or bra) are followed one step."""
import struct, sys, os, re
rom = open(os.environ.get('ROM', os.path.join(os.path.dirname(__file__), '../../firmware/unpacked/V-SynthXT.rom.2.bin')), 'rb').read()
B = 0x89300000
def u16(a): return struct.unpack_from('<H', rom, a - B)[0]
def s16(a): return struct.unpack_from('<h', rom, a - B)[0]
def u32(a): return struct.unpack_from('<I', rom, a - B)[0]
def case(braf, tab, i): return braf + 4 + s16(tab + 2 * i)
def follow(a):                                   # mov.l @(d,PC),r3; jmp @r3, or bra
    w = u16(a)
    if (w & 0xf000) == 0xa000:                   # bra
        d = w & 0xfff
        return a + 4 + 2 * (d - 0x1000 if d & 0x800 else d)
    if (w & 0xff00) == 0xd300 and u16(a + 2) == 0x432b:
        return u32(((a + 4) & ~3) + 4 * (w & 0xff))
    return a
for c in (int(x, 0) for x in sys.argv[1:]):
    print('code %d:' % c)
    print('  DSP     mono 0x%08x  stereo 0x%08x' % (u32(0x89858278 + 4 * c), u32(0x898582cc + 4 * c)))
    print('  binding 0x%08x' % case(0x894f7358, 0x894f735c, c))
    print('  note-on 0x%08x%s' % (case(0x894ee0fa, 0x894ee118, c), '  (none)' if case(0x894ee0fa, 0x894ee118, c) == 0x894ee446 else ''))
    if c >= 1:
        s = case(0x894ec360, 0x894ec380, c - 1); print('  start   0x%08x%s' % (s, '  (none)' if s == 0x894ecaf6 else ''))
        u = case(0x894ee72c, 0x894ee764, c - 1); print('  update  0x%08x%s' % (u, '  (none)' if u == 0x894eef66 else ''))
    b = case(0x894ee4bc, 0x894ee4c0, c); print('  block   0x%08x → 0x%08x' % (b, follow(b)))
    a = case(0x894ecb5a, 0x894ecb60, c - 7) if 7 <= c <= 18 else None
    print('  alloc   %s' % ('four buffers' if a == 0x894ecb78 else 'none'))
