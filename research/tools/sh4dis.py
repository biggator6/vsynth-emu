#!/usr/bin/env python3
"""sh4dis.py <addr> <count> [<addr> <count> ...]: a linear SH-4 listing of rom.2 with literal-pool
values and branch targets, without Ghidra (whose project will not open under a path holding .claude,
as a worktree's does).  Decodes whatever is there, pools included; FP register names are as if
SZ=0, PR=0.  Listings go in build/, never in the repo.  As a module: dis(pc) -> (text, info)."""
import struct, sys, os
ROMP = os.environ.get('ROM', os.path.join(os.path.dirname(os.path.abspath(__file__)), '../../firmware/unpacked/V-SynthXT.rom.2.bin'))
rom = open(ROMP, 'rb').read()
B = 0x89300000
def u16(a): return struct.unpack_from('<H', rom, a - B)[0]
def u32(a): return struct.unpack_from('<I', rom, a - B)[0]
def s16(a): return struct.unpack_from('<h', rom, a - B)[0]
def f32(a): return struct.unpack_from('<f', rom, a - B)[0]
def sx8(v): return v - 256 if v & 0x80 else v
def sx12(v): return v - 4096 if v & 0x800 else v
def cstr(a, n=60):
    o = a - B
    if not (0 <= o < len(rom)): return None
    e = rom.find(b'\0', o, o + n)
    if e < 0: return None
    s = rom[o:e]
    if len(s) >= 3 and all(32 <= c < 127 for c in s): return s.decode('latin1')
    return None
R = lambda n: 'r%d' % n
FR = lambda n: 'fr%d' % n
def dis(pc):
    """returns (text, dict(kind, target, pool))"""
    op = u16(pc); a = op >> 12; n = (op >> 8) & 15; m = (op >> 4) & 15; d = op & 15; i8 = op & 255
    info = {}
    t = None
    if a == 0:
        l = op & 15
        if l == 2:
            t = {0: 'stc sr,%s', 1: 'stc gbr,%s', 2: 'stc vbr,%s', 3: 'stc ssr,%s', 4: 'stc spc,%s'}.get(m)
            t = t % R(n) if t else 'stc r%d_bank,%s' % (m & 7, R(n))
        elif l == 3:
            t = {0: 'bsrf %s', 2: 'braf %s', 8: 'pref @%s', 9: 'ocbi @%s', 10: 'ocbp @%s', 11: 'ocbwb @%s', 12: 'movca.l r0,@%s'}.get(m)
            t = t % R(n) if t else None
            if m in (0, 2): info['kind'] = 'callr' if m == 0 else 'jmpr'
        elif l == 4: t = 'mov.b %s,@(r0,%s)' % (R(m), R(n))
        elif l == 5: t = 'mov.w %s,@(r0,%s)' % (R(m), R(n))
        elif l == 6: t = 'mov.l %s,@(r0,%s)' % (R(m), R(n))
        elif l == 7: t = 'mul.l %s,%s' % (R(m), R(n))
        elif l == 8 and n == 0: t = {0: 'clrt', 1: 'sett', 2: 'clrmac', 3: 'ldtlb', 4: 'clrs', 5: 'sets'}.get(m)
        elif l == 9:
            if n == 0 and m == 0: t = 'nop'
            elif n == 0 and m == 1: t = 'div0u'
            elif m == 2: t = 'movt %s' % R(n)
        elif l == 10:
            t = {0: 'sts mach,%s', 1: 'sts macl,%s', 2: 'sts pr,%s', 5: 'sts fpul,%s', 6: 'sts fpscr,%s', 15: 'stc dbr,%s'}.get(m)
            t = t % R(n) if t else None
        elif l == 11 and n == 0:
            t = {0: 'rts', 1: 'sleep', 2: 'rte'}.get(m)
            if m == 0: info['kind'] = 'rts'
        elif l == 12: t = 'mov.b @(r0,%s),%s' % (R(m), R(n))
        elif l == 13: t = 'mov.w @(r0,%s),%s' % (R(m), R(n))
        elif l == 14: t = 'mov.l @(r0,%s),%s' % (R(m), R(n))
        elif l == 15: t = 'mac.l @%s+,@%s+' % (R(m), R(n))
    elif a == 1: t = 'mov.l %s,@(0x%x,%s)' % (R(m), d * 4, R(n))
    elif a == 2:
        names = {0: 'mov.b %s,@%s', 1: 'mov.w %s,@%s', 2: 'mov.l %s,@%s', 4: 'mov.b %s,@-%s', 5: 'mov.w %s,@-%s', 6: 'mov.l %s,@-%s',
                 7: 'div0s %s,%s', 8: 'tst %s,%s', 9: 'and %s,%s', 10: 'xor %s,%s', 11: 'or %s,%s', 12: 'cmp/str %s,%s', 13: 'xtrct %s,%s', 14: 'mulu.w %s,%s', 15: 'muls.w %s,%s'}
        if d in names: t = names[d] % (R(m), R(n))
    elif a == 3:
        names = {0: 'cmp/eq', 2: 'cmp/hs', 3: 'cmp/ge', 4: 'div1', 5: 'dmulu.l', 6: 'cmp/hi', 7: 'cmp/gt', 8: 'sub', 10: 'subc', 11: 'subv', 12: 'add', 13: 'dmuls.l', 14: 'addc', 15: 'addv'}
        if d in names: t = '%s %s,%s' % (names[d], R(m), R(n))
    elif a == 4:
        tb = {0x00: 'shll %s', 0x01: 'shlr %s', 0x02: 'sts.l mach,@-%s', 0x03: 'stc.l sr,@-%s', 0x04: 'rotl %s', 0x05: 'rotr %s', 0x06: 'lds.l @%s+,mach', 0x07: 'ldc.l @%s+,sr',
              0x08: 'shll2 %s', 0x09: 'shlr2 %s', 0x0a: 'lds %s,mach', 0x0b: 'jsr @%s', 0x0e: 'ldc %s,sr',
              0x10: 'dt %s', 0x11: 'cmp/pz %s', 0x12: 'sts.l macl,@-%s', 0x13: 'stc.l gbr,@-%s', 0x15: 'cmp/pl %s', 0x16: 'lds.l @%s+,macl', 0x17: 'ldc.l @%s+,gbr',
              0x18: 'shll8 %s', 0x19: 'shlr8 %s', 0x1a: 'lds %s,macl', 0x1b: 'tas.b @%s', 0x1e: 'ldc %s,gbr',
              0x20: 'shal %s', 0x21: 'shar %s', 0x22: 'sts.l pr,@-%s', 0x23: 'stc.l vbr,@-%s', 0x24: 'rotcl %s', 0x25: 'rotcr %s', 0x26: 'lds.l @%s+,pr', 0x27: 'ldc.l @%s+,vbr',
              0x28: 'shll16 %s', 0x29: 'shlr16 %s', 0x2a: 'lds %s,pr', 0x2b: 'jmp @%s', 0x2e: 'ldc %s,vbr',
              0x52: 'sts.l fpul,@-%s', 0x62: 'sts.l fpscr,@-%s', 0x56: 'lds.l @%s+,fpul', 0x66: 'lds.l @%s+,fpscr', 0x5a: 'lds %s,fpul', 0x6a: 'lds %s,fpscr', 0xf2: 'stc.l dbr,@-%s', 0xf6: 'ldc.l @%s+,dbr', 0xfa: 'ldc %s,dbr'}
        k = op & 0xff
        if k in tb:
            t = tb[k] % R(n)
            if k == 0x0b: info['kind'] = 'callr'
            if k == 0x2b: info['kind'] = 'jmpr'
        elif d == 12: t = 'shad %s,%s' % (R(m), R(n))
        elif d == 13: t = 'shld %s,%s' % (R(m), R(n))
        elif d == 15: t = 'mac.w @%s+,@%s+' % (R(m), R(n))
    elif a == 5: t = 'mov.l @(0x%x,%s),%s' % (d * 4, R(m), R(n))
    elif a == 6:
        names = {0: 'mov.b @%s,%s', 1: 'mov.w @%s,%s', 2: 'mov.l @%s,%s', 3: 'mov %s,%s', 4: 'mov.b @%s+,%s', 5: 'mov.w @%s+,%s', 6: 'mov.l @%s+,%s', 7: 'not %s,%s', 8: 'swap.b %s,%s', 9: 'swap.w %s,%s', 10: 'negc %s,%s', 11: 'neg %s,%s', 12: 'extu.b %s,%s', 13: 'extu.w %s,%s', 14: 'exts.b %s,%s', 15: 'exts.w %s,%s'}
        t = names[d] % (R(m), R(n))
    elif a == 7: t = 'add #%d,%s' % (sx8(i8), R(n))
    elif a == 8:
        s = n
        if s == 0: t = 'mov.b r0,@(0x%x,%s)' % (d, R(m))
        elif s == 1: t = 'mov.w r0,@(0x%x,%s)' % (d * 2, R(m))
        elif s == 4: t = 'mov.b @(0x%x,%s),r0' % (d, R(m))
        elif s == 5: t = 'mov.w @(0x%x,%s),r0' % (d * 2, R(m))
        elif s == 8: t = 'cmp/eq #%d,r0' % sx8(i8)
        elif s in (9, 11, 13, 15):
            tg = pc + 4 + sx8(i8) * 2
            t = {9: 'bt', 11: 'bf', 13: 'bt/s', 15: 'bf/s'}[s] + ' 0x%08x' % tg
            info['kind'] = 'cond'; info['target'] = tg; info['ds'] = s in (13, 15)
    elif a == 9:
        ad = pc + 4 + i8 * 2; v = u16(ad)
        t = 'mov.w @(0x%08x),%s' % (ad, R(n)); info['pool'] = ad; info['poolv'] = v; info['pools'] = 2
    elif a == 10:
        tg = pc + 4 + sx12(op & 0xfff) * 2; t = 'bra 0x%08x' % tg; info['kind'] = 'bra'; info['target'] = tg
    elif a == 11:
        tg = pc + 4 + sx12(op & 0xfff) * 2; t = 'bsr 0x%08x' % tg; info['kind'] = 'call'; info['target'] = tg
    elif a == 12:
        s = n
        if s == 0: t = 'mov.b r0,@(0x%x,gbr)' % i8
        elif s == 1: t = 'mov.w r0,@(0x%x,gbr)' % (i8 * 2)
        elif s == 2: t = 'mov.l r0,@(0x%x,gbr)' % (i8 * 4)
        elif s == 3: t = 'trapa #%d' % i8
        elif s == 4: t = 'mov.b @(0x%x,gbr),r0' % i8
        elif s == 5: t = 'mov.w @(0x%x,gbr),r0' % (i8 * 2)
        elif s == 6: t = 'mov.l @(0x%x,gbr),r0' % (i8 * 4)
        elif s == 7:
            ad = (pc & ~3) + 4 + i8 * 4; t = 'mova @(0x%08x),r0' % ad; info['mova'] = ad
        elif s == 8: t = 'tst #%d,r0' % i8
        elif s == 9: t = 'and #%d,r0' % i8
        elif s == 10: t = 'xor #%d,r0' % i8
        elif s == 11: t = 'or #%d,r0' % i8
        elif s == 12: t = 'tst.b #%d,@(r0,gbr)' % i8
        elif s == 13: t = 'and.b #%d,@(r0,gbr)' % i8
        elif s == 14: t = 'xor.b #%d,@(r0,gbr)' % i8
        elif s == 15: t = 'or.b #%d,@(r0,gbr)' % i8
    elif a == 13:
        ad = (pc & ~3) + 4 + i8 * 4; v = u32(ad) if ad - B + 4 <= len(rom) else 0
        t = 'mov.l @(0x%08x),%s' % (ad, R(n)); info['pool'] = ad; info['poolv'] = v; info['pools'] = 4
    elif a == 14: t = 'mov #%d,%s' % (sx8(i8), R(n))
    elif a == 15:
        names = {0: 'fadd', 1: 'fsub', 2: 'fmul', 3: 'fdiv', 4: 'fcmp/eq', 5: 'fcmp/gt', 12: 'fmov'}
        if d in names: t = '%s %s,%s' % (names[d], FR(m), FR(n))
        elif d == 6: t = 'fmov.s @(r0,%s),%s' % (R(m), FR(n))
        elif d == 7: t = 'fmov.s %s,@(r0,%s)' % (FR(m), R(n))
        elif d == 8: t = 'fmov.s @%s,%s' % (R(m), FR(n))
        elif d == 9: t = 'fmov.s @%s+,%s' % (R(m), FR(n))
        elif d == 10: t = 'fmov.s %s,@%s' % (FR(m), R(n))
        elif d == 11: t = 'fmov.s %s,@-%s' % (FR(m), R(n))
        elif d == 14: t = 'fmac fr0,%s,%s' % (FR(m), FR(n))
        elif d == 13:
            names = {0: 'fsts fpul,%s', 1: 'flds %s,fpul', 2: 'float fpul,%s', 3: 'ftrc %s,fpul', 4: 'fneg %s', 5: 'fabs %s', 6: 'fsqrt %s', 7: 'fsrra %s', 8: 'fldi0 %s', 9: 'fldi1 %s', 10: 'fcnvsd fpul,dr%d', 11: 'fcnvds dr%d,fpul'}
            if m in names: t = names[m] % (n if m in (10, 11) else FR(n))
            elif m == 14: t = 'fipr fv%d,fv%d' % ((n & 3) * 4, (n >> 2) * 4)
            elif m == 15:
                if n == 0xb: t = 'frchg'
                elif n == 3: t = 'fschg'
                elif n & 3 == 1: t = 'ftrv xmtrx,fv%d' % ((n >> 2) * 4)
                elif n & 1 == 0: t = 'fsca fpul,dr%d' % n
    if t is None: t = '.word 0x%04x' % op; info['bad'] = True
    return t, info
def fmt(pc):
    t, info = dis(pc)
    extra = ''
    if 'pool' in info:
        v = info['poolv']
        extra = '  ; =0x%x' % v
        if info['pools'] == 4:
            fv = struct.unpack('<f', struct.pack('<I', v))[0]
            if 0x30000000 <= v < 0x50000000 or 0xb0000000 <= v < 0xd0000000: extra += ' (%g)' % fv
            s = cstr(v)
            if s: extra += ' "%s"' % s
        else:
            sv = v - 65536 if v & 0x8000 else v
            extra += ' (%d)' % sv
    if 'mova' in info:
        extra = '  ; ->0x%08x' % info['mova']
    return '%08x  %04x  %-34s%s' % (pc, u16(pc), t, extra)
def listing(a, n):
    for i in range(n):
        print(fmt(a + 2 * i))
if __name__ == '__main__':
    args = sys.argv[1:]
    for k in range(0, len(args), 2):
        a = int(args[k], 16); n = int(args[k + 1])
        print('=== %08x' % a)
        listing(a & ~1, n)
