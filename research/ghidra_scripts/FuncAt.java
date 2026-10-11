// For each address: ensure a containing function exists (scan back to previous rts+delay slot),
// then decompile it and list callees (with any string literals they reference).
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.*;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.mem.*;
import java.math.BigInteger;
import java.io.*;
public class FuncAt extends GhidraScript {
    Function ensure(Address a) throws Exception {
        Function f = getFunctionContaining(a);
        if (f != null) return f;
        Memory m = currentProgram.getMemory();
        Address p = a;
        for (int i=0;i<4000;i++) {
            p = p.subtract(2);
            if ((m.getShort(p) & 0xffff) == 0x000b) { // rts
                Address s = p.add(4);
                while ((m.getShort(s)&0xffff)==0x0009 || (m.getShort(s)&0xffff)==0) s = s.add(2);
                if (s.compareTo(a) > 0) continue;
                clearListing(s, a);
                disassemble(s);
                f = createFunction(s, null);
                if (f != null && f.getBody().contains(a)) return f;
                if (f != null) return f;
                break;
            }
        }
        return null;
    }
    @Override public void run() throws Exception {
        String[] args = getScriptArgs(); String dir = args[0];
        ProgramContext pc = currentProgram.getProgramContext();
        for (String rn : new String[]{"FPSCR_PR","FPSCR_SZ"}) {
            Register r = currentProgram.getRegister(rn);
            pc.setValue(r, currentProgram.getMinAddress(), currentProgram.getMaxAddress(), BigInteger.ZERO);
        }
        DecompInterface di = new DecompInterface(); di.openProgram(currentProgram);
        for (int i=1;i<args.length;i++){
            Address a = toAddr(Long.parseUnsignedLong(args[i],16));
            Function f = ensure(a);
            if (f==null) { println("NOFUNC "+args[i]); continue; }
            println("ADDR "+args[i]+" -> "+f.getEntryPoint()+" size "+f.getBody().getNumAddresses());
            DecompileResults res = di.decompileFunction(f, 300, monitor);
            PrintWriter w = new PrintWriter(new FileWriter(dir + "/dec_" + f.getEntryPoint() + ".c"));
            StringBuilder cs = new StringBuilder();
            for (Function c : f.getCallingFunctions(monitor)) cs.append(c.getEntryPoint()).append(' ');
            w.println("// for " + args[i] + "  callers: " + cs);
            w.println(res.decompileCompleted() ? res.getDecompiledFunction().getC() : "FAILED " + res.getErrorMessage());
            w.close();
        }
    }
}
