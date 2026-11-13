#!/usr/bin/env python3
"""ramrd.py <hex addr>[:count] ...: the words the CPU sees after C startup (romfile.ram32), each also as a float.
$ROM picks the image (rom.2 by default; rom.4, the VC-2's, too). build/tools/bin/rd runs rom.2's startup in the
interpreter; this reads the image's own header instead, so it works for rom.4, which Sh4Interp doesn't map yet."""
import struct
import sys

from romfile import ram32

for arg in sys.argv[1:]:
    at, _, count = arg.partition(':')
    first = int(at, 16)
    for k in range(int(count or 1)):
        word = ram32(first + 4 * k)
        print(f'{first + 4 * k:#x} {word:08x} {struct.unpack("<f", struct.pack("<I", word))[0]:.9g}')
