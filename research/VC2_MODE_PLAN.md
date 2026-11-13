# The VC-2 as its own mode: the plan (2026-11-13)

The user's decision, 2026-11-13: the VC-2 Vocal Designer (rom.4) comes into the plugin as its own mode beside the V-Synth, as
the XT's [V-CARD] button switches applications, not merged into the V-Synth's patch. rom.4's parameter IDs shift from 0x37 up,
a `.p02` reads only with rom.4's descriptors, and its code is compiled separately: a merged patch would match neither machine.

- Branch: `vc2-mode` (local), from the tag `known-good-before-vc2` (a5562d6: v195 and the VC-2 research), which the user called
  working well. master stays there until the user says to land.
- Findings: `research/VC2_VOCAL_DESIGNER.md`. The way of working is the project's: each part ported and held to rom.4 in a
  harness until 0 different, then wired in, one version at a time (CLAUDE.md).

## The steps

Each step says what "done" is measured by.

### 1. What the VC-2 shares with the V-Synth (research)

How much of rom.4's tone code behaves as rom.2's decides the size of everything after: if it matches, XtSynth's ports serve
the VC-2 with rom.4's tables and IDs, and the new work narrows to what rom.4 adds.

- 1a. rom.2's port tables found in rom.4 by their bytes, and the code that loads them in each image (`romtables.py`). **Done
  2026-11-13** (VC2_VOCAL_DESIGNER.md, "The V-Synth's tables and code in rom.4").
- 1b. Each block's firmware routine in rom.2 run beside its rom.4 counterpart on the same inputs, by its port's harness
  pointed at rom.4: envelopes, LFOs, pitch, the TVA, the OSC converters, COSM, the analog OSC, the band and SOLO players, key
  assign. Done when each block is "equal" or "differs, here". **In progress** (VC2_VOCAL_DESIGNER.md, "The V-Synth's routines
  in rom.4, run"): the pairs by code shape for every routine the ports name (`codepairs.py`); the LFO and the envelope with
  its converters run and equal, the ports equal to rom.4 (`vc2run … lfo`, `env`); COSM's note-on, update and TVF filters
  equal for 14 of 15 types (`vc2run … cosm`), the Comb differs (its note-on case moved out and reads the block otherwise),
  and COSM's 15 converters take another input block in rom.4 (no MSM term, the Matrix read inside); the OSC block's
  arithmetic (exp2, powf, velocity, pitch10, key-follow key) and key state equal (`vc2run … osc`), its wiring (the
  VariPhrase converter, pitch and gain targets, voice update, portamento settings, note setup, bend) rewritten, and its
  portamento differing (rom.4's constructor, restart and tick: an eased ending, fast glides held to ±500 cents); the analog
  OSC's 11 waveform routines equal, with and without the sub oscillator (`vc2run … analog`; rom.4's sub-object has 4 bytes
  more at +0x94), its note-on, install, update and converter rewritten, and no SUPER-SAW, FEEDBACK-OSC or X-MOD in rom.4;
  BACKING's, ENSEMBLE's and SOLO's players equal (`vc2run … band`, `solo`) on rom.4's object layout (the OSC object's 4
  bytes at +0x94, SOLO's state rearranged past R10), but for SOLO under Robot Voice; key assign equal (in the OSC run);
  LITE's player equal walking forward (`vc2run … lite`), and walking backward once rom.2's six backward reads are made
  rom.4's (the VC-2 reads backward the right way up, the V-Synth inverted); Vari off rewritten (no walk, no beat factor, its
  mode meaning otherwise: ported from rom.4, not run). Not run: kind 1's mode 1, the period-jump player. Next: COSM's other
  DSP routines. The wiring (what reads parameters by ID, COSM's
  converters) differs by construction: it is ported from rom.4, not shared, and the Comb and the portamento with it.
- 1c. Still to map: the pitch tracker (the routines that write `0x899d9bd0`); the VOCODER OSC's IDs (0x354…0x383) into the
  voice's fields; kind 6's mode and where its pitch comes from (Auto Note, Modulator Key Mode, Split Point); the part mixer's
  levels; the 64 factory `.p02` read with rom.4's descriptors; the `WLF08` wave library (58 waves); whether the VC-2 runs
  the effect chip's programs as the V-Synth does (rom.2's effect ID tables are not in rom.4).
- Second sources: the V-Card owner's manual (names and meanings) and the 64 factory patches (the corpus).

### 2. Shared plumbing (plugin/Source)

- `Sh4Interp::loadRom` by the image's header (`research/tools/vc2rom.cpp`'s rule), rom.2's map unchanged: `fcheck.sh`
  against its baseline.
- The ROM search finds rom.4 beside rom.2; `XtSynthRom.cpp`'s one-image rule made two known images (`VSynthEmuRomTest`, the
  `rom-` plants).
- rom.4's parameter tables (names, descriptors, value text) read at run time.
- An audio input: an input or sidechain bus, the mic in the Standalone, External In Type; the panel says when none is
  connected. It also gives the V-Synth's EXT IN OSC its input, so it can come first as a version of its own.

### 3. The ports, one version each

Each with its `XT_*_MAIN` harness against rom.4 at 0 different, a planted-error list and an `fcheck.sh` section.

1. The input stage (unvoiced detector) and the output stage (consonants), both engines'.
2. LEGACY (10 bands): with 3.5, the first vocoder that sounds.
3. HIRESO (32 bands, multirate): the formant warp's model is its start (`vc2warplaw.cpp`).
4. The vocoder object and its settings path, with the hold on CC 64.
5. The VOCODER OSC's carrier (kind 5) and its converter: the growl's model is its start (`vc2growllaw.cpp`).
6. The pitch tracker.
7. Kind 6 (the mic's pitch shifter: harmonizer and Auto Note) and the mic ring.
8. The part mixer (Direct Sound) and the patch EQ.

### 4. Wiring

The VC-2's voices (kinds 5 and 6) and the vocoder after the part's mix, then a whole tone held to rom.4's (as `xt_tone` is to
rom.2's); rom.4's IDs as parameters appended after every other, the session's state append-only (an older session loads); a
`.p02`'s load; a tab of cards by `research/UI_DESIGN.md`; a plugin test (parameters against rom.4's names and text, a `.p02`
through the load, a block with a mic signal against the synth); `verify.sh` lines and `mutate_panel.py` plants.

### 5. Against the machine

Recordings of the user's XT in VC-2 mode with a known signal into the mic input: for what the firmware can't show (the input's
levels, the effect chip).
