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
  assign. Done when each block is "equal" or "differs, here". **Done 2026-11-13** (VC2_VOCAL_DESIGNER.md, "The V-Synth's
  routines in rom.4, run"): the pairs by code shape for every routine the ports name (`codepairs.py`); the LFO and the envelope with
  its converters run and equal, the ports equal to rom.4 (`vc2run … lfo`, `env`); COSM's note-on, update, new block and
  DSP routines, mono and stereo, equal for 15 of 16 types (`vc2run … cosm`, each image's routines from its own tables by
  block code), the Comb differs (its note-on case moved out and reads the block otherwise),
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
  mode meaning otherwise: ported from rom.4, not run). Not run: kind 1's mode 1, the period-jump player, the Comb's DSP (rom.4
  has one Comb routine for mono and stereo), COSM codes 10, 11 and 17 (no converter writes them); the TVA's render the
  V-Synth's gains onto one stereo pair (`vc2run … tva`: equal once rom.2's 1/32.5 is rom.4's, a unit up, and rom.4's
  shortened step, which it keeps, is set aside), with no eight buses, sends or glide, its note-on, update, new block,
  truncated path and converter rewritten; the end test equal. The wiring (what reads parameters by ID, COSM's
  converters) differs by construction: it is ported from rom.4, not shared, and the Comb and the portamento with it.
- 1c. Still to map: the pitch tracker (the routines that write `0x899d9bd0`); the VOCODER OSC's IDs (0x354…0x383) into the
  voice's fields; kind 6's mode and where its pitch comes from (Auto Note, Modulator Key Mode, Split Point); the part mixer's
  levels; the `WLF08` wave library (58 waves); whether the VC-2 runs the effect chip's programs as the V-Synth does (rom.2's
  effect ID tables are not in rom.4). **Begun 2026-11-13:** the 64 factory `.p02` read by rom.4's own routines (`vc2run …
  patches`; VC2_VOCAL_DESIGNER.md, "The factory patches, read by rom.4"): the store's common, tone, envelope, LFO and
  arpeggio sections placed, the vocoder block over the corpus (the Modulator Key Mode and Auto Note disagreements settled:
  0–1 both), and no factory patch on the VOCODER OSC (PCM and EXT IN carriers in the first two tone blocks). Left of it: ~~the
  second descriptor bank (MFX, Chorus, Reverb, COSMs)~~ (read, below) and what the eight tone blocks are against sixteen
  envelope blocks.
  **The pitch tracker, 2026-11-13:** its routines found (the module `0x893edca0…0x893ef400`) and run on known signals
  (`vc2run … track`; VC2_VOCAL_DESIGNER.md, "The pitch tracker, run"): mode 0 is the only one selected, reaching 65.4 Hz to
  1480 Hz from its start, holding the last pitch through silence and noise, following an octave in 20–24 ms. Its analysis
  read the same day ("The pitch tracker's analysis"): zero crossings of a band-pass that follows the pitch (f0/2 to
  f0 + 25 Hz), validated by their peaks, two half-periods a period, limits 29.4–678.46 samples; the band-pass, noise count,
  gates, limits and fallback held to the firmware by the run. Left of it, for the port's harness (step 3.6): the
  estimator's rules by level, period and peak balance (read in outline) and the direction state (read, not run).
  **The VOCODER OSC's IDs, 2026-11-13** (`vc2run … vocosc`; VC2_VOCAL_DESIGNER.md, "The VOCODER OSC's IDs, run"): each of
  0x354…0x361 and the vibrato's followed from the store through the OSC input builder (`0x893a04e0`), the converter
  (`0x8939fbd0`, not `0x8939ced0`, withdrawn) and the carrier's update to its field, with its law; Growl Keyfollow by octave
  from key 60; Harm Formant the EXT IN's. Left of it: where the Harm Formant's depths and the Wave start offset are read,
  and the Dynamics' law.
  **Kind 6's pitch source, 2026-11-13** (`vc2run … autonote`; VC2_VOCAL_DESIGNER.md, "Kind 6's pitch source, run"): kind 6
  always in mode 0 (its mode setter has no caller), so it sings at its note's pitch; every OSC's converter multiplies the
  note's pitch by the pitch-source getter's ratio; Auto Note On plays C4 bent to the sung pitch (no semitone steps: the
  detector that would snap it is unreachable, and the Legato Switch's setter only keeps it off); a key's note takes the
  singer's pitch movement by a depth. Left of it: what sets that depth, Auto Note's 1 against 2 in the tone generator (read,
  not run), the mailbox's note + 12.
  **The part mixer, 2026-11-13** (`vc2run … mixer`; VC2_VOCAL_DESIGNER.md, "The part mixer, run"): one block mixer
  (`0x893b1820`) writes the bus frame: the direct signal (the raw mic when an EXTIN OSC is in zone 0 or 1, else the voices'
  second buffer) to MAIN at Direct Sound × 0.025 and to the Reverb send at that × zone 0's Reverb Send, the vocoder's output
  to MULTI at zone 0's MFX Send × 0.025, the Chorus send 0, DIR untouched; each level eases 3% a block; suspend and resume
  bypass the vocoder. Left of it: what drives the levels' offsets, which voices render into which buffer, the vocoder's
  output scale.
  **The second descriptor bank, 2026-11-13** (`vc2run … typed`; VC2_VOCAL_DESIGNER.md, "The second descriptor bank,
  read"): at `0x895131a8`, rom.2's form; the MFX, Chorus, Reverb and COSM sections placed by the firmware's own pointers and
  their types' tables found and confirmed by the code that loads them; the 64 factory patches' typed values all in range;
  rom.4 has ~~four reverb types rom.2 lacks~~ (withdrawn, "The effect chip's programs": seven new ones, 1–7), and its
  COSM type 8 is not the V-Synth's Comb Filter as it stands. Left of it: what COSM type 8 ~~and the four reverbs~~ is, and
  the typed IDs no table lists.
  **The WLF08 wave library, 2026-11-13** (`research/tools/wlflib.py`; VC2_VOCAL_DESIGNER.md, "The wave libraries, read"):
  an index, "VC-2 Wave.bin" (58 waves → 43 key maps → 192 samples), in the V-Synth's own format (its "V-Synth Wave.bin",
  224 one to one); the samples are the last 192 of the 416 preset-area streams, in "VC-2 Preset.bin"'s `"TABLE"`/`"DATA"`
  bank; all held to the link files and the bank with 0 differences, and to rom.4's parsers. Left of it: how a VC-2 wave's
  split is chosen when it plays (the port needs it: a VC-2 wave is a multisample).
  **The effect chip's programs, 2026-11-13** (`vc2run … fxprog`; VC2_VOCAL_DESIGNER.md, "The effect chip's programs"):
  rom.2's driver, paired in rom.4; the 42 MFX programs and the V-Synth's reverbs equal but for registers renumbered one
  down, the Chorus's but for 13 words, the reverbs' transition program equal; program A (routing, mix, EQ, levels)
  rewritten (110 of 768 words equal). Reverb Types: Off, seven new stereo reverbs, then Room 1–3, Hall 1–3 and Garage;
  Plate and Non-Linear only past the range, Delay in no type. So the effects go by XtFxChip and XtFxControl as the
  V-Synth's do, with rom.4's programs. Left of it: program A's routing and the seven new reverbs run on `XtFxChip`.
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
