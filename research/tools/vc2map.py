#!/usr/bin/env python3
"""vc2map.py: rom.4's parameters as SysEx addresses, for the VC-2 captures (vc2capture.py), read from the user's rom.4
at run time (VC2_VOCAL_DESIGNER.md, "The parameter tables": names, descriptors, value text; "The second descriptor
bank, read": the type tables).

The map is the V-Synth's (its manual's Parameter Address Map, which fxcapture.py's runs use) on rom.4's own IDs: in a
block, address offset = ID - the block's first ID.  Nothing in rom.4 that receives or sends SysEx has been read, so
this is inferred, from this: block by block, rom.4's IDs after each block's first are rom.2's name for name (the
system's 72 but two, which rom.4 gave to MIDI MODE for VariOS and USB External In Type; the patch common's 115, then
the vocoder block from +0x73; the MFX's, Chorus's and Reverb's heads; zone 1's 128).  `Map(rom4, rom2)` checks that
again on the images it is given, and refuses to write a run when it fails.  The XT itself can settle the map: with
Transmit Edit Data on, an edit at its panel sends a DT1, and `vc2capture.py --learn` names each one by this map.

Not mapped: a zone's IDs past its 128th (the VOCODER OSC's own, 0x354 on), which the V-Synth's zone blocks, 0x80
addresses apart, have no room for; the runs leave them at PATCH Init's."""
import struct

from capsysex import ROM_BASE, Rom, addr_add

ROM4_BSS = 0x8994fc20                 # rom.4's header word +0x1c: another image is refused rather than misread
NAMES4, DESC4, TEXT4, COUNT4 = 0x898b5798, 0x8950f0b0, 0x898b8154, 0x9ac
NAMES2 = 0x89790338
TYPES4 = {'MFX Type': 0x895164ec, 'Chorus Type': 0x895165b8, 'Reverb Type': 0x895165e4}
# Each block: its title, its address (the V-Synth's), the name its first ID has on both images, the IDs it holds here,
# where a typed slot's four nibbles start after the Type (fxcapture.py's P_MFX + 4, P_CHO + 3, P_REV + 2), and how
# many of its IDs are checked by name against rom.2's (the system's, from Master Tune's 3 before it).
BLOCKS = [
    ('system', [0x02, 0x00, 0x00, 0x00], 'Master Tune', -3, 0x48, None),
    ('common', [0x10, 0x00, 0x00, 0x00], 'Patch Name 1', 0, 0x96, None),
    ('mfx', [0x10, 0x00, 0x02, 0x00], 'MFX Type', 0, 4, 4),
    ('chorus', [0x10, 0x00, 0x04, 0x00], 'Chorus Type', 0, 3, 3),
    ('reverb', [0x10, 0x00, 0x06, 0x00], 'Reverb Type', 0, 2, 2),
    ('zone 1', [0x10, 0x00, 0x10, 0x00], 'Structure Type', 0, 0x80, None)]
SAME_AS_ROM2 = {'system': 0x48, 'common': 0x72, 'mfx': 4, 'chorus': 3, 'reverb': 2, 'zone 1': 0x80}
ROM4_OWN = {('system', 0x43), ('system', 0x44)}
COUNT2 = 0x955                        # rom.2's IDs


def names(rom, table, count):
    out = []
    for i in range(count):
        p = struct.unpack_from('<I', rom.b, table + 4 * i - ROM_BASE)[0] - ROM_BASE
        out.append(rom.b[p:rom.b.index(b'\0', p)].decode('latin1').strip())
    return out


class Map:
    """rom.4's IDs by name, each one's address, descriptor and text, and its types' typed defaults."""
    def __init__(self, rom4_path, rom2_path):
        self.rom = Rom(rom4_path, bank=0x895131a8, first=0x56a)
        if struct.unpack_from('<I', self.rom.b, 0x1c)[0] != ROM4_BSS:
            raise SystemExit('%s is not rom.4 (the VC-2): its header says otherwise' % rom4_path)
        self.names = names(self.rom, NAMES4, COUNT4)
        n2 = names(Rom(rom2_path), NAMES2, COUNT2)
        self.blocks = []
        for title, base, first_name, back, count, slot in BLOCKS:
            first = self.names.index(first_name) + back
            first2 = n2.index(first_name) + back
            wrong = [o for o in range(SAME_AS_ROM2[title]) if (title, o) not in ROM4_OWN
                     and self.names[first + o] != n2[first2 + o]]
            if wrong:
                raise SystemExit('the map does not hold on these images: %s offset %#x is %r on rom.4, %r on rom.2'
                                 % (title, wrong[0], self.names[first + wrong[0]], n2[first2 + wrong[0]]))
            self.blocks.append(dict(title=title, base=base, first=first, count=count, slot=slot))

    def u16(self, a): return struct.unpack_from('<H', self.rom.b, a - ROM_BASE)[0]

    def block_of(self, i):
        return next((b for b in self.blocks if b['first'] <= i < b['first'] + b['count']), None)

    def id_of(self, name):
        """The one ID in a mapped block with this name."""
        ids = [i for i, n in enumerate(self.names) if n == name and self.block_of(i)]
        if len(ids) != 1:
            raise SystemExit('rom.4 has %d mapped IDs named %r' % (len(ids), name))
        return ids[0]

    def address(self, i):
        b = self.block_of(i)
        return addr_add(b['base'], i - b['first'])

    def descriptor(self, i):
        """(centre, default, min, max): the first bank's, 12 bytes an ID."""
        return tuple(self.u16(DESC4 + 12 * i + 4 + 2 * k) for k in range(4))

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
        """A type's typed defaults: [(address, nibbles' value)], each slot after its block's Type."""
        b = self.block_of(self.id_of(type_name))
        return [(addr_add(b['base'], b['slot'] + 4 * slot), v + 32768)
                for slot, v in sorted(self.rom.params(TYPES4[type_name], t).items())]

    def named(self, address):
        """The ID this map gives an address, or None: what --learn says of a DT1 the XT sent."""
        a = (address[0] << 21) | (address[1] << 14) | (address[2] << 7) | address[3]
        for b in self.blocks:
            base = (b['base'][0] << 21) | (b['base'][1] << 14) | (b['base'][2] << 7) | b['base'][3]
            if 0 <= a - base < b['count']:
                return b['first'] + a - base
        return None
