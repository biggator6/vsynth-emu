// Decompile all functions that call an imported/exported symbol whose name contains the given substring.
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.symbol.*;
import java.io.*; import java.util.*;
public class CallersOf extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(a[0]));
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        Set<Function> done = new HashSet<>();
        for (int k = 1; k < a.length; k++) {
            for (Symbol s : currentProgram.getSymbolTable().getAllSymbols(true)) {
                if (!s.getName().contains(a[k])) continue;
                for (Reference r : getReferencesTo(s.getAddress())) {
                    Function f = getFunctionContaining(r.getFromAddress());
                    if (f == null || !done.add(f)) continue;
                    out.println("// ===== " + f.getEntryPoint() + " refs " + s.getName() + " at " + r.getFromAddress());
                    DecompileResults res = di.decompileFunction(f, 120, monitor);
                    out.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "FAILED");
                }
            }
        }
        out.close();
    }
}
