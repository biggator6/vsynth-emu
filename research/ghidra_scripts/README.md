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
5. Execute firmware code in the P-code emulator:
   `-postScript EmuD5A0.java <out.tsv> [f0n preset]` runs the SOLO grain-
   parameter routine `0x8950d5a0` over an input grid. It prints one TSV row
   per case; use `-readOnly` so the project isn't modified. The script is
   the template for emulating other leaf routines: preset memory and
   registers, set PR to a sentinel, run to it, read the fields back.
6. Render a whole note with the firmware's SOLO player (0x8950cd90, one
   32-sample block per call), about 1 ms per output sample:
   `python3 -I $S/emu_inputs.py <xt_wave.wav> <raw.wav> /tmp/x` (records,
   samples and a VSE_CUTSFILE from an XT SOLO wave), then
   `-postScript EmuSolo.java /tmp/x_recs.bin /tmp/x_samp.bin out.f32 <R> <F> <walk> [energy] [maxOut] [trace.tsv] [traceFrom]`.
   The optional trace dumps the player state per sample in the same columns
   as the engine's `VSE_TRACE=file`, for diffing. For parallel runs I used
   one copy of the Ghidra project per process.
7. Run the XT's own SOLO encoder (0x8951e120) on a raw wave:
   `-postScript EmuEnc.java /tmp/x_samp.bin out.tsv` (int16 LE mono 44.1 kHz
   in; record stream "start len info" in 1/16 samples out). It takes about 3
   minutes per second of audio, and reproduces the XT's wave 003/004 encodes
   (see HARDWARE_FIRMWARE.md, "The XT's own SOLO encoder, emulated").
8. `DecompFn.java <out.c> <addr>...` or `... range <lo> <hi>` decompiles the
   functions of any program (used on vpenc.dll in `Vsynth_software/ghidra_vt`).

Why each step: SH-4 code switches FPU mode (`fschg`, `lds rN,fpscr`) inside
functions; Ghidra's decompiler needs FPSCR_PR/SZ assumed per address
(FpscrFix) and needs literal pools in read-only memory to fold the mode
constants (ReadOnlyCode + DumpRange sets RespectReadOnly). Without these,
float code decompiles as paired-register garbage.

Keep decompiler output OUT of the repo (Roland copyright). Record findings
in `research/HARDWARE_FIRMWARE.md`.
