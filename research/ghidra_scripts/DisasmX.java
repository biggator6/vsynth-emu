// Plain listing of [addr, addr+n instructions) for any processor.
// Usage: -postScript DisasmX.java <out> <addr> <count> [<addr> <count> ...]
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*;
public class DisasmX extends GhidraScript {
    @Override public void run() throws Exception {
        String[] a = getScriptArgs();
        PrintWriter out = new PrintWriter(new FileWriter(a[0]));
        for (int k = 1; k + 1 < a.length; k += 2) {
            out.println("=== " + a[k]);
            Instruction ins = getInstructionAt(toAddr(a[k]));
            if (ins == null) ins = getInstructionAfter(toAddr(a[k]));
            for (int i = 0; ins != null && i < Integer.parseInt(a[k + 1]); ++i, ins = ins.getNext())
                out.println("  " + ins.getAddress() + "  " + ins);
        }
        out.close();
    }
}
