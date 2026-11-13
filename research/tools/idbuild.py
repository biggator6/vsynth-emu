#!/usr/bin/env python3
"""idbuild.py <lo> <hi> [from] [to] (hex): each place a register's value comes to lie in [lo, hi], by a linear pass that
follows constants: mov #imm, PC-relative mov.w/mov.l, shll/shll2/shll8/shll16, add #imm, add Rm,Rn, mov Rm,Rn, exts.w.
The firmware builds a parameter ID in registers (mov #2; shll8; add #-97 is 0x19f) before passing it to a getter in r5,
so a search for the ID as a literal misses it. Linear only: a branch target isn't modelled, a call forgets r0-r7, a
return or jump forgets all, and any other write to a register forgets it. $ROM picks the image (romfile.py)."""
import struct
import sys

from romfile import B, rom

lo, hi = int(sys.argv[1], 16), int(sys.argv[2], 16)
start = int(sys.argv[3], 16) - B if len(sys.argv) > 3 else 0
end = int(sys.argv[4], 16) - B if len(sys.argv) > 4 else len(rom) - 2
SHIFTS = {0x00: 1, 0x08: 2, 0x18: 8, 0x28: 16}
known = {}


def put(n, value, at, why):
    if value is None:
        known.pop(n, None)
        return
    value &= 0xffffffff
    known[n] = value
    signed = value - (1 << 32) if value & 0x80000000 else value
    if lo <= signed <= hi:
        print(f'{B + at:#x} r{n}={signed:#05x} {why}')


def literal(fmt, at):
    return struct.unpack_from(fmt, rom, at)[0] if at + struct.calcsize(fmt) <= len(rom) else None


for o in range(start, end, 2):
    op = struct.unpack_from('<H', rom, o)[0]
    top, n, m, imm = op >> 12, (op >> 8) & 15, (op >> 4) & 15, op & 0xff
    simm = imm - 256 if imm & 0x80 else imm
    if top == 0xE:
        put(n, simm, o, 'mov #')
    elif top == 0x9:
        put(n, literal('<h', o + 4 + imm * 2), o, 'mov.w')
    elif top == 0xD:
        put(n, literal('<I', ((o + 4) & ~3) + imm * 4), o, 'mov.l')
    elif top == 0x7:
        put(n, known[n] + simm if n in known else None, o, 'add #')
    elif top == 0x4 and imm in SHIFTS:
        put(n, known[n] << SHIFTS[imm] if n in known else None, o, f'shll{SHIFTS[imm]}')
    elif top == 0x6 and op & 15 == 0x3:
        put(n, known.get(m), o, f'mov r{m}')
    elif top == 0x6 and op & 15 == 0xF:
        v = known.get(m)
        put(n, None if v is None else (v & 0xffff) - (0x10000 if v & 0x8000 else 0), o, f'exts.w r{m}')
    elif top == 0x3 and op & 15 == 0xC:
        put(n, known[n] + known[m] if n in known and m in known else None, o, f'add r{m}')
    elif op == 0x000B or top in (0xA, 0xB) or (top == 0x4 and imm in (0x0B, 0x2B)):
        for k in range(8):
            known.pop(k, None)
        if op == 0x000B or top == 0xA or (top == 0x4 and imm == 0x2B):
            known.clear()
    elif top in (0x3, 0x4, 0x5, 0x6) or (top == 0x0 and op & 15 in (0x9, 0xA, 0xC, 0xD, 0xE)):
        known.pop(n, None)
    elif top in (0x8, 0xC):
        known.pop(0, None)
