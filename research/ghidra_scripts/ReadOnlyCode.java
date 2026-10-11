import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.*;
import ghidra.program.model.mem.*;
public class ReadOnlyCode extends GhidraScript {
    @Override public void run() throws Exception {
        Memory m = currentProgram.getMemory();
        Address cut = toAddr(Long.parseUnsignedLong(getScriptArgs()[0],16));
        MemoryBlock b = m.getBlock(cut);
        if (b.getStart().compareTo(cut) < 0) { m.split(b, cut); }
        MemoryBlock code = m.getBlock(currentProgram.getMinAddress());
        code.setWrite(false);
        for (MemoryBlock x : m.getBlocks()) println("BLOCK " + x.getName() + " " + x.getStart() + "-" + x.getEnd() + " W=" + x.isWrite());
    }
}
