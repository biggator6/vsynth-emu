#!/usr/bin/env python3
"""xtrecs.py <xt_enc> <rom>: the port's SOLO records against the records stored in the project's own
waves, the only ones made by an instrument and not by rom.2 in the interpreter.  One line a wave with
one event (start to end), its encoder from the file's ISFT tag.  The vpdt: at word 1 the offset of the
count, then the records, u16 length (0xffff: go on) and u16 info, the (256, x) terminator counted.  In a
stereo wave the same list follows again.  HARDWARE_FIRMWARE.md, "The SOLO encoder's stereo mode"."""
import glob, os, struct, subprocess, sys, tempfile

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
xt_enc, rom = sys.argv[1], sys.argv[2]

def chunks(d):
    i, c = 12, {}
    while i + 8 <= len(d):
        size = struct.unpack_from('<I', d, i + 4)[0]
        c[d[i:i + 4]] = d[i + 8:i + 8 + size]
        i += 8 + size + (size & 1)
    return c

def stored(v):
    o = struct.unpack_from('<I', v, 4)[0]
    n = struct.unpack_from('<I', v, o)[0]
    out, carry = [], 0
    for k in range(n - 1):
        length, info = struct.unpack_from('<HH', v, o + 4 + 4 * k)
        carry += length
        if length != 0xffff:
            out.append((carry, info))
            carry = 0
    return out

def isft(c):
    t = c.get(b'LIST', b'')
    j = t.find(b'ISFT')
    if j < 0: return '?'
    return t[j + 8:j + 8 + struct.unpack_from('<I', t, j + 4)[0]].rstrip(b'\0').decode('latin1')

for path in sorted(glob.glob(os.path.join(REPO, 'analysis/projects/Emu_test.prj/Wave/*.wav'))):
    if not os.path.getsize(path): continue
    c = chunks(open(path, 'rb').read())
    if b'vpsc' not in c or c[b'vpsc'][1] != 1 or b'vpdt' not in c: continue    # SOLO
    if struct.unpack_from('<I', c[b'vpdt'], 8)[0] >> 16 != 2: continue           # a start and an end
    if struct.unpack_from('<H', c[b'fmt '], 14)[0] != 16: continue
    channels = struct.unpack_from('<H', c[b'fmt '], 2)[0]
    with tempfile.NamedTemporaryFile(suffix='.bin') as f:
        f.write(c[b'data']); f.flush()
        args = [xt_enc, rom, f.name] + (['stereo'] if channels == 2 else [])
        port = [tuple(map(int, l.split()))[1:] for l in subprocess.run(args, capture_output=True, text=True).stdout.splitlines()]
    file = stored(c[b'vpdt'])
    diffs = [k for k, (a, b) in enumerate(zip(port, file)) if a != b]
    name = '%s, %d ch, %s' % (os.path.basename(path)[:3], channels, isft(c))
    if not diffs and len(port) == len(file):
        print('xt %s: %d records identical' % (name, len(file)))
    else:
        first = diffs[0] if diffs else min(len(port), len(file))
        at = ': port %d 0x%x, stored %d 0x%x' % (port[first] + file[first]) if diffs else ''
        print('xt %s: port %d, stored %d records, %d different, first at %d%s' % (name, len(port), len(file), len(diffs), first, at))
