import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.lang.*;
import java.math.BigInteger;
import java.io.*;
public class Decomp extends GhidraScript {
    @Override public void run() throws Exception {
        String[] args = getScriptArgs(); String dir = args[0];
        ProgramContext pc = currentProgram.getProgramContext();
        for (String rn : new String[]{"FPSCR_PR","FPSCR_SZ"}) {
            Register r = currentProgram.getRegister(rn);
            if (r != null) pc.setValue(r, currentProgram.getMinAddress(), currentProgram.getMaxAddress(), BigInteger.ZERO);
        }
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        FunctionManager fm = currentProgram.getFunctionManager();
        for (int i=1;i<args.length;i++){
            Function f = fm.getFunctionAt(toAddr(Long.parseUnsignedLong(args[i],16)));
            if (f==null) { println("no func "+args[i]); continue; }
            DecompileResults res = di.decompileFunction(f, 300, monitor);
            PrintWriter w = new PrintWriter(new FileWriter(dir + "/dec_" + args[i] + ".c"));
            StringBuilder cs = new StringBuilder();
            for (Function c : f.getCallingFunctions(monitor)) cs.append(c.getEntryPoint()).append(' ');
            w.println("// callers: " + cs);
            w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "FAILED " + res.getErrorMessage());
            w.close();
        }
    }
}
