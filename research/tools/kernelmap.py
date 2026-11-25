#!/usr/bin/env python3
"""kernelmap.py [rom.1] [rom.4]: what rom.1 is, read from the user's images at run time (gitignored, never committed;
firmware/unpacked's by default). rom.1 is the XT's kernel: the task scheduler, its event flags, alarms, cyclic handlers
and semaphores, the interrupt table, and the library rom.2 and rom.4 reach through stubs (files, strings, the panel).
It prints, from the bytes alone:
- where rom.1 runs: the VBR its reset code sets, the exception vectors at that base, the reset code's jump into itself;
- its sections, by its C start-up's words: .data copied from the image's last bytes, the .bss areas, the stack;
- its kernel's own names: the strings of its debug dump (the system's words, a task's control block, the queues);
- its service table: 16-byte slots from 0x88008000, each loading a routine's address and jumping to it;
- whether every slot word rom.4 loads, and every address Vc2WaveBootKernel.cpp (v503) answers, is a slot's start.
Exits 1 when one isn't. HARDWARE_FIRMWARE.md's "rom.1, the kernel" (v352) is what it found."""
import re
import struct
import sys
from pathlib import Path

from romfile import image

ROOT = Path(__file__).resolve().parents[2]
ROM1 = sys.argv[1] if len(sys.argv) > 1 else str(ROOT / 'firmware/unpacked/V-SynthXT.rom.1.bin')
ROM4 = sys.argv[2] if len(sys.argv) > 2 else str(ROOT / 'firmware/unpacked/V-SynthXT.rom.4.bin')
WAYS = ROOT / 'plugin/Source/Vc2WaveBootKernel.cpp'

RESET = 0x1000              # the reset code's file offset (rom.0 enters it; its first VBR load is the base)
VECTORS = (0x100, 0x400, 0x600)   # the SH-4's general exception, TLB miss and interrupt vectors, from VBR
TABLE, TABLE_END, SLOT = 0x8000, 0xa000, 16   # the service table's file span and a slot's size
ROM4_BASE = 0x89300000      # rom.4's load address by its own header (romfile.py)
# [FW] the C start-up's words (the code at 0x88001080 reads them): .data's source, its end, its copy; three .bss
# areas, each from and to; the heap's start and the stack's top
STARTUP = {'data from': 0x106c, 'data from end': 0x1070, 'data to': 0x1074, 'bss 1': (0x1054, 0x1058),
           'bss 2': (0x105c, 0x1060), 'bss 3': (0x1064, 0x1068), 'heap': 0x1078, 'stack top': 0x107c}
DUMP = (0x615b8, 0x619f0)   # [FW] the debug dump's strings: "rtmSys:" to the last link's name
# Each slot rom.4 reaches, by what its routine does (read in the routine, HARDWARE_FIRMWARE.md "rom.1, the kernel")
ROLES = {
    0x88008000: 'semaphore wait', 0x88008010: 'semaphore signal', 0x88008020: 'interrupts locked out of dispatch',
    0x88008030: 'that undone', 0x88008040: 'event set, from an interrupt', 0x88008060: 'event wait',
    0x88008070: 'event set', 0x88008080: 'cyclic handler made', 0x88008090: 'cyclic handler on or off',
    0x880080a0: 'task suspended', 0x880080b0: 'task resumed', 0x880080d0: 'the tick count',
    0x880080e0: 'the current task', 0x88008100: "the task's own alarm interval", 0x88008110: 'dispatch held',
    0x88008120: 'dispatch released', 0x88008150: 'task restarted', 0x88008160: 'task by name',
    0x88008170: "a level's ready queue", 0x88008180: 'the first dispatch', 0x88008190: "a task's context loaded",
    0x880081a0: 'semaphore k', 0x880081b0: 'cyclic record k', 0x88008240: 'interrupt handler set',
}


def word(img, off):
    return struct.unpack_from('<I', img, off)[0]


def half(img, off):
    return struct.unpack_from('<H', img, off)[0]


def pool(off, op):
    """The file offset a `mov.l @(disp,pc),Rn` at `off` reads (pc-relative, so the same at any 4-aligned base)."""
    return (off & ~3) + 4 + (op & 0xff) * 4


def loads(img, lo=0, hi=None):
    """Every (file offset, word) a `mov.l @(disp,pc),Rn` in img[lo:hi] loads."""
    for off in range(lo, (len(img) if hi is None else hi) - 3, 2):
        op = half(img, off)
        if op >> 12 == 0xd and pool(off, op) + 4 <= len(img):
            yield off, word(img, pool(off, op))


def base_of(rom1):
    """rom.1's base and the ldc's offset: the reset code's first `ldc Rn,vbr`, with the word its mov.l gave Rn."""
    given = {}
    for off in range(RESET, RESET + 0x40, 2):
        op = half(rom1, off)
        if op >> 12 == 0xd:
            given[(op >> 8) & 15] = word(rom1, pool(off, op))
        if op & 0xf0ff == 0x402e:
            return given[(op >> 8) & 15], off
    sys.exit('kernelmap.py: no VBR set in the reset code; is this rom.1?')


def slot_kind(rom1, off):
    """What the 16 bytes at `off` are: ('jump', target) for `mov.l @(d,pc),Rn; jmp @Rn`, ('word', its address) for
    `mov.l @(d,pc),Rn; rts; mov.l @Rn,r0`, ('return', None) for `rts; nop`, None for anything else (pool, fill)."""
    op, nxt, third = half(rom1, off), half(rom1, off + 2), half(rom1, off + 4)
    reg = (op >> 8) & 15
    if op >> 12 == 0xd and nxt == 0x402b | reg << 8:
        return 'jump', word(rom1, pool(off, op))
    if op >> 12 == 0xd and nxt == 0x000b and third == 0x6002 | reg << 4:
        return 'word', word(rom1, pool(off, op))
    if (op, nxt) == (0x000b, 0x0009):
        return 'return', None
    return None


def show_base(rom1, base, at):
    print(f'rom.1: {len(rom1)} bytes. Its reset code (file offset {RESET:#x}) sets VBR to {base:#010x} '
          f'(the ldc at {base + at:#010x}); the bytes below 0x100 are all 0xff: {all(b == 0xff for b in rom1[:0x100])}')
    for v in VECTORS:
        print(f'  VBR+{v:#05x}: {"code" if half(rom1, v) != 0xffff else "empty"} '
              f'({half(rom1, v):04x} {half(rom1, v + 2):04x})')
    for off, w in loads(rom1, RESET, RESET + 0x40):
        if base + RESET <= w < base + RESET + 0x100:
            print(f'  the reset code at {base + off:#010x} loads {w:#010x}, file offset {w - base:#x}: '
                  f'{half(rom1, w - base):04x} {half(rom1, w - base + 2):04x}')


def show_sections(rom1, base):
    print("Its C start-up's words:")
    for name, at in STARTUP.items():
        if isinstance(at, tuple):
            print(f'  {name}: {word(rom1, at[0]):#010x} to {word(rom1, at[1]):#010x}')
        else:
            print(f'  {name}: {word(rom1, at):#010x}')
    src, end, to = (word(rom1, STARTUP[k]) for k in ('data from', 'data from end', 'data to'))
    print(f'  .data is {end - src:#x} bytes, copied to {to:#010x}…{to + end - src:#010x}; its source ends '
          f'{base + len(rom1) - end} byte(s) before the image does')


def show_names(rom1, base):
    text = rom1[DUMP[0]:DUMP[1]].split(b'\0')
    names = [t.decode('latin1').strip() for t in text if t.strip()]
    print(f"Its debug dump's strings ({base + DUMP[0]:#010x}…), {len(names)}:")
    print('  ' + ' | '.join(n.replace('\n', ' ') for n in names))


def slots_of(rom1, base):
    found = {}
    for off in range(TABLE, TABLE_END, SLOT):
        kind = slot_kind(rom1, off)
        if kind:
            found[base + off] = kind
    return found


def show_slots(found):
    pages = sorted({a & ~0xff for a in found})
    odd = [f'{a:#010x} ({k[0]}' + (f' [{k[1]:#010x}])' if k[1] else ')') for a, k in found.items() if k[0] != 'jump']
    print(f'Its service table: {len(found)} slots in {len(pages)} pages from {min(found):#010x} to {max(found):#010x}; '
          f'all jumps but {", ".join(odd)}')


def rom4_slot_words(rom4, base, found):
    """Every word in the table's span rom.4 loads, with where it's loaded; and those not on a slot's start."""
    lo, hi = base + TABLE, base + TABLE_END
    used = {}
    for off, w in loads(rom4):
        if lo <= w < hi:
            used.setdefault(w, []).append(ROM4_BASE + off)
    bad = sorted(w for w in used if w not in found)
    print(f"rom.4 loads {len(used)} distinct words in the table's span; on a slot's start: "
          f'{len(used) - len(bad)}, not: {len(bad)} {" ".join(hex(w) for w in bad)}')
    return used, bad


def show_roles(found, used):
    print('The slots rom.4 reaches, with their routines (and what each does, where read):')
    for a in sorted(used):
        target = found.get(a, (None, None))[1] or 0
        print(f'  {a:#010x} -> {target:#010x}  {len(used[a]):3d} loads (first {used[a][0]:#010x})  {ROLES.get(a, "")}')


def v503_answers(found):
    ways = [int(m, 16) for m in re.findall(r'\{\s*(0x8800[0-9a-f]{4}),', WAYS.read_text(encoding='utf-8'))]
    bad = [a for a in ways if a not in found]
    print(f"Vc2WaveBootKernel.cpp answers {len(ways)} addresses; on a slot's start: {len(ways) - len(bad)}, not: "
          f'{len(bad)} {" ".join(hex(a) for a in bad)}')
    for a in ways:
        print(f'  {a:#010x} -> {found.get(a, (None, 0))[1] or 0:#010x}')
    return bad


def main():
    rom1, rom4 = image(ROM1), image(ROM4)
    base, at = base_of(rom1)
    show_base(rom1, base, at)
    show_sections(rom1, base)
    show_names(rom1, base)
    found = slots_of(rom1, base)
    show_slots(found)
    used, bad4 = rom4_slot_words(rom4, base, found)
    show_roles(found, used)
    bad503 = v503_answers(found)
    sys.exit(1 if bad4 or bad503 else 0)


if __name__ == '__main__':
    main()
