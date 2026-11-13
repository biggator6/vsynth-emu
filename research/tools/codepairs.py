#!/usr/bin/env python3
"""codepairs.py <rom.2> <rom.4> [hex addr ...]: rom.2's routines in rom.4 by the shape of their code (likeat.py's shape:
what moves when code moves masked out), the pairs the VC-2 mode's step 1b runs side by side (research/VC2_MODE_PLAN.md).
codepairs.py --pools <rom.2> <rom.4> <a2> <a4> [cap]: for one pair, the literals that differ: what moved between them.
With no addresses, every address in plugin/Source's sources in rom.2's code (0x89300000…0x89540000): the routines the
ports' firmware checks call and hook.

A line an address: its name, rom.2 address, how many instructions were compared (up to MAX_INSTRS, or to the first rts and
its delay slot), and where rom.4 has them: one address and the distance (rom.2 − rom.4), several, or none. A summary counts
the distances: a block of code moved whole keeps one. Short routines (under MIN_INSTRS) are matched but marked, as a few
instructions are found by chance. Read only; both images are the user's, never committed."""
import collections
import re
import struct
import sys
from pathlib import Path

from likeat import shape, shapes
from romfile import B, image

SOURCE = Path(__file__).resolve().parents[2] / 'plugin/Source'
LITERAL = re.compile(r'\b(?:(k\w+)\s*=\s*)?(0x89[0-9a-fA-F]{6})u?\b')
CODE_LO, CODE_HI = 0x89300000, 0x89540000   # rom.2's routines (its tables from 0x89788000; a few small ones inside)
INDEX_HI = 0x89530000                        # rom.4's code searched up to here
KEY = 6                                      # instructions in the index's key
MAX_INSTRS, MIN_INSTRS = 32, 10
RTS = 0x000b


def source_addresses():
    """(name, address) for each rom.2 code address written in plugin/Source, its constant's name or its file."""
    seen = {}
    for path in sorted(SOURCE.glob('*.*')):
        for name, value in LITERAL.findall(path.read_text(errors='replace')):
            addr = int(value, 16)
            if CODE_LO <= addr < CODE_HI:
                seen.setdefault(addr, f'{name} ({path.name})' if name else path.name)
    return sorted((name, addr) for addr, name in seen.items())


def length(img, addr):
    """Instructions from addr to the first rts and its delay slot, at most MAX_INSTRS."""
    for k in range(MAX_INSTRS):
        if struct.unpack_from('<H', img, addr - B + 2 * k)[0] == RTS:
            return min(k + 2, MAX_INSTRS)
    return MAX_INSTRS


def index(img):
    """rom.4's code by the shapes of KEY instructions from each halfword: key → file offsets."""
    halves = struct.unpack_from(f'<{(INDEX_HI - B) // 2}H', img, 0)
    shaped = [shape(h) for h in halves]
    found = collections.defaultdict(list)
    for o in range(len(shaped) - KEY):
        found[tuple(shaped[o:o + KEY])].append(o)
    return shaped, found


def matches(rom2, addr, n, shaped4, keyed):
    want = shapes(rom2, addr, n)
    return [B + 2 * o for o in keyed.get(tuple(want[:KEY]), []) if shaped4[o:o + n] == want]


def same_run(rom2, a2, rom4, a4, cap):
    """Instructions from a2 and a4 shaped alike, at most cap."""
    n = 0
    while n < cap and shape(struct.unpack_from('<H', rom2, a2 - B + 2 * n)[0]) == \
            shape(struct.unpack_from('<H', rom4, a4 - B + 2 * n)[0]):
        n += 1
    return n


def pool_word(img, pc, op):
    """The literal a mov.l (0xD) or mov.w (0x9) at pc loads."""
    if op >> 12 == 0xD:
        return struct.unpack_from('<I', img, (pc & ~3) + 4 + (op & 0xff) * 4 - B)[0]
    return struct.unpack_from('<H', img, pc + 4 + (op & 0xff) * 2 - B)[0]


def pool_pairs(rom2, a2, rom4, a4, n):
    """(rom.2 literal, rom.4 literal) where n alike instructions from a2 and a4 load different literals: the map of what
    moved (globals, callees, tables) between the two images, as each routine sees it."""
    pairs = {}
    for k in range(n):
        op = struct.unpack_from('<H', rom2, a2 - B + 2 * k)[0]
        if op >> 12 in (0x9, 0xD):
            v2 = pool_word(rom2, a2 + 2 * k, op)
            v4 = pool_word(rom4, a4 + 2 * k, struct.unpack_from('<H', rom4, a4 - B + 2 * k)[0])
            if v2 != v4:
                pairs[v2] = v4
    return pairs


def pools(argv):
    """--pools <rom.2> <rom.4> <a2> <a4> [cap]: the literals that differ between two routines shaped alike."""
    rom2, rom4, a2, a4 = image(argv[0]), image(argv[1]), int(argv[2], 16), int(argv[3], 16)
    n = same_run(rom2, a2, rom4, a4, int(argv[4]) if len(argv) > 4 else 2000)
    print(f'{a2:#x} / {a4:#x}: {n} instructions alike')
    for v2, v4 in sorted(pool_pairs(rom2, a2, rom4, a4, n).items()):
        print(f'    {v2:#010x} -> {v4:#010x}')


def main(argv):
    if argv[:1] == ['--pools']:
        pools(argv[1:])
        return
    if len(argv) < 2:
        sys.exit(__doc__)
    rom2, rom4 = image(argv[0]), image(argv[1])
    targets = [(f'{int(a, 16):#x}', int(a, 16)) for a in argv[2:]] or source_addresses()
    shaped4, keyed = index(rom4)
    distances = collections.Counter()
    tally = collections.Counter()
    for name, addr in targets:
        n = length(rom2, addr)
        if n < KEY:
            tally['too short to key'] += 1
            print(f'{name} {addr:#x}: {n} instructions, too short to look for')
            continue
        hits = matches(rom2, addr, n, shaped4, keyed)
        short = ' (short: may be chance)' if n < MIN_INSTRS else ''
        if len(hits) == 1:
            distances[addr - hits[0]] += 1
            tally['one'] += 1
            print(f'{name} {addr:#x}: {n} instrs, rom.4 {hits[0]:#x}, distance {addr - hits[0]:#x}{short}')
        else:
            tally['several' if hits else 'none'] += 1
            print(f'{name} {addr:#x}: {n} instrs, rom.4 ' + (' '.join(hex(h) for h in hits[:3]) or 'none') + short)
    print('summary:', ', '.join(f'{k} {v}' for k, v in sorted(tally.items())))
    print('distances:', ', '.join(f'{d:#x} ×{c}' for d, c in distances.most_common(12)))


if __name__ == '__main__':
    main(sys.argv[1:])
