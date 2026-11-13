"""romfile.py: the user's rom.2 for the research tools (gitignored, never committed): its path ($ROM, else
firmware/unpacked's), its bytes, and words read by CPU address (rom.2 maps at 0x89300000; so does rom.4, the VC-2's).
ram32/ramf32 read what the CPU sees after C startup, through the image's own .data copy.
cosmcases.py, pnames.py, poolrefs.py, sh4dis.py, ramrd.py, idbuild.py, vc2params.py and romtables.py import it.
image(path), sections(img) and offsets(img, addr) take any image, for a tool that reads two at once."""
import os
import struct
from pathlib import Path

ROMP = os.environ.get('ROM', str(Path(__file__).resolve().parents[2] / 'firmware/unpacked/V-SynthXT.rom.2.bin'))
B = 0x89300000


def image(path):
    return Path(path).read_bytes()


def sections(img):
    """C startup, by the image's header from +0x1c: .data is copied to RAM [DATA, BSS) from the ROM just below DATA
    (the source ends where the copy begins: Sh4Interp's loadRom for rom.2, and rom.4's header word +0x2c names that
    source), and .bss [BSS, BSS_END) is zeroed. A table in [DATA, BSS) read in place is something else.
    Returns (BSS, BSS_END, DATA, DATA_FROM)."""
    bss, _, _, bss_end, _, data = struct.unpack_from('<6I', img, 0x1c)
    return bss, bss_end, data, data - (bss - data)


def offsets(img, addr):
    """The file offset the CPU reads `addr` from after C startup: .data through its copy, the image in place; None for
    .bss and past the image."""
    bss, bss_end, data, data_from = sections(img)
    if data <= addr < bss:
        return addr - data + data_from - B
    if B <= addr < B + len(img) and not bss <= addr < bss_end:
        return addr - B
    return None


rom = image(ROMP)
BSS, BSS_END, DATA, DATA_FROM = sections(rom)


def _reader(fmt):
    return lambda a: struct.unpack_from(fmt, rom, a - B)[0]


u16 = _reader('<H')
s16 = _reader('<h')
u32 = _reader('<I')
f32 = _reader('<f')


def _ram(fmt):
    def read(a):
        if BSS <= a < BSS_END:
            return 0
        return struct.unpack_from(fmt, rom, (a - DATA + DATA_FROM if DATA <= a < BSS else a) - B)[0]
    return read


ram32 = _ram('<I')
ramf32 = _ram('<f')
