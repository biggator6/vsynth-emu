#!/usr/bin/env python3
"""rppstate.py <project.RPP> <out-dir>: a REAPER project's V-Synth Emu, read: every track's routing (its name, master
send, receives, record arm and input, FX, bypassed or not) printed, and the first V-Synth Emu VST3 or VST instance's
saved state taken from its base64 chunk: in it JUCE's state block (the magic 'VC2!', a 32-bit size, then the XML:
copyXmlToBinary's form, PluginState.cpp), written to <out-dir>/state.bin (what setStateInformation takes) and
state.xml. It reads the project only. An AU's chunk is a plist, not read here.
"""
import base64
import os
import struct
import sys

MAGIC = b'VC2!'          # JUCE's copyXmlToBinary
PLUGIN = 'V-Synth Emu'


def tracks(lines):
    """Each track's lines, from its <TRACK to the line before the next"""
    starts = [i for i, l in enumerate(lines) if l.strip().startswith('<TRACK')] + [len(lines)]
    return [lines[a:b] for a, b in zip(starts, starts[1:])]


def routing(n, block):
    """A track's routing in a line or two"""
    fields = {}
    for l in block:
        key, _, rest = l.strip().partition(' ')
        fields.setdefault(key, []).append(rest)
    name = fields.get('NAME', ['""'])[0]
    rec = fields.get('REC', ['0'])[0].split()
    fx = [l.strip().split('"')[1] for l in block if l.strip().startswith(('<VST ', '<AU ', '<JS '))]
    print('track %d %s: master send %s, record %s (input %s, monitor %s), receives %s; FX %s; bypass %s' % (
        n, name, fields.get('MAINSEND', ['?'])[0], rec[0], rec[1] if len(rec) > 1 else '?',
        rec[2] if len(rec) > 2 else '?', fields.get('AUXRECV', ['none']), fx or 'none',
        fields.get('BYPASS', ['-'])))


def chunk(lines):
    """The first V-Synth Emu VST instance's chunk, decoded"""
    start = next(i for i, l in enumerate(lines) if l.strip().startswith('<VST ') and PLUGIN in l)
    end = next(i for i in range(start + 1, len(lines)) if lines[i].strip() == '>')
    return base64.b64decode(''.join(l.strip() for l in lines[start + 1:end]))


def main():
    project, out = sys.argv[1], sys.argv[2]
    with open(project, encoding='utf-8', errors='replace') as f:
        lines = f.read().split('\n')
    for n, block in enumerate(tracks(lines)):
        routing(n, block)
    data = chunk(lines)
    at = data.find(MAGIC)
    if at < 0:
        sys.exit('no JUCE state block in the V-Synth Emu chunk (%d bytes)' % len(data))
    size = struct.unpack_from('<I', data, at + 4)[0]
    state = data[at:at + 8 + size]
    os.makedirs(out, exist_ok=True)
    with open(os.path.join(out, 'state.bin'), 'wb') as f:
        f.write(state)
    with open(os.path.join(out, 'state.xml'), 'w', encoding='utf-8') as f:
        f.write(state[8:].decode('utf-8', 'replace').rstrip('\0'))
    print('state: %d bytes of XML at %d of the chunk\'s %d, in %s' % (size, at, len(data), out))


if __name__ == '__main__':
    main()
