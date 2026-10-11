import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*; import java.util.*;
public class FpscrSurvey extends GhidraScript {
    @Override public void run() throws Exception {
        PrintWriter out = new PrintWriter(new FileWriter(getScriptArgs()[0]));
        Listing L = currentProgram.getListing();
        Map<String,Integer> mn = new TreeMap<>();
        int shown = 0;
        for (Instruction ins : L.getInstructions(true)) {
            String t = ins.toString();
            if (t.contains("FPSCR") || t.startsWith("fschg") || t.startsWith("frchg")) {
                String key = t.replaceAll("r\\d+","rN");
                mn.merge(key,1,Integer::sum);
                if (t.startsWith("lds") && shown < 40) {
                    shown++;
                    Function f = getFunctionContaining(ins.getAddress());
                    out.println("== " + ins.getAddress() + " in " + (f==null?"?":f.getName()));
                    Instruction p = ins; List<String> ctx = new ArrayList<>();
                    for (int i=0;i<8 && p!=null;i++){ p = p.getPrevious(); if(p!=null) ctx.add(0, p.getAddress()+"  "+p); }
                    for (String c: ctx) out.println("   "+c);
                    out.println(" > "+ins.getAddress()+"  "+t);
                }
            }
        }
        out.println("MNEMONIC COUNTS"); for (var e: mn.entrySet()) out.println("  "+e.getValue()+"  "+e.getKey());
        out.close();
    }
}
