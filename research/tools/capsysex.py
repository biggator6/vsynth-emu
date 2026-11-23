#!/usr/bin/env python3
"""capsysex.py: the MIDI side of the capture runs (fxcapture.py's for the effects, vc2capture.py's for the VC-2): Roland
DT1 messages on a four-byte address, a run as a Standard MIDI File of them with notes and cues, the file read back and
checked, and an image's typed parameters' defaults by its second descriptor bank.  Split from fxcapture.py as it stood;
the bank's place is a parameter (rom.4's is not rom.2's), and a run can carry cues for fxplay's input (`cue`)."""
import os, pathlib, struct

ROM_BASE = 0x89300000


class Rom:
    """An image's typed parameters, read at run time: by default rom.2's second descriptor bank, 8 bytes an ID from
    0x55d; rom.4's is at 0x895131a8 from 0x56a (VC2_VOCAL_DESIGNER.md, "The second descriptor bank, read")."""
    def __init__(self, path, bank=0x897a56d8, first=0x55d):
        self.b = pathlib.Path(path).read_bytes()
        self.bank, self.first = bank, first
    def s16(self, a): return struct.unpack_from('<h', self.b, a - ROM_BASE)[0]
    def params(self, table, t):
        """A type's parameters: {slot: default}, from its first and last ID and the second descriptor bank."""
        first, last = self.s16(table + 4 * t), self.s16(table + 4 * t + 2)
        out = {}
        for i in range(first, last + 1):
            d = self.bank + 8 * (i - self.first)
            out[(self.s16(d) - 3) // 4] = self.s16(d + 2)
        return out


def addr_add(a, n):
    """Add to a four-byte address, 7 bits a byte."""
    v = (a[0] << 21) | (a[1] << 14) | (a[2] << 7) | a[3]
    v += n
    return [(v >> 21) & 127, (v >> 14) & 127, (v >> 7) & 127, v & 127]

def dt1(dev, addr, data):
    body = list(addr) + list(data)
    return bytes([0xF0, 0x41, dev, 0x00, 0x53, 0x12] + body + [(128 - sum(body) % 128) % 128, 0xF7])

def nibbles(v):
    return [(v >> 12) & 15, (v >> 8) & 15, (v >> 4) & 15, v & 15]


class Run:
    """One millisecond per tick; SysEx paced, so the XT's parser is never rushed: 20 ms, the interval its manual gives for its own packets."""
    def __init__(self, dev, ch):
        self.ev, self.ms, self.dev, self.ch = [], 0, dev, ch
        self.state = {}
    def wait(self, ms): self.ms += int(ms)
    def sx(self, addr, data):
        self.ev.append((self.ms, b'S' + dt1(self.dev, addr, data))); self.ms += 20
    def note(self, on, n): self.ev.append((self.ms, b'N' + bytes([(0x90 if on else 0x80) | self.ch, n, 127 if on else 0])))
    def cue(self, text):
        """A marker (FF 06) here: fxplay --audio starts that part of its input at it."""
        self.ev.append((self.ms, b'M' + text.encode('ascii')))
    def smf(self):
        def var(n):
            out = [n & 127]; n >>= 7
            while n: out.insert(0, 128 | (n & 127)); n >>= 7
            return bytes(out)
        trk = bytearray(b'\x00\xff\x51\x03\x07\xa1\x20')                              # 500000 us per quarter; at 500 ticks per quarter, 1 ms a tick
        last = 0
        for ms, e in sorted(self.ev, key=lambda x: x[0]):
            trk += var(ms - last); last = ms
            if e[:1] == b'S': trk += b'\xf0' + var(len(e) - 2) + e[2:]                # the file form: F0, a length, then the rest with F7
            elif e[:1] == b'M': trk += b'\xff\x06' + var(len(e) - 1) + e[1:]
            else: trk += e[1:]
        trk += b'\x00\xff\x2f\x00'
        return b'MThd' + struct.pack('>IHHH', 6, 0, 1, 500) + b'MTrk' + struct.pack('>I', len(trk)) + bytes(trk)


def check(path):
    """Reads a run back: format, every SysEx's checksum, the length; prints a summary.  Exits non-zero on any fault."""
    b = pathlib.Path(path).read_bytes()
    assert b[:4] == b'MThd' and b[14:18] == b'MTrk', 'not a type 0 file'
    fmt, ntrk, div = struct.unpack('>HHH', b[8:14]); assert (fmt, ntrk, div) == (0, 1, 500), 'header'
    n = struct.unpack('>I', b[18:22])[0]; i = 22; end = 22 + n; assert end == len(b), 'track length'
    def var():
        nonlocal i
        v = 0
        while True:
            c = b[i]; i += 1; v = (v << 7) | (c & 127)
            if not c & 128: return v
    t = 0; sx = notes = 0; first = None
    while i < end:
        t += var(); c = b[i]; i += 1
        if c == 0xff:
            kind = b[i]; i += 1; ln = var(); i += ln
            if kind == 0x2f: break
        elif c == 0xf0:
            ln = var(); m = b[i:i + ln]; i += ln
            assert m[-1] == 0xf7 and m[:5] == bytes([0x41, m[1], 0x00, 0x53, 0x12]), 'header at %d ms' % t
            body = m[5:-2]; assert all(x < 128 for x in body), 'a data byte over 127 at %d ms' % t
            assert (sum(body) + m[-2]) % 128 == 0, 'checksum at %d ms' % t
            sx += 1; first = first or (b'\xf0' + m).hex(' ')
        elif c & 0xf0 in (0x80, 0x90):
            i += 2; notes += 1
        else:
            raise AssertionError('event %02x at %d ms' % (c, t))
    print('%s: %d SysEx, all checksums good; %d note events; %.1f minutes; first %s' % (os.path.basename(path), sx, notes, t / 60000.0, first))
