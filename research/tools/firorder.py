#!/usr/bin/env python3
"""firorder.py <listing>: the order of additions in an unrolled FIR, by a
symbolic run of its instructions (v71: the Frequency Shifter's Hilbert FIR,
0x894fdaf4…0x894fdbe2 in the back bank, its loop at 0x894fdb60).  <listing> is
a Disasm.java output covering that range (ghidra.sh Disasm.java out.asm
894fda60 540).  Prints, for each accumulator X12…X15, its (tap j, sample
offset k) terms in order (k from the write index, 0 the newest), plus the
terms the routine adds as each later sample arrives.  For another unrolled sum,
change the range, the loop and the frames' terms below."""
import re, sys
lines = {}
for l in open(sys.argv[1]):
    m = re.match(r'\s+(894f[0-9a-f]+)\s+(.*?)(\s+\[.*)?$', l)
    if m: lines[int(m.group(1), 16)] = m.group(2).strip()
addrs = sorted(a for a in lines if 0x894fdaf4 <= a <= 0x894fdbe2)
reg = {}; acc = {12: [], 13: [], 14: [], 15: []}
off = 0; coef = 0; r6 = None; T = False
i = 0
while i < len(addrs):
    a = addrs[i]; ins = lines[a].lstrip('_'); nxt = i + 1
    if ins.startswith('fldi0 fr'): reg[int(ins[8:])] = ('zero',)
    elif ins == 'fmov.s @r11,fr' + ins[14:] and ins.startswith('fmov.s @r11,fr'): reg[int(ins[14:])] = ('s', off)
    elif ins == 'add #-0x4,r11': off -= 1
    elif ins.startswith('fmov.s @r2+,fr0'): reg[0] = ('c', coef); coef += 1
    elif re.match(r'fmov fr\d+,fr\d+$', ins):
        m = re.match(r'fmov fr(\d+),fr(\d+)', ins); reg[int(m.group(2))] = reg[int(m.group(1))]
    elif ins.startswith('fmac fr0,fr'):
        m = re.match(r'fmac fr0,fr(\d+),fr(\d+)', ins); src, dst = int(m.group(1)), int(m.group(2))
        assert reg[0][0] == 'c' and reg[src][0] == 's', (hex(a), ins, reg[0], reg[src])
        acc[dst].append((reg[0][1], reg[src][1]))
    elif ins == 'mov #0x1b,r6': r6 = 0x1b
    elif ins == 'add #-0x4,r6': r6 -= 4
    elif ins == 'cmp/pz r6': T = r6 >= 0
    elif ins.startswith('bt 0x894fdb60') and T:
        nxt = addrs.index(0x894fdb60)
    i = nxt
# the frames' own terms (frame 1: X13 c0·s+1, X15 c1·s+1; frame 2: X14 c0·s+2; frame 3: X15 c0·s+3)
acc[13].append((0, 1)); acc[15].append((1, 1)); acc[14].append((0, 2)); acc[15].append((0, 3))
for k in range(12, 16):
    t = acc[k]
    mono = all(t[n][0] + 1 == t[n + 1][0] for n in range(len(t) - 1))
    print('X%d: %d terms, first %s, last %s, j consecutive: %s, k = %d - 2j: %s' % (k, len(t), t[:3], t[-3:], mono,
          k - 12, all(kk == (k - 12) - 2 * j for j, kk in t)))
print('coefficients read:', coef)
