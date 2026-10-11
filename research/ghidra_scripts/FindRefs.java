// Find code that uses given strings / table addresses via SH-4 literal pools.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.symbol.*;
import java.util.*;
import java.io.*;
public class FindRefs extends GhidraScript {
    PrintWriter out;
    Memory mem;
    List<Address> findBytes(byte[] pat) throws Exception {
        List<Address> r = new ArrayList<>();
        Address a = currentProgram.getMinAddress();
        while (a != null) {
            a = mem.findBytes(a, pat, null, true, monitor);
            if (a == null) break;
            r.add(a); a = a.add(1);
        }
        return r;
    }
    void users(String label, long target) throws Exception {
        byte[] le = new byte[]{(byte)target,(byte)(target>>8),(byte)(target>>16),(byte)(target>>24)};
        Set<Function> funcs = new LinkedHashSet<>();
        int pools = 0, insns = 0;
        for (Address pool : findBytes(le)) {
            if (pool.getOffset() % 4 != 0) continue;
            pools++;
            for (Reference ref : getReferencesTo(pool)) {
                insns++;
                Function f = getFunctionContaining(ref.getFromAddress());
                if (f != null) funcs.add(f);
                else out.printf("    ref from %s (no function)%n", ref.getFromAddress());
            }
        }
        out.printf("  %s 0x%08x: %d pool words, %d instr refs%n", label, target, pools, insns);
        for (Function f : funcs)
            out.printf("    -> %s @ %s size %d%n", f.getName(), f.getEntryPoint(), f.getBody().getNumAddresses());
    }
    @Override public void run() throws Exception {
        mem = currentProgram.getMemory();
        String outPath = getScriptArgs()[0];
        out = new PrintWriter(new FileWriter(outPath));
        out.printf("functions: %d%n", currentProgram.getFunctionManager().getFunctionCount());
        String[] strs = {"SAMPLE Encode","Encode Type :","Encodeing Error !","Encode Depth","HID_SPL_ENCODE_DEPTH",
                         "HID_SPL_ENCODE_SET","ENSEMBLE","BACKING","Formant","OSC 1 Formant","OSC 1 Robot Voice Sw","OSC 1 Vari Sw"};
        for (String s : strs) {
            byte[] b = (s + "\0").getBytes("ASCII");
            List<Address> hits = findBytes(b);
            out.printf("STRING \"%s\": %d hits%n", s, hits.size());
            for (Address h : hits) {
                // also try with leading-space padded variants: string may start earlier
                users("str", h.getOffset());
            }
        }
        long[] tabs = {0x8950ee00L, 0x8950ddccL, 0x8950ddd0L, 0x897eebb0L};
        for (long t : tabs) { out.printf("TABLE%n"); users("tab", t); }
        out.close();
    }
}
