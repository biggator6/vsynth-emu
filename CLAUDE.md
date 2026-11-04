# V-Synth XT emulator

A JUCE plugin (AU/VST3/Standalone) that plays samples through ports of the
Roland V-Synth XT's own firmware: its VariPhrase encoders and players, OSC
converters, envelopes, LFOs, pitch path, key assign, COSM filters, TVA. Algorithms
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
  `XtMultiBandPlayer`, `XtBeat.h`, `XtSoloEncoder`), `XtFirmware` (firmware harness: runs
  rom.2 routines), `XtFloat.h` (SH-4 single precision), `PluginProcessor`/
  `PluginEditor`. Each port's firmware check is its `XT_*_MAIN` block; `XtToneCheck`
  (`XT_TONE_MAIN`) is XtSynth's: a whole firmware tone against it.
- `research/HARDWARE_FIRMWARE.md`: the findings, newest version first (addresses,
  layouts, formulas, checks). `notes.txt`, `ARCHITECTURE.md`, `RESEARCH_LOG.md`.
- `research/tools/`: the scripts below. `research/ghidra_scripts/`: Ghidra scripts.

## Tools (`research/tools/`, paths in `env.sh`; work files in `build/tools/`)
- `verify.sh [--full] [rev]`: the standard validation, one line each — firmware checks
  vs baseline, A/B renders vs `rev` (default HEAD), plugin build, PluginTest,
  engine tests, auval. Run it for every version; `--full` before a push.
- `fcheck.sh [--update] [--full]`: every firmware harness on fixed cases against
  `fcheck_baseline.txt`, side by side. A section is reused when its binary, command,
  ROM and inputs are those of a run that matched the baseline (`build/tools/fcheck/kept`
  in the main checkout, shared by the worktrees); `--full` runs them all. `--update`
  only when adding checks/cases on purpose.
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
  deliberate error in XtCosm.cpp (`PORT=analog`: xt_analog, XtAnalog.cpp;
  `PORT=analogcosm`: xt_analog, XtCosm.cpp; `PORT=analogband`: xt_analog, XtMultiBandPlayer.cpp;
  `PORT=analogfw`: xt_analog, XtFirmware.cpp; `PORT=tone`: xt_tone, XtSynth.cpp;
  `PORT=tonecosm`: xt_tone, XtCosm.cpp; `PORT=toneanalog`: xt_tone, XtAnalog.cpp;
  `PORT=toneband`: xt_tone, XtMultiBandPlayer.cpp; `PORT=tonecheck`: xt_tone,
  XtToneCheck.cpp; `PORT=tonefw`: xt_tone, XtFirmware.cpp; `PORT=toneosc`: xt_tone, XtOsc.cpp; `PORT=tonelfo`: xt_tone, XtLfo.cpp;
  `PORT=toneenv`: xt_tone, XtEnvelope.cpp; `PORT=tonesolo`: xt_tone, XtSoloVoice.cpp; `SEED=<n>`: xt_tone's seed, default 2024; `PORT=solo`: xt_fw
  solocmp on the baseline's cases, XtSoloVoice.cpp; `PORT=enc`: xt_enc on the baseline's waves, XtSoloEncoder.cpp)
  and runs it, to show the check catches it (a run the error crashes counts). `mutate_tone.py [cases]`: the list in
  `tone_muts.txt` (`name|old|new[|port]`; port `solo` for what only `solocmp` reaches), each run stopping at its first difference (`XT_TONE_STOP`); `[cases] name ...`: only those.
- `xt_tone <rom> [cases] [seed] [inputs]` (in fcheck; `build/tools/fcheck/bin`): XtSynth's wiring
  against a whole firmware tone. `XT_TONE_FIRST=1`: each case's first difference, `XT_TONE_DUMP=<case>|a`:
  its settings. A new `Controls` field needs its ID in `patchOf` and a draw in `drawControls`, and a field
  in a list of what changed in `listedFields`.
- `xt_enc <rom> cmp <file[:2]> ...` (in fcheck): `xtSoloEncode` against the firmware's encoder, every record and the encoder's
  memory after each step (said only if it differs); `:2` is stereo, the firmware's mode 3. `xt_enc <rom> <file> [stereo]` prints
  the records. `mutate_enc.py [names]`: the planted errors in `enc_muts.txt`, through `PORT=enc mutate.sh`.
- `xtrecs.py <xt_enc> <rom>` (in fcheck): the port against the records stored in the project's own waves, the only ones made by
  an instrument.
- `firorder.py <listing>`: the order of additions in an unrolled sum, by a
  symbolic run of its disassembly (written for the Frequency Shifter's FIR).

## What to run when
- While working: the harness of the code you changed, at a few hundred cases (`xt_tone` at 300
  takes about 10 s), and `mutate_tone.py 2000 <new names>` for the planted errors just added.
- Each version: `verify.sh` (1–2 minutes). The whole of `mutate_tone.py` (about 15 minutes) if
  `XtSynth.cpp`, `XtToneCheck.cpp` or a port changed: a planted error can stop meaning anything
  when the code under it changes (v107), and only re-running the list shows it.
- Extra seeds and larger case counts: when a version changes what `xt_tone` draws or when chasing
  a difference, not as a routine.
- If `XtSoloEncoder.cpp` changed: `mutate_enc.py` as well (about 8 minutes).
- Before a push: `verify.sh --full`.

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
- The voice buffer is int16 × 65536 on the XT: `XtSoloVoice::renderXt` writes it as it is (`render` is
  that × 2^-31, ±1), the band players' (int16) are ×2^16 from it.
- `xt_fw solocmp` takes R and F as ratios (F 0 = none) and Energy as a gain
  0..1; `bandcmp` type 3 BACKING / 5 ENSEMBLE.
- `XtSoloVoice` is bit for bit: its grains, filter and gain are plain float arithmetic under FE_TOWARDZERO, as `XtMbPlayer`'s
  (HARDWARE_FIRMWARE.md, "SOLO bit for bit"). The rounding mode is set inside `noteOn` and `renderXt`, so nothing called
  from them may use XtFloat's sums: TwoSum needs round to nearest. `XtBeat` does, so it runs under `RoundNearest` there.
- A band player's backward repeat (`0x8950f50a`) can overflow its position to near 2^31, in the port too.
  An Alternate loop reaches it with the walk positive, as the bounce negates the step. The reads then leave the
  wave, which on the XT gets whatever is in RAM. `xt_tone` and `xt_analog` give them zeros, as the port reads
  (`xtFwHookBandReads`; HARDWARE_FIRMWARE.md, "A band read far from its wave", "`xt_analog` past the far band reads").
- SOLO's anchor, grain timer and ages are int32 on the XT and can overflow. `XtSoloVoice` wraps the anchor and keeps the others
  as int32: a backward repeat ends only by overflowing (HARDWARE_FIRMWARE.md, "The anchor's overflow"). A loop ported to `double`
  from int32 may not end; `solocmp` dies by `alarm(120)` so a hang fails the check.
- `xtSoloEncode` gives the records of the firmware's encoder, mono (`0x8951e120(2)`) and stereo (`(3)`, which is not mono on
  `(L+R)>>1`): `xtFwSoloEncode`, `xt_enc cmp` in fcheck, HARDWARE_FIRMWARE.md, "The SOLO encoder against the firmware's" and "The
  SOLO encoder's stereo mode". `solocmp` gives both sides the firmware's records and `xt_tone` both sides the port's, so neither
  shows a difference, bar `xt_tone`'s "the SOLO records of" lines. A rounding in the filter's paths rarely reaches a record, which
  is why `xt_enc` compares memory as well. The project's stored records were written by V-Synth 1.50 or 2.01, not the XT's 2.02:
  27 of 28 waves equal (`xtrecs.py`). Wave 373 differs by one step of the tracker's period at one mark, cause not known.
- The Patch Tempo goes to the clock's object and is read back, its tenths dropped, so 233 BPM reads as 233.4. An LFO's or envelope's value from 128 on is a
  note value. Tempo Sync and Time Trip steer the player's walk by a factor (`XtBeat`, `0x8950c280`), not its position. In `xt_tone` the firmware's clock (`0x894f4da0`)
  runs on XtSynth's, so that comparison can't see the rate, and a section of its own runs the firmware's timer routine against it (HARDWARE_FIRMWARE.md, "Tempo sync and Time Trip").
- Clock Source EXTERNAL (ID 0x41, the clock object's +0x40): one clock to an F8 (`0x894712f0`), and the tempo read is 150000/(+0x48), the whole ms over the last six clocks, so it steps. The plugin's HOST is that, on the host's
  transport (`XtSynth::hostClock`): a clock on each 24th of a quarter it passes; stopped, they run on at its tempo (a choice, not the firmware's). `xt_tone`'s firmware is fed the port's F8s, so a section of its own holds them to the
  transport's position (HARDWARE_FIRMWARE.md, "The host's tempo, by the firmware's external clock").
- A tone's path (constructor `0x894f5860`, note start, update, the scheduler, what is hooked) is in
  HARDWARE_FIRMWARE.md, "The whole tone against XtSynth". Any change is a tone update (`0x894dc0b0`): all
  the blocks again, COSMs before the envelopes.
- A COSM's new block reads levels it holds from its note-on or last 10 ms update (object +0x104…+0x110),
  not the envelopes' output: `CosmVoice::hold` (v107). So the order of blocks and envelopes in an update has no effect.
- The patch TVA's block (`0x894ea380`, live `0x894eab40`) holds Level, Pan, the sends and CC 7, 11, 10, read from C + 0x84 + cc at each
  conversion pass. Its pan is two things: the law on the output bus's pair, which glides 3% per block, and the LFO's on the gains. A frame
  on the bus is 32 bytes: MAIN, Reverb and Chorus, MULTI, DIR; `xt_tone` reads MULTI's (HARDWARE_FIRMWARE.md, "The patch TVA's level and pan").
- A controller: the channel keeps numbers below 96 (C + 0x84 + number), then its handler (table built at `0x894d7400`) queues an update of the
  part's notes (`0x894d7760`), for any number, value moved or not. `*K` is N, the part-and-channel state (+0x11c Hold, +0x11d Sostenuto). A note's
  +0x48 is key, Hold, Sostenuto: it stops when all three are clear (HARDWARE_FIRMWARE.md, "Hold, Sostenuto and the mode messages").
- The Matrix is the note's object M (the input table's +0x68), read through the getters `0x894e7010`…`0x894e7860`; a slot is not a Dest (`xtFwMatrixTables`).
  A getter forms its own group of sums and no other. A pad's move (TRIP-R) brings no update, so a key-off, which converts only release times, reaches only the envelopes' groups: `matrixRelease`. A note value (128 on) takes no sum (`withSum`).
  `0x894d96d0` on `0x89cf7634`, which older sections call "the matrix", is the Multi Step Modulator, not ported (HARDWARE_FIRMWARE.md, "The Matrix").
- The analog oscillator's map (waveform codes, routines, the converter's
  input, the hidden calibration IDs 0x528…0x55b) is in HARDWARE_FIRMWARE.md,
  "The analog oscillator, and SAW". A patch's waveform number is not the
  routine's code.

## Porting a COSM type (v65–v78 did it this way; all sixteen are done)
The type map (converters, block codes, DSP routines, which types are done) is
in HARDWARE_FIRMWARE.md, "COSM: the Dual Filter, and the map of the types". Each
type also has a stereo routine (v104, "COSM: the stereo routines"): `process(…, stereo)`,
checked in `XT_COSM_MAIN`'s last section.
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
  (installs AU/VST3), test: `--target VSynthEmuTest`, both (verify.sh): `--target VSynthEmuVerify`. Configure once with
  `cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release` (see notes.txt).
- Use `.venv/bin/python3` (system python lacks numpy). Write multi-step shell
  work as bash scripts (zsh word-splits differently).
- The editor is fixed-layout: new rows need `kXtStripH`, `setSize` and the
  resize limits raised together. A COSM type's own rows go in the shared
  three-row type area instead (`layoutCosm`, shown by `showCosmType`), which
  needs no size change.
