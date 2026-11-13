#!/usr/bin/env python3
"""codealign.py <rom.2> <rom.4> <lo2> <hi2> <lo4> <hi4>: the routines of a region of rom.2 lined up with a region of rom.4
by their order and sizes, for code rom.4 compiled anew whose shape codepairs.py can't match (research/VC2_MODE_PLAN.md,
step 1b; the way COSM's note-on and update were paired). A routine starts at a 16-aligned prologue instruction (a register
pushed on r15, pr or macl saved, r15 lowered) after nop padding; leaf routines without one merge into the routine before.
The two lists of sizes are matched as sequences, so a routine one image adds or drops doesn't shift the rest.

A line a pair: rom.2's start and size, rom.4's start and size, '=' when the sizes agree, '~' when paired by place in a run
of the same length whose sizes don't, and '-' with a routine one image alone has. Read only; both images are the
user's.

codealign.py --near <rom.2> <rom.4> <distance> <a2> ...: the longest alike run near where each routine should be."""
import difflib
import struct
import sys

from codepairs import same_run
from romfile import B, image, option

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
    rom2, rom4, distance = image(argv[0]), image(argv[1]), int(argv[2], 16)
    for a2 in (int(a, 16) for a in argv[3:]):
        at = a2 - distance
        n, a4 = max((same_run(rom2, a2, rom4, x, 4000), x) for x in range(at - NEAR_SPAN, at + NEAR_SPAN, 2))
        print(f'{a2:#x}: rom.4 {a4:#x} (distance {a2 - a4:#x}), {n} instructions alike')


def main(argv):
    if option(argv, {'--near': near}):
        return
    if len(argv) < 6:
        sys.exit(__doc__)
    rom2, rom4 = image(argv[0]), image(argv[1])
    lo2, hi2, lo4, hi4 = (int(a, 16) for a in argv[2:6])
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
