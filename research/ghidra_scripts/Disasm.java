import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.address.*;
import java.io.*;
public class Disasm extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(a[0]));
        for (int k = 1; k + 1 < a.length; k += 2) {
            Address s = toAddr(Long.parseUnsignedLong(a[k],16)); int n = Integer.parseInt(a[k+1]);
            out.println("=== " + s);
            Instruction ins = getInstructionAt(s); if (ins == null) ins = getInstructionAfter(s);
            for (int i = 0; i < n && ins != null; i++, ins = ins.getNext()) {
                String extra = "";
                for (var r : ins.getReferencesFrom()) {
                    try { long v = currentProgram.getMemory().getInt(r.getToAddress()) & 0xffffffffL;
                          extra += String.format("  [%s]=%08x (%g)", r.getToAddress(), v, Float.intBitsToFloat((int)v)); } catch (Exception e) {}
                }
                out.println("  " + ins.getAddress() + "  " + ins + extra);
            }
        }
        out.close();
    }
}
