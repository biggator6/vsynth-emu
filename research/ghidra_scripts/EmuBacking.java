// Render a whole BACKING note with the firmware's own multi-band player
// (0x8950e6e0, one 32-sample block per call) in Ghidra's P-code emulator.
// Usage: -postScript EmuBacking.java <vpdt.bin> <mem.bin> <bands> <channels> <out.f32> <R> <walk> [maxOut]
//   vpdt.bin  the wave's vpdt chunk (multi-band layout); the player reads it
//             in place, offsets relative to its start
//   mem.bin   sample memory from 0x8C100000: the original audio followed by
//             the band PCM (EmuBand.java's <out.heap>.samp)
//   bands     five comma-separated hex addresses of bands 1..5 in mem.bin
//             (EmuBand prints them; band 0 is the original at 0x8C100000)
//   channels  1 or 2
//   out.f32   float32 stereo pairs, scaled to ±1
//   R pitch ratio (+0x70), walk = 1/stretch (+0x7c)
// Note-on as the dispatcher 0x8950bf70 does it for mode 3: the setup
// 0x89510980 alone (start offset 0, so no seek through 0x89510bf0).  The
// wave struct it reads: +0 vpdt, +4..+0x18 band sample pointers, +0x28 a
// tuning struct (s16 +0x14 / +0x16, zero here), +0x2c the loop/cache struct.
// Memory is set up as the C startup leaves it (.data from ROM, .bss zeroed).
// EMUBK_MODE=5: ENSEMBLE instead (setup 0x89510130, player 0x8950db10);
// bands 1..5 are then the frame data EmuLoad.java builds for type 5.
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import java.io.*;
import java.nio.*;
import java.nio.file.*;

public class EmuBacking extends GhidraScript {
    EmulatorHelper emu;
    static final long P = 0x8C000000L, W = 0x8C002000L, TUNE = 0x8C002100L, W2 = 0x8C003000L,
        VPDT = 0x8C010000L, SAMP = 0x8C100000L, OUT = 0x8C800000L, STACK = 0x8C7F0000L,
        SENTINEL = 0x8CF00000L;
    void wb(long a, byte[] b) throws Exception { emu.writeMemory(toAddr(a), b); }
    void w32(long a, int v)  throws Exception { wb(a, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()); }
    void wf(long a, float v) throws Exception { w32(a, Float.floatToRawIntBits(v)); }
    void w8(long a, int v)   throws Exception { wb(a, new byte[]{(byte)v}); }
    void w16(long a, int v)  throws Exception { wb(a, new byte[]{(byte)v, (byte)(v >> 8)}); }

    void call(long pc, long r4, long r5) throws Exception {
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
            if (at == 0x8950c570L && trace != null) { dumpAcc(); emu.step(monitor); continue; }
            if (isFipr(at)) { fipr(at); continue; }
            break;
        }
        if (at != SENTINEL) throw new RuntimeException("stopped at " + Long.toHexString(at) + " " + emu.getLastError());
    }

    // Ghidra's little-endian SH-4 spec stores the float registers pairwise
    // swapped (fr1 fr0 fr3 fr2 ...), and its fipr P-code addresses FVn by
    // offset, so the dot product lands in fr2 instead of fr3.  The only fipr
    // instructions in the players are these four, in the band recombination
    // tree (0x8950c650 / 0x8950c720); they are executed here instead.  The
    // sum is formed in double and rounded once (the SH-4's FIPR is an
    // approximate inner product; LSB-level differences are possible).
    static final long[] FIPR = { 0x8950c6c6L, 0x8950c6d4L, 0x8950c72aL, 0x8950c756L };
    boolean isFipr(long at) { for (long f : FIPR) if (f == at) return true; return false; }
    float fr(int i) { return Float.intBitsToFloat((int)emu.readRegister("fr" + i).longValue()); }
    void fipr(long at) throws Exception {
        int w = ByteBuffer.wrap(emu.readMemory(toAddr(at), 2)).order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xffff;
        int n = 4 * ((w >> 10) & 3), m = 4 * ((w >> 8) & 3);
        double sum = 0;
        for (int i = 0; i < 4; ++i) sum += (double)fr(n + i) * (double)fr(m + i);
        emu.writeRegister("fr" + (n + 3), Float.floatToRawIntBits((float)sum) & 0xffffffffL);
        emu.writeRegister(emu.getPCRegister(), at + 2);
    }

    // EMUBK_TRACE=file: at the recombination (0x8950c570) of each block, the
    // band accumulation buffers P+0x418 .. P+0x7a8 as floats, one line per block.
    PrintWriter trace = null;
    void dumpAcc() throws Exception {
        ByteBuffer bb = ByteBuffer.wrap(emu.readMemory(toAddr(P + 0x418), 0x7a8 - 0x418)).order(ByteOrder.LITTLE_ENDIAN);
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < (0x7a8 - 0x418) / 4; ++i) sb.append(i == 0 ? "" : "\t").append(bb.getFloat(4 * i));
        trace.println(sb);
    }

    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        byte[] vpdt = Files.readAllBytes(Paths.get(a[0]));
        byte[] mem = Files.readAllBytes(Paths.get(a[1]));
        String[] bands = a[2].split(",");
        int ch = Integer.parseInt(a[3]);
        float R = Float.parseFloat(a[5]), walk = Float.parseFloat(a[6]);
        int n;   // frames: taken from the end event position the setup reads
        emu = new EmulatorHelper(currentProgram);
        emu.setBreakpoint(toAddr(SENTINEL));
        for (long f : FIPR) emu.setBreakpoint(toAddr(f));
        try {
            wb(0x8985858cL, emu.readMemory(toAddr(0x89834018L), 0x8985858c - 0x89834018));
            wb(0x8987cb00L, new byte[0x8a01ceb4 - 0x8987cb00]);
            wb(P, new byte[0x1000]); wb(W, new byte[0x100]); wb(TUNE, new byte[0x100]); wb(W2, new byte[0x400]);
            wb(VPDT, vpdt); wb(SAMP, mem); wb(SAMP + mem.length, new byte[1 << 16]);
            // wave struct
            w32(W, (int)VPDT); w32(W + 4, (int)SAMP);
            for (int k = 0; k < 5; ++k) w32(W + 8 + 4 * k, (int)Long.parseLong(bands[k].trim(), 16));
            w32(W + 0x28, (int)TUNE); w32(W + 0x2c, (int)W2);
            w32(W2 + 0x16c, 0x7fffffff); w32(W2 + 0xb8, 0x7fffffff);   // no loop points / cache bounds
            // player struct (as EmuSolo, plus the wave pointer at +0x44)
            w8(P, ch == 2 ? 0x40 : 0);
            w16(P + 0x14, 0); w16(P + 0x16, 0xffff);                    // start / end event
            w32(P + 0x28, 0x7fffffff); w32(P + 0x2c, 0x7fffffff);
            w32(P + 0x38, 0); w32(P + 0x3c, 0);
            w32(P + 0x44, (int)W);
            w32(P + 0x48, 44100); w32(P + 0x4c, 44100);
            wf(P + 0x70, R); wf(P + 0x98, R); wf(P + 0x74, 1f); wf(P + 0x78, 0f);
            wf(P + 0x7c, walk); wf(P + 0x9c, 1f);
            wf(P + 0x80, 1f / 65536f); wf(P + 0x84, 1f / 65536f); wf(P + 0x88, 1f / 64f);
            final boolean ens = "5".equals(System.getenv("EMUBK_MODE"));
            final long setupFn = ens ? 0x89510130L : 0x89510980L, playFn = ens ? 0x8950db10L : 0x8950e6e0L;
            call(setupFn, P, 0);
            ByteBuffer pb = ByteBuffer.wrap(emu.readMemory(toAddr(P), 0x40)).order(ByteOrder.LITTLE_ENDIAN);
            n = pb.getInt(0x1c) / 16;                                   // end event position
            w32(P + 0x20, n * 16);
            int outLen = a.length > 7 && Integer.parseInt(a[7]) > 0 ? Integer.parseInt(a[7]) : Math.max(1, Math.round(n / walk));
            println(String.format("setup done: start %d end %d (n %d) events %d; out %d samples",
                pb.getInt(0x18), pb.getInt(0x1c), n, pb.getShort(0xe), outLen));

            if (System.getenv("EMUBK_TRACE") != null) {
                trace = new PrintWriter(new BufferedWriter(new FileWriter(System.getenv("EMUBK_TRACE"))));
                emu.setBreakpoint(toAddr(0x8950c570L));
            }
            DataOutputStream out = new DataOutputStream(new BufferedOutputStream(new FileOutputStream(a[4])));
            long t0 = System.currentTimeMillis();
            ByteBuffer le = ByteBuffer.allocate(8).order(ByteOrder.LITTLE_ENDIAN);
            for (int done = 0; done < outLen; done += 32) {
                wb(OUT, new byte[32 * 8]);
                call(playFn, OUT, P);
                ByteBuffer bb = ByteBuffer.wrap(emu.readMemory(toAddr(OUT), 32 * 8)).order(ByteOrder.LITTLE_ENDIAN);
                for (int i = 0; i < 32 && done + i < outLen; ++i) {
                    le.clear(); le.putFloat(bb.getFloat(i * 8) / 32768f); le.putFloat(bb.getFloat(i * 8 + 4) / 32768f);
                    out.write(le.array());
                }
                if ((done & 0x7fff) == 0) println("block " + (done / 32) + " of " + (outLen / 32) + "  " + (System.currentTimeMillis() - t0) + " ms");
            }
            out.close();
            if (trace != null) trace.close();
            println("done " + outLen + " samples in " + (System.currentTimeMillis() - t0) + " ms");
        } finally { emu.dispose(); }
    }
}
