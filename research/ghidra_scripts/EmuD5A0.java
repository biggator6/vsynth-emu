// Pilot: run the firmware SOLO grain-parameter routine 0x8950d5a0 in Ghidra's
// P-code emulator over a grid of inputs and print its outputs.
// Usage: -postScript EmuD5A0.java <out.tsv> [f0n preset, default 0]
// Struct layout as set up by the player entry 0x8950cd90:
//   r8 = player struct, r9 = r8 + 0xe8 (record state), r10 = r9 + 0x3c (grain).
import ghidra.app.script.GhidraScript;
import ghidra.app.emulator.EmulatorHelper;
import ghidra.program.model.address.Address;
import java.io.*;
import java.math.BigInteger;
import java.nio.*;

public class EmuD5A0 extends GhidraScript {
    EmulatorHelper emu;
    long R8 = 0x8C000000L, R9 = R8 + 0xe8, R10 = R9 + 0x3c, SENTINEL = 0x8C100000L;
    void w32(long a, int v)   throws Exception { emu.writeMemory(toAddr(a), ByteBuffer.allocate(4).order(ByteOrder.LITTLE_ENDIAN).putInt(v).array()); }
    void wf(long a, float v)  throws Exception { w32(a, Float.floatToRawIntBits(v)); }
    void w16(long a, int v)   throws Exception { emu.writeMemory(toAddr(a), ByteBuffer.allocate(2).order(ByteOrder.LITTLE_ENDIAN).putShort((short)v).array()); }
    void w8(long a, int v)    throws Exception { emu.writeMemory(toAddr(a), new byte[]{(byte)v}); }
    int r32(long a)           throws Exception { return ByteBuffer.wrap(emu.readMemory(toAddr(a), 4)).order(ByteOrder.LITTLE_ENDIAN).getInt(); }
    float rf(long a)          throws Exception { return Float.intBitsToFloat(r32(a)); }

    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(args[0]));
        float f0nPreset = args.length > 1 ? Float.parseFloat(args[1]) : 0f;
        out.println("C\tN\tP\tsegStart\tR\tF\tK\tinfo\tppw256\tos\tvel\tfrac\tslope\tf0n\tstep30");
        int[] Cs = {5427, 3200};                       // periods in 1/16 sample (339.2, 200 samples)
        float[] Rs = {1.0f, 1.4983f, 0.5f};            // pitch rate (r8+0x98)
        float[] Fs = {1.0f, 0.5f, 2.0f, 1.25f};         // formant ratio (r8+0x74)
        int[] infos = {7, 3, 1, 2, 0, 4};
        int[] Ks = {256, 0};                            // r10[0]: A = K*R/256; 0 → robot path
        for (int C : Cs) for (float R : Rs) for (float F : Fs) for (int info : infos) for (int K : Ks) {
            emu = new EmulatorHelper(currentProgram);
            try {
                for (long a = R8; a < R8 + 0x200; a += 4) w32(a, 0);
                int N = C + 160, segStart = 100000, P = segStart + C / 3;
                w32(R9 + 0x2c, C); w32(R9 + 0x30, N);
                w8(R8 + 3, 0x80);                       // frac / period interpolation enabled
                w32(R8 + 0x38, P);
                w32(R9 + 0x20, segStart); w32(R9 + 0x24, segStart + C);
                wf(R8 + 0x98, R); wf(R8 + 0x74, F);
                w32(R10 + 0, K);
                wf(R10 + 0x24, f0nPreset);
                w16(R9 + 0xa, info);
                emu.writeRegister("r8", R8); emu.writeRegister("r9", R9); emu.writeRegister("r10", R10);
                emu.writeRegister("FPSCR", 0x00040001L);
                emu.writeRegister("FPSCR_PR", 0); emu.writeRegister("FPSCR_SZ", 0);
                emu.writeRegister("PR", SENTINEL);
                emu.writeRegister(emu.getPCRegister(), 0x8950d5a0L);
                emu.setBreakpoint(toAddr(SENTINEL));
                boolean ok = emu.run(monitor);
                long pc = emu.readRegister(emu.getPCRegister()).longValue();
                if (!ok && pc != SENTINEL) { out.println("# FAIL C="+C+" R="+R+" F="+F+" info="+info+" K="+K+" pc="+Long.toHexString(pc)+" "+emu.getLastError()); continue; }
                out.printf("%d\t%d\t%d\t%d\t%.4f\t%.4f\t%d\t%d\t%d\t%d\t%.6f\t%.6f\t%.8f\t%.6f\t%d%n",
                    C, N, P, segStart, R, F, K, info,
                    r32(R10 + 4), r32(R10 + 8), rf(R10 + 0x28), rf(R10 + 0x34), rf(R10 + 0x40), rf(R10 + 0x24), r32(R8 + 0x30));
            } finally { emu.dispose(); }
        }
        out.close();
    }
}
