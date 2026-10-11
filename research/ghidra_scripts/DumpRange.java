import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.io.*; import java.util.*;
public class DumpRange extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        long lo = Long.parseUnsignedLong(a[0],16), hi = Long.parseUnsignedLong(a[1],16);
        PrintWriter idx = new PrintWriter(new FileWriter(a[2] + ".idx"));
        PrintWriter dec = new PrintWriter(new FileWriter(a[2] + ".c"));
        Memory m = currentProgram.getMemory();
        DecompInterface di = new DecompInterface(); DecompileOptions opts = new DecompileOptions(); opts.grabFromProgram(currentProgram); opts.setRespectReadOnly(true); di.setOptions(opts); di.openProgram(currentProgram);
        for (Function f : currentProgram.getFunctionManager().getFunctions(toAddr(lo), true)) {
            if (f.getEntryPoint().getOffset() >= hi) break;
            int fpu=0, tot=0; Set<String> data = new TreeSet<>();
            for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                tot++; String mn = ins.getMnemonicString().toLowerCase();
                if (mn.startsWith("f")) fpu++;
                for (Reference r : ins.getReferencesFrom()) {
                    if (!r.getReferenceType().isData()) continue;
                    try { long v = m.getInt(r.getToAddress()) & 0xffffffffL;
                          if (v >= 0x89300000L && v < 0x89c00000L) data.add(String.format("%08x", v)); } catch (Exception e) {}
                }
            }
            StringBuilder cs = new StringBuilder();
            for (Function c : f.getCallingFunctions(monitor)) cs.append(c.getEntryPoint()).append(' ');
            idx.printf("%s size=%d insns=%d fpu=%d callers=[%s] ptrs=%s%n", f.getEntryPoint(), f.getBody().getNumAddresses(), tot, fpu, cs.toString().trim(), data);
            DecompileResults res = di.decompileFunction(f, 120, monitor);
            dec.println("// ===== " + f.getEntryPoint() + " fpu=" + fpu + " callers=[" + cs.toString().trim() + "]");
            dec.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "FAILED " + res.getErrorMessage());
        }
        idx.close(); dec.close();
    }
}
