#!/usr/bin/env python3
"""caprec.py: a recording read as it is, for the capture cutters (fxcut.py, vc2cut.py): any WAV a recorder writes, by memmap,
and the two measures every check is in, dB and rms.  Split from fxcut.py as it stood."""
import os, struct
import math
import numpy as np


def db(x):
    return 20.0 * math.log10(x) if x > 1e-12 else -240.0


def rms(x):
    return math.sqrt(float(np.mean(np.square(x)))) if len(x) else 0.0


class Bad(Exception):
    """The file can't be read as a recording: what to do is in the message."""


class Recording:
    """A WAV, by memmap: integer PCM of 16, 24 or 32 bits, or float; plain or extensible; any chunks around the data."""
    def __init__(self, path):
        self.path = path
        size = os.path.getsize(path)
        fmt = data = None
        with open(path, 'rb') as f:
            head = f.read(12)
            if head[:4] != b'RIFF' or head[8:12] != b'WAVE':
                raise Bad('%s is not a RIFF WAV file (an AIFF or a 64-bit RF64 needs exporting as WAV first)' % os.path.basename(path))
            pos = 12
            while pos + 8 <= size:
                f.seek(pos)
                cid = f.read(4); n = struct.unpack('<I', f.read(4))[0]
                if cid == b'fmt ':
                    fmt = f.read(n)
                elif cid == b'data':
                    data = (pos + 8, n if n and pos + 8 + n <= size else size - pos - 8)   # a recorder that died leaves 0, or too much
                pos += 8 + n + (n & 1)
        if fmt is None or data is None:
            raise Bad('%s has no fmt or data chunk' % os.path.basename(path))
        self.fmt_chunk = fmt
        tag, self.ch, self.fs, _, self.align, self.bits = struct.unpack('<HHIIHH', fmt[:16])
        if tag == 0xfffe and len(fmt) >= 26:
            tag = struct.unpack('<H', fmt[24:26])[0]
        self.float = tag == 3
        if (tag == 1 and self.bits not in (16, 24, 32)) or (tag == 3 and self.bits not in (32, 64)) or tag not in (1, 3):
            raise Bad('%s: format %d, %d bits; only 16, 24 or 32-bit integer and float can be read' % (os.path.basename(path), tag, self.bits))
        self.offset, self.data_size = data
        self.frames = self.data_size // self.align
        self.bytes = self.bits // 8
        self.mm = np.memmap(path, dtype=np.uint8, mode='r', offset=self.offset, shape=(self.frames * self.align,))
        self.scale = 1.0 if self.float else float(1 << (self.bits - 1))

    def raw(self, a, b):
        """Frames a..b as read, one row per frame, the first two channels; no padding."""
        a, b = max(0, a), min(self.frames, b)
        if b <= a:
            return np.zeros((0, 2), dtype=np.float64 if self.float else np.int32)
        v = np.asarray(self.mm[a * self.align:b * self.align])
        n = b - a
        if self.bits == 24:
            w = v.reshape(n, self.ch, 3).astype(np.int32)
            x = w[..., 0] | (w[..., 1] << 8) | (w[..., 2] << 16)
            x = (x ^ 0x800000) - 0x800000
        else:
            t = {16: '<i2', 32: '<f4' if self.float else '<i4', 64: '<f8'}[self.bits]
            x = np.frombuffer(v.tobytes(), dtype=t).reshape(n, self.ch)
        if self.ch == 1:
            x = np.concatenate([x, np.zeros_like(x)], axis=1)
        return x[:, :2]

    def read(self, a, b):
        """Frames a..b as two channels of float, full scale 1; zeros outside the file."""
        out = np.zeros((max(0, b - a), 2))
        x = self.raw(a, b)
        if len(x):
            out[max(0, -a):max(0, -a) + len(x)] = x / self.scale
        return out

    def is_full_scale(self, x):
        """Samples of x (float, full scale 1) at the file's top or bottom code."""
        lim = 1.0 if self.float else (self.scale - 1.0) / self.scale
        return np.abs(x) >= lim - 1e-12

