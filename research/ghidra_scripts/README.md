# Ghidra helper scripts (V-Synth XT firmware)

Our own analysis scripts — no Roland code or data. Firmware itself lives in
gitignored `firmware/`. Run headless (Ghidra 12.1.4 via Homebrew):

```bash
export JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home
H=$(brew --prefix ghidra)/libexec/support/analyzeHeadless
S=research/ghidra_scripts
```

Rebuild the project from scratch (order matters):

1. `python3 -I $S/unpack_firmware.py firmware/extracted/.../V-SynthXT.rom firmware/unpacked`
2. Import + analyse rom.2 (~8 min):
   `$H firmware/ghidra vsynthxt -import firmware/unpacked/V-SynthXT.rom.2.bin -loader BinaryLoader -loader-baseAddr 0x89300000 -processor SuperH4:LE:32:default -scriptPath $S -preScript SetupSH4.java`
3. Fill in the DSP region's functions, then FPU modes, then mark code read-only:
   `$H firmware/ghidra vsynthxt -process V-SynthXT.rom.2.bin -noanalysis -scriptPath $S -postScript SweepRegion.java 894d0000 89540000 -postScript FpscrFix.java /tmp/fpscr.txt -postScript ReadOnlyCode.java 89560000`
3b. `-postScript AssumeR0.java` — players that take the FPU mode from the
    caller in r0 (`lds r0,fpscr`): assume r0 = 0x00040001 at entry.
4. Dump/decompile a range: `-postScript DumpRange.java <lo> <hi> <outprefix>` (writes `.idx` + `.c`).
   `Disasm.java <out> <addr> <count> ...` — raw listing with pool values.

Why each step: SH-4 code switches FPU mode (`fschg`, `lds rN,fpscr`) inside
functions; Ghidra's decompiler needs FPSCR_PR/SZ assumed per address
(FpscrFix) and needs literal pools in read-only memory to fold the mode
constants (ReadOnlyCode + DumpRange sets RespectReadOnly). Without these,
float code decompiles as paired-register garbage.

Keep decompiler output OUT of the repo (Roland copyright). Record findings
in `research/HARDWARE_FIRMWARE.md`.
