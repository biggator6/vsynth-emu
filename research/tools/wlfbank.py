#!/usr/bin/env python3
"""wlfbank.py: the VC-2's sample bank, "VC-2 Preset.bin", in the disk image's preset area (research/VC2_VOCAL_DESIGNER.md,
"The wave libraries, read" and "The sample bank, loaded"), for wlflib.py.

The bank: '"TABLE"', an ASCII count, 60-byte entries (a 52-byte name, u32 size, u32 offset from the end of '"DATA"'),
then '"DATA"' and the streams. A stream is a .vpw's chunks: an id, a u32 size and the data, each padded to an even
length, as XtWaveFile's findChunks walks them; its vpsc chunk's byte 1 is the encode type."""
import struct
from dataclasses import dataclass

import diskls

img = diskls.img
TABLE, DATA = b'"TABLE"', b'"DATA"'
ENTRY, NAME, COUNT_DIGITS = 60, 52, 4


@dataclass
class Bank:
    at: int        # where '"TABLE"' is in the image: the file's first byte
    entries: list  # (name, size, offset from the streams' base)
    data: int      # where '"DATA"' is
    base: int      # where the streams start, '"DATA"' + 6


def vc2_bank(start):
    """The first bank after `start` in the image."""
    t = img.find(TABLE, start)
    count = int(img[t + len(TABLE):t + len(TABLE) + COUNT_DIGITS])
    first = t + len(TABLE) + COUNT_DIGITS
    entries = [(img[first + ENTRY * k:first + ENTRY * k + NAME].split(b'\0')[0].decode('latin1'),
                *struct.unpack_from('<II', img, first + ENTRY * k + NAME)) for k in range(count)]
    data = first + ENTRY * count
    return Bank(t, entries, data, data + len(DATA))


def bank_bytes(bank):
    """The bank's file: from '"TABLE"' to the end of its last stream."""
    return img[bank.at:bank.base + max(offset + size for _, size, offset in bank.entries)]


def vpsc_type(stream):
    """The stream's encode type (its vpsc chunk's byte 1), or None when it has no vpsc."""
    i = 0
    while i + 8 <= len(stream):
        n = struct.unpack_from('<I', stream, i + 4)[0]
        if stream[i:i + 4] == b'vpsc':
            return stream[i + 9]
        i += 8 + n + (n & 1)
    return None
