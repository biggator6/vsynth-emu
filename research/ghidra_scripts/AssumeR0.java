// For functions whose 2nd instruction is `lds r0,fpscr` (caller-supplied mode), assume r0=0x00040001 at entry.
import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import ghidra.program.model.lang.*;
import java.math.BigInteger;
public class AssumeR0 extends GhidraScript {
    @Override public void run() throws Exception {
        Register r0 = currentProgram.getRegister("r0");
        int n = 0;
        for (Function f : currentProgram.getFunctionManager().getFunctions(true)) {
            Instruction i = getInstructionAt(f.getEntryPoint());
            for (int k = 0; k < 3 && i != null; k++, i = i.getNext()) {
                if (i.toString().equals("lds r0,fpscr")) {
                    currentProgram.getProgramContext().setValue(r0, f.getEntryPoint(), i.getAddress(), BigInteger.valueOf(0x40001));
                    println("R0MODE " + f.getEntryPoint()); n++; break;
                }
            }
        }
        println("R0MODE total " + n);
    }
}
