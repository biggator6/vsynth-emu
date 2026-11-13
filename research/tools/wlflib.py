#!/usr/bin/env python3
"""wlflib.py: the disk image's WLF08 wave libraries, the V-Synth's (VSYN2) and the VC-2's (VC2), each read whole and held to
what it points at (research/VC2_VOCAL_DESIGNER.md, "The wave libraries, read").

A library: "WLF08", a section count at +8, two version strings at +0x10 and +0x20, and a 16-byte entry a section from +0x40
(u16 number, u16 1, u32 offset from the library, u32 size). A section: u32 count, u32 the index's offset (0xc), u32 an
entry's size: 8 is an index of (u32 offset from the section, u32 size), anything else inline entries of that size.
Section 1, the waves: a 16-character name, then 4 bytes a split (upper velocity, a byte, u16 map number). Section 2, the maps:
a name, then 6 bytes a split (upper key, level, a u16 0 or 12, u16 sample number). Section 3, the samples: 36-byte paths
"VPW\\<file>.vpw". Numbers count from 1.

Checks: the sections follow each other; every number in range and every entry used; each split list rising to key 127;
each library's waves against the link files that name them (a "link" chunk, the wave's number, and a "name" chunk); the
VC-2's samples against its sample bank ('"TABLE"', an ASCII count, 60-byte entries of a 52-byte name, u32 size and u32
offset from the end of '"DATA"', which follows them, then the streams); the V-Synth's against the streams that start on a
sector. Prints what it found and every difference; exits 1 when there is one. $DISK picks the image (diskls.py).

wlflib.py get <VSYN2|VC2> <out>: the library's bytes, as the firmware's "D:\\... Wave.bin".
wlflib.py choices <VSYN2|VC2>: each wave's split at every velocity and each map's at every key by the rule the
firmware's readers are read to keep (the first split whose upper byte is at or above the value), in the form
`vc2run <rom.4> splits` prints the firmware's own."""
import re
import struct
import sys
from dataclasses import dataclass

import diskls

img = diskls.img
SECTOR = 512


@dataclass
class Library:
    at: int
    tag: str
    versions: tuple
    sections: list
    waves: list   # (name, [(upper velocity, byte, map number)])
    maps: list    # (name, [(upper key, level, a word 0 or 12, sample number)])
    paths: list


def partition_end():
    total = struct.unpack_from('<H', img, diskls.start + 19)[0] or struct.unpack_from('<I', img, diskls.start + 32)[0]
    return diskls.start + total * SECTOR


def entries(at):
    count, index, size = struct.unpack_from('<III', img, at)
    if size == 8:
        pairs = (struct.unpack_from('<II', img, at + index + 8 * k) for k in range(count))
        return [img[at + o:at + o + n] for o, n in pairs]
    return [img[at + index + size * k:at + index + size * (k + 1)] for k in range(count)]


def name_of(record):
    return record[:16].decode('latin1').rstrip()


def splits(record, width, fmt):
    return [struct.unpack_from(fmt, record, 16 + width * k) for k in range((len(record) - 16) // width)]


def library(at):
    count = struct.unpack_from('<I', img, at + 8)[0]
    sections = [struct.unpack_from('<HHII', img, at + 0x40 + 16 * k) for k in range(count)]
    waves, maps, paths = (entries(at + s[2]) for s in sections)
    versions = (img[at + 0x10:at + 0x20].decode('latin1'), img[at + 0x20:at + 0x30].decode('latin1'))
    tag = 'VSYN2' if 'VSYN2' in versions[0] else 'VC2'
    return Library(at, tag, versions, sections, [(name_of(r), splits(r, 4, '<BBH')) for r in waves],
                   [(name_of(r), splits(r, 6, '<BBHH')) for r in maps],
                   [p.split(b'\0')[0].decode('latin1') for p in paths])


def numbers_problems(what, lists, count, at_field):
    """Numbers out of 1..count, entries never used, and split lists not rising to 127."""
    out, used = [], set()
    for name, entry in lists:
        keys = [s[0] for s in entry]
        if keys != sorted(keys) or not keys or keys[-1] != 127:
            out.append(f'{what} {name!r}: split keys {keys}')
        for s in entry:
            n = s[at_field]
            used.add(n)
            if not 1 <= n <= count:
                out.append(f'{what} {name!r}: number {n} outside 1..{count}')
    unused = sorted(set(range(1, count + 1)) - used)
    if unused:
        out.append(f'{what}: {len(unused)} entries never used, first {unused[:5]}')
    return out


def link_files(folder):
    """The link files in a folder: (wave number, name), from their "link" and "name" chunks."""
    out = []
    for path, cluster, size in diskls.all_files():
        if path.startswith(folder) and size:
            f = diskls.read(cluster, size)
            if f[0:4] == b'link' and f[12:16] == b'name':
                out.append((path, struct.unpack_from('<I', f, 8)[0], f[20:36].decode('latin1').rstrip()))
    return out


def links_problems(lib, folder):
    files = link_files(folder)
    out = [f'{path}: wave {n} is {lib.waves[n - 1][0]!r}, the file names {name!r}'
           for path, n, name in files if not 1 <= n <= len(lib.waves) or lib.waves[n - 1][0] != name]
    named = len({n for _, n, _ in files})
    print(f'  link files in {folder}: {len(files)}, naming {named} of the {len(lib.waves)} waves')
    return out


def streams():
    return [m.start() for m in re.finditer(b'vpfl', img) if m.start() >= partition_end()]


def vc2_bank_problems(lib):
    t = img.find(b'"TABLE"', partition_end())
    count = int(img[t + 7:t + 11])
    table = [(img[t + 11 + 60 * k:t + 63 + 60 * k].split(b'\0')[0].decode('latin1'),
              *struct.unpack_from('<II', img, t + 63 + 60 * k)) for k in range(count)]
    data = t + 11 + 60 * count
    out = [] if img[data:data + 6] == b'"DATA"' else [f'no "DATA" at {data:#x}']
    base = data + 6
    starts = {base + offset for _, _, offset in table}
    out += [f'bank entry {n!r}: no stream at its offset'
            for n, _, o in table if img[base + o:base + o + 4] != b'vpfl']
    ordered = sorted(table, key=lambda e: e[2])
    out += [f'bank entry {b[0]!r} starts at {b[2]}, not {a[2] + a[1]}'
            for a, b in zip(ordered, ordered[1:]) if b[2] != a[2] + a[1]]
    names = {n for n, _, _ in table}
    out += [f'sample {p!r} is not in the bank' for p in lib.paths if p.split('\\')[-1] not in names]
    unaligned = {s for s in streams() if s % SECTOR}
    if starts != unaligned:
        out.append(f'the bank\'s {len(starts)} streams are not the {len(unaligned)} that start off a sector')
    print(f'  sample bank: "TABLE" at {t:#x}, {count} entries, "DATA" at {data:#x}, streams {base:#x}…'
          f'{base + ordered[-1][2] + ordered[-1][1]:#x}; {len(starts & unaligned)} of its streams start off a sector')
    return out


def vsynth_bank_problems(lib):
    aligned = [s for s in streams() if s % SECTOR == 0]
    print(f'  streams on a sector: {len(aligned)}, {aligned[0]:#x}…{aligned[-1]:#x}, against {len(lib.paths)} samples')
    return [] if len(aligned) == len(lib.paths) else [f'{len(aligned)} streams on a sector, {len(lib.paths)} samples']


# Each library's link files and its samples' bank, by its version tag
KINDS = {
    'VC2': ('/Internal.vc2/Wave/', vc2_bank_problems),
    'VSYN2': ('/Internal.prj/Wave/', vsynth_bank_problems),
}


def check(lib):
    print(f'WLF08 at {lib.at:#x}: {lib.versions[0]!r}, {lib.versions[1]!r}; sections '
          + ', '.join(f'{s[0]} at +{s[2]:#x} ({s[3]:#x} bytes)' for s in lib.sections))
    print(f'  {len(lib.waves)} waves ({sum(len(s) for _, s in lib.waves)} splits), {len(lib.maps)} maps '
          f'({sum(len(s) for _, s in lib.maps)} splits), {len(lib.paths)} samples')
    out = [f'section {a[0]} ends at {a[2] + a[3]:#x}, section {b[0]} starts at {b[2]:#x}'
           for a, b in zip(lib.sections, lib.sections[1:]) if a[2] + a[3] != b[2]]
    out += numbers_problems('wave', lib.waves, len(lib.maps), 2)
    out += numbers_problems('map', lib.maps, len(lib.paths), 3)
    folder, bank = KINDS[lib.tag]
    out += links_problems(lib, folder)
    out += bank(lib)
    for p in out:
        print('  differs:', p)
    print(f'  {len(out)} differences')
    return out


def libraries():
    return [library(m.start()) for m in re.finditer(b'WLF08', img) if m.start() >= partition_end()]


def library_bytes(lib):
    """The library's own bytes, from its header to the end of its last section, as the firmware reads the file."""
    return img[lib.at:lib.at + max(s[2] + s[3] for s in lib.sections)]


def chosen(split_list, value):
    """The first split whose upper byte is at or above the value, or None: the rule rom.4's readers are read to keep."""
    return next((s for s in split_list if value <= s[0]), None)


def say_ranges(what, number, name, split_list, carried):
    """One wave's or map's choice at each value 0..127, consecutive values with one result as a range."""
    runs = []
    for value in range(128):
        s = chosen(split_list, value)
        result = carried(s) if s else 'none'
        if runs and runs[-1][2] == result:
            runs[-1][1] = value
        else:
            runs.append([value, value, result])
    print(f'{what} {number:3d} {name:<16} ' + ', '.join(f'{lo}-{hi} {r}' for lo, hi, r in runs))


def say_choices(lib):
    """What the rule gives for each wave at every velocity and each map at every key, in vc2run splits' form."""
    for k, (name, s) in enumerate(lib.waves, 1):
        say_ranges('wave', k, name, s, lambda x: f'map {x[2]} (byte {x[1]})')
    for k, (name, s) in enumerate(lib.maps, 1):
        say_ranges('map', k, name, s, lambda x: f'sample {x[3]} (byte {x[1]}, {x[2] & 0xff})')


def main():
    if len(sys.argv) > 2 and sys.argv[1] in ('get', 'choices'):
        lib = next((x for x in libraries() if x.tag == sys.argv[2]), None)
        if lib is None:
            sys.exit(f'wlflib.py: no library {sys.argv[2]} (VSYN2 or VC2)')
        if sys.argv[1] == 'choices':
            say_choices(lib)
            return
        with open(sys.argv[3], 'wb') as f:
            f.write(library_bytes(lib))
        return
    starts = libraries()
    problems = [p for lib in starts for p in check(lib)]
    sys.exit(1 if problems or not starts else 0)


if __name__ == '__main__':
    main()
