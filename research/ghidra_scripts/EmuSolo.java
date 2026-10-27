// Render a whole SOLO note with the firmware's own player (0x8950cd90, one
// 32-sample block per call) in Ghidra's P-code emulator.
// Usage: -postScript EmuSolo.java <recs.bin> <samp.bin> <out.f32> <R> <F> <walk> [energy] [maxOut] [trace.tsv] [traceFrom]
//   recs.bin  raw vpdt records {u16 len/16, u16 info} incl. the terminator
//   samp.bin  int16 LE mono samples (the wave)
//   out.f32   float32 output, scaled to ±1 (left channel; mono path writes L = R)
//   R pitch ratio (+0x70), F formant ratio (+0x74), walk = 1/stretch (+0x7c)
// Note-on state is built by hand following 0x8950fba0 (which also touches
// global RAM via 0x8950fa60): first record loaded by 0x8950d4c0, grain params
// by 0x8950d5a0, timer/ages 0, channel B window at 0.5, q 1.2, cutoff 0.
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import java.io.*;
import java.nio.*;
import java.nio.file.*;

public class EmuSolo extends GhidraScript {
    EmulatorHelper emu;
    static final long R8 = 0x8C000000L, R9 = R8 + 0xe8, R10 = R9 + 0x3c, W2 = 0x8C001000L,
        REC = 0x8C010000L, SAMP = 0x8C100000L, OUT = 0x8C800000L, STACK = 0x8C7F0000L,
        SENTINEL = 0x8CF00000L;
    void wb(long a, byte[] b) throws Exception { emu.writeMemory(toAddr(a), b); }
    void w32(long a, int v)  throws Exception { wb(a, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()); }
    void wf(long a, float v) throws Exception { w32(a, Float.floatToRawIntBits(v)); }
    void w8(long a, int v)   throws Exception { wb(a, new byte[]{(byte)v}); }
    int r32(long a)          throws Exception { return ByteBuffer.wrap(emu.readMemory(toAddr(a), 4)).order(ByteOrder.LITTLE_ENDIAN).getInt(); }
    float rf(long a)         throws Exception { return Float.intBitsToFloat(r32(a)); }

    void call(long pc, long r4, long r5) throws Exception {
        emu.writeRegister("r8", R8); emu.writeRegister("r9", R9); emu.writeRegister("r10", R10);
        emu.writeRegister("r4", r4); emu.writeRegister("r5", r5);
        emu.writeRegister("r0", 0x00040001L);
        emu.writeRegister("r15", STACK);
        emu.writeRegister("FPSCR", 0x00040001L);
        emu.writeRegister("FPSCR_PR", 0); emu.writeRegister("FPSCR_SZ", 0);
        emu.writeRegister("PR", SENTINEL);
        emu.writeRegister(emu.getPCRegister(), pc);
        long at;
        while (true) {
            emu.run(monitor);
            at = emu.readRegister(emu.getPCRegister()).longValue();
            if (at == 0x8950d380L && trace != null) { dumpState(); emu.step(monitor); continue; }
            break;
        }
        if (at != SENTINEL) throw new RuntimeException("stopped at " + Long.toHexString(at) + " " + emu.getLastError());
    }

    PrintWriter trace = null;
    int traceN = 0, traceFrom = 0;
    void dumpState() throws Exception {
        if (traceN < traceFrom) { traceN++; return; }
        // at 0x8950d380 entry: this sample's walk/record/trigger (0x8950ce80) is done
        trace.printf("%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%d\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%d\t%d%n",
            traceN++, r32(R8 + 0x38), r32(R9 + 0x28), r32(R9 + 0x20), r32(R9 + 0x24), r32(R9 + 0x2c),
            r32(R10 + 0xc), r32(R10 + 4), r32(R10 + 0x10), r32(R10 + 0x14), r32(R10 + 0x18), r32(R10 + 0x1c),
            emu.readMemory(toAddr(R9 + 4), 1)[0] & 1,
            rf(R10 + 0x44), rf(R10 + 0x48), rf(R10 + 0x2c), rf(R10 + 0x30), rf(R10 + 0x38), rf(R10 + 0x3c), rf(R10 + 0x40),
            r32(R9 + 0x34), r32(R9 + 0x38));
    }

    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        byte[] recs = Files.readAllBytes(Paths.get(a[0]));
        byte[] samp = Files.readAllBytes(Paths.get(a[1]));
        float R = Float.parseFloat(a[3]), F = Float.parseFloat(a[4]), walk = Float.parseFloat(a[5]);
        float energy = a.length > 6 ? Float.parseFloat(a[6]) : 0f;
        int n = samp.length / 2;
        int outLen = a.length > 7 && Integer.parseInt(a[7]) > 0 ? Integer.parseInt(a[7]) : Math.max(1, Math.round(n / walk));
        emu = new EmulatorHelper(currentProgram);
        emu.setBreakpoint(toAddr(SENTINEL));
        if (a.length > 9) traceFrom = Integer.parseInt(a[9]);
        if (a.length > 8) {
            trace = new PrintWriter(new BufferedWriter(new FileWriter(a[8])));
            trace.println("t\tP\tanchor\trecStart\trecEnd\tC\ttimer\tppw256\tageA\tageB\tstartA\tstartB\ttoggle\teA\teB\tvelA\tvelB\ttapWA\ttapWB\tslope\ttapA\ttapB");
        }
        try {
            wb(R8, new byte[0x400]); wb(W2, new byte[0x400]);
            wb(REC, recs); wb(SAMP, samp); wb(SAMP + samp.length, new byte[1 << 16]);
            // wave-2 struct (r8+0x6c): no loop points / cache bounds
            w32(W2 + 0x16c, 0x7fffffff); w32(W2 + 0xb8, 0x7fffffff);
            // player
            w8(R8 + 3, 0x80); w8(R8 + 7, 8); w8(R8 + 8, 10); w8(R8 + 9, 6);
            w32(R8 + 0x10, (int)REC);
            w32(R8 + 0x18, 0); w32(R8 + 0x1c, n * 16); w32(R8 + 0x20, n * 16);
            w32(R8 + 0x28, 0x7fffffff); w32(R8 + 0x2c, 0x7fffffff);
            w32(R8 + 0x38, 0); w32(R8 + 0x3c, 0);
            w32(R8 + 0x48, 44100); w32(R8 + 0x4c, 44100);           // rate ratio 1 (0x8950f860)
            w32(R8 + 0x50, (int)(5.644799e11 / 44100)); w32(R8 + 0x54, 2 * (int)(5.644799e11 / 44100));
            w32(R8 + 0x6c, (int)W2);
            wf(R8 + 0x70, R); wf(R8 + 0x98, R); wf(R8 + 0x74, F); wf(R8 + 0x78, energy);
            wf(R8 + 0x7c, walk); wf(R8 + 0x8c, 1f); wf(R8 + 0x90, 1f); wf(R8 + 0x9c, 1f);
            wf(R8 + 0x80, 1f / 65536f); wf(R8 + 0x84, 1f / 65536f); wf(R8 + 0x88, 1f / 64f);
            // record state
            w32(R9 + 0, (int)SAMP);
            w32(R9 + 0x10, 0x7fffffff);                              // no checkpoint jumps
            w32(R9 + 0x18, (int)(W2 + 0x300));
            w32(R9 + 0x1c, (int)REC); w32(R9 + 0x24, 0); w32(R9 + 0x28, 0);
            PrintWriter tr = trace; trace = null;                    // no tracing during setup
            call(0x8950d4c0L, R8, R9);                               // load record 0
            w32(R10 + 0, 256);
            call(0x8950d5a0L, R8, R9);                               // grain params
            // 0x8950fba0 tail: timer = ages = (anchor − recStart)·256 = 0
            w32(R10 + 0xc, 0); w32(R10 + 0x10, 0); w32(R10 + 0x14, 0);
            w32(R10 + 0x18, r32(R9 + 0x20)); w32(R10 + 0x1c, r32(R9 + 0x20));
            w32(R10 + 0x20, n - 1);
            wf(R10 + 0x44, 0f); wf(R10 + 0x48, 0.5f);
            w32(R10 + 0x2c, r32(R10 + 0x28)); w32(R10 + 0x30, r32(R10 + 0x28));
            w32(R10 + 0x38, r32(R10 + 0x34)); w32(R10 + 0x3c, r32(R10 + 0x34));
            w32(R9 + 0x34, r32(R9 + 0x2c)); w32(R9 + 0x38, r32(R9 + 0x2c));
            wf(R10 + 0x24, 0f); wf(R10 + 0x4c, 0f); wf(R10 + 0x50, 1.2f);
            trace = tr;
            if (trace != null) emu.setBreakpoint(toAddr(0x8950d380L));

            DataOutputStream out = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(a[2])));
            long t0 = System.currentTimeMillis();
            ByteBuffer le = ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN);
            for (int done = 0; done < outLen; done += 32) {
                call(0x8950cd90L, OUT, R8);
                byte[] blk = emu.readMemory(toAddr(OUT), 32 * 8);
                ByteBuffer bb = ByteBuffer.wrap(blk).order(ByteOrder.LITTLE_ENDIAN);
                for (int i = 0; i < 32 && done + i < outLen; ++i) {
                    float l = bb.getFloat(i * 8);
                    le.clear(); le.putFloat(l / 32768f); out.write(le.array());
                }
                if ((done & 0x7fff) == 0) println("block " + (done / 32) + " of " + (outLen / 32) + "  " + (System.currentTimeMillis() - t0) + " ms");
            }
            out.close();
            if (trace != null) trace.close();
            println("done " + outLen + " samples in " + (System.currentTimeMillis() - t0) + " ms");
        } finally { emu.dispose(); }
    }
}
