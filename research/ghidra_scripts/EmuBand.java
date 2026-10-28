// Run the V-Synth XT's multi-band (BACKING / ENSEMBLE) encoder — rom.2
// driver 0x89516000 — in Ghidra's P-code emulator, stopping at the sample
// writer 0x89512f10, and dump the heap for offline analysis.
// Usage: -postScript EmuBand.java <samp.bin> <channels> <out.heap> [trace.txt hexaddr...]
//   samp.bin  int16 LE samples (interleaved if 2 channels), 44.1 kHz
// Setup as the C startup and the UI would leave it: .data copied from ROM,
// .bss zeroed, frame count [0x8987c948], channels [0x8987c942], sample
// pointer [0x8987c9bc]; pool factor [0x8987ca90] left at the ROM default.
// EMUBAND_FULL=1 stubs the writer and runs on.  Stubs: 0x89511360 (progress/UI) returns; 0x89512570
// (attack-event list from the sample's markers) returns {2, (0,0), (n,n)};
// malloc 0x89527910 / free 0x89527950 / calloc-like 0x89527930 bump-allocate.
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;

public class EmuBand extends GhidraScript {
    EmulatorHelper emu;
    static final long SAMP = 0x8C100000L, EV = 0x8C0F0000L, HEAP = 0x8D000000L, STACK = 0x8C0E0000L,
        SENTINEL = 0x8CF00000L, MALLOC = 0x89527910L, FREE = 0x89527950L, CALLOC = 0x89527930L,
        UI = 0x89511360L, EVLIST = 0x89512570L, WRITER = 0x89512f10L;
    long heap = HEAP;
    PrintWriter trace = null; Set<Long> traceAt = new HashSet<>();
    void wb(long a, byte[] b) throws Exception { emu.writeMemory(toAddr(a), b); }
    void w32(long a, int v)  throws Exception { wb(a, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()); }
    int r32(long a)          throws Exception { return ByteBuffer.wrap(emu.readMemory(toAddr(a), 4)).order(ByteOrder.LITTLE_ENDIAN).getInt(); }
    long reg(String r)       { return emu.readRegister(r).longValue(); }
    void ret(long v)         { emu.writeRegister("r0", v); emu.writeRegister(emu.getPCRegister(), reg("PR")); }
    long alloc(long sz) throws Exception { sz = (sz + 15) & ~15L; long p = heap; wb(p, new byte[(int)Math.min(sz, 1 << 24)]); heap += sz; return p; }

    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        byte[] samp = Files.readAllBytes(Paths.get(a[0]));
        int ch = Integer.parseInt(a[1]);
        int n = samp.length / (2 * ch);
        emu = new EmulatorHelper(currentProgram);
        for (long bp : new long[]{ SENTINEL, MALLOC, FREE, CALLOC, UI, EVLIST, WRITER }) emu.setBreakpoint(toAddr(bp));
        if (a.length > 3) {
            trace = new PrintWriter(new FileWriter(a[3]));
            for (int i = 4; i < a.length; ++i) { long t = Long.parseLong(a[i], 16); traceAt.add(t); emu.setBreakpoint(toAddr(t)); }
        }
        try {
            // C startup (entry 0x89300040): .data image ROM [0x89834018, 0x8985858c)
            // → RAM [0x8985858c, 0x8987cb00); zero [0x8987cb00, 0x8a01ceb4).
            wb(0x8985858cL, emu.readMemory(toAddr(0x89834018L), 0x8985858c - 0x89834018));
            wb(0x8987cb00L, new byte[0x8a01ceb4 - 0x8987cb00]);
            wb(SAMP, samp); wb(SAMP + samp.length, new byte[1 << 16]);
            w32(0x8987c948L, n); wb(0x8987c942L, new byte[]{ (byte)ch }); w32(0x8987c9bcL, (int)SAMP);
            // pool factor [0x8987ca90]: ROM default (5); more chunks are allocated as needed
            int[] ev = { 2, 0, 0, n, n };
            for (int i = 0; i < ev.length; ++i) w32(EV + 4 * i, ev[i]);
            emu.writeRegister("r15", STACK); emu.writeRegister("r0", 0x00040001L);
            emu.writeRegister("FPSCR", 0x00040001L); emu.writeRegister("FPSCR_PR", 0); emu.writeRegister("FPSCR_SZ", 0);
            emu.writeRegister("PR", SENTINEL);
            emu.writeRegister(emu.getPCRegister(), 0x89516000L);
            long t0 = System.currentTimeMillis();
            String why = "";
            while (true) {
                emu.run(monitor);
                long at = emu.readRegister(emu.getPCRegister()).longValue();
                if (at == SENTINEL) { why = "returned r0=" + (int)reg("r0"); break; }
                if (at == MALLOC) { ret(alloc(reg("r4") & 0xffffffffL)); continue; }
                if (at == CALLOC) { ret(alloc((reg("r4") & 0xffffffffL) * (reg("r5") & 0xffffffffL))); continue; }
                if (at == FREE || at == UI) { ret(0); continue; }
                if (at == EVLIST) { ret(EV); continue; }
                if (at == WRITER) {
                    if (System.getenv("EMUBAND_FULL") == null) { why = "reached writer 0x89512f10"; break; }
                    // dump at the writer, then stub it (marker rescale; returns 1) and run on
                    try (FileOutputStream f = new FileOutputStream(a[2] + ".atwriter")) { f.write(emu.readMemory(toAddr(HEAP), (int)(heap - HEAP))); }
                    ret(1); continue;
                }
                if (traceAt.contains(at)) {
                    trace.printf("%x r0=%d r4=%d r5=%d r6=%d r7=%d pr=%x%n", at, (int)reg("r0"), (int)reg("r4"), (int)reg("r5"),
                                 (int)reg("r6"), (int)reg("r7"), reg("PR"));
                    emu.step(monitor); continue;
                }
                throw new RuntimeException("stopped at " + Long.toHexString(at) + " " + emu.getLastError());
            }
            println(why + " in " + (System.currentTimeMillis() - t0) + " ms; heap " + (heap - HEAP) + " bytes");
            try (FileOutputStream f = new FileOutputStream(a[2])) { f.write(emu.readMemory(toAddr(HEAP), (int)(heap - HEAP))); }
            // also the encoder's work buffer and band globals
            int cab0 = r32(0x8987cab0L);
            println(String.format("[8987cab0]=%08x [89ff0fd4]=%08x [89ff0ff4]=%08x [8a015480]=%08x", cab0, r32(0x89ff0fd4L), r32(0x89ff0ff4L), r32(0x8a015480L)));
            // band buffers collected by 0x89515960 (multi-band case): [0x89ff0dd4 + 4k], k = 0..6; total [0x89ff0df0]
            StringBuilder sb = new StringBuilder("bands:");
            for (int k = 0; k < 7; ++k) sb.append(String.format(" %08x", r32(0x89ff0dd4L + 4 * k)));
            println(sb + String.format("  [89ff0df0]=%d", r32(0x89ff0df0L)));
            try (FileOutputStream f = new FileOutputStream(a[2] + ".ram")) { f.write(emu.readMemory(toAddr(0x89ff0000L), 0x30000)); }
            if (trace != null) trace.close();
        } finally { emu.dispose(); }
    }
}
