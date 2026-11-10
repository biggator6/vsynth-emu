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
  existing behaviour is unchanged unless the change is intended. `XtFxChip` goes by JE-8086's interpreter the same way, on the XT's own programs loaded by the firmware's driver.
- The effect chip's programs have no firmware to run, so their reference is captures from the user's XT (the user's decision,
  2026-11-03; HARDWARE_FIRMWARE.md, "The effect captures"). The SH-4 side of the effects still goes by the harness. The digital outs carry the CS8420 converter's output, not the chip's words (v121): USB or Resampling for exact data.
- The project is GPL-3.0 (`LICENSE`, `NOTICE.md`, the user's decision), and JUCE under the AGPLv3 alongside it. JE-8086's ESP core (gearmulator, GPL-3.0) may be used with credit. Its copy in `research/third_party/gearmulator_esp/` stays unmodified: it's the effect chip's reference.
- Files over the kit's 300-line limit are split as the work reaches them, not all at once (the user's decision, 2026-11-09). The kit's file-size-guard refuses any added line in one, so a change to such a file starts with splitting it by what its parts do, and the planted-error lists and `mutate.sh`'s `PORT=` mapping follow the code to its new file.

## Where things are
- `plugin/Source/`: `XtBuildId` (which build this is, from git at each build by `plugin/BuildId.cmake`; shown under the wordmark), `XtFxChip` (the effect chip; `xt_fx`), `XtFxControl` (the firmware's effect driver in the interpreter, off the audio thread, feeding the chip through a queue; and `XtFxUnit`, the two with the frame's way through, in the sound behind the plugin's Effects switch; `xt_fx control`), `FxPanel` (the editor's page for them, by `xtFxCatalog`'s names and ranges from rom.2, and `XtFxValueText`, each value's text by the firmware's own routine), `XtSynth` (the synth: voices, controls, tick; `XtSynthRom.cpp`, the image the process plays from), the ports
  (`XtOsc`, `XtAnalog`, `XtEnvelope`, `XtLfo`, `XtCosm`, `XtKeys.h`, `XtSoloVoice`, `XtLinearPlayer` (Vari off, and LITE with it off), `XtLitePlayer` (LITE, Vari on),
  `XtMultiBandPlayer`, `XtBeat.h`, `XtSoloEncoder`), `XtFiles` (V-Synth's wave and patch files; a wave's chunks in `XtWaveFile.cpp`), `XtPatch` (a patch's zone as `XtSynth::Controls`) and `XtVpwFormat` (a `.vpw` as a JUCE format), `XtFirmware` (firmware harness: runs
  rom.2 routines; the machine they run on, its memory and how long a call may run, in `XtFwMachine`), `XtFloat.h` (SH-4 single precision), `PluginProcessor`/
  `PluginEditor`. Each port's firmware check is its `XT_*_MAIN` block; `XtToneCheck`
  (`XT_TONE_MAIN`) is XtSynth's: a whole firmware tone against it.
- `research/UI_DESIGN.md`: the editor's design, which new UI work follows.
- `research/HARDWARE_FIRMWARE.md`: the findings, newest version first (addresses,
  layouts, formulas, checks). `notes.txt`, `ARCHITECTURE.md`, `RESEARCH_LOG.md`.
- `research/tools/`: the scripts below. `research/ghidra_scripts/`: Ghidra scripts.

## Tools (`research/tools/`, paths in `env.sh`; work files in `build/tools/`)
- `verify.sh [--full] [rev]`: the standard validation, one line each — firmware checks
  vs baseline, A/B renders vs `rev` (default HEAD), plugin build, PluginTest, the same through the chip,
  the effects' panel, the editor's layout, engine tests, auval. Run it for every version; `--full` before a push. Every line
  fails the run when its program fails. `verify_test.sh` (when verify.sh changes; it runs it whole): a stub auval that fails must fail the run.
- `fcheck.sh [--update] [--full]`: every firmware harness on fixed cases against
  `fcheck_baseline.txt`, side by side. In a worktree set `PY` to the main checkout's `.venv/bin/python3`. A section is reused when its binary, command,
  ROM, disk image, inputs and the files it reads besides (`FILES=` before its `part` line: xtrecs.py, a project's waves) are those of a run that matched the baseline (`build/tools/fcheck/kept`
  in the main checkout, shared by the worktrees); `--full` runs them all. A new section that reads a script or data names it in `FILES`. `fcheck_keys_test.sh` (when fcheck.sh changes, a few minutes): a changed xtrecs.py and a changed wave must run again. `--update`
  only when adding checks/cases on purpose, and read its diff: in a worktree with no `analysis/projects` link it runs without the project's waves and quietly loses their 28 lines.
- `ab.sh [rev]`: XtSynth CLI renders, `rev` vs working tree; `XT_*` env vars
  set controls (see the `XT_SYNTH_MAIN` comment in XtSynth.cpp).
- `pt.sh [note]`: PluginTest (processor vs XtSynth); `TEST_*` env vars (see
  `plugin/Tests/PluginTest.cpp`); `TEST_FX=1` runs both through the chip, with `TEST_FX_KNOBS`, `TEST_FX_EDIT`, `TEST_FX_LIVE`, `TEST_BUS`, and each effect parameter's name, range and default against its ID. `TEST_FX_PANEL=1` walks the panel's pages, with what each box says against the firmware's routine (VSynthEmuTest alone: it stops there); `TEST_SNAPSHOT_FX=page` draws one. `TEST_UI_LAYOUT=1` checks the editor's layout at three sizes, that every control sets a parameter and every parameter has a control (the same: `=list` lists them); `TEST_SNAPSHOT=file.png` draws the open tab, `TEST_SNAPSHOT_TAB=n` another, `TEST_SNAPSHOT_SIZE=WxH` at that size. `TEST_ROM_SEARCH=1` checks where a new instance finds the firmware image and what's kept (the same). PluginTest sets `VSE_APP_DIR` to a temporary folder for every run, so no test reads or writes the user's settings.
- `build/plugin/VSynthEmuTeardownTest <rom>` (in `verify.sh`; `TestAppDir.h` shared with PluginTest): a processor closed while a 30 s BACKING wave encodes, which must close in ms, and the next one encode as before (v168). An encode is an `EncodeJob` (`addEncode`, `PluginEncodes.cpp`) whose firmware stops on the pool's exit signal through `Sh4StopScope`; a call with none held runs as before.
- `build/plugin/VSynthEmuRestoreTest <rom>` (in `verify.sh`, v172): a session reopened without its parts says so: a wave whose file has gone and a patch's zone waves not in its folder (`PluginState.cpp`, in the editor's "not here" and the status line), and effects whose driver won't start (`PluginEffects.cpp`'s `fxTick`, which uses no driver that isn't `ok()`). `VSynthEmuWaveFileTest <rom>` (in `verify.sh`, v169): a wave whose header claims more than its data holds is refused. `VSynthEmuRomTest <rom>` (in `verify.sh`, v173): another image given while one is in use is refused, saying why, the note an instance plays and the path the firmware's calls read as they were; a copy of the image in use is taken with nothing read again. `VSynthEmuSwitchTest <rom>` (in `verify.sh`, v174): the harnesses' research switches (XT_ROUND, XT_TRACE, the dumps, VSE_*) set in the plugin's environment change nothing. A switch is read through `XtSwitches.h`, never getenv; only build.sh's and ab.sh's programs read the environment (`HARNESS_SWITCHES` in env.sh). `VSynthEmuStepTest <rom>` (in `verify.sh`, v175): a copy of the image with the patch store, an RDAC block and the attack detection made to loop; a patch, a `.vpw` and an encode each give up within their step limit and say so. A call's limit is its machine's (`XtFwMachine.h`): `kCallSteps`, and `kStepsPerSample` more a sample of the wave it was given (`waveMachine`), where every call used to be allowed 20e9 steps, 73 s; `fwsteps.sh [every]` measures each kind of call against them. The tests share `TestMain.h` and `TestNote.h`.
- `buildid.sh`: the test, the AU and the VST3 this build installed against this checkout's id and git (in `verify.sh`). `buildid.sh --installed`: every copy found and which build it is, the way to answer "what's installed", as any checkout's build overwrites `~/Library`'s. `buildid_test.sh`: `BuildId.cmake` on a repository of its own (in `verify.sh`).
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
  `PORT=toneband`: xt_tone, XtMultiBandPlayer.cpp; `PORT=tonelite`: xt_tone, XtLitePlayer.cpp; `PORT=tonecheck`: xt_tone,
  XtToneCheck.cpp; `PORT=tonefw`: xt_tone, XtFirmware.cpp; `PORT=toneosc`: xt_tone, XtOsc.cpp; `PORT=tonelin`: xt_tone, XtLinearPlayer.cpp; `PORT=tonelfo`: xt_tone, XtLfo.cpp;
  `PORT=toneenv`: xt_tone, XtEnvelope.cpp; `PORT=tonesolo`: xt_tone, XtSoloVoice.cpp; `PORT=tonepatch`: xt_tone, XtPatch.cpp; `PORT=tonefiles`: xt_tone, XtFiles.cpp; `PORT=tonewave`: xt_tone, XtWaveFile.cpp; `SEED=<n>`: xt_tone's seed, default 2024; `PORT=solo`: xt_fw
  solocmp on the baseline's cases, XtSoloVoice.cpp; `PORT=enc`: xt_enc on the baseline's waves, XtSoloEncoder.cpp)
  and runs it, to show the check catches it (a run the error crashes counts, and one it hangs, stopped after MUT_TIMEOUT seconds, default 1500). `mutate_tone.py [cases]`: the list in
  `tone_muts.txt` (`name|old|new[|port[|seed]]`; port `solo` for what only `solocmp` reaches; seed for what the default seed's cases don't reach), each run stopping at its first difference (`XT_TONE_STOP`); `[cases] name ...`: only those.
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
- `sh4dis.py <addr> <n> ...`: a linear SH-4 listing of rom.2, no Ghidra. Ghidra's project won't open under a path holding `.claude`, so
  `ghidra.sh` fails in a worktree; run `analyzeHeadless` on the main checkout's `firmware/ghidra` instead.
- `xt_tone` opens with every zone of the image's patches against `patchOf`, then runs 512 of them and random cases on tuned waves through the whole tone; `XT_TONE_PATCHES=<folder>` for a project's. `TEST_PATCH_APPLY=1` (`pt.sh`'s binary; `TEST_PATCH_DIR=<folder>` besides): the same through the plugin's parameters, and a project on disk. `TEST_SNAPSHOT_PATCH=file.p00` draws the editor with one loaded. `TEST_PATCH_PLAY=<folder>` (`TEST_PATCH_EVERY=n`) loads and plays a project's patches, with each one's peak and what isn't here.
- `xt_files <rom>` (in fcheck, about 8 s; `build/tools/fcheck/bin`): the disk image's preset waves against the firmware's whole-wave routines and its patches' values against its reads; `XT_FILES_FIRST=1` says what differs. `wave <file.vpw> [out.raw]`, `patch <file.p00> [lo hi]`, `extract <n> <out.vpw>`. `mutate_files.py [names]`: the planted errors in `files_muts.txt`, through `PORT=files`, `PORT=wavefile` (XtWaveFile.cpp) or `PORT=filesfw mutate.sh`, against the baseline's whole `xt_files` section, about 20 minutes.
- `fxmap.cpp` (not in build.sh; build line at its top): the firmware's effect driver in the interpreter. `table`: per type, program, active words, what
  an update writes, memory operations. `mfx <type> [i v]`: a type's words as fields, and what wrote each. `boot`: what's loaded. Output goes in `build/`.
- `fxchip.cpp` (the same): a throwaway model of the effect chip, to test the decoding; a sine or impulse into a frame pair, gains at its slots. Not a port.
- `xt_fx <rom> [samples] [seed]` (in fcheck; `build/tools/fcheck/bin`): `XtFxChip` against JE-8086's interpreter, whole state each sample, every type; `XT_FX_FIRST=1` says where each case first differs. `numbers`: the firmware's own numbers on it. `speed`: its share of a core, by CPU time. `mutate_fx.py [samples] [names]`: the planted errors in `fx_muts.txt`, through `PORT=fx mutate.sh`, about a minute.
- `xt_fx <rom> control [events] [seed]` (in fcheck, about 7 s): `XtFxControl`, the same driver through the queue into `XtFxChip` against on JE-8086's chip with its waits running the samples, whole state each sample; its readers against the routines one by one; live edits against a patch; what each edit costs on the bus; two threads. `XT_FX_FIRST=1` says what differs, `XT_FX_COUNTS=1` the costs. `mutate_fx.py --control [events] [names]`: the planted errors in `fxc_muts.txt`, through `PORT=fxc mutate.sh`, about 3.5 minutes. `fxthreads.sh [events] [seed]`: the threads part under the thread sanitizer, which the plain run needs for ordering errors.
- `mutate_panel.py [--editor] [names]`: planted errors in `FxPanel.cpp`, the values' text in `XtFxControl.cpp`, the effects' wiring and the ROM search in `PluginProcessor.cpp` and the layout in `PluginEditor.cpp` (`--editor`: those and the Effects tab's cards, about 15 minutes), each built into PluginTest and run with the settings that should show it, about an hour. Each kind of run must pass with nothing planted first, or it stops. It edits the sources in place while it runs, so nothing else that reads them can run alongside it.
- A plant's verdict means something only if the run could have passed: every `mutate_*.py` first runs with nothing planted and stops unless that's clean, and `mutate.sh` refuses to run with no ROM, so a plant then reads ERROR, not caught or MISSED.
- `fxcut.py <recording>`: checks a recording of a run and cuts it into takes, levels and all; the last line says whether to go on. `fxcut_test.py` (about 90 s, when it changes): that against a simulated XT, with faults planted.
- `fxcapture.py`: the effect captures' test wave, MIDI runs and take list, into `analysis/test_files/fx/` (gitignored; the runs hold firmware defaults). `fxplay.cpp`: plays a run
  through CoreMIDI; `--test` and `--listen` check it on a virtual output. Steps: RECORDING_GUIDE.md, "Effects captures".

## What to run when
- While working: the harness of the code you changed, at a few hundred cases (`xt_tone` at 300
  takes about 10 s), and `mutate_tone.py 2000 <new names>` for the planted errors just added.
- Each version: `verify.sh` (1–2 minutes). The whole of `mutate_tone.py` (about 15 minutes) if
  `XtSynth.cpp`, `XtToneCheck.cpp` or a port changed: a planted error can stop meaning anything
  when the code under it changes (v107), and only re-running the list shows it.
- Extra seeds and larger case counts: when a version changes what `xt_tone` draws or when chasing
  a difference, not as a routine.
- If `XtSoloEncoder.cpp` changed: `mutate_enc.py` as well (about 8 minutes). If `XtFxChip.cpp` changed: `mutate_fx.py`. If `XtFxControl.cpp` changed: `mutate_fx.py --control` and `fxthreads.sh`. If `FxPanel.cpp`, `XtFxValueText` or the effects' wiring in `PluginProcessor.cpp` changed: `mutate_panel.py`. If `PluginEditor.cpp` or `UiParts.h` changed: `mutate_panel.py --editor`. If `XtFiles.cpp`, `XtWaveFile.cpp` or the file routines in `XtFirmware.cpp` changed: `mutate_files.py`. If the ROM search or `XtSynthRom.cpp` changed: `mutate_panel.py` with the eight `rom-` names (two run `VSynthEmuRomTest`), about 5 minutes. If `XtSwitches.h` changed: `mutate_panel.py switch-read`. If `XtFwMachine` or the firmware routines' work changed: `fwsteps.sh` (about 6 minutes; `1`, every preset wave, about 40) and `mutate_panel.py step-unlimited step-no-wave`.
- Before a push: `verify.sh --full`.

## Firmware facts worth knowing
- Applying a patch (HARDWARE_FIRMWARE.md, "Applying a patch"; `XtPatch`, `VSynthEmuProcessor::loadPatch`): a new `Controls` field read from a patch needs its line there as well as in `patchOf`, and `setControls` beside `xtControls`. A wave's own fine tuning (`vplp` +0, up to 14 semitones in real waves) is on `XtEncoded`, not a control: it reaches the pitch, the root, the formant ratio and the walk. A patch holds only its type's parameters, read by slot (`xtPatchTyped`). Not done: STEP and EVENT on SOLO waves, arpeggio, Multi Step Modulator; `xtPatchMissing` says which a patch asks for.
- Zones (v154; HARDWARE_FIRMWARE.md, "Zones"; the user's decision, 2026-11-05): zones 2–16 are a patch's data (`XtSynth::Zones`, `setZones`; `loadPatch` encodes their waves on the pool), zone 1 the parameters and editor. A note's zone is the first of the part's table of upper keys (IDs 0x124…, `0x894794f0`) its key isn't above; a legato note keeps its zone. A zone's IDs (0x293…0x3e8, the COSMs') are read through objects its routines point at its block; the rest is the patch's common, zone 1's (`zoneControls`). The free-running LFOs, a key-unsynced LFO's following and the MOD's block are by zone. Everything a voice reads goes through `zc(v)`: a voice reading `ctl` is a zone bug. `xt_tone`'s getter answers by the tag the eight routines leave in the object; its zone cases are the last section.
- V-Synth's files (HARDWARE_FIRMWARE.md, "V-Synth's files"; `XtFiles`): a `.vpw`'s audio is RDAC-packed, unpacked by the firmware's block routine, and the plugin plays one. A `.p00` is in a file layout: the firmware's defaults under it, then `0x8947dae0`, give the store the descriptors (12 bytes an ID: offset, width, bit, centre, default, min, max) read. Applying a patch to the plugin isn't done. The disk image's files are fragmented, so go through its FAT; 416 preset waves follow its partition, in no file.
- The ports' tables are the process's, read by every instance without a lock (`XtSynthRom.cpp`, v173): written once, by the first image to load,
  and published by an atomic flag only when all of them are in. Another file is taken only if it holds the same bytes, with nothing written;
  a different image is refused, saying why, until the host starts again. A new table goes in `loadTables` there, never written after.
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
- Every controller ends at the pad's handler (`0x894d6f10` → `0x894d8360`, `XtSynth::padController`), on Time Trip Assign X and Y, then its queued update: a pad's controller is an ordinary one too. X-Y mode's pair, Pad Assign X and Y, only feeds the Matrix's PAD sources.
  The defaults, CC82, CC83 and CC80, CC81, are Roland's chart; the descriptors say 0, but nothing else writes the pad (HARDWARE_FIRMWARE.md, "Time Trip by its controllers"). Pad Mode is the XT's own pad's choice of pair, and no tone generator code reads it. A SOLO wave taken back past its start stays silent after letting go, as its seek keeps the end flag: that is the firmware's.
- A tone's path (constructor `0x894f5860`, note start, update, the scheduler, what is hooked) is in
  HARDWARE_FIRMWARE.md, "The whole tone against XtSynth". Any change is a tone update (`0x894dc0b0`): all
  the blocks again, COSMs before the envelopes.
- A COSM's new block reads levels it holds from its note-on or last 10 ms update (object +0x104…+0x110),
  not the envelopes' output: `CosmVoice::hold` (v107). So the order of blocks and envelopes in an update has no effect.
- The patch TVA's block (`0x894ea380`, live `0x894eab40`) holds Level, Pan, the sends and CC 7, 11, 10, read from C + 0x84 + cc at each
  conversion pass. Its pan is two things: the law on the output bus's pair, which glides 3% per block, and the LFO's on the gains. A frame
  on the bus is 32 bytes: MAIN, Reverb and Chorus, MULTI, DIR, which `XtSynth::frame` holds and `xt_tone` compares word by word. A sounding tone's DIR pair doesn't update (HARDWARE_FIRMWARE.md, "The patch TVA's level and pan", "The whole frame in XtSynth").
- A controller: the channel keeps numbers below 96 (C + 0x84 + number), then its handler (table built at `0x894d7400`) queues an update of the
  part's notes (`0x894d7760`), for any number, value moved or not. `*K` is N, the part-and-channel state (+0x11c Hold, +0x11d Sostenuto). A note's
  +0x48 is key, Hold, Sostenuto: it stops when all three are clear (HARDWARE_FIRMWARE.md, "Hold, Sostenuto and the mode messages").
- The Matrix is the note's object M (the input table's +0x68), read through the getters `0x894e7010`…`0x894e7860`; a slot is not a Dest (`xtFwMatrixTables`).
  A getter forms its own group of sums and no other. A pad's move (TRIP-R) brings no update, so a key-off, which converts only release times, reaches only the envelopes' groups: `matrixRelease`. A note value (128 on) takes no sum (`withSum`).
  `0x894d96d0` on `0x89cf7634`, which older sections call "the matrix", is the Multi Step Modulator, not ported (HARDWARE_FIRMWARE.md, "The Matrix").
- The effects' driver takes messages: flags at `0x89ced200`, read by `0x894ac970` (second byte 0x20 system, 0x10 Attenuation; fourth byte 0x80 patch, 0x40 switches, 0x20 MFX, 0x10 Chorus, 0x08 Reverb), and a reader per block fetches its parameters by ID. `XtFxControl` sends those, so which readers run, and how, stays the firmware's. Its hooks are the bus, the sleep and the two parameter reads. A read is the stored value less the descriptor's centre. Some things depend on how a parameter got there, in the firmware itself: Resonant Filter's Sweep Depth does nothing when edited alone, and Reverb Delay keeps the last type's pre-EQ and the last tempo it heard: HARDWARE_FIRMWARE.md, "XtFxControl".
- A value's text is `0x89461a70(id, value, 1 to add the unit)` (`0x89462120` with 0, `0x89462130` with 1); the screens reach it through `0x89370ca0`. Each ID's string (`0x89792ba0`, from 0x55d `0x89794114`) has names split by 1, a range by 2, the unit after a 3. A value past the range faults. Note values come back as LCD font codes (`XtFxValueText`'s `noteOf`, measured against the driver). The strings are a row out at Non-Linear's last three. The routine has no format for four IDs, Hall 2's, Hall 3's and Garage's Room Size and Plate's Xover Freq, so they run as a sibling's (`formatOf`; the user's decision, 2026-11-04: the manual's scale). The 11 Reverb Times show the driver's time, `0x894b2bf0` (`isReverbTime`; samples at 44.1 kHz; the user's decision): the routine's text is wrong between values 6 and 21 and missing on three types. So the panel parts from the XT's code there on purpose: HARDWARE_FIRMWARE.md, "The effects' values as text", "The seven on the manual's scale", "Reverb Time by the driver's own table".
- The effects' tempo: the driver reads ID 0x11d itself, the Patch Tempo as it stands, not the clock's read-back. With Clock Source (0x41) at 1 it polls the clock's object (`0x894acb10`, `XtFxControl::outsideClock`) and takes a reading only past a margin of 25 + 1%, in tenths of a BPM. The driver's sleep doesn't sleep here, so `XtFxControl::tick` sends a message only once the chip has all of the last, as the XT's task takes them: without it a tempo moving for 10 s leaves the chip 150 s behind (HARDWARE_FIRMWARE.md, "The effects' parameters and panel").
- The effects don't run on the SH-4: MFX, Chorus, Reverb, the system's EQ, Master Level and Output Gain are microcode on a separate chip, Roland's ESP4 (RA0A-101; the JP-8000's instruction set, v123; ported in `XtFxChip`, v126; its delay memory 2^17 words on a 256 K × 16 DRAM; programs in rom.2,
  8 channels by DMA), with no reference but a real XT. Only the SH-4's driver and converters run in `Sh4Interp`. Map, the word as far as decoded, and the proposed
  order: HARDWARE_FIRMWARE.md, "The effects". The tones' `g` (v114) is ID 0x526, Out Gain for TG, default −24; the user's Output Gain is ID 0x76, on the chip. The plugin plays at the XT's level by these numbers (v149, the user's decision; not measured on a real XT): a full-scale wave at ⅛, both in `XtSynth::busOut` and through `XtFxUnit::fromChip`, the chip's full scale being ±1.
- Vari Sw off (v151): a SOLO wave plays on `XtLinearPlayer` (`0x89509870`/`0x89509f52`), a band wave keeps its player with its walk × the pitch (`0x8950f810`).
  The player's byte 0 takes the block's flags at each update but for 0x20, so Robot follows during a note, its grains at the next event load. Each wave's loops have rate
  factors (`vplp` +0x14, +0x16), Vari on too. HARDWARE_FIRMWARE.md, "Vari off".
- Start Offset (v152, IDs 0x2c6, 0x2fa): samples under RETRIGGER and LEGATO (the block's +0xb4 bit 0x80; STEP and EVENT give event numbers). The converter puts it at +0xb8, the last
  sample at most, compared unsigned; the voice gives each player it × 16 at +0x18: where a note walking forward starts (`0x8950fa60`) and where a walk back stops, Vari off's first
  sample. Read at note-on only. Vari off's stereo hold going backward in an Alternate loop compares its fade with FR11, which the XT never sets there (`XtLinearPlayer::setStaleFr11`:
  the plugin 0, `xt_tone` the firmware's own). HARDWARE_FIRMWARE.md, "Start Offset".
- Playback Mode (v155, IDs 0x2d2, 0x306): LEGATO takes the earlier note's player (the allocator's, 0x894d4c04; 0x894f2d00) per OSC of the same kind, not one at its wave's end
  (its +4 at 1; a LITE player never writes +4, so it is stale slot memory: `OscVoice::staleEnd`, the plugin 0, `xt_tone` the firmware's own). STEP and EVENT play between two events of the wave's
  `vpno` (STEP's count at 0x89cf7210 by part, zone and OSC; EVENT by key from C4), on band waves; a LITE wave's converter doesn't read the mode; SOLO's are not done (v156).
  The player's byte 0 takes the block's flags at each update (0x894f3fb6), so the band player's Loop bit follows the mode during a note (`XtMbPlayer::setLoopBit`). Records past the vpdt
  that leave nothing to step (a seek to the end on the last event, an event note at the last event) mark the player (`readPastWave`, silent from then); `xt_tone` ends the case. HARDWARE_FIRMWARE.md, "Playback Mode".
- LITE waves (v153): type 0, the samples alone, the voice's kind 1 (`0x894e2ec0`), with a block of its own (+0x80 mode, Vari on 2, off 0; +0xc4 the Start Offset, +0xc8 the length),
  note-on `0x8950b5b0`, update `0x8950bbb0` (no 70% steps, no Energy). Vari off is `XtLinearPlayer` (`setLite`); on, `XtLitePlayer`, two heads under windows (`0x8950a7f0`/`0x8950a7fc`).
  Its kernels run through an inline constant, H'0000 then H'3f00, so `Sh4Interp` takes H'0000 as doing nothing. Every update re-installs the players by the block's mode, so `xt_tone`
  holds Vari during a LITE note; and an analog note after a LITE one on a voice reads that note's +0x20c, +0x214 as limits (the check carries 0). A project's LITE wave is a RIFF with
  `vpsc` and no `vpfl`. HARDWARE_FIRMWARE.md, "LITE waves".
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
   (`showCosmType`, `layoutCosmType`, the type card's `extra` in `buildLayout`), `TEST_*` in PluginTest, `XT_*` in the CLI.
   Check `pt.sh` with the type, `TEST_UI_LAYOUT=1`, and a `TEST_SNAPSHOT_TYPE` snapshot.
6. `fcheck.sh --update` (the diff must only add the new lines), `verify.sh`,
   then the docs: a HARDWARE_FIRMWARE.md section (newest first) and its
   type-map row, notes.txt, RESEARCH_LOG.md. Commit `vNN: …`.

## Building
- Plugin (JUCE in ./JUCE): `SDKROOT=$SDK cmake --build build/plugin -j 10`
  (installs AU/VST3), test: `--target VSynthEmuTest`, both (verify.sh): `--target VSynthEmuVerify`. Configure once with
  `cmake -S plugin -B build/plugin -DCMAKE_BUILD_TYPE=Release` (see notes.txt).
- Use `.venv/bin/python3` (system python lacks numpy). Write multi-step shell
  work as bash scripts (zsh word-splits differently).
- The editor follows `research/UI_DESIGN.md` (the user's decision, 2026-11-04): tabs of titled cards, with the regions, palette and how to add a control there. `buildLayout()` removes what it doesn't place, so a new control
  must go in a card or its `chrome` list; `TEST_UI_LAYOUT` says if one didn't. Pages don't scroll, so cards must fit. A COSM type's own rows go in the shared three-row area (`layoutCosmType`, shown by `showCosmType`). The palette, cards and joined buttons are in `UiParts.h`; the Effects tab is `FxPanel`, itself a `Ui::Page`, and only its list scrolls.

<!-- claude-packs:start -->
<!-- Generated by claude-packs from .claude/layout.json. Edit that file and run `npx --no-install @thedecipherist/claude-packs update`; edits here are replaced. -->
## Project layout

<constraints>
Always do:
- Put every kind of thing in its one place. The rules, checks and skills for it load only there:
  - PluginTest and its checks; a port's firmware check stays in its XT_*_MAIN block, beside the port → `plugin/Tests/`
  - the findings, decisions and designs → `research/`
  - scripts and research tools that never ship → `research/tools/`
  - processBlock, the voices and XtSynth → `plugin/Source/`
  - a port of a firmware block (COSM, LFO, envelope, TVA, the effect chip) → `plugin/Source/`
  - an oscillator or wave player port → `plugin/Source/`
  - parameter IDs and layout, patch and state load/save → `plugin/Source/`
  - the editor and its UI parts (research/UI_DESIGN.md) → `plugin/Source/`
- The other folders:
  - `analysis/`: the earlier VariPhrase analysis: scripts, test files and results; the projects' waves
  - `design/`: shape.json, which the kit's shape law reads
  - `IOS_companion/`: the iOS controller, built and tested elsewhere
  - `plugin/`: the JUCE plugin's build (CMakeLists.txt, BuildId.cmake)
- Ask the user before inventing a new folder. A new folder goes into `.claude/layout.json` first.
- When a hook, check or guard blocks work that is correct, stop and report its exact message. A blocking hook is a red check: fix the hook first, with a selftest, then continue.
- When a command is refused or fails, the step fails loudly. Never report a refused write or a failed command as done.
- claude-packs (`@thedecipherist/claude-packs`) is the source of truth for every rule, check, hook and skill used to build synths. A fix to one of them, or a lesson that should hold in every synth, is made in the kit, with its test, and comes here with `npx --no-install @thedecipherist/claude-packs update`, so every synth gets it. A project edits its installed copy only to try a fix before it goes into the kit.

Never do:
- Create source files outside these folders, or put a kind of thing anywhere but its place. The folder-law and placement-law hooks refuse both.
- Put drawing code in audio or world folders, or audio and world logic in the painter or UI folders.
- Edit files in `.claude/rules`, `.claude/checks` or `.claude/hooks` to get past a block. Fix the code, or ask the user to change the law.
- Route around a block: no override variables, no writing through the shell after a refusal, no parking files in a scratchpad, no putting new code into an unrelated existing file.
- Point at a document by line number. Cite a fixed anchor (`{#id}`) or a symbol name; lines move.
</constraints>

This project has no package.json: run the kit as `npx @thedecipherist/claude-packs <command>`, wherever its files say `npx --no-install @thedecipherist/claude-packs`.

Installed packs: base, cpp, audio-core, python, shell, reverse-engineering, midi, sound-design, instrument-shell.
<!-- claude-packs:end -->
