import ghidra.app.script.GhidraScript;
import ghidra.app.cmd.function.CreateFunctionCmd;
import ghidra.program.model.address.*;
import ghidra.program.model.listing.*;
public class MkFunc extends GhidraScript {
    @Override public void run() throws Exception {
        for (String a : getScriptArgs()) {
            Address ad = toAddr(Long.parseUnsignedLong(a,16));
            Function f = getFunctionContaining(ad);
            Instruction i = getInstructionAt(ad);
            println("ADDR "+a+" existing func="+(f==null?"none":f.getEntryPoint()+" size "+f.getBody().getNumAddresses())+" insn="+(i==null?"none":i.toString()));
            if (f == null) {
                if (i == null) disassemble(ad);
                CreateFunctionCmd cmd = new CreateFunctionCmd(ad);
                boolean ok = cmd.applyTo(currentProgram, monitor);
                f = getFunctionAt(ad);
                println("  create ok="+ok+" status="+cmd.getStatusMsg()+" -> "+(f==null?"null":f.getBody().getNumAddresses()+" bytes"));
            }
        }
    }
}
