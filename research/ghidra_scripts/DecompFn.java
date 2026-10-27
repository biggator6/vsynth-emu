// Decompile the function containing each address (any processor; for x86
// programs such as vpenc.dll).  Usage: -postScript DecompFn.java <out.c> <addr>...
//   or: -postScript DecompFn.java <out.c> range <lo> <hi>   (every function in [lo, hi))
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import java.io.*;
import java.util.*;

public class DecompFn extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(a[0]));
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        Set<Function> done = new HashSet<>();
        List<String> addrs = new ArrayList<>();
        if (a.length == 4 && a[1].equals("range")) {
            for (Function f : currentProgram.getFunctionManager().getFunctions(toAddr(a[2]), true)) {
                if (f.getEntryPoint().compareTo(toAddr(a[3])) >= 0) break;
                addrs.add(f.getEntryPoint().toString());
            }
        } else addrs.addAll(Arrays.asList(a).subList(1, a.length));
        for (String ad : addrs) {
            Function f = getFunctionContaining(toAddr(ad));
            if (f == null) { out.println("// no function at " + ad); continue; }
            if (!done.add(f)) continue;
            DecompileResults r = di.decompileFunction(f, 120, monitor);
            out.println("// ===== " + f.getEntryPoint() + " " + f.getName() + " (for " + ad + ")");
            out.println(r.decompileCompleted() ? r.getDecompiledFunction().getC() : "// decompile failed: " + r.getErrorMessage());
        }
        out.close();
    }
}
