"""romfile.py: the user's rom.2 for the research tools (gitignored, never committed): its path ($ROM, else
firmware/unpacked's), its bytes, and words read by CPU address (rom.2 maps at 0x89300000).
cosmcases.py, pnames.py, poolrefs.py and sh4dis.py import it."""
import os
import struct
from pathlib import Path

ROMP = os.environ.get('ROM', str(Path(__file__).resolve().parents[2] / 'firmware/unpacked/V-SynthXT.rom.2.bin'))
rom = Path(ROMP).read_bytes()
B = 0x89300000


def _reader(fmt):
    return lambda a: struct.unpack_from(fmt, rom, a - B)[0]


u16 = _reader('<H')
s16 = _reader('<h')
u32 = _reader('<I')
f32 = _reader('<f')
