# V-Synth XT emulator

A JUCE plugin (AU/VST3/Standalone) that plays samples through ports of the
Roland V-Synth XT's own firmware: its VariPhrase encoders and players, OSC
converters, envelopes, LFOs, pitch path, key assign, COSM filters. Algorithms
are taken from the user's rom.2 (Ghidra) and every port is checked against the
firmware itself, run in an SH-4 interpreter (`plugin/Source/Sh4Interp.*`).

## Rules
- Firmware is Roland copyright: never commit firmware files, disassembly
  listings, or tables copied from them. Ports read tables from the user's
  `firmware/unpacked/V-SynthXT.rom.2.bin` at run time (gitignored).
- Each version is one local commit, `vNN: <what>`, with the attribution line.
  Push only when the user asks, and only the versions named
  (`git push origin <sha>:refs/heads/master`). "Proceed" is not a push.
- A port is done when it equals the firmware in a harness (0 different), and
  existing behaviour is unchanged unless the change is intended.

## Where things are
- `plugin/Source/`: `XtSynth` (the synth: voices, controls, tick), the ports
  (`XtOsc`, `XtAnalog`, `XtEnvelope`, `XtLfo`, `XtCosm`, `XtKeys.h`, `XtSoloVoice`,
  `XtMultiBandPlayer`, `XtSoloEncoder`), `XtFirmware` (firmware harness: runs
  rom.2 routines), `XtFloat.h` (SH-4 single precision), `PluginProcessor`/
  `PluginEditor`. Each port's firmware check is its `XT_*_MAIN` block.
- `research/HARDWARE_FIRMWARE.md`: the findings, newest version first (addresses,
  layouts, formulas, checks). `notes.txt`, `ARCHITECTURE.md`, `RESEARCH_LOG.md`.
- `research/tools/`: the scripts below. `research/ghidra_scripts/`: Ghidra scripts.

## Tools (`research/tools/`, paths in `env.sh`; work files in `build/tools/`)
- `verify.sh [rev]`: the standard validation, one line each — firmware checks
  vs baseline, A/B renders vs `rev` (default HEAD), plugin build, PluginTest,
  engine tests, auval. Run it for every version.
- `fcheck.sh [--update]`: every firmware harness on fixed cases against
  `fcheck_baseline.txt`. `--update` only when adding checks/cases on purpose.
- `ab.sh [rev]`: XtSynth CLI renders, `rev` vs working tree; `XT_*` env vars
  set controls (see the `XT_SYNTH_MAIN` comment in XtSynth.cpp).
- `pt.sh [note]`: PluginTest (processor vs XtSynth); `TEST_*` env vars (see
  `plugin/Tests/PluginTest.cpp`).
- `build.sh`: all harnesses and the XtSynth CLI. `make_inputs.py`: the raw test inputs.
- `ghidra.sh Script.java args`: headless Ghidra on rom.2 (DecompFn, Disasm,
  MkFunc then DecompFn for undefined functions, FuncAt, CallersOf).
- `poolrefs.py [--16] <hex>`: code loading a constant/address, and raw words
  equal to it (vtables, tables). `build/tools/bin/rd <rom> <addr> <n>`:
  RAM words after C startup (.data tables).
- `pnames.py <lo> <hi>`: parameter names, defaults and ranges by ID (hex), and
  each one's offset in a COSM converter's input.
- `cosmcases.py <code>`: for a COSM block code, its DSP routines (mono, stereo)
  and its cases in the binding, note-on, start values, block copy, update and
  buffer allocator.
- `mutate.sh <name> <old> <new> [cases] [pattern]`: builds xt_cosm with one
  deliberate error in XtCosm.cpp (`PORT=analog`: xt_analog, XtAnalog.cpp)
  and runs it, to show the check catches it.
- `firorder.py <listing>`: the order of additions in an unrolled sum, by a
  symbolic run of its disassembly (written for the Frequency Shifter's FIR).

## Firmware facts worth knowing
- rom.2 maps at 0x89300000; .data/.bss up to 0x8a020000 (the interpreter sets
  them up). Calls take r4–r7, then the stack; `Sh4::call(addr, {args}, sp)`.
- The SH-4 runs round-to-zero (FPSCR.RM), FMAC = product then sum (two
  roundings), denormals flushed. `XtFloat` reproduces this exactly (v62);
  harnesses flush denormals on the host too.
- Decompiles of FPU-heavy code are unreliable (double/single mode confusion):
  read the disassembly for DSP and control code.
- Parameter names by ID: pointers from 0x89790338. Descriptors (centre,
  default, min, max): 12 bytes per ID from 0x897a167c; IDs ≥ 0x55d use a
  second bank, 8 bytes per ID from 0x897a56d8.
- The voice buffer is int16 × 65536 on the XT: XtSynth's SOLO buffer (±1) is
  ×2^31 from it, the band players' (int16) ×2^16.
- `xt_fw solocmp` takes R and F as ratios (F 0 = none) and Energy as a gain
  0..1; `bandcmp` type 3 BACKING / 5 ENSEMBLE.
- The analog oscillator's map (waveform codes, routines, the converter's
  input, the hidden calibration IDs 0x528…0x55b) is in HARDWARE_FIRMWARE.md,
  "The analog oscillator, and SAW". A patch's waveform number is not the
  routine's code.

## Porting a COSM type (v65–v78 did it this way; all sixteen are done)
The type map (converters, block codes, DSP routines, which types are done) is
in HARDWARE_FIRMWARE.md, "COSM: the Dual Filter, and the map of the types".
1. `pnames.py` on the type's IDs; disassemble the converter (`ghidra.sh
   Disasm.java`): it writes the code at +0 and the block. Note its curve
   helpers (several call the firmware's powf/logf/expf; see v68, v69).
2. `cosmcases.py <code>`, then disassemble each case and the mono DSP routine.
   Read the asm, not the decompile. Note which envelope the binding puts in
   slot 1 (it varies by type), what note-on clears (D+0x1c…0x88), and the
   state the routine stores back.
3. Port in XtCosm: Params, Block, `xt…Block` (the converter), and the object
   (`noteOn`, `setBlock`, `update`, `process`). Read rom.2 tables in
   `xtCosmLoadTables`; single precision through XtFloat, constants as `K()`
   bit patterns (rom.2 often stores values a unit off the nearest float).
   Types that depend on absolute level (followers, quantizers, envelopes) take
   `process(lr, scale)` and work in the XT's units.
4. Add a section to `XT_COSM_MAIN`, modelled on the last one: the converter's
   whole block compared byte for byte; the firmware's own note-on
   (`0x894edf70`, with the buffer pool's `0x894ec2b0` hooked for types with
   buffers); updates; new blocks; the filter with its state and buffers
   compared every block. Then `mutate.sh` a few likely mistakes.
5. Wire it: XtSynth (`Controls::Cosm` fields, a type number, the block
   function, `cosmSetBlock`, note-on, update, process), PluginProcessor
   parameters (`cosm…`/`cosm2…`, the XT's defaults), the editor's type area
   (`showCosmType`, `layoutCosm`), `TEST_*` in PluginTest, `XT_*` in the CLI.
   Check `pt.sh` with the type, and a `TEST_SNAPSHOT_TYPE` snapshot.
6. `fcheck.sh --update` (the diff must only add the new lines), `verify.sh`,
   then the docs: a HARDWARE_FIRMWARE.md section (newest first) and its
   type-map row, notes.txt, RESEARCH_LOG.md. Commit `vNN: …`.

## Building
- Plugin (JUCE in ./JUCE): `SDKROOT=$SDK cmake --build build/plugin -j 10`
  (installs AU/VST3), test: `--target VSynthEmuTest`. Configure once with
  `cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release` (see notes.txt).
- Use `.venv/bin/python3` (system python lacks numpy). Write multi-step shell
  work as bash scripts (zsh word-splits differently).
- The editor is fixed-layout: new rows need `kXtStripH`, `setSize` and the
  resize limits raised together. A COSM type's own rows go in the shared
  three-row type area instead (`layoutCosm`, shown by `showCosmType`), which
  needs no size change.
