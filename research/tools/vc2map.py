#!/usr/bin/env python3
"""vc2map.py [--rom4 FILE] [--list] [--check-run FILE]: rom.4's own SysEx map, read from the user's rom.4 at run time
(HARDWARE_FIRMWARE.md, "rom.4's SysEx: the VC-2's own address map"; names, descriptors and value text as
VC2_VOCAL_DESIGNER.md, "The parameter tables").

In VC-2 mode the XT takes DT1 and RQ1 of model 00 00 0D (0x20d in the model table 0x8950cfd0), at its Device ID or 7F,
and sends edit data and RQ1 answers with it; the V-Synth's 00 53 is parsed and dropped.  The DT1 handler (0x8931c1b0)
finds each address byte's entry in an address table (.data, 40 bytes an entry): its value's width in bytes and this
byte's place in it, the Category condition, and the (group, ID) pairs it writes, a value named by its last byte's
entry.  The tables' places and the blocks' addresses are the handler's; their contents are read here.  An entry with
a condition writes only under the Patch Categories (ID 0x12c) its test (0x8931c5d0) passes, decoded from that
routine's own jump tables.  What no entry names has no address: most of the vocoder block and the tone (the list).

--list prints the map; --check-run holds it to `vc2run <rom.4> sysex`'s output (rom.4's own parser and setters, run),
the second source, address by address and Category by Category.  This map replaces one inferred from the V-Synth's,
withdrawn (VC2_MODE_PLAN.md, step 5)."""
import argparse, os, pathlib, re, struct

from capsysex import ROM_BASE, VC_2, Rom, addr_add
from romfile import image, offsets

ROM4_BSS = 0x8994fc20                 # rom.4's header word +0x1c: another image is refused rather than misread
NAMES4, DESC4, TEXT4, COUNT4 = 0x898b5798, 0x8950f0b0, 0x898b8154, 0x9ac
TYPES4 = {'MFX Type': 0x895164ec, 'Chorus Type': 0x895165b8, 'Reverb Type': 0x895165e4}
MODEL = VC_2                          # [FW] 0x20d: three bytes, two zeros counted (0x89310640)
# [FW] The handler's blocks: title, first address, its address table, the .data word with its count of entries, how
# many such blocks a b2 apart (the MIC settings', 03 00 0i 00), by 0x8931c1b0's tests and the counts' maker 0x89316a40
BLOCKS = [('setup', [0x01, 0, 0, 0], 0x8992e408, 0x899343d0, 1),
          ('system', [0x02, 0, 0, 0], 0x8992e5e8, 0x899343d4, 1),
          ('MIC setting', [0x03, 0, 0, 0], 0x8992fc90, 0x899343d8, 8),
          ('common', [0x10, 0, 0, 0], 0x89930118, 0x899343dc, 1),
          ('common', [0x10, 0, 3, 0], 0x899334d0, 0x899343e0, 1),
          ('common', [0x10, 0, 4, 0], 0x89933f48, 0x899343e4, 1),
          ('common', [0x10, 0, 5, 0], 0x899341a0, 0x899343e8, 1)]
STORED, STORED_COUNT, STORED_FIRST = [0x20, 0, 0, 0], 448, 65   # [FW] stored patch k at 20 00 00 00 + k b1's
ENTRY = 40                            # [FW] bytes an address table entry
CATEGORY_TEST, CATEGORIES = 0x8931c5d0, 12   # [FW] (Category, condition); Patch Category's range 0…11
GROUPS = {1: 'setup', 2: 'system', 3: 'system', 4: 'MIC', 5: 'common', 6: 'common', 7: 'MFX', 8: 'Chorus',
          9: 'Reverb', 10: 'env', 11: 'OSC', 12: 'COSM', 13: 'env', 14: 'OSC', 15: 'COSM'}   # 13…15 zone 2's


def names(rom, table, count):
    out = []
    for i in range(count):
        p = struct.unpack_from('<I', rom.b, table + 4 * i - ROM_BASE)[0] - ROM_BASE
        out.append(rom.b[p:rom.b.index(b'\0', p)].decode('latin1').strip())
    return out


class Code:
    """rom.4's bytes as the CPU reads them after C startup (.data through its copy)."""
    def __init__(self, b):
        self.b = b
    def at(self, a, n): return self.b[offsets(self.b, a):offsets(self.b, a) + n]
    def u16(self, a): return struct.unpack('<H', self.at(a, 2))[0]
    def s16(self, a): return struct.unpack('<h', self.at(a, 2))[0]
    def u32(self, a): return struct.unpack('<I', self.at(a, 4))[0]
    def switch(self, mova):
        """A switch's table and the base its braf adds to: mova @(table),r0; mov.w @(r0,rN),r0; braf r0."""
        op = self.u16(mova)
        if op >> 8 != 0xc7 or self.u16(mova + 2) & 0xf0ff != 0x002d or self.u16(mova + 4) != 0x0023:
            raise SystemExit('%#x is not the switch the Category test was read with' % mova)
        return (mova & ~3) + 4 + 4 * (op & 0xff), mova + 8


def verdict(code, a):
    """What a case of the Category test answers: rts with r0 1 or 0, through any bra."""
    for _ in range(4):
        op = code.u16(a)
        if op >> 12 == 0xa:
            a += 4 + 2 * ((op & 0xfff) - (0x1000 if op & 0x800 else 0))
        elif op == 0x000b and code.u16(a + 2) in (0xe000, 0xe001):
            return code.u16(a + 2) == 0xe001
        else:
            break
    raise SystemExit('the Category test at %#x is not as read' % a)


def category_law(code):
    """{condition: the Categories 0…11 its entries write under}, from 0x8931c5d0: condition 0 every Category, a
    Category past 11 none, else a case per Category, each a switch over the condition less its first."""
    table, base = code.switch(CATEGORY_TEST + 0x18)
    law = {}
    for c in range(CATEGORIES):
        case = base + code.s16(table + 2 * c)
        add, top = code.u16(case), code.u16(case + 2)
        if add >> 8 != 0x75 or top >> 8 != 0xe7:
            raise SystemExit('Category %d\'s case at %#x is not as read' % (c, case))
        first, last = -((add & 0xff) - 256 if add & 0x80 else add & 0xff), top & 0xff
        mova = next(a for a in range(case, case + 0x20, 2) if code.u16(a) >> 8 == 0xc7)
        cases, at = code.switch(mova)
        for cond in range(1, 16):
            k = cond - first
            if 0 <= k <= last and verdict(code, at + code.s16(cases + 2 * k)):
                law.setdefault(cond, set()).add(c)
    return law


class Entry:
    """An address byte's entry: width, place, condition, its (group, ID) pairs."""
    def __init__(self, raw):
        self.width, self.place, _, self.condition = raw[0], raw[1], raw[2], raw[3]
        n = struct.unpack_from('<I', raw, 4)[0]
        self.pairs = [struct.unpack_from('<II', raw, 8 + 8 * k) for k in range(n)]


class Map:
    """rom.4's IDs by name and address, each one's descriptor and text, and its types' typed defaults."""
    def __init__(self, rom4_path):
        self.rom = Rom(rom4_path, bank=0x895131a8, first=0x56a)
        if struct.unpack_from('<I', self.rom.b, 0x1c)[0] != ROM4_BSS:
            raise SystemExit('%s is not rom.4 (the VC-2): its header says otherwise' % rom4_path)
        code = Code(image(rom4_path))
        self.names = names(self.rom, NAMES4, COUNT4)
        self.law = category_law(code)
        self.values = []              # (address, group, ID, width, Categories, title), a value by its first byte
        for title, base, table, count, repeats in BLOCKS:
            raw = [code.at(table + ENTRY * k, ENTRY) for k in range(code.u32(count))]
            for r in range(repeats):
                self.add_block(title + (' %d' % (r + 1) if repeats > 1 else ''), addr_add(base, 128 * r), raw)

    def add_block(self, title, base, raw):
        entries = [Entry(e) for e in raw]
        for k, e in enumerate(entries):
            if e.place:
                continue
            last = entries[k + e.width - 1]
            cats = set(range(CATEGORIES)) | {'past'} if e.condition == 0 else self.law.get(e.condition, set())
            for (group, _), (_, named) in zip(e.pairs, last.pairs):
                self.values.append((addr_add(base, k), group, named, e.width, cats, title))

    def addresses(self, i, zone=1):
        """Where ID i is received: the temporary patch's, the system's, MIC setting 1's (a zoned ID's in that zone)."""
        def here(v):
            if v[2] != i or (v[5].startswith('MIC setting') and v[5] != 'MIC setting 1'):
                return False
            return v[1] < 10 or (v[1] >= 13) == (zone == 2)
        return [v for v in self.values if here(v)]

    def address(self, i, zone=1):
        a = self.addresses(i, zone)
        if len(a) != 1:
            raise SystemExit('rom.4 has %d SysEx addresses for %#x %s (zone %d)' % (len(a), i, self.names[i], zone))
        return a[0][0]

    def id_of(self, name):
        """The one ID with this name that has an address."""
        ids = sorted({v[2] for v in self.values if self.names[v[2]] == name})
        if len(ids) != 1:
            raise SystemExit('rom.4 has %d SysEx-addressed IDs named %r' % (len(ids), name))
        return ids[0]

    def descriptor(self, i):
        """(centre, default, min, max): the first bank's, 12 bytes an ID."""
        return tuple(struct.unpack_from('<H', self.rom.b, DESC4 + 12 * i + 4 + 2 * k - ROM_BASE)[0] for k in range(4))

    def text(self, i, v):
        """A value in the panel's words: an item of its list, or its range's low end counted on, with its unit."""
        p = struct.unpack_from('<I', self.rom.b, TEXT4 + 4 * i - ROM_BASE)[0] - ROM_BASE
        s, _, unit = self.rom.b[p:self.rom.b.index(b'\0', p)].decode('latin1').partition('\x03')
        _, _, lo, _ = self.descriptor(i)
        if '\x02' in s:
            low = s.split('\x02')[0].strip()
            signed = low.startswith(('-', '+'))
            word = (('%+d' if signed else '%d') % (int(low) + v - lo)) if low.lstrip('+-').isdigit() else str(v)
        else:
            items = s.split('\x01')
            word = items[v - lo] if 0 <= v - lo < len(items) and items[v - lo] else str(v)
        return word + (' ' + unit.strip('[]') if unit else '')

    def typed(self, type_name, t):
        """A type's typed defaults: [(address, nibbles' value)], each slot at its Parameter's address."""
        head = type_name[:-len(' Type')]
        return [(self.address(self.id_of('%s Parameter %d' % (head, slot + 1))), v + 32768)
                for slot, v in sorted(self.rom.params(TYPES4[type_name], t).items())]

    def named(self, address):
        """The ID a DT1 at this address sets first (a value's first byte), or None: what --learn says of the XT's."""
        a = list(address)
        hit = [v[2] for v in self.values if v[0] == a]
        if hit or a[0] < STORED[0] or a[0] >= STORED[0] + 4:
            return hit[0] if hit else None
        k = (a[0] - STORED[0]) * 128 + a[1]
        return self.named([0x10, 0, a[2], a[3]]) if k < STORED_COUNT else None


def categories_text(cats):
    if 'past' in cats:
        return 'all'
    return '0-11' if cats == set(range(CATEGORIES)) else ','.join(str(c) for c in sorted(cats))


def listing(m):
    for a, group, i, width, cats, title in sorted(m.values, key=lambda v: (v[0], v[1])):
        zone = ' zone %d' % (2 if group >= 13 else 1) if group >= 10 else ''
        print('%s  %-13s g%-2d %-6s%-7s %#05x %-30s %d byte%s  Categories %s' % (
            ' '.join('%02x' % b for b in a), title, group, GROUPS[group], zone, i, m.names[i], width,
            's' if width > 1 else ' ', categories_text(cats)))
    print('stored patch k (%d…%d): 20 00 00 00 + k b1\'s, the temporary patch\'s 10 00 blocks' % (
        STORED_FIRST, STORED_FIRST + STORED_COUNT - 1))


LINE = re.compile(r'^((?:[0-9a-f]{2} ){3}[0-9a-f]{2})  g(\d+) +\S+ +(?:zone (\d))? +(0x[0-9a-f]+) .* Categories (\S+)')


def check_run(m, path):
    """The map against vc2run sysex's section 3, line by line, and its other sections' verdicts."""
    mine = {(' '.join('%02x' % b for b in a), g - 3 if g >= 13 else g, 2 if g >= 13 else (1 if g >= 10 else 0), i):
            categories_text(cats) for a, g, i, _, cats, title in m.values if title in ('MIC setting 1',)
            or not title.startswith('MIC setting')}
    run, text = {}, pathlib.Path(path).read_text()
    for line in text.splitlines():
        x = LINE.match(line)
        if x and not x.group(1).startswith('20'):
            run[(x.group(1), int(x.group(2)), int(x.group(3) or 0), int(x.group(4), 16))] = x.group(5)
    wrong = sorted(set(mine) ^ set(run)) + [k for k in mine if k in run and mine[k] != run[k]]
    for k in wrong[:20]:
        print('  differs: %s g%d zone %d %#05x: map %s, run %s' % (*k, mine.get(k), run.get(k)))
    ok = '448 the same, 448 with patch number k + 65' in text and 'DIFFERS' not in text
    print('%d values on the map, %d on the run, %d differ; the stored patches and MIC settings %s' % (
        len(mine), len(run), len(wrong), 'agree' if ok else 'DO NOT agree'))
    return 0 if not wrong and ok else 1


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    ap = argparse.ArgumentParser()
    ap.add_argument('--rom4', default=os.path.join(here, '../../firmware/unpacked/V-SynthXT.rom.4.bin'))
    ap.add_argument('--list', action='store_true')
    ap.add_argument('--check-run')
    a = ap.parse_args()
    m = Map(a.rom4)
    if a.list:
        listing(m)
    if a.check_run:
        raise SystemExit(check_run(m, a.check_run))


if __name__ == '__main__':
    main()
