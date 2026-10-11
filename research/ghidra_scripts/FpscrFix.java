// 1) create functions for orphan code containing lds ...,fpscr (hand-written asm)
// 2) linear FPSCR_PR / FPSCR_SZ tracking; write assumed values into the program context
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.lang.*;
import ghidra.program.model.mem.*;
import ghidra.program.model.scalar.Scalar;
import java.math.BigInteger;
import java.util.*;
import java.io.*;
public class FpscrFix extends GhidraScript {
    Memory mem;
    Long constLoadedInto(Instruction ins, String reg) throws Exception {
        Instruction p = ins;
        for (int i = 0; i < 6 && p != null; i++) {
            p = p.getPrevious(); if (p == null) break;
            String t = p.toString();
            if (t.startsWith("mov.l ") && t.endsWith("," + reg)) {
                for (Object o : p.getOpObjects(0)) {
                    if (o instanceof Address) return mem.getInt((Address)o) & 0xffffffffL;
                }
                // fall back to reference
                for (var r : p.getReferencesFrom()) return mem.getInt(r.getToAddress()) & 0xffffffffL;
                return null;
            }
            if (t.startsWith("mov #") && t.endsWith("," + reg)) {
                Scalar s = p.getScalar(0); return s == null ? null : s.getValue() & 0xffffffffL;
            }
        }
        return null;
    }
    @Override public void run() throws Exception {
        mem = currentProgram.getMemory();
        PrintWriter out = new PrintWriter(new FileWriter(getScriptArgs()[0]));
        Listing L = currentProgram.getListing();
        // Pass 1: orphan asm routines
        int created = 0;
        List<Instruction> ldsSites = new ArrayList<>();
        for (Instruction ins : L.getInstructions(true))
            if (ins.toString().matches("lds r\\d+,fpscr")) ldsSites.add(ins);
        for (Instruction ins : ldsSites) {
            if (getFunctionContaining(ins.getAddress()) != null) continue;
            Address a = ins.getAddress(), p = a;
            for (int i = 0; i < 200; i++) {
                p = p.subtract(2);
                int h = mem.getShort(p) & 0xffff;
                if (h == 0x000b) { // rts
                    Address s = p.add(4);
                    while ((mem.getShort(s) & 0xffff) == 0x0009 || (mem.getShort(s) & 0xffff) == 0) s = s.add(2);
                    if (getInstructionAt(s) == null) disassemble(s);
                    Function f = createFunction(s, null);
                    if (f != null) { created++; out.println("created " + f.getEntryPoint() + " size " + f.getBody().getNumAddresses()); }
                    break;
                }
            }
        }
        // Pass 2: linear mode tracking
        Register PR = currentProgram.getRegister("FPSCR_PR"), SZ = currentProgram.getRegister("FPSCR_SZ");
        ProgramContext pc = currentProgram.getProgramContext();
        pc.setValue(PR, currentProgram.getMinAddress(), currentProgram.getMaxAddress(), BigInteger.ZERO);
        pc.setValue(SZ, currentProgram.getMinAddress(), currentProgram.getMaxAddress(), BigInteger.ZERO);
        int pr = 0, sz = 0, runs = 0, unknownLds = 0;
        Address runStart = null; Function cur = null;
        Deque<int[]> saved = new ArrayDeque<>();
        for (Instruction ins : L.getInstructions(true)) {
            Function f = getFunctionContaining(ins.getAddress());
            if (f != cur) { // new function: reset to default mode
                cur = f; pr = 0; sz = 0; saved.clear();
            }
            String t = ins.toString();
            // apply current state to this instruction
            if (pr != 0) pc.setValue(PR, ins.getMinAddress(), ins.getMaxAddress(), BigInteger.ONE);
            if (sz != 0) pc.setValue(SZ, ins.getMinAddress(), ins.getMaxAddress(), BigInteger.ONE);
            // state changes take effect for following instructions
            if (t.equals("fschg")) sz ^= 1;
            else if (t.startsWith("sts.l FPSCR,@-")) saved.push(new int[]{pr, sz});
            else if (t.startsWith("lds.l @") && t.endsWith(",fpscr")) { int[] s = saved.isEmpty() ? new int[]{0,0} : saved.pop(); pr = s[0]; sz = s[1]; }
            else if (t.matches("lds r\\d+,fpscr")) {
                Long v = constLoadedInto(ins, t.substring(4, t.indexOf(',')));
                if (v == null) { unknownLds++; out.println("unknown lds at " + ins.getAddress()); }
                else { pr = (int)((v >> 19) & 1); sz = (int)((v >> 20) & 1);
                       out.printf("lds %s const 0x%08x -> PR=%d SZ=%d%n", ins.getAddress(), v, pr, sz); }
            }
        }
        out.printf("created %d functions, unknown lds %d%n", created, unknownLds);
        out.close();
    }
}
