#!/bin/bash
# ghidra.sh <Script.java> <script args...>: a script from research/ghidra_scripts on
# rom.2 in the headless analyzer (read-only), output to the files the script names.
#   ghidra.sh DecompFn.java out.c 894dd870 [more addrs | range lo hi]
#   ghidra.sh Disasm.java out.asm 894eecb0 80
#   ghidra.sh MkFunc.java 894f27c0 -postScript DecompFn.java out.c 894f27c0   (create, then decompile)
#   ghidra.sh FuncAt.java outdir 894dbd26 ...   (find/create the containing function, decompile)
source "$(dirname "$0")/env.sh"
H=$(brew --prefix ghidra)/libexec/support/analyzeHeadless
cd "$REPO" && scr=$1 && shift
"$H" firmware/ghidra vsynthxt -process V-SynthXT.rom.2.bin -noanalysis -readOnly -scriptPath research/ghidra_scripts \
    -postScript "$scr" "$@" > "$WORK/ghidra.log" 2>&1 || { tail -5 "$WORK/ghidra.log"; exit 1; }
