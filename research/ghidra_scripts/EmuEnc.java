// Run the V-Synth XT's own SOLO encoder (0x8951e120, the vpenc.dll algorithm
// compiled for the SH-4) in Ghidra's P-code emulator and print its marks.
// Usage: -postScript EmuEnc.java <samp.bin> <out.tsv>
//   samp.bin  int16 LE mono samples (44.1 kHz)
//   out.tsv   one line per record: start len info (1/16 sample; the vpdt
//             record stream without its (256, 1) terminator)
// Setup as the encode driver 0x89515e20 does it: 0x8951dc40(n, samples,
// eventCount, events) with the event list {n·16} (a sustained note has no
// attack events), template defaults (no 0x8951dbb0 call), then
// 0x8951e120(2) = mono.  The image's .bss (0x8985858c to the end, 0xFF in
// the file) is zeroed as the C startup would; DAT_8987caa0 (node-pool size
// factor, set through 0x89522a30, default 5) = 50, so the pool holds n/16
// nodes.  malloc (0x89527910) / free (0x89527950) are stubbed
// with a bump allocator.  Reproduces the XT's own wave 004 records (raw
// vocal): positions 458/462 exact, the rest ±1/16 (FPU rounding), info 100 %.
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import java.io.*;
import java.nio.*;
import java.nio.file.*;
import java.util.*;

public class EmuEnc extends GhidraScript {
    EmulatorHelper emu;
    static final long SAMP = 0x8C100000L, EV = 0x8C0F0000L, HEAP = 0x8D000000L,
        STACK = 0x8C0E0000L, SENTINEL = 0x8CF00000L, MALLOC = 0x89527910L, FREE = 0x89527950L;
    long heap = HEAP;
    void wb(long a, byte[] b) throws Exception { emu.writeMemory(toAddr(a), b); }
    void w32(long a, int v)  throws Exception { wb(a, ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()); }
    int r32(long a)          throws Exception { return ByteBuffer.wrap(emu.readMemory(toAddr(a), 4)).order(ByteOrder.LITTLE_ENDIAN).getInt(); }
    int r16(long a)          throws Exception { return ByteBuffer.wrap(emu.readMemory(toAddr(a), 2)).order(ByteOrder.LITTLE_ENDIAN).getShort() & 0xffff; }
    long reg(String r)       { return emu.readRegister(r).longValue(); }

    long call(long pc, long... args) throws Exception {
        String[] ar = { "r4", "r5", "r6", "r7" };
        for (int i = 0; i < args.length; ++i) emu.writeRegister(ar[i], args[i]);
        emu.writeRegister("r15", STACK);
        emu.writeRegister("r0", 0x00040001L);
        emu.writeRegister("FPSCR", 0x00040001L);
        emu.writeRegister("FPSCR_PR", 0); emu.writeRegister("FPSCR_SZ", 0);
        emu.writeRegister("PR", SENTINEL);
        emu.writeRegister(emu.getPCRegister(), pc);
        while (true) {
            emu.run(monitor);
            long at = emu.readRegister(emu.getPCRegister()).longValue();
            if (at == SENTINEL) return reg("r0");
            if (at == MALLOC) {
                long sz = (reg("r4") + 15) & ~15L;
                emu.writeRegister("r0", heap); wb(heap, new byte[(int)Math.min(sz, 1 << 24)]); heap += sz;
                emu.writeRegister(emu.getPCRegister(), reg("PR"));
                continue;
            }
            if (at == FREE) { emu.writeRegister(emu.getPCRegister(), reg("PR")); continue; }
            throw new RuntimeException("stopped at " + Long.toHexString(at) + " " + emu.getLastError());
        }
    }

    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        byte[] samp = Files.readAllBytes(Paths.get(a[0]));
        int n = samp.length / 2;
        emu = new EmulatorHelper(currentProgram);
        emu.setBreakpoint(toAddr(SENTINEL)); emu.setBreakpoint(toAddr(MALLOC)); emu.setBreakpoint(toAddr(FREE));
        try {
            wb(0x8985858cL, new byte[0x89b5858d - 0x8985858c]);     // .bss = 0
            w32(0x8987caa0L, 50); wb(0x8987ca9aL, new byte[]{1});
            wb(SAMP, samp); wb(SAMP + samp.length, new byte[1 << 16]);
            w32(EV, n * 16);
            long t0 = System.currentTimeMillis();
            call(0x8951dc40L, n, SAMP, 1, EV);
            long rc = call(0x8951e120L, 2);
            println("encoder returned " + rc + " in " + (System.currentTimeMillis() - t0) + " ms, heap used " + (heap - HEAP));
            // Node pool (0x895222a0 allocates 16-byte nodes from the chunk list
            // at [0x8a015260]): {u16 low, u16 v, i32 pos (1/16 sample), u32
            // lower neighbour, u32 higher neighbour}.  The list head is the
            // node with no lower neighbour; walk it upwards.
            Map<Long, long[]> nodes = new HashMap<>();
            for (long p = HEAP + 16; p + 16 <= heap; p += 16) {
                byte[] b = emu.readMemory(toAddr(p), 16);
                ByteBuffer bb = ByteBuffer.wrap(b).order(ByteOrder.LITTLE_ENDIAN);
                long low = bb.getShort(0) & 0xffff, v = bb.getShort(2) & 0xffff, pos = bb.getInt(4);
                long lo = bb.getInt(8) & 0xffffffffL, hi = bb.getInt(12) & 0xffffffffL;
                if (low != 0 || v != 0 || pos != 0 || lo != 0 || hi != 0) nodes.put(p, new long[]{ low, v, pos, lo, hi });
            }
            List<long[]> best = new ArrayList<>();
            for (Map.Entry<Long, long[]> e : nodes.entrySet()) {
                if (e.getValue()[3] != 0 || !nodes.containsKey(e.getValue()[4])) continue;
                List<long[]> seq = new ArrayList<>(); Set<Long> seen = new HashSet<>();
                for (Long p = e.getKey(); p != null && nodes.containsKey(p) && seen.add(p); p = nodes.get(p)[4]) seq.add(nodes.get(p));
                if (seq.size() > best.size()) best = seq;
            }
            // Records as the writer pairs them (vpenc 0x10004fa0/0x100045e0):
            // [mark_k, mark_k+1) takes the CLOSING mark's info
            // ((v >> 8) & 0xf8) | low; then the (256, 1) terminator.
            PrintWriter out = new PrintWriter(new FileWriter(a[1]));
            for (int k = 0; k + 1 < best.size(); ++k) {
                long[] c = best.get(k + 1);
                out.printf("%d\t%d\t%d%n", best.get(k)[2], c[2] - best.get(k)[2], ((c[1] >> 8) & 0xf8) | c[0]);
            }
            out.close();
            println("marks " + best.size() + ", records " + Math.max(0, best.size() - 1) + " (pos len info, 1/16 sample)");
        } finally { emu.dispose(); }
    }
}
