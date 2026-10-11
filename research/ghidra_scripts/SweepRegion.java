import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
import ghidra.program.model.mem.*;
public class SweepRegion extends GhidraScript {
    @Override public void run() throws Exception {
        Memory m = currentProgram.getMemory();
        long lo = Long.parseUnsignedLong(getScriptArgs()[0],16), hi = Long.parseUnsignedLong(getScriptArgs()[1],16);
        int made = 0;
        for (long a = lo; a < hi; a += 2) {
            Address p = toAddr(a);
            if ((m.getShort(p) & 0xffff) != 0x000b) continue;          // rts
            Instruction ri = getInstructionAt(p);
            if (ri == null) continue;                                    // only real code rts
            Address s = p.add(4);
            while (s.getOffset() < hi && ((m.getShort(s) & 0xffff) == 0x0009 || (m.getShort(s) & 0xffff) == 0)) s = s.add(2);
            if (s.getOffset() >= hi || getFunctionContaining(s) != null) continue;
            int h = m.getShort(s) & 0xffff;
            // plausible prologue: sts.l pr/macl/mach/fpscr,@-r15 (4f22/4f12/4f02/4f62) or mov.l rN,@-r15 (2fx6) or fmov/etc
            boolean ok = (h & 0xff0f) == 0x4f02 || (h & 0xf00f) == 0x2006 || (h & 0xf00f) == 0xf00b || (h & 0xf000) == 0xd000 || (h & 0xf000) == 0xe000;
            if (!ok) continue;
            if (getInstructionAt(s) == null) disassemble(s);
            CreateFunctionCmd cmd = new CreateFunctionCmd(s);
            if (cmd.applyTo(currentProgram, monitor)) made++;
        }
        println("SWEEP created " + made);
    }
}
