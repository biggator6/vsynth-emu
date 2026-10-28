// Run the V-Synth XT's sample-load band rebuild for a BACKING (type 3) or
// ENSEMBLE (type 5) wave in Ghidra's P-code emulator, and dump the band data
// the players read.
// Usage: -postScript EmuLoad.java <samp.bin> <channels> <type> <out>
//   samp.bin  int16 LE samples (interleaved if 2 channels), 44.1 kHz
//   writes <out>.samp (sample memory from 0x8C100000), <out>.heap (from
//   0x8D000000) and prints the seven band pointers.
// Sequence as the loader runs it: 0x89529e90(ctx, info) — copies {type,
// samples, frames, channels} and calls the type's init (3: 0x89522af0,
// 5: 0x89525360) — then 0x894a9940(L): the type's chunk routine
// (0x89529f20 → 3: 0x89522e20, 5: 0x89525640) over 0x2000-frame chunks,
// the finish 0x89529f60 (5: 0x895255c0) and the band pointers 0x8952a1c0(k)
// (3: 0x89522dc0, 5: 0x89525550) into L+0x10 + 4k, k = 0..6.
// Stubs: malloc 0x89527910 / calloc-like 0x89527930 / free 0x89527950 (bump
// allocator), 0x895278f0 and the UI hook 0x89511360 return 0.
// Memory is set up as the C startup leaves it (.data from ROM, .bss zeroed).
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import java.io.*;
import java.nio.*;
import java.nio.file.*;

public class EmuLoad extends GhidraScript {
    EmulatorHelper emu;
    static final long SAMP = 0x8C100000L, L = 0x8C0F0000L, INFO = 0x8C0F0100L, HEAP = 0x8D000000L,
        STACK = 0x8C0E0000L, SENTINEL = 0x8CF00000L, MALLOC = 0x89527910L, CALLOC = 0x89527930L,
        FREE = 0x89527950L, OSINIT = 0x895278f0L, UI = 0x89511360L;
    long heap = HEAP;
    void wb(long a, byte[] b) throws Exception { emu.writeMemory(toAddr(a), b); }
    void w32(long a, int v)  throws Exception { wb(a, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()); }
    int r32(long a)          throws Exception { return ByteBuffer.wrap(emu.readMemory(toAddr(a), 4)).order(ByteOrder.LITTLE_ENDIAN).getInt(); }
    long reg(String r)       { return emu.readRegister(r).longValue(); }
    void ret(long v)         { emu.writeRegister("r0", v); emu.writeRegister(emu.getPCRegister(), reg("PR")); }
    long alloc(long sz) throws Exception { sz = (sz + 15) & ~15L; long p = heap; wb(p, new byte[(int)Math.min(sz, 1 << 24)]); heap += sz; return p; }

    void call(long pc, long r4, long r5) throws Exception {
        emu.writeRegister("r4", r4); emu.writeRegister("r5", r5);
        emu.writeRegister("r15", STACK); emu.writeRegister("r0", 0x00040001L);
        emu.writeRegister("FPSCR", 0x00040001L); emu.writeRegister("FPSCR_PR", 0); emu.writeRegister("FPSCR_SZ", 0);
        emu.writeRegister("PR", SENTINEL);
        emu.writeRegister(emu.getPCRegister(), pc);
        while (true) {
            emu.run(monitor);
            long at = emu.readRegister(emu.getPCRegister()).longValue();
            if (at == SENTINEL) return;
            if (at == MALLOC) { ret(alloc(reg("r4") & 0xffffffffL)); continue; }
            if (at == CALLOC) { ret(alloc((reg("r4") & 0xffffffffL) * Math.max(1, reg("r5") & 0xffffffffL))); continue; }
            if (at == FREE || at == OSINIT || at == UI) { ret(0); continue; }
            throw new RuntimeException("stopped at " + Long.toHexString(at) + " " + emu.getLastError());
        }
    }

    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        byte[] samp = Files.readAllBytes(Paths.get(a[0]));
        int ch = Integer.parseInt(a[1]), type = Integer.parseInt(a[2]);
        int n = samp.length / (2 * ch);
        emu = new EmulatorHelper(currentProgram);
        for (long bp : new long[]{ SENTINEL, MALLOC, CALLOC, FREE, OSINIT, UI }) emu.setBreakpoint(toAddr(bp));
        try {
            wb(0x8985858cL, emu.readMemory(toAddr(0x89834018L), 0x8985858c - 0x89834018));
            wb(0x8987cb00L, new byte[0x8a01ceb4 - 0x8987cb00]);
            wb(SAMP, samp); wb(SAMP + samp.length, new byte[0x80000]);
            wb(L, new byte[0x100]);
            w32(L + 4, n);
            int[] info = { type, (int)SAMP, n, ch, 0, 0, 0 };
            for (int i = 0; i < info.length; ++i) w32(INFO + 4 * i, info[i]);
            long t0 = System.currentTimeMillis();
            call(0x89529e90L, L + 0x34, INFO);
            println("init r0=" + (int)reg("r0"));
            call(0x894a9940L, L, 0);
            println("load done in " + (System.currentTimeMillis() - t0) + " ms; heap " + (heap - HEAP) + " bytes");
            StringBuilder sb = new StringBuilder("bands:");
            long end = SAMP + samp.length;
            for (int k = 0; k < 7; ++k) {
                long p = r32(L + 0x10 + 4 * k) & 0xffffffffL;
                sb.append(String.format(" %08x", p));
                if (p >= SAMP && p < HEAP) end = Math.max(end, p);
            }
            println(sb.toString());
            end = Math.min(end + 0x40000, HEAP);
            try (FileOutputStream f = new FileOutputStream(a[3] + ".samp")) { f.write(emu.readMemory(toAddr(SAMP), (int)(end - SAMP))); }
            try (FileOutputStream f = new FileOutputStream(a[3] + ".heap")) { f.write(emu.readMemory(toAddr(HEAP), (int)(heap - HEAP))); }
        } finally { emu.dispose(); }
    }
}
