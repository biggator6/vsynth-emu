import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;
import java.io.*;
public class Explore extends GhidraScript {
    @Override public void run() throws Exception {
        String dir = getScriptArgs()[0];
        PrintWriter out = new PrintWriter(new FileWriter(dir + "/explore.txt"));
        Memory mem = currentProgram.getMemory();
        FunctionManager fm = currentProgram.getFunctionManager();
        // 1. pointers near tables
        long[][] ranges = {{0x8950e000L,0x89510200L},{0x8950dc00L,0x8950e200L},{0x897ee000L,0x897ef000L}};
        Address a = currentProgram.getMinAddress(); Address end = currentProgram.getMaxAddress();
        long base = a.getOffset();
        for (long[] r : ranges) out.printf("RANGE %08x-%08x%n", r[0], r[1]);
        for (long off = 0; base+off+4 <= end.getOffset(); off += 4) {
            Address p = a.add(off);
            int v; try { v = mem.getInt(p); } catch (Exception e) { continue; }
            long uv = v & 0xffffffffL;
            for (long[] r : ranges) if (uv >= r[0] && uv < r[1]) {
                StringBuilder sb = new StringBuilder();
                for (Reference ref : getReferencesTo(p)) {
                    Function f = getFunctionContaining(ref.getFromAddress());
                    sb.append(" ").append(ref.getFromAddress()).append(f==null?"":"("+f.getName()+")");
                }
                out.printf("  ptr @%s = %08x refs:%s%n", p, uv, sb);
            }
        }
        // 2. FPU-heavy functions
        List<long[]> rank = new ArrayList<>();
        for (Function f : fm.getFunctions(true)) {
            int fpu=0, tot=0;
            for (Instruction ins : currentProgram.getListing().getInstructions(f.getBody(), true)) {
                tot++; String m = ins.getMnemonicString().toLowerCase();
                if (m.startsWith("f") && !m.equals("fschg") ) fpu++;
            }
            rank.add(new long[]{fpu, tot, f.getEntryPoint().getOffset()});
        }
        rank.sort((x,y)->Long.compare(y[0],x[0]));
        out.println("TOP FPU FUNCTIONS (fpu insns, total insns, entry, callers, callees)");
        for (int i=0;i<60 && i<rank.size();i++){
            long[] r=rank.get(i); Function f=fm.getFunctionAt(toAddr(r[2]));
            out.printf("  %5d %6d %08x callers=%d callees=%d%n", r[0], r[1], r[2], f.getCallingFunctions(monitor).size(), f.getCalledFunctions(monitor).size());
        }
        out.close();
        // 3. decompile selected
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        for (int i=1;i<getScriptArgs().length;i++){
            long ea = Long.parseUnsignedLong(getScriptArgs()[i],16);
            Function f = fm.getFunctionAt(toAddr(ea));
            if (f==null) { disassemble(toAddr(ea)); f = createFunction(toAddr(ea), null); }
            if (f==null) continue;
            DecompileResults res = di.decompileFunction(f, 120, monitor);
            PrintWriter w = new PrintWriter(new FileWriter(dir + "/dec_" + getScriptArgs()[i] + ".c"));
            w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "FAILED " + res.getErrorMessage());
            w.close();
        }
    }
}
