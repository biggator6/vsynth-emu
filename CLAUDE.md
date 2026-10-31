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
  (`XtOsc`, `XtEnvelope`, `XtLfo`, `XtCosm`, `XtKeys.h`, `XtSoloVoice`,
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
  equal to it (vtables, tables). `build/tools/fcheck/bin/rd <rom> <addr> <n>`:
  RAM words after C startup (.data tables).

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

## Building
- Plugin (JUCE in ./JUCE): `SDKROOT=$SDK cmake --build build/plugin -j 10`
  (installs AU/VST3), test: `--target VSynthEmuTest`. Configure once with
  `cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release` (see notes.txt).
- Use `.venv/bin/python3` (system python lacks numpy). Write multi-step shell
  work as bash scripts (zsh word-splits differently).
- The editor is fixed-layout: new rows need `kXtStripH`, `setSize` and the
  resize limits raised together.
