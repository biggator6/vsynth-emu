import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;
import java.io.*;
public class FindConsts extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(args[0]));
        double[] cs = {440.0, 1.4426950, 0.6931472, 0.0577623, 17.312340, 1.0594631, 0.9438743, 69.0, 3.3219281, 0.30103,
                       8.17579891, 1200.0, 0.0008333333, 48000.0, 44100.0, 2.0833333e-05, 0.6666667, 1.6666666, 0.75};
        Memory mem = currentProgram.getMemory();
        Address a = currentProgram.getMinAddress(); long n = currentProgram.getMaxAddress().subtract(a);
        Map<String, Map<Function,Integer>> hits = new TreeMap<>();
        for (long off = 0; off + 4 <= n; off += 4) {
            Address p = a.add(off);
            float v; try { v = Float.intBitsToFloat(mem.getInt(p)); } catch (Exception e) { continue; }
            for (double c : cs) {
                if (Math.abs(v - c) <= Math.abs(c) * 2e-6) {
                    for (Reference r : getReferencesTo(p)) {
                        Function f = getFunctionContaining(r.getFromAddress());
                        String key = String.format("%.7g", c);
                        hits.computeIfAbsent(key, k -> new LinkedHashMap<>()).merge(f, 1, Integer::sum);
                    }
                }
            }
        }
        for (var e : hits.entrySet()) {
            out.println("CONST " + e.getKey());
            for (var fe : e.getValue().entrySet()) {
                Function f = fe.getKey();
                out.printf("   %s size=%s%n", f==null?"(nofunc)":f.getEntryPoint().toString(), f==null?"-":""+f.getBody().getNumAddresses());
            }
        }
        out.close();
    }
}
