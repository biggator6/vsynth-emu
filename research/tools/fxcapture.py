#!/usr/bin/env python3
"""fxcapture.py [--out DIR] [--dev 17] [--ch 1] [--note 60]: what the effect-chip captures need (RECORDING_GUIDE.md, "Effects
captures"; HARDWARE_FIRMWARE.md, "The effect captures").  Writes, by default to analysis/test_files/fx/ (gitignored):

  fxprobe.wav    the test wave to load on the XT: 44.1 kHz, 16-bit, stereo
  fx_test.mid    15 seconds, to see that the XT takes the messages: MFX Type changes on its screen, the wave plays, and Master Level
                 drops during its sweep, which shows a System write arriving by ear
  fx_run_1.mid   the takes that settle routing, gain and the decoding (8 minutes)
  fx_run_2.mid   every MFX, Reverb and Chorus type at its defaults (22 minutes)
  fx_takes.csv   every take: when it starts, what it sets, in the panel's words

A run is a Standard MIDI File of System Exclusive and notes, for fxplay.cpp or any DAW that sends SysEx.  Addresses are the
V-Synth's map (its manual's MIDI Implementation), which the XT's parameter IDs follow, ID order for address order; the XT's own
transmit routine (0x8946e480) uses the same model ID.  Not yet tried on an XT.  Parameter defaults are read from the user's
rom.2 at run time, so the runs are derived from firmware tables: they stay in the gitignored folder.

fxcapture.py --check FILE.mid  parses a run back and checks every checksum."""
import argparse, csv, math, os, struct, sys, wave

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '../..')
ROM_BASE = 0x89300000
FS = 44100

# ---------------------------------------------------------------- the test wave
LEAD = 0.5                                   # silence before the first mark
SECTIONS = [                                 # name, start in s, for the analysis
    ('mark', 0.5), ('impulse_l', 2.0), ('impulse_r', 4.0), ('sweep', 6.0), ('steps', 12.5), ('noise', 14.8), ('end', 16.8)]

def probe():
    """Stereo int16.  Everything is deterministic and needs no library, so it can be regenerated exactly."""
    n = int(16.8 * FS)
    L, R = [0.0] * n, [0.0] * n
    at = lambda t: int(round(t * FS))
    CLICK = 10 ** (-12 / 20)                           # low enough to pass unclipped through a path of up to +12 dB
    L[at(0.5)] = R[at(0.5)] = CLICK                    # the mark: aligns a take to the sample
    L[at(2.0)] = CLICK                                 # left only, then right only: what crosses over
    R[at(4.0)] = CLICK
    s, m = at(6.0), at(5.0)                            # a log sweep, 20 Hz to 20 kHz, -18 dBFS
    k = math.log(20000.0 / 20.0)
    for i in range(m):
        t = i / FS
        ph = 2 * math.pi * 20.0 * 5.0 / k * (math.exp(k * t / 5.0) - 1.0)
        g = min(1.0, min(i, m - 1 - i) / (0.01 * FS)) * 10 ** (-18 / 20)
        L[s + i] = R[s + i] = g * math.sin(ph)
    s = at(12.5)                                       # 1 kHz, -72 to 0 dBFS in 6 dB steps of 100 ms, phase running on
    for step in range(13):
        g = 10 ** ((-72 + 6 * step) / 20)
        for i in range(at(0.1)):
            j = s + step * at(0.1) + i
            L[j] = R[j] = g * math.sin(2 * math.pi * 1000.0 * (step * at(0.1) + i) / FS)
    s = at(14.8)                                       # noise, a different sequence each side, 1 s (xorshift32, so it never changes)
    x = 2463534242
    for i in range(at(1.0)):
        for ch in (L, R):
            x ^= (x << 13) & 0xffffffff; x ^= x >> 17; x ^= (x << 5) & 0xffffffff
            ch[s + i] = (x / 2147483648.0 - 1.0) * 10 ** (-12 / 20)      # uniform; -12 dBFS peak, about -17 rms
    frames = bytearray()
    for a, b in zip(L, R):
        frames += struct.pack('<hh', int(round(a * 32767)), int(round(b * 32767)))
    return bytes(frames), n / FS

# ---------------------------------------------------------------- the firmware's tables, read at run time
class Rom:
    def __init__(self, path):
        self.b = open(path, 'rb').read()
    def s16(self, a): return struct.unpack_from('<h', self.b, a - ROM_BASE)[0]
    def params(self, table, t):
        """A type's parameters: {slot: default}, from its first and last ID and the second descriptor bank."""
        first, last = self.s16(table + 4 * t), self.s16(table + 4 * t + 2)
        out = {}
        for i in range(first, last + 1):
            d = 0x897a56d8 + 8 * (i - 0x55d)
            out[(self.s16(d) - 3) // 4] = self.s16(d + 2)
        return out
MFX_IDS, CHO_IDS, REV_IDS = 0x897a8218, 0x897a82e4, 0x897a8310

MFX_NAMES = ['Thru', 'Parametric EQ', 'Graphic EQ', 'Resonant Filter', 'Isolator and Filter', 'Distortion / OD', 'Amp Simulator', 'Auto Wah', 'Humanizer',
             'Dynamic Processor', 'Tape Echo Simulator', 'Stereo Delay', 'Multi Tap Delay', 'Reverse Delay', 'Vocal Echo', 'Band Pass Delay', 'AnalogDelay->Chorus',
             'Digital Chorus', 'Space Chorus', 'Hexa Chorus', 'Analog Flanger', 'BOSS Flanger', 'Step Flanger', 'Analog Phaser', 'Digital Phaser', 'Rotary',
             'Tremolo/Auto Pan', 'Stereo Pitch Shifter', 'OD/DS->Cho/Flg', 'OD/DS->Delay', 'Cho/Flg->Delay', 'Enh->Cho/Flg', 'Enh->Delay', 'Vocal Multi', 'Guitar Multi',
             'Bass Multi', 'EP Multi', 'Keyboard Multi', 'Phonograph', 'Radio Tuning', 'Bit Rate Converter', 'Pseudo Stereo']
CHO_NAMES = ['Off', 'Chorus 1', 'Chorus 2', 'Chorus 3', 'Chorus 4', 'Feedback Chorus', 'Flanger', 'Short Delay', 'Fbk Short Delay']
REV_NAMES = ['Off', 'Room 1', 'Room 2', 'Room 3', 'Hall 1', 'Hall 2', 'Hall 3', 'Garage', 'Plate', 'Non-Linear', 'Delay']

# ---------------------------------------------------------------- System Exclusive
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

TEMP = [0x10, 0x00]                          # Temporary Patch, Part 1
COMMON, P_MFX, P_CHO, P_REV = TEMP + [0x00, 0x00], TEMP + [0x02, 0x00], TEMP + [0x04, 0x00], TEMP + [0x06, 0x00]
ZONE, COSM1, COSM2 = TEMP + [0x10, 0x00], TEMP + [0x50, 0x00], TEMP + [0x60, 0x00]       # Zone 1
SYSTEM = [0x02, 0x00, 0x00, 0x00]
# Offsets in a zone, in the XT's ID order from Structure Type (0x293); in the system, from 0x39.  They agree with the V-Synth's map where it lists them.
Z_MOD, Z_TVA, Z_MFX, Z_CHO, Z_REV, Z_ASSIGN = 0x01, 0x06, 0x0d, 0x0e, 0x0f, 0x10
Z_OSC1, Z_TYPE, Z_GAIN = 0x11, 0x12, 0x13
ASSIGN = {'MULTI': 0, 'MAIN': 1, 'DIR': 2}

BASE = dict(assign='MULTI', mfx_sw=0, cho_sw=0, rev_sw=0, mfx=0, mfx_over={}, mfx_master=127, mfx_cho=0, mfx_rev=0,
            cho=0, cho_master=127, cho_rev=0, rev=0, rev_master=127, tva_mfx=127, tva_cho=0, tva_rev=0,
            master=127, out_gain=0, wave_gain=0, note=True, tail=1.0)

def take(name, why='', **kw):
    t = dict(BASE); t.update(kw); t['name'] = name; t['why'] = why
    return t

def run1():
    w = lambda **k: dict(assign='MULTI', mfx_sw=1, mfx=0, mfx_master=0, **k)     # wet only: the dry path through Master at 0
    return [
        take('silence', 'noise floor; no note', note=False, assign='MAIN'),
        take('main', 'the baseline: level and latency without the MFX', assign='MAIN'),
        take('main_again', 'the same again: is a take repeatable, to the sample?', assign='MAIN'),
        take('multi_off', 'MFX Switch off, into Master: should equal Thru', mfx_sw=0),
        take('multi_thru', 'MFX Thru: three samples behind MAIN in the model', mfx_sw=1),
        take('multi_off_m64', 'Switch off, Master 64: Master should still apply', mfx_sw=0, mfx_master=64),
        take('multi_thru_m64', 'Thru, Master 64', mfx_sw=1, mfx_master=64),
        take('master_100', 'Master Level', assign='MAIN', master=100),
        take('gain_plus6', 'Output Gain +6 dB', assign='MAIN', out_gain=6),
        take('gain_minus6', 'Output Gain -6 dB', assign='MAIN', out_gain=-6),
        take('peq_flat', 'Parametric EQ at its defaults; Wave Gain -12, as in all four, so +15 stays clear of clipping', mfx_sw=1, mfx=1, wave_gain=-12),
        take('peq_mid_plus15', 'Mid 1 Gain +15: the model says exactly +15.00 dB at 800 Hz', mfx_sw=1, mfx=1, mfx_over={4: 30}, wave_gain=-12),
        take('peq_mid_minus15', 'Mid 1 Gain -15', mfx_sw=1, mfx=1, mfx_over={4: 0}, wave_gain=-12),
        take('peq_total_plus15', 'Total Gain +15: the shift code', mfx_sw=1, mfx=1, mfx_over={10: 30}, wave_gain=-12),
        take('sdelay_stereo', 'Stereo Delay: taps at 22050 and 28664 samples, as the driver computes them', mfx_sw=1, mfx=11, tail=2.0),
        take('sdelay_mono', 'Mode MONO', mfx_sw=1, mfx=11, mfx_over={0: 0}, tail=2.0),
        take('mtap', 'Multi Tap: direct reads', mfx_sw=1, mfx=12, tail=2.0),
        take('wet_chorus', 'Chorus 1 alone, from the tone\'s send', tva_cho=127, cho_sw=1, cho=1, **w()),
        take('wet_reverb', 'Room 1 alone, from the tone\'s send', tva_rev=127, rev_sw=1, rev=1, tail=4.0, **w()),
        take('mfx_to_chorus', 'the MFX\'s send to Chorus, taken before Master', mfx_cho=127, cho_sw=1, cho=1, **w()),
        take('mfx_to_reverb', 'the MFX\'s send to Reverb, taken before Master', mfx_rev=127, rev_sw=1, rev=1, tail=4.0, **w()),
        take('chorus_to_reverb', 'Chorus to Reverb only, Chorus Master 0', tva_cho=127, cho_sw=1, cho=1, cho_master=0, cho_rev=127, rev_sw=1, rev=1, tail=4.0, **w()),
        take('hot', 'Output Gain +12: where does it clip?', assign='MAIN', out_gain=12),
        take('hotter', 'Output Gain +12, Wave Gain +12: the output stage', assign='MAIN', out_gain=12, wave_gain=12),
    ]

def run2():
    t = []
    for i in range(1, 42):
        t.append(take('mfx%02d' % i, MFX_NAMES[i] + ' at its defaults', mfx_sw=1, mfx=i, tail=2.0))
    for i in range(1, 11):
        t.append(take('rev%02d' % i, REV_NAMES[i] + ' at its defaults, wet', mfx_sw=1, mfx=0, mfx_master=0, tva_rev=127, rev_sw=1, rev=i, tail=6.0))
    for i in range(1, 9):
        t.append(take('cho%02d' % i, CHO_NAMES[i] + ' at its defaults, wet', mfx_sw=1, mfx=0, mfx_master=0, tva_cho=127, cho_sw=1, cho=i, tail=1.0))
    return t

class Writer:
    """One millisecond per tick; SysEx paced, so the XT's parser is never rushed: 20 ms, the interval its manual gives for its own packets."""
    def __init__(self, dev, ch):
        self.ev, self.ms, self.dev, self.ch = [], 0, dev, ch
        self.state = {}
    def wait(self, ms): self.ms += int(ms)
    def sx(self, addr, data):
        self.ev.append((self.ms, b'S' + dt1(self.dev, addr, data))); self.ms += 20
    def note(self, on, n): self.ev.append((self.ms, b'N' + bytes([(0x90 if on else 0x80) | self.ch, n, 127 if on else 0])))
    def setup(self):
        """What the base patch must not bring: nothing but OSC 1, no VariPhrase, no COSM; the EQ off; 120 BPM."""
        for off, v in ((0x00, 0), (Z_MOD, 0), (Z_TVA, 0), (Z_OSC1, 1), (Z_TYPE, 1), (Z_GAIN, 16), (0x14, 64), (0x16, 64), (0x17, 64), (0x18, 64), (0x19, 64),
                       (0x1a, 0), (0x1b, 127), (0x1c, 32), (0x1d, 64), (0x3d, 0), (0x3e, 0), (0x3f, 0), (0x40, 0), (0x41, 0), (0x43, 0), (0x45, 0)):
            self.sx(addr_add(ZONE, off), [v])
        self.sx(COSM1, [0]); self.sx(COSM2, [0])
        self.sx(addr_add(COMMON, 0x12), [0])                                          # Portamento off
        self.sx(addr_add(COMMON, 0x17), nibbles(1200))                                # Patch Tempo 120.0
        self.sx(addr_add(SYSTEM, 0x37), [0])                                          # EQ off
    def apply(self, t, rom):
        """Writes what differs from the last take; returns whether a program was reloaded."""
        s = self.state; reload = False
        def put(key, val, addr, data):
            if s.get(key) != val: self.sx(addr, data); s[key] = val
        put('master', t['master'], addr_add(SYSTEM, 0x05), [t['master']])
        put('out_gain', t['out_gain'], addr_add(SYSTEM, 0x3d), [16 + t['out_gain']])
        put('wave_gain', t['wave_gain'], addr_add(ZONE, Z_GAIN), [16 + t['wave_gain']])
        put('assign', t['assign'], addr_add(ZONE, Z_ASSIGN), [ASSIGN[t['assign']]])
        put('tva_mfx', t['tva_mfx'], addr_add(ZONE, Z_MFX), [t['tva_mfx']])
        put('tva_cho', t['tva_cho'], addr_add(ZONE, Z_CHO), [t['tva_cho']])
        put('tva_rev', t['tva_rev'], addr_add(ZONE, Z_REV), [t['tva_rev']])
        for key, off in (('mfx_sw', 0x1d), ('cho_sw', 0x1e), ('rev_sw', 0x1f)):       # switches off first, so nothing sounds through a change
            if t[key] == 0: put(key, 0, addr_add(COMMON, off), [0])
        want = (t['mfx'], tuple(sorted(t['mfx_over'].items())))
        if s.get('mfx') != want:
            if s.get('mfx', (None,))[0] != t['mfx']: self.sx(P_MFX, [t['mfx']]); reload = True
            for slot, v in sorted({**rom.params(MFX_IDS, t['mfx']), **t['mfx_over']}.items()):
                self.sx(addr_add(P_MFX, 4 + 4 * slot), nibbles(v + 32768))
            s['mfx'] = want
        put('mfx_master', t['mfx_master'], addr_add(P_MFX, 1), [t['mfx_master']])
        put('mfx_cho', t['mfx_cho'], addr_add(P_MFX, 2), [t['mfx_cho']])
        put('mfx_rev', t['mfx_rev'], addr_add(P_MFX, 3), [t['mfx_rev']])
        if s.get('cho') != t['cho']:
            self.sx(P_CHO, [t['cho']]); reload = True
            for slot, v in sorted(rom.params(CHO_IDS, t['cho']).items()):
                self.sx(addr_add(P_CHO, 3 + 4 * slot), nibbles(v + 32768))
            s['cho'] = t['cho']
        put('cho_master', t['cho_master'], addr_add(P_CHO, 1), [t['cho_master']])
        put('cho_rev', t['cho_rev'], addr_add(P_CHO, 2), [t['cho_rev']])
        if s.get('rev') != t['rev']:
            self.sx(P_REV, [t['rev']]); reload = True
            for slot, v in sorted(rom.params(REV_IDS, t['rev']).items()):
                self.sx(addr_add(P_REV, 2 + 4 * slot), nibbles(v + 32768))
            s['rev'] = t['rev']
        put('rev_master', t['rev_master'], addr_add(P_REV, 1), [t['rev_master']])
        for key, off in (('mfx_sw', 0x1d), ('cho_sw', 0x1e), ('rev_sw', 0x1f)):
            if t[key] == 1: put(key, 1, addr_add(COMMON, off), [1])
        return reload
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
            else: trk += e[1:]
        trk += b'\x00\xff\x2f\x00'
        return b'MThd' + struct.pack('>IHHH', 6, 0, 1, 500) + b'MTrk' + struct.pack('>I', len(trk)) + bytes(trk)

def settings(t):
    """A take in the panel's words (the XT manual's pp. 97-98), for doing it by hand."""
    p = ['Output Assign %s' % t['assign'], 'MFX %s' % ('ON' if t['mfx_sw'] else 'OFF')]
    if t['mfx_sw'] or t['mfx']: p.append('MFX Type %02d %s' % (t['mfx'], MFX_NAMES[t['mfx']]))
    if t['mfx_over']: p.append('MFX params (from 1) ' + ', '.join('%d=%d' % (k + 1, v) for k, v in sorted(t['mfx_over'].items())))
    if t['mfx_master'] != 127: p.append('MFX Master Level %d' % t['mfx_master'])
    if t['mfx_cho']: p.append('MFX To CHO %d' % t['mfx_cho'])
    if t['mfx_rev']: p.append('MFX To REV %d' % t['mfx_rev'])
    if t['tva_cho']: p.append('CHO Send %d' % t['tva_cho'])
    if t['tva_rev']: p.append('REV Send %d' % t['tva_rev'])
    if t['cho_sw']: p.append('CHO ON, CHO Type %s' % CHO_NAMES[t['cho']] + (', CHO Master Level %d' % t['cho_master'] if t['cho_master'] != 127 else '') + (', CHO To REV %d' % t['cho_rev'] if t['cho_rev'] else ''))
    if t['rev_sw']: p.append('REV ON, REV Type %s' % REV_NAMES[t['rev']])
    if t['master'] != 127: p.append('Master Level %d' % t['master'])
    if t['out_gain']: p.append('Output Gain %+d dB' % t['out_gain'])
    if t['wave_gain']: p.append('Wave Gain %+d dB' % t['wave_gain'])
    if not t['note']: p.append('play nothing')
    return '; '.join(p)

def build(takes, rom, dev, ch, note, wave_s):
    w = Writer(dev, ch); rows = []
    w.wait(3000); w.setup(); w.wait(2000)
    for t in takes:
        reload = w.apply(t, rom)
        w.wait(2500 if reload else 800)                                               # a program load, a mute ramp, a reverb's clear
        start = w.ms
        if t['note']:
            w.note(True, note); w.wait(wave_s * 1000 + 200); w.note(False, note)
        else:
            w.wait(5000)
        off = w.ms
        w.wait(t['tail'] * 1000)
        rows.append(dict(take=t['name'], note_on_s='%.3f' % (start / 1000), note_off_s='%.3f' % (off / 1000), next_s='%.3f' % (w.ms / 1000), settings=settings(t), why=t['why']))
    w.wait(1000); w.note(False, note)
    return w.smf(), rows, w.ms / 1000.0

def check(path):
    """Reads a run back: format, every SysEx's checksum, the length; prints a summary.  Exits non-zero on any fault."""
    b = open(path, 'rb').read()
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

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--out', default=os.path.join(REPO, 'analysis/test_files/fx'))
    ap.add_argument('--rom', default=os.environ.get('ROM', os.path.join(REPO, 'firmware/unpacked/V-SynthXT.rom.2.bin')))
    ap.add_argument('--dev', type=int, default=17, help='Device ID as the XT shows it, 17-32')
    ap.add_argument('--ch', type=int, default=1, help='MIDI channel, 1-16')
    ap.add_argument('--note', type=int, default=60, help="the wave's Original Pitch")
    ap.add_argument('--check', nargs='+')
    a = ap.parse_args()
    if a.check:
        for p in a.check: check(p)
        return
    # The manual's own example must come out right, or nothing else will.
    assert dt1(0x10, [0x10, 0x00, 0x04, 0x00], [0x01]).hex() == 'f041100053121000040001' + '6bf7'
    rom = Rom(a.rom)
    os.makedirs(a.out, exist_ok=True)
    frames, wave_s = probe()
    with wave.open(os.path.join(a.out, 'fxprobe.wav'), 'wb') as f:
        f.setnchannels(2); f.setsampwidth(2); f.setframerate(FS); f.writeframes(frames)
    w = Writer(a.dev - 1, a.ch - 1)                                                   # the test: Parametric EQ on the screen, the wave's clicks, its sweep with Master Level dropping
    w.wait(1000); w.setup(); w.wait(500); w.sx(addr_add(COMMON, 0x1d), [1]); w.sx(P_MFX, [1]); w.wait(1500)
    w.note(True, a.note); w.wait(8000); w.sx(addr_add(SYSTEM, 0x05), [40]); w.wait(2000); w.note(False, a.note)
    w.wait(500); w.sx(addr_add(SYSTEM, 0x05), [127]); w.sx(P_MFX, [0]); w.wait(100)
    open(os.path.join(a.out, 'fx_test.mid'), 'wb').write(w.smf())
    rows = []
    for name, takes in (('fx_run_1', run1()), ('fx_run_2', run2())):
        smf, r, total = build(takes, rom, a.dev - 1, a.ch - 1, a.note, wave_s)
        p = os.path.join(a.out, name + '.mid')
        open(p, 'wb').write(smf)
        for x in r: x['run'] = name
        rows += r
        print('%s.mid: %d takes, %.1f minutes' % (name, len(takes), total / 60))
    with open(os.path.join(a.out, 'fx_takes.csv'), 'w', newline='') as f:
        w = csv.DictWriter(f, fieldnames=['run', 'take', 'note_on_s', 'note_off_s', 'next_s', 'settings', 'why'])
        w.writeheader(); w.writerows(rows)
    print('fxprobe.wav: %.1f s, stereo, 44.1 kHz, 16-bit; marks at %s' % (wave_s, ', '.join('%s %.1f' % s for s in SECTIONS)))
    print('in', os.path.abspath(a.out))

if __name__ == '__main__':
    main()
