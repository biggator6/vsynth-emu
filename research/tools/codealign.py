#!/usr/bin/env python3
"""codealign.py <rom.2> <rom.4> <lo2> <hi2> <lo4> <hi4>: the routines of a region of rom.2 lined up with a region of rom.4
by their order and sizes, for code rom.4 compiled anew whose shape codepairs.py can't match (research/VC2_MODE_PLAN.md,
step 1b; the way COSM's note-on and update were paired). A routine starts at a 16-aligned prologue instruction (a register
pushed on r15, pr or macl saved, r15 lowered) after nop padding; leaf routines without one merge into the routine before.
The two lists of sizes are matched as sequences, so a routine one image adds or drops doesn't shift the rest.

A line a pair: rom.2's start and size, rom.4's start and size, '=' when the sizes agree, '~' when paired by place in a run
of the same length whose sizes don't, and '-' with a routine one image alone has. Read only; both images are the
user's.

codealign.py --near <rom.2> <rom.4> <distance> <a2> ...: the longest alike run near where each routine should be.
codealign.py --similar <rom.2> <rom.4> <a2> <a4> <n2> <n4>: the share of n2 instructions from a2 and n4 from a4 alike in
order (likeat.py's shape, difflib's ratio), for a pair compiled anew: 1.0 the same, near 0 unrelated code.
codealign.py --fields <rom.2> <rom.4> <a2> <a4> <n2> <n4>: the same with the values masked too (field offsets, immediates,
literals), and the values that differ where the two line up, counted: an object's field map, read from the code."""
import collections
import difflib
import struct
import sys

from codepairs import pool_word, same_run
from likeat import shape, shapes
from romfile import B, images, option

NOP = 0x0009
ALIGN = 16


def prologue(op):
    return (op & 0xff0f) == 0x2f06 or op in (0x4f22, 0x4f12) or (op & 0xff0f) == 0xff0b or op >> 8 == 0x7f


def half(img, a):
    return struct.unpack_from('<H', img, a - B)[0]


def starts(img, lo, hi):
    """(start, size) of each routine in [lo, hi), the last one's size to hi."""
    found = [a for a in range(lo - lo % ALIGN, hi, ALIGN) if prologue(half(img, a)) and half(img, a - 2) == NOP]
    return [(a, (found[i + 1] if i + 1 < len(found) else hi) - a) for i, a in enumerate(found)]


def say(left, right, mark):
    l = f'{left[0]:#x} {left[1]:#6x}' if left else ' ' * 17
    r = f'{right[0]:#x} {right[1]:#6x}' if right else ''
    print(f'{l}  {mark}  {r}')


NEAR_SPAN = 0x400   # bytes either side of the expected place that --near searches


def near(argv):
    """--near <rom.2> <rom.4> <distance> <a2> ...: for each rom.2 routine, the rom.4 address within NEAR_SPAN of
    a2 − distance where the longest run of alike instructions starts (codepairs.py's same_run), and the run's length: how a
    routine moved with its neighbours is found when its first instructions alone don't match."""
    rom2, rom4, rest = images(argv)
    distance = int(rest[0], 16)
    for a2 in (int(a, 16) for a in rest[1:]):
        at = a2 - distance
        n, a4 = max((same_run(rom2, a2, rom4, x, 4000), x) for x in range(at - NEAR_SPAN, at + NEAR_SPAN, 2))
        print(f'{a2:#x}: rom.4 {a4:#x} (distance {a2 - a4:#x}), {n} instructions alike')


def similar(argv):
    """--similar <rom.2> <rom.4> <a2> <a4> <n2> <n4>: the share of two routines' instructions matching in order."""
    rom2, rom4, rest = images(argv)
    a2, a4, n2, n4 = (int(a, 16) for a in rest[:4])
    alike = difflib.SequenceMatcher(a=shapes(rom2, a2, n2), b=shapes(rom4, a4, n4), autojunk=False).ratio()
    print(f'{a2:#x} ({n2:#x} instructions) / {a4:#x} ({n4:#x}): {alike:.3f} alike')


def sx8(v):
    return v - 256 if v & 0x80 else v


# An instruction whose value --fields masks: (opcode mask, opcode, the value's bits, kind, the value). Offsets are in bytes.
VALUES = [
    (0xf000, 0x5000, 0x000f, 'offset', lambda img, pc, op: (op & 0xf) * 4),     # mov.l @(disp,Rm),Rn
    (0xf000, 0x1000, 0x000f, 'offset', lambda img, pc, op: (op & 0xf) * 4),     # mov.l Rm,@(disp,Rn)
    (0xfe00, 0x8400, 0x000f, 'offset', lambda img, pc, op: (op & 0xf) << (op >> 8 & 1)),   # mov.b/w @(disp,Rm),R0
    (0xfe00, 0x8000, 0x000f, 'offset', lambda img, pc, op: (op & 0xf) << (op >> 8 & 1)),   # mov.b/w R0,@(disp,Rn)
    (0xf000, 0xe000, 0x00ff, 'immediate', lambda img, pc, op: sx8(op & 0xff)),  # mov #imm,Rn
    (0xf000, 0x7000, 0x00ff, 'immediate', lambda img, pc, op: sx8(op & 0xff)),  # add #imm,Rn
    (0xf000, 0xd000, 0x00ff, 'literal', pool_word),                             # mov.l @(disp,pc),Rn
    (0xf000, 0x9000, 0x00ff, 'literal', pool_word),                             # mov.w @(disp,pc),Rn
]


def value_row(op):
    return next((row for row in VALUES if op & row[0] == row[1]), None)


def field_shape(op):
    row = value_row(op)
    return op & ~row[2] if row else shape(op)


def field_shapes(img, a, n):
    return [field_shape(half(img, a + 2 * k)) for k in range(n)]


def changed_values(img2, a2, img4, a4, pairs):
    """For lined-up instruction indexes (k2, k4): (kind, rom.2's value, rom.4's) where the values differ, with the
    instructions' addresses in each image."""
    out = collections.defaultdict(list)
    for k2, k4 in pairs:
        op2, op4 = half(img2, a2 + 2 * k2), half(img4, a4 + 2 * k4)
        row = value_row(op2)
        if row is None:
            continue
        v2, v4 = row[4](img2, a2 + 2 * k2, op2), row[4](img4, a4 + 2 * k4, op4)
        if v2 != v4:
            out[(row[3], v2, v4)].append((a2 + 2 * k2, a4 + 2 * k4))
    return out


def fields(argv):
    """--fields <rom.2> <rom.4> <a2> <a4> <n2> <n4>: two routines lined up by their field shapes; the share alike,
    and the values that differ where they line up (an offset moved, a constant changed, a callee or global moved)."""
    rom2, rom4, rest = images(argv)
    a2, a4, n2, n4 = (int(a, 16) for a in rest[:4])
    m = difflib.SequenceMatcher(a=field_shapes(rom2, a2, n2), b=field_shapes(rom4, a4, n4), autojunk=False)
    pairs = [(i + k, j + k) for i, j, n in m.get_matching_blocks() for k in range(n)]
    print(f'{a2:#x} ({n2:#x} instructions) / {a4:#x} ({n4:#x}): {m.ratio():.3f} alike by field shape')
    for (kind, v2, v4), at in sorted(changed_values(rom2, a2, rom4, a4, pairs).items(), key=str):
        print(f'    {kind} {v2:#x} -> {v4:#x} ×{len(at)} (first {at[0][0]:#x} / {at[0][1]:#x})')


def main(argv):
    if option(argv, {'--near': near, '--similar': similar, '--fields': fields}):
        return
    if len(argv) < 6:
        sys.exit(__doc__)
    rom2, rom4, rest = images(argv)
    lo2, hi2, lo4, hi4 = (int(a, 16) for a in rest[:4])
    s2, s4 = starts(rom2, lo2, hi2), starts(rom4, lo4, hi4)
    m = difflib.SequenceMatcher(a=[n for _, n in s2], b=[n for _, n in s4], autojunk=False)
    for op, i1, i2, j1, j2 in m.get_opcodes():
        if op == 'equal' or (op == 'replace' and i2 - i1 == j2 - j1):
            for k in range(i2 - i1):
                say(s2[i1 + k], s4[j1 + k], '=' if op == 'equal' else '~')
            continue
        for k in range(i1, i2):
            say(s2[k], None, '-')
        for k in range(j1, j2):
            say(None, s4[k], '-')


if __name__ == '__main__':
    main(sys.argv[1:])
