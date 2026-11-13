#!/usr/bin/env python3
"""romtables.py <rom.2> <rom.4> [--window N] [hex addr ...]: rom.2's tables found in rom.4 by their bytes, and the code in
each image that loads them: a map from the V-Synth's firmware routines to the VC-2's, since the two are compiled
differently and their code can't be matched (research/VC2_VOCAL_DESIGNER.md, "The vocoder's code"). With no addresses,
every address constant in plugin/Source's ports (kName = 0x89......) that the CPU reads from rom.2's image or .data.

A line a constant: its name and rom.2 address, then for each copy of its first N bytes (default 32) in rom.4 at the same
alignment: how many bytes are equal from there (up to 64 KB), rom.4's address as the CPU sees it (.data through its
copy), and the code that loads it in each image (poolrefs.py's loads). "plain" when the window has too few distinct
bytes to mean anything; "not in rom.4" when no copy is found. Read only; both images are the user's, never committed."""
import re
import sys
from pathlib import Path

from poolrefs import loads
from romfile import B, image, offsets, sections

SOURCE = Path(__file__).resolve().parents[2] / 'plugin/Source'
CONSTANT = re.compile(r'\b(k\w+)\s*=\s*(0x89[0-9a-fA-F]{6})u?\b')
MIN_DISTINCT = 6        # a window with fewer distinct bytes is a run of zeros or a ramp, found anywhere
MAX_MATCH = 0x10000
MAX_COPIES = 4
MAX_LOADERS = 6


def port_constants():
    """(name, address) for each address constant in plugin/Source, first name kept for an address."""
    seen = {}
    for path in sorted(SOURCE.glob('Xt*.*')):
        for name, value in CONSTANT.findall(path.read_text(errors='replace')):
            seen.setdefault(int(value, 16), f'{name} ({path.name})')
    return sorted((name, addr) for addr, name in seen.items())


def cpu_address(img, off):
    """The CPU address code uses for file offset off: a .data source byte by its RAM copy, the rest in place."""
    _, _, data, data_from = sections(img)
    return off + B + (data - data_from) if data_from - B <= off < data - B else off + B


def copies(img, pattern, align):
    """File offsets in img where pattern starts, at the given alignment mod 4."""
    found, i = [], img.find(pattern)
    while i >= 0:
        if i % 4 == align:
            found.append(i)
        i = img.find(pattern, i + 1)
    return found


def equal_run(a, a_off, b, b_off):
    n = 0
    while n < MAX_MATCH and a_off + n < len(a) and b_off + n < len(b) and a[a_off + n] == b[b_off + n]:
        n += 1
    return n


def loaders(img, addr):
    hits, _ = loads(img, addr)
    shown = ' '.join(hex(h) for h in hits[:MAX_LOADERS])
    return shown + (f' (+{len(hits) - MAX_LOADERS})' if len(hits) > MAX_LOADERS else '') or '-'


def report(rom2, rom4, name, addr, window):
    off = offsets(rom2, addr)
    if off is None:
        return f'{name} {addr:#x}: .bss or past the image'
    pattern = rom2[off:off + window]
    if len(set(pattern)) < MIN_DISTINCT:
        return f'{name} {addr:#x}: plain'
    found = copies(rom4, pattern, off % 4)
    if not found:
        return f'{name} {addr:#x}: not in rom.4 | rom.2 loaders {loaders(rom2, addr)}'
    lines = [f'{name} {addr:#x}: rom.2 loaders {loaders(rom2, addr)}']
    for at in found[:MAX_COPIES]:
        at4 = cpu_address(rom4, at)
        lines.append(f'    rom.4 {at4:#x}, {equal_run(rom2, off, rom4, at)} bytes equal | loaders {loaders(rom4, at4)}')
    if len(found) > MAX_COPIES:
        lines.append(f'    and {len(found) - MAX_COPIES} more copies')
    return '\n'.join(lines)


def main(argv):
    if len(argv) < 2:
        sys.exit(__doc__)
    rom2, rom4 = image(argv[0]), image(argv[1])
    rest = argv[2:]
    window = 32
    if rest[:1] == ['--window']:
        window, rest = int(rest[1]), rest[2:]
    targets = [(f'{int(a, 16):#x}', int(a, 16)) for a in rest] or port_constants()
    for name, addr in targets:
        print(report(rom2, rom4, name, addr, window))


if __name__ == '__main__':
    main(sys.argv[1:])
