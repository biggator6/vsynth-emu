#!/usr/bin/env python3
"""diskls.py [filter] | diskls.py get <path in the image> <out file>: the firmware's disk image's FAT16 partition (behind
its MBR), every file and folder with long names: size, first cluster, path, filtered by a substring of the path; or one
file copied out. $DISK picks the image (default firmware/unpacked/V-SynthXTDiskImage.bin.0.bin). A chain ends at a
cluster it has been through, as XtDiskImage's does. The preset waves past the partition are in no file (xt_files)."""
import os
import struct
import sys
from pathlib import Path

DEFAULT_DISK = Path(__file__).resolve().parents[2] / 'firmware/unpacked/V-SynthXTDiskImage.bin.0.bin'
DISK = os.environ.get('DISK', str(DEFAULT_DISK))
with open(DISK, 'rb') as f:
    img = f.read()
start = struct.unpack_from('<I', img, 0x1be + 8)[0] * 512
per_sector, per_cluster, reserved, fats, roots = struct.unpack_from('<HBHBH', img, start + 11)
fat_sectors = struct.unpack_from('<H', img, start + 22)[0]
fat = start + reserved * per_sector
root = fat + fats * fat_sectors * per_sector
data = root + roots * 32
cluster = per_cluster * per_sector


def chain(c):
    seen = set()
    while 2 <= c < 0xfff8 and c not in seen:
        seen.add(c)
        yield c
        c = struct.unpack_from('<H', img, fat + 2 * c)[0]


def read(c, size=None):
    whole = b''.join(img[data + (k - 2) * cluster:data + (k - 1) * cluster] for k in chain(c))
    return whole if size is None else whole[:size]


def entries(block):
    long_name = []
    for i in range(0, len(block), 32):
        e = block[i:i + 32]
        if e[0] == 0:
            return
        if e[0] == 0xe5:
            long_name = []
            continue
        if e[11] == 0x0f:
            long_name.insert(0, e[1:11] + e[14:26] + e[28:32])
            continue
        short = e[0:8].decode('latin1').rstrip() + ('.' + e[8:11].decode('latin1').rstrip() if e[8:11].strip() else '')
        name = b''.join(long_name).decode('utf-16le', 'ignore').split('\x00')[0] if long_name else short
        long_name = []
        if name in ('.', '..') or e[11] & 0x08:
            continue
        yield name, e[11] & 0x10, struct.unpack_from('<H', e, 26)[0], struct.unpack_from('<I', e, 28)[0]


def walk(block, path):
    for name, is_dir, c, size in entries(block):
        full = f'{path}/{name}'
        yield full, c, size
        if is_dir:
            yield from walk(read(c), full)


files = walk(img[root:root + roots * 32], '')
if sys.argv[1:2] == ['get']:
    wanted, out = sys.argv[2], sys.argv[3]
    found = [(c, size) for full, c, size in files if full == wanted]
    if not found:
        sys.exit(f'diskls.py: no {wanted} in {DISK}')
    with open(out, 'wb') as f:
        f.write(read(*found[0]))
else:
    needle = sys.argv[1].lower() if len(sys.argv) > 1 else ''
    for full, c, size in files:
        if needle in full.lower():
            print(f'{size:9d} {c:6d} {full}')
