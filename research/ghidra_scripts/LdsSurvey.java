import ghidra.app.script.GhidraScript;
import ghidra.program.model.listing.*;
import java.io.*; import java.util.*;
public class LdsSurvey extends GhidraScript {
    @Override public void run() throws Exception {
        PrintWriter out = new PrintWriter(new FileWriter(getScriptArgs()[0]));
        Map<String,Integer> mn = new TreeMap<>(); int shown=0;
        for (Instruction ins : currentProgram.getListing().getInstructions(true)) {
            String m = ins.getMnemonicString();
            if (m.startsWith("lds") || m.startsWith("sts")) {
                String key = ins.toString().replaceAll("r\\d+","rN");
                mn.merge(key,1,Integer::sum);
                if (key.toLowerCase().contains("fpscr") && m.startsWith("lds") && shown++ < 30) {
                    Function f = getFunctionContaining(ins.getAddress());
                    out.println("== " + ins.getAddress() + " in " + (f==null?"?":f.getName()));
                    Instruction p = ins; List<String> ctx = new ArrayList<>();
                    for (int i=0;i<6 && p!=null;i++){ p = p.getPrevious(); if(p!=null) ctx.add(0, p.getAddress()+"  "+p); }
                    for (String c: ctx) out.println("   "+c);
                    out.println(" > "+ins.getAddress()+"  "+ins);
                    Instruction n = ins; for (int i=0;i<3;i++){ n=n.getNext(); if(n!=null) out.println("   "+n.getAddress()+"  "+n); }
                }
            }
        }
        out.println("COUNTS"); for (var e: mn.entrySet()) out.println("  "+e.getValue()+"  "+e.getKey());
        out.close();
    }
}
