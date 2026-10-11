// Pre-analysis: mark entry, enable aggressive instruction finder
import ghidra.app.script.GhidraScript;
import ghidra.program.model.address.Address;
public class SetupSH4 extends GhidraScript {
    @Override public void run() throws Exception {
        Address entry = toAddr(0x89300040L);
        currentProgram.getSymbolTable().addExternalEntryPoint(entry);
        disassemble(entry);
        createFunction(entry, "app_entry");
        setAnalysisOption(currentProgram, "Aggressive Instruction Finder", "true");
        println("setup done");
    }
}
