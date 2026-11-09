#!/usr/bin/env python3
# vpdt_term.py <in> <out> <length>: a copy of a V-Synth SOLO wave (.vpw, or a project's .wav) with its record stream's terminator
# (the record after the last) given another length word, in 1/16 samples.  Too short to reach the wave's last event, the plugin can't
# play the wave's vpdt as it is and encodes it again on its own events (v160), which no wave there is does otherwise: verify.sh and
# mutate_panel.py's solo-own run use it for that path.
import struct, sys

src, dst, length = sys.argv[1], sys.argv[2], int(sys.argv[3])
b = bytearray(open(src, 'rb').read())
i = b.find(b'vpdt')
if i < 0: sys.exit('vpdt_term: no vpdt chunk in ' + src)
v = i + 8                                              # the chunk's data
r32 = lambda o: struct.unpack_from('<I', b, v + o)[0]
rec0 = r32(28)                                         # the records, after their count
words = r32(rec0 - 4)
term = v + rec0 + 4 * (words - 1)
if not (0 < words and term + 4 <= len(b)): sys.exit('vpdt_term: no record stream in ' + src)
struct.pack_into('<H', b, term, length)
open(dst, 'wb').write(b)
