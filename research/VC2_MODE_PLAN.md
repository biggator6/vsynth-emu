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
  COSM type 8 is not the V-Synth's Comb Filter as it stands. Left of it: ~~what COSM type 8 is~~ (below), ~~the four
  reverbs~~, and the typed IDs no table lists.
  **COSM type 8, 2026-11-13** (`vc2run … cosmgrowl`; VC2_VOCAL_DESIGNER.md, "COSM type 8, the Growl, run"): the GROWL,
  which takes the Comb Filter's place and block code. Two taps 512 back on 3267-sample lines, one moved in level and one
  in position by a phase at a subharmonic of 440 × the pitch ratio, 5…110 Hz by Speed (about 50 Hz at 64), its depth
  Intencity; the other five parameters do nothing. Held to a model bit for bit, 8000 DSP blocks, 0 different. For step 3:
  a COSM type of its own, ported from `vc2cosmgrowllaw.cpp` (ported at v234, step 4.2: `Vc2CosmGrowl`).
  **The WLF08 wave library, 2026-11-13** (`research/tools/wlflib.py`; VC2_VOCAL_DESIGNER.md, "The wave libraries, read"):
  an index, "VC-2 Wave.bin" (58 waves → 43 key maps → 192 samples), in the V-Synth's own format (its "V-Synth Wave.bin",
  224 one to one); the samples are the last 192 of the 416 preset-area streams, in "VC-2 Preset.bin"'s `"TABLE"`/`"DATA"`
  bank; all held to the link files and the bank with 0 differences, and to rom.4's parsers. Left of it: how a VC-2 wave's
  split is chosen when it plays (the port needs it: a VC-2 wave is a multisample). (Since, below.)
  **A VC-2 wave's split, 2026-11-13** (`vc2run … splits`; VC2_VOCAL_DESIGNER.md, "A VC-2 wave's split, chosen"): rom.4's
  PCM converter sends a wave the store doesn't hold through new readers: the wave's splits by the note's velocity to a
  map, the map's by the key-follow key (xtKeyFollowKey) to a sample, the first split at or above each, run on the real
  index at every velocity and key and equal to that rule, 0 different; a sample is the store's id 1…192. The wave
  splits' "upper key" withdrawn: it is velocity. Left of it: ~~what the bytes passed on do (a level, the bass maps' 12)~~
  (below), and how the store takes the bank's samples.
  **The split's bytes, 2026-11-13** (`vc2run … splitbytes`; VC2_VOCAL_DESIGNER.md, "The split's bytes, followed"): fields
  the V-Synth always had, in the OSC's input, which its own fetch fills neutral. The wave split's byte and the map split's
  are two levels on the OSC's gain through the level curve (the port's `xtOscGain`, its two 127s); the 0 or 12 shifts the
  sample's root key down, so 12 plays an octave up (the port's `xtPitchCents`, its shift 0); the one-split flag only
  changes EVENT on a VariPhrase sample, which the factory index never gives a 0. Both laws equal the firmware over every
  value, 0 different. 179 of the 192 samples are LITE. For step 3: `xtOscGain` and `xtPitchCents` take the bytes.
  **The sample bank, 2026-11-13** (`vc2run … bank`; VC2_VOCAL_DESIGNER.md, "The sample bank, loaded"): rom.4 loads
  "VC-2 Preset.bin"'s table at start-up, then every sample of every wave the library names (Wave Numbers 1…128; the
  link files name waves 1…58), each as the store's id n, its index number, its stream found by its file name and read
  as memory. Run on the real files and equal to wlflib's own reading, 192 of 192. The kind is the index's byte, not the
  stream's header: 179 SOLO-encoded streams play as LITE. For step 3: a VC-2 sample is its stream by name, read as the
  plugin reads the preset area, with the index's kind. Left of it: ~~the wave reader itself (`0x893474d0`)~~ (below), and
  how the link files fill the library.
  **The wave reader, 2026-11-13** (`vc2run … wavein`; VC2_VOCAL_DESIGNER.md, "The wave reader, run"): the VC-2's own
  reader of a stream from memory, run for real into rom.4's store. For all 192 samples the header, loop record, vpno,
  vpdt and frames equal `xtWaveRead`'s, the BACKING sample's bands `xtMbBuildBacking`'s, and the audio the plugin's
  decoder over whole RDAC blocks. `xtWaveRead`'s audio differs in the last part block of 177 samples, where it feeds
  zeros past the frames. For step 3: a VC-2 sample is read by `xtWaveRead` with its audio decoded over whole blocks.
  **The V-Synth's own wave load, 2026-11-13** (HARDWARE_FIRMWARE.md, "The V-Synth's wave load, read"): read, not
  settled. The file reader takes the chunks and skips the audio, a stream apart pumps the audio through the same RDAC
  decoder object as the VC-2's, and the count it is fed comes from a sample count in code that needs rom.2's store,
  the preset drive and a task to run. So whether the plugin's zeros in a preset wave's last part block are the
  V-Synth's is open; the plugin is left as it is.
  **The effect chip's programs, 2026-11-13** (`vc2run … fxprog`; VC2_VOCAL_DESIGNER.md, "The effect chip's programs"):
  rom.2's driver, paired in rom.4; the 42 MFX programs and the V-Synth's reverbs equal but for registers renumbered one
  down, the Chorus's but for 13 words, the reverbs' transition program equal; program A (routing, mix, EQ, levels)
  rewritten (110 of 768 words equal). Reverb Types: Off, seven new stereo reverbs, then Room 1–3, Hall 1–3 and Garage;
  Plate and Non-Linear only past the range, Delay in no type. So the effects go by XtFxChip and XtFxControl as the
  V-Synth's do, with rom.4's programs. Left of it: ~~program A's routing and the seven new reverbs run on `XtFxChip`~~
  (both done below).
  **Program A's routing, 2026-11-13** (`vc2run … fxroute`; VC2_VOCAL_DESIGNER.md, "Program A's routing"): rom.4's
  effect driver run on an `XtFxChip` (`vc2fxdriver.h`) beside rom.2's `XtFxUnit`, impulses into each GRAM word: MAIN,
  MULTI, the MFX and the returns reach the outputs as on the V-Synth, at its gains; the Reverb's input is a stereo pair
  (0x72, 0x74) fed 6 dB lower, the MFX's input moved to 0x76, 0x78, the system EQ's mid bands are the patch's EQ 1 and
  EQ 2 on MULTI alone, DIR goes nowhere, and the outside input's right reaches 0x9a alone. Left of it: ~~whether the
  Chorus's sides are swapped~~ (settled, below), and what reads 0x90…0x9a outside the chip.
  **The new reverbs on XtFxChip, 2026-11-13** (`vc2run … fxreverb`; VC2_VOCAL_DESIGNER.md, "The new reverbs on
  XtFxChip"): the seven, set by rom.4's driver, decay as reverbs, stereo in and out (Church as two, one a side), and
  with their damping at 0 dB their T30 is the panel's Rev Time within 1 to 2% on Plate, OperaHouse, Cathedral and
  Church (Room and StudioSpring short at long settings; Echoes is repeats). Left of it: Room Size, Repeat Time, the
  pre-EQ and Pre Delay's laws; any check against a real VC-2.
  **The Chorus's sides, 2026-11-13** (`vc2run … fxchorus <rom.2> [lfo]`; VC2_VOCAL_DESIGNER.md, "The Chorus's sides"):
  Chorus 1 on both images; with Depth 0 rom.4's outs are rom.2's sample for sample; at the defaults both LFOs are one
  triangle with the sides in antiphase, and lined up rom.4's outs are rom.2's to a unit, its sides as they are or
  crossed at half a cycle on; the LFO runs free through every reload on both, so nothing but the time since the chip
  started says which side leads. The VC-2 mode runs rom.4's program B as it stands. Left of it: what program B's 13
  differing words do, the other Chorus types.
- Second sources: the V-Card owner's manual (names and meanings) and the 64 factory patches (the corpus).

### 2. Shared plumbing (plugin/Source)

- `Sh4Interp::loadRom` by the image's header (`research/tools/vc2rom.cpp`'s rule), rom.2's map unchanged: `fcheck.sh`
  against its baseline. **Done 2026-11-14, v197** (HARDWARE_FIRMWARE.md, "A firmware image's own header"): the loader
  and the rest of the interpreter's setup split into `Sh4Setup.cpp`; the header's sections, rom.2's the very addresses
  its loader held, its region to 0x8a020000 as before; `vc2rom.cpp` calls it; fcheck as baseline with all 165 sections
  run, verify.sh clean, seven vc2run runs on rom.4 HEAD's output to the byte, `rd` reading rom.4's .data.
- The ROM search finds rom.4 beside rom.2; `XtSynthRom.cpp`'s one-image rule made two known images (`VSynthEmuRomTest`, the
  `rom-` plants). The plugin loads both images at its start, so that switching modes is seamless (the user's decision,
  2026-11-14): each held once a process in its own slot, neither reloaded nor needing a restart at a switch, and the VC-2's
  tables, when its ports come (step 3), read at that start as rom.2's are. **Done 2026-11-14, v198**
  (HARDWARE_FIRMWARE.md, "Two known images"): each image known by its header's sections, a file of the other or of
  neither refused saying so; rom.4 sought at every start, beside rom.2 first and with no rom.2; VSynthEmuRomTest 17/17,
  the rom- plants moved with their code and four rom4- plants, all caught.
- rom.4's parameter tables (names, descriptors, value text) read at run time. **Done 2026-11-14, v199**
  (HARDWARE_FIRMWARE.md, "rom.4's parameter tables in the plugin"): `Vc2Params.cpp`, read in `loadVc2` at the start; the
  text by rom.4's routine through `XtFxValueText` made an image's (split out of XtFxControl.cpp first); `xt_vc2params` in
  fcheck, 0 different against rom.4's getter, store reader, typed reader and unit wrapper; ten plants caught. Left for the
  VC-2 panel: the IDs rom.4's routine has no format for, and the five it faults on (VC2_VOCAL_DESIGNER.md, "rom.4's values
  as text").
- An audio input: an input or sidechain bus, the mic in the Standalone, External In Type; the panel says when none is
  connected. It also gives the V-Synth's EXT IN OSC its input, so it can come first as a version of its own.
  Found 2026-11-14, before it: the input reaches a tone through the effect chip. Its program reads the outside input at
  GRAM 0x11 and 0x13 and copies it to 0x90…0x9a (VC2_VOCAL_DESIGNER.md, "Program A's routing"); the chip's DMA back to
  the SH-4 is 8 int32 a frame, and the EXT IN OSC's player (`0x895097d0`) reads words 4 and 5 of each frame through
  `[0x8987c7a0]` (the returned block + 16, set at `0x894daca2`), times a gain gliding 1/32 a sample; External In Type and
  Source (IDs 0x78, 0x7c) are the effect driver's, kept at `0x89ced225` +6, +7 and applied by `0x894aee80` as the chip's
  coefficients. So two versions, the SH-4 side first:
  - v203 (committed as 6a1ace1 under v200, master's number; renumbered by a new commit): the EXT IN OSC held to rom.2
    in `xt_tone`: its converter (`0x894e3830`), the voice of kind 4 and its player,
    with XtSynth given an input a block in the returned words' form, both sides fed the same. **Done 2026-11-14**
    (HARDWARE_FIRMWARE.md, "The EXT IN OSC"): `XtExtIn`, kind 4 in XtSynth's start, update and render; nothing bound to
    it (its OSC's envelopes and LFO hold, the tone's neutral envelope 0 and LFO 0 in their place), stereo, root 440;
    `xt_tone`'s EXT IN mode (`xt_tone rom extin`) 0 different, the plain run's cases as they were; XtSynth.cpp split first.
  - v204: the input's way in: the plugin's input bus (the Standalone's mic), through the chip's program as on the XT,
    External In Type and Source by the driver; the panel says when an EXT IN OSC has no input. With the Effects switch
    off, when the chip doesn't run, the EXT IN OSC hears the input as the chip's program would hand it back, computed
    without the chip and held to it by a check (the user's decision, 2026-11-14: "Copy without the chip", over running
    the chip for the input alone or leaving EXT IN silent with Effects off). Split in three (2026-11-14):
    - v204, the chip's side: the firmware's EXT IN request (`0x894a7dd0`: External Input Type, ID 0x78, to the input
      routing's byte 6 at `0x89ced225`; Source, 0x7c, to byte 7; bytes 1, 2 and 5 to 0; then event 0x40 by `0x894aee70`)
      and the event's routine (`0x894aee80`, which applies what changed, with its fades) run by XtFxControl; the host
      input into the chip's input slots (analog 0x10, 0x12; USB 0x18, 0x1a) and words 4 and 5 back (GRAM 0x98, 0x9a)
      into `XtSynth::input`. Checked by `xt_fx control` with an input and the two settings among its events, on
      JE-8086's chip. What calls the request on the XT isn't found (no pointer to it in rom.2, no branch to it within
      12 KB): the plugin's input is EXT IN's alone, so it asks at the start and at each change. **Done 2026-11-14**
      (HARDWARE_FIRMWARE.md, "The input's way in, the chip's side"): `XtFxInput.cpp`; the driver against JE-8086's chip
      with the analog pair's noise and the two settings drawn, 0 different, once `XtFxChip` ran core 0's last twelve
      clocks, which the routing's path is the first to reach; each Type under each Source against the manual's law, 8 of
      8; the effects' sound with no input as it was, to the bit. XtFxChip.cpp's check split into `XtFxCheck*` first, and
      `XtSynth::busOut` with `render` into `XtSynthOut.cpp`. For v206 besides: the host's input at its own rate, which
      the chip takes at 44.1 kHz.
    - v205, Effects off: the copy without the chip, from the routing's coefficients as the driver leaves them, held to
      `XtFxChip` under every Type and Source. **Done 2026-11-14** (HARDWARE_FIRMWARE.md, "The input with the Effects
      off"): `XtExtInCopy`, program A's path settled as a law (a frame late; STEREO, MONO MIX each side halved, MONO L,
      MONO R), measured bit for bit on `XtFxUnit` and held to it in `xt_fx control`, 9 of 9. The plugin's choice of it
      or the unit by the Effects switch goes to v206.
    - v206, the plugin: its input bus (the Standalone's mic), OSC Type's third choice, a patch's EXT IN read
      (`XtPatch`, `xtPatchMissing`), External Input Type and Source as parameters, and the panel's word when an EXT IN
      OSC has no input. Split in two (2026-11-14): v206 the parameters and controls, **Done 2026-11-14**
      (HARDWARE_FIRMWARE.md, "The input's parameters and controls"): OSC Type's EXT IN, the two settings as host
      parameters by the firmware's own names, ranges and text, a patch's EXT IN played, the OSC panel's EXT IN face
      and the FX page's two boxes.
    - v207, the audio: the plugin's input bus and the Standalone's mic, the host's rate to the chip's 44.1 kHz, the
      unit (Effects on) or `XtExtInCopy` (off, by External In Type) into `XtSynth::input`, and the panel's word when an
      EXT IN OSC has no input; PluginTest with an input against XtSynth. **Done 2026-11-15** (HARDWARE_FIRMWARE.md,
      "The input's audio"): a stereo input bus (off, mono or stereo), `XtHostInput` bringing the host's rate to 44.1 kHz
      on XtSynth's own sinc (`XtSinc`) behind a fixed latency, handed to `XtSynth::busOut` as its feed, which sends each
      block the way the bus goes, so the two ways are one decision and aligned; an EXT IN OSC alone renders; the OSC
      panel says when there's no input. `VSynthEmuExtInTest`: the processor against XtSynth given the input by v205's
      law, Effects off and on, sample for sample (busOut's push of the returned words held at last), and the feed at
      nine rates.

### 3. The ports, one version each

Each with its `XT_*_MAIN` harness against rom.4 at 0 different, a planted-error list and an `fcheck.sh` section.

1. The input stage (unvoiced detector) and the output stage (consonants), both engines'. Split in two (2026-11-15):
   - v208, the input stage (`0x893ed040`, its reset's part, the settings' copy and the voiced query): `Vc2Unvoiced`,
     xt_vc2in at 0 different, 35 plants caught, its coefficients read by `loadVc2` at the start. **Done 2026-11-15**
     (HARDWARE_FIRMWARE.md, "The VC-2's unvoiced detector").
   - v209, the output stage (`0x893ed510` mono, `0x893ed690` stereo): the consonants from the delayed mic, gated by
     v208's flag. `Vc2Unvoiced::consonants` (Vc2Consonants.cpp), xt_vc2in on the whole path at 0 different, 66 plants
     caught. **Done 2026-11-15** (HARDWARE_FIRMWARE.md, "The VC-2's consonants"). The mic ring's writer, the part
     mixer's copy at `0x893ed410`'s pointer, is item 8's.
2. LEGACY (10 bands): with 3.5, the first vocoder that sounds. Split in two where the firmware divides it (2026-11-15):
   - v210, the analysis, a function on .bss globals (`0x893ef740`, its init, the rows' reader, the hold's three
     routines): `Vc2LegacyAnalysis`, xt_vc2legacy at 0 different, 47 plants caught, its tables read by `loadVc2` at the
     start. **Done 2026-11-15** (HARDWARE_FIRMWARE.md, "LEGACY's analysis").
   - v211, the object (`0x893f23c0`…`0x893f3000`): its constructor, reset, mono synthesis (slot 2, the carrier's bands,
     the output EQ) and settings (slots 4 to 7, onto v210's hold): `Vc2Legacy`, xt_vc2legacy's second part at 0
     different, 98 plants caught (with v210's). Split again by its routines (2026-11-15). **Done 2026-11-15**
     (HARDWARE_FIRMWARE.md, "LEGACY's synthesis").
   - v212, the stereo synthesis (slot 3, `0x893f2980`): left and right through every band together, the sum as it goes,
     slot 7's level each sample: `Vc2Legacy::stereo`, xt_vc2legacy's object part run a second time at 0 different.
     **Done 2026-11-15** (HARDWARE_FIRMWARE.md, "LEGACY's stereo synthesis"). LEGACY is in, not yet heard: it sounds with
     item 5's carrier and item 4's object.
3. HIRESO (32 bands, multirate): the formant warp's model is its start (`vc2warplaw.cpp`). Split by job (2026-11-15):
   - v213, the formant warp (slot 4, `0x893f15d0`): `vc2HiresoMapOf`, the model's law in XtFloat, xt_vc2hireso at 0
     different on the model's 40,352 maps, 19 plants caught; the model went with it. **Done 2026-11-15**
     (HARDWARE_FIRMWARE.md, "HIRESO's formant warp").
   - done (v214): the rate filters, `Vc2HiresoRate` (`0x893efc30`'s objects: the decimators `0x893efd20`, `0x893efdb0`
     and the carrier's `0x893efc80`, the interpolators `0x893efef0`, `0x893efe40`; the stereo path's four come with
     slot 3), on their three tables from rom.4, against the routines in xt_vc2hireso's second part (HARDWARE_FIRMWARE.md,
     "HIRESO's rate filters").
   - done (v215): the analysis, `Vc2HiresoAnalysis` (slot 1's `0x893ed930` on its .bss globals, its init `0x893ed8f0`,
     slot 0's clear `0x893ede50`, the envelopes' update `0x893edd10` and getter `0x893eddf0`), against the routines in
     xt_vc2hireso's third part (HARDWARE_FIRMWARE.md, "HIRESO's analysis").
   - done (v216): the object, `Vc2Hireso` (constructor `0x893f0a10`, slot 0's reset, slots 1 and 4 onto the analysis
     and the warp, slot 5's settings with the templates, slots 6 and 7's hold), against rom.4's object in
     xt_vc2hireso's fourth part (HARDWARE_FIRMWARE.md, "HIRESO's object").
   - done (v217): the mono synthesis, `Vc2Hireso::mono` (slot 2 `0x893f0c60`, the carrier bank `0x893f1a10`, the
     groups' synthesis `0x893f1b10`), against slot 2 in xt_vc2hireso's fourth part (HARDWARE_FIRMWARE.md, "HIRESO's mono
     synthesis").
   - done (v218): the stereo synthesis, `Vc2Hireso::stereo` (slot 3 `0x893f1240`, its carrier bank `0x893f1d40`, groups'
     synthesis `0x893f1ec0` and the stereo rate filters), against slot 3 and each stereo routine in xt_vc2hireso
     (HARDWARE_FIRMWARE.md, "HIRESO's stereo synthesis"). HIRESO is ported whole; it reaches the sound with step 4.
4. The vocoder object and its settings path, with the hold on CC 64. Split in two (2026-11-15):
   - done (v219): the ground. The engines are one interface, `Vc2Engine`, which LEGACY and HIRESO implement. The
     vocoder's settings struct is `Vc2VocoderSettings`. The checks' compares are one shared file, `Vc2CheckWords`. The
     vocoder object's routines are read (HARDWARE_FIRMWARE.md, "The vocoder object, read").
   - done (v220): `Vc2Vocoder`, the builder, the mode switch with HIRESO's input hook, the settings path with the edit
     words, the block drivers, the pedal and the glue, against rom.4's own object in xt_vc2vocoder
     (HARDWARE_FIRMWARE.md, "The vocoder object"). The CC 64 and CC 121 routing to the pedal belongs to the wiring.
     Step 4 is done; the vocoder reaches the sound with the wiring.
5. The VOCODER OSC's carrier (kind 5) and its converter: the growl's model is its start (`vc2growllaw.cpp`). Split in two
   where the firmware divides it (2026-11-15):
   - done (v221): the carrier, `Vc2Carrier`, the object at the voice + 0xa90: its init `0x893ebed0` with the phase's
     start on the OS clock (`0x893f31a0`), settings `0x893ec450`, update `0x893ec4d0` (the balance, the noise filters by
     pitch, the Dynamics on the input stage's peak) and render `0x893ebfa0` with the growl, against rom.4's routines on
     a carrier in its RAM in xt_vc2carrier (HARDWARE_FIRMWARE.md, "The VOCODER OSC's carrier").
   - done (v222): its converter's own part (`0x8939db00` from its P stores: `vc2VocoderOscConvert`) and kind 5's update
     (`0x893afff0`: the gain, the pitch, the growl and the balance with their modulators: `vc2Kind5Update`), which give
     the carrier P, against rom.4's in xt_vc2carrier's second part (HARDWARE_FIRMWARE.md, "The VOCODER OSC's converter
     and kind 5's update"). The converter's first half, the OSC block it shares with the analog kind, comes with the
     voice in the wiring. Step 5 is done.
6. The pitch tracker. Split in two by the module's calls (2026-11-16):
   - done (v223): the frame, `Vc2Tracker`: the module's state word for word, its static initializers, the reset, the mode
     setter, the getters, and the block entry (`0x893ee280`: the band-pass, the peaks and counters, the fallback
     period, the noise count, the noisy-hold flag), its crossings through an interface, against rom.4's routines with
     the crossing routines hooked to record their calls, in xt_vc2tracker (HARDWARE_FIRMWARE.md, "The pitch tracker's
     frame").
   - done (v224): the analysis, `Vc2TrackerAnalysis`: the crossing routines (`0x893ee570`, `0x893ee7d0`, one routine
     over a direction table), the onset check (`0x893eea50`), the estimator (`0x893eeb90`) and taking an estimate
     (`0x893ef110`), and the tracker whole against rom.4's module with no hook, on held tones, pitch steps, noise and
     silence (HARDWARE_FIRMWARE.md, "The pitch tracker's analysis"). Step 6 is done.
7. Kind 6 (the mic's pitch shifter: harmonizer and Auto Note) and the mic ring. Split in two by what the module holds
   (2026-11-16):
   - done (v225): the pitch shifter, `Vc2Kind6`, and the mic ring, `Vc2MicRing`: the ring's globals' setter, reset,
     write and mode setter; a voice's setters, start and render (the glides, the filter's targets, T_out, the core's
     epochs, grains, two heads under the window, lift, SVF, unvoiced mix and gain), against rom.4's routines on voices
     in its RAM in xt_vc2kind6, both modes, the interval (the harmonizer) by the mode setter that nothing calls
     (HARDWARE_FIRMWARE.md, "Voice kind 6"). rom.4's shared math (the cubic, sine, cosine and window) one module,
     `Vc2Math`, the carrier's copies taken out.
   - done (v226): Auto Note and the pitch source, `Vc2AutoNote`: the input stage's wrapper (`0x893ebb00`: the
     tracker's block, Auto Note's state and its notes to the tone generator through an interface), the On Switch's and
     Legato Switch's setters, the release request, the init, the pitch-source getter (`0x89394a40`) and its bend by the
     singer's movement (`0x893ebc70`), against rom.4's with the tracker inside in xt_vc2autonote; the semitone detector
     and snap not ported, as nothing sets their switch (HARDWARE_FIRMWARE.md, "Auto Note and the pitch source").
     Step 7 is done.
8. The part mixer (Direct Sound) and the patch EQ.
   - done (v227): the part mixer, `Vc2PartMixer`: the block (`0x893b1820`, both paths: the mic and the second input word,
     the input stage, kind 6's ring, the unvoiced detector when no engine plays, the voices into the carriers' buffer or
     the other, the detector's ring, the vocoder unless bypassed, the bus frames at three easing levels), the level
     routine, the targets and their setters, suspend and resume, the EXTIN flag's setter and getter and the reset,
     against rom.4's in xt_vc2mixer with the voices and calls hooked (HARDWARE_FIRMWARE.md, "The part mixer"). The
     shared clamp moved into `Vc2Math`.
   - moved to the wiring: the patch EQ, the effect chip's (rom.4's driver reads it into program A's mid bands, on MULTI
     alone; VC2_VOCAL_DESIGNER.md, "Program A's routing"), comes with rom.4's effects; and the voices'
     update and schedule (`0x893b16a0`, `0x893b1710`), which call the voices, come with them. Step 8 is done, and with it
     step 3.

### 4. Wiring

The VC-2's voices (kinds 5 and 6) and the vocoder after the part's mix, then a whole tone held to rom.4's (as `xt_tone` is to
rom.2's); rom.4's IDs as parameters appended after every other, the session's state append-only (an older session loads); a
`.p02`'s load; a tab of cards by `research/UI_DESIGN.md`; a plugin test (parameters against rom.4's names and text, a `.p02`
through the load, a block with a mic signal against the synth); `verify.sh` lines and `mutate_panel.py` plants.

Split (2026-11-16), from what rom.4's tone turned out to be. Of rom.2's tone routines, rom.4 pairs the constructor
(`0x894f5860` → `0x893b2db0`), the free LFOs' tick and a few of the allocator's (`codepairs.txt`); the note start, the
tone's note-on, start, update and render and the converter pass have no counterpart. The VC-2's tone is its own object
(vtable `0x898cd568`, in the mixer's file): its update (+0x0c, `0x893b4940`) ticks the envelopes and LFOs (rom.2's,
ported) and updates two OSC voices (`0x893ae1c0`); its render (+0x24, `0x893b4bb0`) runs one OSC voice (`0x893ae9f0`)
into a buffer and then the TVA (`0x893b2640`); its carrier's render (+0x18, `0x893b4b10`) runs two, mixes them
(`0x893ade50`) and goes through a member pointer before the TVA. Three tasks drive it: the notes' (`0x8938fc0e`, the
schedule `0x893b1710`), the update's (`0x893953c0…`, the voices' update `0x893b16a0` after the pitch source), and the
block's (`0x89395ace`: the input frames' pointer, then the part mixer). So the wiring goes bottom-up, as step 3 did, each
version against rom.4's routines with what it calls hooked:
1. The tone object's frame: the constructor and every method of the vtable, its parts (OSC voices, COSMs, TVA, envelopes,
   LFOs) through interfaces, against rom.4's with the parts hooked. Split in two (2026-11-16):
   - done (v228): `Vc2Tone` and `Vc2TonePool`: the tone's own words, the pool of 24, the constructor and destructor,
     the update, both renders, the clock, the two counts, the level test, held, start and the install pass, against
     rom.4's on its own pool in xt_vc2tone, every part routine hooked (HARDWARE_FIRMWARE.md, "The VC-2's tone object").
     The three structure renders are left out: nothing in rom.4 reaches their table.
   - done (v229): the note-on, `Vc2Tone::noteOn` (`0x893b3390` with `0x893b4380` and `0x893b4660`): the parts'
     note-ons and bindings by the input's blocks, the key-unsynced LFOs following a tone of the same id or a free
     object, the legato's take of the tone of the input's serial, the input's fix-up, the COSMs' set-ups by the
     structure, the TVA, against rom.4's in xt_vc2tone with the input, the lists and the part words drawn on both sides
     (HARDWARE_FIRMWARE.md, "The VC-2's tone's note-on"). Step 4.1 is done: the tone's own code, its parts behind an
     interface.
2. The tone's parts with rom.4's own wiring, a version each where rom.4 differs from rom.2 (step 1b's list): the OSC
   voice (its block, portamento, the players' install on rom.4's object layout, Vari off), the analog OSC's note-on,
   install, update and converter, COSM's converters and the Growl (type 8), the TVA's render on one stereo pair, and
   kinds 5 and 6 into the tone (their ports, steps 3.5 and 3.7). The players, envelopes and LFOs are XtSynth's ports.
   The OSC voice split by what rom.4 holds (2026-11-16): its frame, then its note-on by kind with the pitch and gain
   targets ("its block"), the portamento, the players' installs on rom.4's object, Vari off.
   - done (v230): the OSC voice's frame, `Vc2OscVoice`: its own words (the block's copy, the OSC object a take moves
     whole, the legato word, the tail), the constructor, the install, the update with the re-seek, the note-on up to the
     kind's own (`0x893af0b0`), the ramp's start, the counts, the settings, the legato, the render, the getters and the
     take, each switch on the kind one row of `Vc2OscVoiceKinds.cpp`, against rom.4's in xt_vc2osc with every callee
     hooked (HARDWARE_FIRMWARE.md, "The VC-2's OSC voice").
   - done (v232): the kind's note-on, `0x893af0b0` (kinds 0 and 4 to 7 inline, kinds 1 to 3 their own note-ons, LITE's
     `0x893c7c30`, VariPhrase's `0x893af420`, the analog's `0x893c22e0`, through the parts), and the targets
     `0x893af9f0…0x893aff60` (the cents, the ratios, the walk's and the formant's, the gain and the level, the getters'
     tails among them), each kind's way a field of its row, against rom.4's in xt_vc2osc; the tail's words named by
     rom.2's and rom.4's use (HARDWARE_FIRMWARE.md, "The VC-2's OSC voice's note-on and targets"). This item's
     `0x893af118` is no routine: it lies inside `0x893af0b0`, the `nop` in a `bra`'s delay slot after one case's `jsr`
     (the next case starts at 0x893af120; the switch's table is the halfwords at 0x893af0e0); and the targets end at
     `0x893aff60`'s pool (`0x893affe0`), not 0x893affc0.
   - done (v236): the portamento, `Vc2PitchRamp`: rom.4's pitch ramp, 0x64 bytes word for word, its constructor,
     start, restart (the settings' routine), legato update, tick (the glide eased under 100 ms, type 0's gain factor)
     and value, with logf behind `Vc2ToneShared` (rom.2's core, shared); the voice owns its three ramps, xt_vc2osc's
     ramp hooks out and a ramp part of its own (HARDWARE_FIRMWARE.md, "The VC-2's portamento").
   - done (v238): the players' installs on rom.4's object (`0x893af7a0`, `0x893af870`) and VariPhrase's own
     note-on (`0x893af420`), the three in rom.4's OSC code beside the voice (split 2026-11-17 by what rom.4 holds),
     each kind's way a row's field, against rom.4's in xt_vc2osc with the players' renders, setups, lookup and seeks
     hooked (HARDWARE_FIRMWARE.md, "The VC-2's players' installs and VariPhrase's note-on").
   - done (v240): LITE's own note-on (`0x893c7c30`), in the players' code: the object's head and the targets' words,
     its byte 0 from the block, then by the wave's type Vari off's words, the mode-1 player's range or the two heads'
     words (rom.2's XtLitePlayer's, four bytes up), `0x893cbf40` on the object and the type's seek, against rom.4's
     in xt_vc2osc; what it shares with VariPhrase's note-on one code (`Vc2OscPlayers.h`), and the check's LITE
     note-ons on a real wave's root Hz (HARDWARE_FIRMWARE.md, "The VC-2's LITE note-on").
   - done (v242): the analog's own note-on (`0x893c22e0`, about 1,000 instructions to 0x893c2ac2), in the analog
     OSC's code, whole: its head, LA-SQUARE's and LA-SAW's way, the other codes' (the VC-2's own Pulse Width laws by
     waveform) and its small callees `0x893c4a20` and `0x893c3570`, against rom.4's in xt_vc2osc with the sub
     oscillator's note-on hooked (HARDWARE_FIRMWARE.md, "The VC-2's analog note-on").
   - done (v244): the sub oscillator's note-on (`0x893c3b90`, about 350 instructions), rom.2's subNoteOn law for law,
     each code's way a row, against rom.4's in xt_vc2osc with its shape hooked (HARDWARE_FIRMWARE.md, "The VC-2's sub
     oscillator's note-on"). Split by what rom.4 holds: the shape (`0x893c4020`, about 1,200 instructions) is next.
   - done (v246): the sub oscillator's shape (`0x893c4020`, about 1,200 instructions to 0x893c4a1a, two switches),
     rom.2's subShape law for law with the VC-2's Pulse Width lines, each code's way a row, the six waves one routine
     over a row of each case's own pool words, LA's way beside the note-on's; against rom.4's in xt_vc2osc with
     nothing of the analog's hooked (HARDWARE_FIRMWARE.md, "The VC-2's sub oscillator's shape").
   - done (v248): the analog's install (`0x893c2ad0`, with the sub oscillator's own `0x893c3e60`), update
     (`0x893c2ce0`, LA's targets `0x893c31d0`) and settings (`0x893c3460`), rom.2's laws on rom.4's own words: the
     note-on's plain codes' and LA's laws one code over each routine's words; `0x893c30c0`, LA's note-on as a
     function, is dead (nothing calls it); against rom.4's in xt_vc2osc with the analog's 19 render routines hooked
     (HARDWARE_FIRMWARE.md, "The VC-2's analog install, update and settings").
   - done (v251): the OSC block's converter, the analog's converter's first part (`0x8939cf70` to its waveform switch
     `0x8939d3b4`; `0x8939e300` is PCM's) and the VOCODER's first half, one code but for the calibration row:
     `vc2OscBlockConvert`, the two pitch10s and the block's +0x04…+0x68 with its helpers, against rom.4's whole
     converters in xt_vc2carrier's third part (HARDWARE_FIRMWARE.md, "The VC-2's OSC block converter").
   - done (v256): the analog's converter's second part (`0x8939d3a2…0x8939da96`), `vc2OscAnalogConvert`: the block's
     +0xb0…+0xf3, rom.2's analog block's words 0x30 up, with the Pulse Width `0x8939c5e0`, the Detune's `0x8939c6e0` and
     the Slope `0x8939cae0`, against rom.4's whole converter in xt_vc2carrier's third part (HARDWARE_FIRMWARE.md, "The
     VC-2's analog converter's second part"). The analog OSC is whole but for its renders, which are rom.2's.
   - done (v261): Vari off's two renders, rom.4's own `0x893c63c0` and `0x893c66e0`, `vc2OscVariOffMono` and
     `vc2OscVariOffStereo`, against rom.4's in xt_vc2osc's Vari off part (HARDWARE_FIRMWARE.md, "The VC-2's Vari off").
   - done (v257): the shared math's logf (`0x895065f4`, its core rom.2's `0x89531b78`) and powf (`0x89506e54`),
     `Vc2LibMath`, against rom.4's own in xt_vc2libm (HARDWARE_FIRMWARE.md, "The VC-2's logf and powf"). The wiring
     takes it next: `Vc2ToneShared` still gives rom.4's routines through `RomShared`.
   - done (v266): kinds 5 and 6 on the OSC voice: `Vc2OscVoice::carrier` at +0xa90 (the init `0x893ebed0`, the settings
     `0x893ec450`, the render `0x893ebfa0`) and `Vc2OscVoice::kind6` at +0xafc (the start `0x893ec700`, the render
     `0x893ec730`), their hooks out of xt_vc2osc, rom.4's own run against the port's words (HARDWARE_FIRMWARE.md, "The
     VC-2's kinds 5 and 6 on the OSC voice"). What they still need from outside the voice, for the part's wiring:
     - the input stage's peak (`Vc2Unvoiced::peak`, `0x899d9a5c`), for the carrier's Dynamics. Kind 5's update
       `0x893afff0` still goes by its address through `Vc2OscParts`; `vc2Kind5Update` is its port, not called yet.
     - the mic ring, one for the whole part (`Vc2OscParts::micRing`, `0x8a0f31cc`): the block mixer writes each block
       of the mic into it (`Vc2MicRing::write`, `0x893ec820`), with Unvoice Level (`0x899d9968`) set by its converter.
     - the tracker's period for each of those blocks (`Vc2Tracker`, `0x899d9bd0`), which `Vc2MicRing::write` is given.
     - the input stage's unvoiced decision for the block (`Vc2OscParts::unvoiced`, `0x899d99dc`), and the OS's
       millisecond word (`Vc2OscParts::osClock`, `0x893018d0`), where a carrier's phase starts.
     - kind 6's update `0x893b0150`, which ends in the setter `0x893ec790` (`Vc2Kind6::set`), also by its address still.
   - done (v267): the vibrato, `Vc2OscVibrato`, on the voice's first 0x54 bytes: the build `0x893b29b0`, the note-on
     `0x893b29e0`, the settings `0x893b2b70` and the update `0x893b2c10`, rom.4's own (rom.2 has no such routine), against
     rom.4's in xt_vc2osc's vibrato part and in the voice, its hooks gone (HARDWARE_FIRMWARE.md, "The VC-2's OSC vibrato").
   - done (v268): the tone's envelopes and LFOs, `Vc2ToneEnvelope`, `Vc2ToneLfo` and `Vc2ToneModulators`: rom.4's objects
     word for word over XtEnvelope and XtLfo, whose code rom.4 shares opcode for opcode; constructors, note-ons, ticks, the
     clock and the words the tone, the OSC voice and the TVA read, against rom.4's own routines in xt_vc2tone
     (HARDWARE_FIRMWARE.md, "The VC-2's envelopes and LFOs"). Split by what rom.4 holds (the VC-2 session's decision,
     2026-11-18): the free LFOs' manager is v271.
   - done (v271): the free LFOs' manager at `0x8a0c6d84` whole, `Vc2FreeLfos`: its constructor `0x893ad4b0`, the 384 free
     objects, `unfollow` `0x893ad720`, the take `0x893ad640`, the lists `0x893ad780` and `0x893ad800`, the attach
     `0x893acfc0`, the free start `0x893acf10`, the follow `0x893ace60`, the free objects' tick (`0x893aca60`, the manager's
     pass `0x893ad5f0`) and clock (`0x893acba0`, the manager's pass `0x893ad870`), their hooks out of xt_vc2tone
     (HARDWARE_FIRMWARE.md, "The VC-2's free LFOs"). For the wiring: nothing in rom.4 calls the tick pass, so it must not
     run at each control tick as rom.2's does; the clock pass runs from `0x893b173e`.
   - done (v269): the tone's OSC mix, `Vc2ToneMix`: rom.4's object at the tone's +0x1ab8, its constructor, note-on, install,
     empty update and eight renders, and the delay lines `Vc2ToneMixRings`, against rom.4's own routines in xt_vc2tone
     (HARDWARE_FIRMWARE.md, "The VC-2's OSC mix").
   - done (v273, step 4.3's second version): the mix's settings update `0x893add60(mix, block)`, which the tone calls at
     `0x893b32e6`: new targets during a note, by type.
   - done (v233, beside v232's OSC note-on): the TVA, `Vc2Tva`, whole in one version (about 400 instructions; no split):
     rom.4's 0xc8-byte object at the tone's +0x19f0 word for word, the constructor, the note-on, the new block, the
     update, the end tests, the two truncations, silent, the render on one stereo pair (its shortened step kept, its
     truncated fade and the OS's call) and the envelope's level, against rom.4's in xt_vc2tva with the OS's call hooked
     (HARDWARE_FIRMWARE.md, "The VC-2's TVA"). XtSynth's TVA gains don't serve it: rom.4 multiplies by the block's +0x48
     and +0x4c when its +0x40 is above 0, on the object's own words.
   COSM's converters and the Growl split by job (2026-11-16): the Growl; the converters' frame with a first type; the
   other 14 converters; the COSM object in the tone. XtCosm stays as it is: rom.4 shares its 15 types' voice side and
   DSP whole (step 1b), so the object's frame calls them.
   - done (v234): the Growl, `Vc2CosmGrowl`: its converter (`0x89398780`, its Matrix sums through an interface), the
     note-on's, update's and new block's cases (`0x893b9d30`, `0x893b9d40`, `0x893b9dc0`) with the COSM clamp
     (`0x893bf240`), and the DSP (`0x893b9b40`, mono and stereo alike), on the COSM object's 0x164 bytes word for word,
     against rom.4's own routines in xt_vc2cosm with the Matrix readers hooked (HARDWARE_FIRMWARE.md, "The VC-2's COSM
     type 8, the Growl").
   - done (v235): the converters' frame: the dispatcher (`0x89399920`: the velocity factors by `0x893a7fb0`, through
     `Vc2ToneShared`; the switch on the type, the Speaker Simulator's inline), the input's layout (by its builder
     `0x89399b10`), the Matrix's COSM readers (`0x893a2960`, `0x893a2aa0`) as one routine over a group's row with the
     getter `0x893a2350` on M's words, the refresh `0x893a2230` behind an interface; the TVF's converter
     (`0x89398ae0`) the first type; against rom.4's in xt_vc2cosm's second part (HARDWARE_FIRMWARE.md, "The VC-2's
     COSM converters' frame").
   - ~~next: the other 14 converters~~ (rom.4's `0x89397980…0x89399620`, rom.2's `0x894dc8b0…0x894de110` on another
     input: VC2_VOCAL_DESIGNER.md, "COSM in rom.4"), a few a version, each against rom.4's through the dispatcher on the
     same input, each taking its hook out of xt_vc2cosm's second part. They were 13, not 14: the switch's 15 cases call
     15 converters, the Speaker Simulator's (type 4) is inline in the dispatcher and came with v235, and the TVF's and
     the Growl's were done. Split by what rom.4 holds (2026-11-16): six converters on the shared holds and the depth law
     alone, and seven that reach rom.4's libm (`powf` `0x89506e54`, `logf` `0x895065f4`) through seven helpers before
     the first converter (`0x89397450…0x89397900`).
   - done (v237): the six without libm: the Amp Simulator (`0x89397eb0`, type 3), the Dual Filter (`0x893988a0`, 9), the
     Dynamic TVF (`0x89398d40`, 11, the TVF's routine on its ten parameters with two Own Env depths), the Compressor
     (`0x89398fd0`, 12), the Limiter (`0x893990d0`, 13) and the TB Filter (`0x89399620`, 16), with the keyfollow
     `0x893976f0` (the TVF's Cutoff law), the level's gain `0x89397900` and the gain table `0x89541f34` (129 words), and
     the port's converters by type (`vc2CosmPortedConvert`), against rom.4's in xt_vc2cosm's second part, which keeps
     its own list of the types ported (HARDWARE_FIRMWARE.md, "The VC-2's COSM converters, the first batch").
   - ~~next: the seven on libm~~, with `logf` and `powf` behind `Vc2ToneShared` (rom.4's own on both sides of the
     check, as the velocity curve and exp2 are; `logf` as v236's `Vc2ToneShared::log`, `powf` beside it the same way):
     the OD/Dist (`0x89397980`, type 1) and the Wave Shaper (`0x89397cf0`, 2) on the level's keyfollow
     `0x89397480`/`0x89397510` (two copies of one routine: the held level × 0.9/127 + 0.1, times powf(2, its two
     arguments' product / 1200), held to 0.1…1) and the gain table; the Resonator (`0x89398080`, 5) on its Size's
     `0x89397760` (logf(2)); the Frequency Shifter (`0x893991d0`, 14) on `0x89397800` and the Lo-Fi (`0x893993f0`, 15)
     on `0x89397870`; then the two Side Band Filters (`0x89398260`, 6, and `0x893984e0`, 7: the LPF's `0x893975a0`,
     powf(x, 2) and logf(2), and the Width's `0x89397650`), the largest, last. One version if they come as alike as
     v237's did; else the Side Band Filters a version of their own. rom.4 also holds a Speaker Simulator converter of
     its own (`0x89398070`, rom.2's `0x894dcef0` recompiled) and one that writes code 0 (`0x89397970`): no branch or
     word in rom.4 reaches either, so neither is ported. Split (2026-11-16, v239): the five on one helper each, then
     the Side Band Filters a version of their own (an HPF with no sum, the LPF's square and logf, Sub Detune, the
     Octave byte, and Detune and Width each with three depths).
   - done (v239): the five, with powf as `Vc2ToneShared::pow`: OD/Dist and the Wave Shaper as one routine over a row
     (Vc2CosmDrive.cpp), the Resonator, the Frequency Shifter and Lo-Fi as one routine past each one's setting
     (Vc2CosmBalanced.cpp), the level keyfollow, the keyed scale `0x89397800` (the Width's `0x89397650` the same law)
     and the Dry/Effect Balance shared (Vc2CosmConvert.cpp), against rom.4's in xt_vc2cosm's second part
     (HARDWARE_FIRMWARE.md, "The VC-2's COSM converters on rom.4's libm").
   - done (v241): the two Side Band Filters (`0x89398260`, type 6, code 7; `0x893984e0`, 7, code 8), their LPF's
     `0x893975a0` (powf(x, 2), logf(2)) and the Width on the keyed scale (`0x89397650`, `vc2CosmScaled`'s law on its
     own words), one routine for their Detune and Width words over a row (Vc2CosmSideBand.cpp); every converter is
     ported and xt_vc2cosm's second part hooks none (HARDWARE_FIRMWARE.md, "The VC-2's COSM Side Band Filters", with
     rom.4's whole type map). COSM's converters are done.
   - the Matrix core, a part of its own (every group's, not COSM's alone): the refresh `0x893a2230` (the sources'
     routines, their terms by the depths and limits, the dirty bits), M's other words and the other groups' readers
     (the OSC's `0x893a23b0`…, the next group's from `0x893a2bf0`; rows of `vc2MatrixRead` where their shape is the
     COSM readers', which is read only at their starts), before the part (step 4.3) needs a whole M. Split by what rom.4
     holds (2026-11-17): rom.4's Matrix module runs from the set-up's routines (`0x893a1ce0…`, each source's routine
     by its choice; `0x893a31a0…`, a destination's uses bit and number) through the sources' routines (`0x893a1de0…
     0x893a21f0`, reading M's words past +0x166: the channel's pointer at +0x168 and a controller's assignment and
     value through the curve `0x895401f6`) to the core. The core first, then the sources and the set-up.
   - done (v243): the core, `Vc2MatrixCore` (Vc2Matrix.cpp): the refresh, its sources behind `Vc2MatrixSources`, and
     every group's reader as a row of `vc2MatrixRead` (all ten have the COSM readers' shape; `0x893a2bf0` has the
     getter's sum inline for 76…78; no reader takes group 7), against rom.4's in xt_vc2cosm's third part, only the
     sources' routines hooked (HARDWARE_FIRMWARE.md, "The VC-2's Matrix core").
   - done (v245): the sixteen sources' routines (`0x893a1de0…0x893a21f0`) and the choice `0x893a1c70` (a source
     parameter's routine), `Vc2MatrixSourceRoutines` and `vc2MatrixSourceRoutine` (Vc2MatrixSources.cpp), on M's +0x168
     (the channel's state) and +0x16c (the part), the seven assignments at `0x89997398` + 0x848 and the curve
     `0x895401f6`, read with the limits as one run (a limit past 78 is the curve's word, as rom.4 reads it); xt_vc2cosm's
     third part with nothing hooked (HARDWARE_FIRMWARE.md, "The VC-2's Matrix sources").
   - done (v247): M's set-up, `Vc2MatrixSetup` (Vc2MatrixSetup.cpp), split by what rom.4 holds. The "six builders"
     are two routines' pools: part n's record in G (`0x893a46f0`, the Matrix's settings at `0x89997398`, sixteen
     records of M's front), the system's assignments (`0x893a50b0`) and both (`0x893a51c0`); the destination switches
     (`0x893a30d0`, `0x893a36a0`) tabulated at load by running rom.4's own; M's init from its part's record
     (`0x893a2ee0`), with group 2's levels into the part mixer's floats (`0x893a2e50`); a note's head (`0x893a2eb0`).
     xt_vc2cosm's fourth part, hooking only the parameter reads and `0x893b1ee0` (HARDWARE_FIRMWARE.md, "The VC-2's
     Matrix set-up"). Left to the part (step 4.3): which channel state G names (`0x893a5050`, by Receive Switch and
     Channel), the static M at `0x89997108` and its initializers (`0x893a2fe0`, `0x893a3020`), G's allocation
     (`0x893a5030`). The Matrix is whole but for those.
   - then the COSM object in the tone: the set-up `0x893aa590`, the note-on `0x893aa0e0` (the clears, the allocator
     `0x893a8c50`, the start values `0x893a85f0`, the levels it holds, the new block, the code's case), the binding
     `0x893aa7b0`, the install `0x893aafe0`, the update `0x893aa7d0`, the new block `0x893aa5d0` and the DSP chosen by
     block code, over XtCosm's types and the Growl: the parts `Vc2ToneParts` names for the COSMs. Split by what rom.4
     holds (2026-11-17), as rom.2's object is (XtCosm: each type's note-on case, new block, update and DSP):
     - done (v249): the frame, `Vc2CosmObject` (Vc2CosmObject.cpp): the object's words (0x164 bytes: the set-up's,
       the block, D, the pool and slot, the DSP routine and the install's float, the bindings, the vtable), the
       constructor `0x893aa030`, the set-up, the binding and the install (rom.2's chooser `0x894eef70`: the mono or
       stereo DSP routine by the code byte, a float per type and variant, its return COSM 2's flag); xt_vc2cosm's
       fifth part (HARDWARE_FIRMWARE.md, "The VC-2's COSM object's frame").
     - done (v250): the note-on's and new block's frames, `Vc2CosmNoteOn` (the clears, the allocator `0x893a8c50`, the
       start values' switch, the held levels and rates, the new block, each code's case), the buffer pools
       `Vc2CosmPools` (their initializer `0x893ab550`, a slot's take and release, the destructor `0x893aa060`), and the
       types needing no adapter: Through, the Speaker Simulator, the Growl (HARDWARE_FIRMWARE.md, "The VC-2's COSM
       note-on frame"). rom.4's per-type routines are rom.2's opcode for opcode (the TB Filter's five words relocations
       only), so the rest are adapters over XtCosm (the VC-2 session's choice (A), 2026-11-17): one row a type, rom.4's
       words into XtCosm's class and back, a word XtCosm doesn't hold carried as rom.4's.
     - done (v252): the first adapters, `Vc2CosmAdapters` (Vc2CosmAdapters.cpp): the Resonator's and the TVF's start
       values and new blocks, XtCosmRes's laws and `xtTvfQ`, `xtTvfCutoff` on rom.4's words, each routine's copy of
       the block rom.4's own (HARDWARE_FIRMWARE.md, "The VC-2's COSM adapters: the Resonator and the TVF").
     - done (v253): Lo-Fi's adapter, with the Resonator's one template (`Vc2CosmPairType`), after XtCosmLofi moved
       into XtCosmLofi.h with four const getters for its new block's words (the VC-2 session's choice, 2026-11-17)
       (HARDWARE_FIRMWARE.md, "The VC-2's COSM adapters: Lo-Fi").
     - done (v254): OD/Dist's (1, 2), the Wave Shaper's (3) and the Amp Simulator's (4) adapters, the first with
       note-on cases (rom.4's copies of the object's words into D's, `Vc2CosmCaseCopy`); the types of one or two
       values one template (`Vc2CosmValueType`), each through its XtCosm class's own note-on (HARDWARE_FIRMWARE.md,
       "The VC-2's COSM adapters: OD/Dist, the Wave Shaper and the Amp Simulator").
     - done (v255): the Dual Filter's (12), the Dynamic TVF's (14) and the TB Filter's (20) adapters, the TVF (13)
       moved onto XtCosmTvf with the Dynamic TVF; the TB Filter's in Vc2CosmTbAdapter.cpp, its new block's targets and
       note-on case's coefficients XtCosmTb's (HARDWARE_FIRMWARE.md, "The VC-2's COSM adapters: the Dual Filter, the
       Dynamic TVF and the TB Filter").
     - done (v258): the Compressor's (15), the Limiter's (16) and the Frequency Shifter's (18) adapters,
       `Vc2CosmBufferedAdapters`, on XtCosm objects made once with the note-on frame (HARDWARE_FIRMWARE.md, "The VC-2's
       COSM adapters: the Compressor, the Limiter and the Frequency Shifter").
     - done (v260): the Side Band Filters' adapters (7, 8), after XtCosmSbf moved into XtCosmSbf.h with const getters
       for the words their new blocks and note-on cases form (the VC-2 session's conditions, 2026-11-17). Every code a
       converter gives now has its start values, new block and note-on case (HARDWARE_FIRMWARE.md, "The VC-2's COSM
       adapters: the Side Band Filters").
     - done (v262): the update `0x893aa7d0`, `Vc2CosmNoteOn::update` and each type's case, a row's fifth part
       (HARDWARE_FIRMWARE.md, "The VC-2's COSM update").
     - done (v263): the DSP by code, mono and stereo, for Through and the twelve codes of OD/Dist, the Wave Shaper, the
       Amp and Speaker Simulators, the Resonator, the Dual Filter, the TVF, the Compressor, the Limiter, the Frequency
       Shifter and Lo-Fi: `Vc2CosmDsp`, one `XtCosmSection` a COSM (HARDWARE_FIRMWARE.md, "The VC-2's COSM DSP: the first
       twelve codes").
     - done (v264): the DSP of the Side Band Filters, the Growl, the Dynamic TVF and the TB Filter, so every code a
       converter gives has its sound; the rings' ends kept in the pools for the first Side Band Filter's early read
       (HARDWARE_FIRMWARE.md, "The VC-2's COSM DSP: the last four types, and the pools' memory").
3. The part: the notes (the mailbox, note start and off, the allocator, legato, truncation), the conversion by rom.4's
   IDs, the three tasks with the mixer, the vocoder, the tracker and Auto Note: `Vc2Synth`, against rom.4's whole tone
   (`xt_vc2tone`, as `xt_tone` for rom.2).
   The map (2026-11-18, v270; VC2_VOCAL_DESIGNER.md, "The part's voices and the COSM pools"), read and, for the notes,
   run on rom.4's own handlers (`vc2run … voices`):
   - **The three tasks.** All three wait on the OS's events; a port runs them in one order per block.

     | Task | Entry | Wakes on | Does |
     |---|---|---|---|
     | Notes | `0x8938fc00` (loop from `0x8938fc66`), object `0x8994fec8` | 0x002 MIDI queue `0x89981918`; 0x004 a tone ended (`0x89390ee0`); 0x008 reset all (`0x89390340` × 3, while `0x89981904` is set); 0x010 the schedule, `0x893b1710` once per count at `0x89981b40` + 0x20; 0x020, 0x040 Auto Note's on and off; 0x080 all notes off; 0x100 `0x893930a0`; 0x200 a held C4 | every note, every voice, every tone made or destroyed |
     | Update | `0x893952b0`, object `0x8994ff64` | 0x1: the pitch source `0x89394a60`, then the voices' update `0x893b16a0`; 0x2 (a timer, `0x893952a0`): the queue at `0x89989cc4`, each node's +0x0c | envelopes, LFOs, OSC updates; a voice's reconversion |
     | Block | `0x89395ac0` (`0x89395ace` is inside its prologue) | 4 the input frames (`0x8993eadc`); 2 the mixer `0x893b1820`, then the output's DMA | the part mixer and every tone's render |

   - **The objects** (24 of each, by the voice's number at its +0x40):

     | Object | Where | Bytes | What it is |
     |---|---|---|---|
     | Voice V | table `0x895354b0`, from `0x89981b6c` | 0x60 | vtable +0x10 (`0x898cd3dc`), list links +0x14/+0x18, a pointer to its age +0x1c, state +0x2c, channel state +0x34, part +0x3c, number +0x40, its routine on leaving a list +0x44 (`0x89391740`, the virtual destroy, or `0x89391750`, none), flags +0x48, note W +0x4c, conversion set C +0x5c |
     | Note W | `0x8998a51c` + 0x744 × n (`0x893964f0`) | 0x744 | the tone's input (`Vc2ToneInput`, 0x700 bytes) at +0, tone +0x738, partner +0x73c, id bytes +0x741/+0x742 |
     | Conversion set C | `0x89997fe8` + 0x6c × n (`0x893a76a0`) | 0x6c | one converter object per block of W, M at +0x68 |
     | M | `0x89e4741c` + 0x184 × n (`0x893a5d80`) | 0x184 | the note's Matrix (`Vc2MatrixWords`) |
     | Tone | pool `0x8999f524` (`Vc2TonePool`) | 0x261c | taken by W's constructor, given back by its destructor |

   - **The four lists** at `0x899826e0`: sounding `0x89982594`, free `0x899825e4`, prepared `0x89982634`, reserved
     `0x89982684`. A voice moves by `0x893919b0` (to prepared), `0x893918e0` (to free), `0x89391930` (to reserved); each
     runs the voice's +0x44 first, which destroys its tone, so a voice never carries a tone into a list.
   - **Note start** (`0x89392da0`, source 1…4, 4 being Auto Note's): the key table (`0x89391e90`), then each part node
     on the channel's list (`0x899890d0` + (source − 1) × 64 + channel × 4), by its vtable's +0x0c: `0x89396a80`. That takes
     a voice (`0x893969e0` → `0x893905a0`: the free list's first, else the sounding voice of kind 0 with the lowest age,
     whose tone is destroyed there) and starts the prepared list (`0x89390860`): for each, a legato partner of the same
     channel state, part and kind; its +0x3c (`0x89397130`: W, the tone, C); the tone's note-on (`0x893965e0`); then while
     the sum of the tones' costs (`0x893b4020`, plus `0x899826dc`) reaches the budget at `0x899826d8`, stop the oldest
     (`0x893918e0`); then start the tone (`0x89396630` → `0x893b1cd0`), and join the sounding list. The budget is CPU Power
     Max for TG (0x530), less 400 for every Patch Category but 6 (`0x89392c60`), ÷ 10⁶.
   - **Note off** (`0x89392e70` → `0x89396c90`, then `0x89390c00`): `0x89391760` clears the key's flags, the pedals'
     held bits (`0x89391820`, `0x89391880` when they lift), and at none left the voice's +0x30 (`0x89397280`): C's
     release (`0x893a7c50`), the tone's key-off (`0x893968b0` → `0x893b3ad0`).
   - **Legato and truncation.** Mono with Legato on, a key already held, retunes the sounding voice in place (`0x89390b20`,
     the voice's +0x18, `0x893971f0`) and starts no tone. A new voice that finds a partner passes it to the tone's note-on,
     which takes its player (`0x893aee20`). `0x89396590` ends a tone: at once when it's silent (`0x893b2fc0`), else
     by `0x893b3bc0`, the TVA truncated and **the notes' task asleep on event 0x1000 until the block task has rendered the
     fade**. The port must keep that order: a voice is free only when its tone is gone.
   - **The conversion pass.** `0x893a7b20` (a note) and `0x893a7cc0` (an update) run C's children into W's blocks: the
     common +0x04 (`0x893a19c0`, W+0x0c), the OSCs +0x08/+0x0c (`0x893a0040`, W+0x434, +0x53c), the mix +0x10 (`0x8939be40`,
     W+0x644), 13 envelopes +0x14 (`0x8939a790`, W+0x124…), 5 LFOs +0x48 (`0x8939b750`, W+0x24…), the COSMs +0x5c/+0x60
     (`0x89399b90`, W+0x658, +0x6a0), the TVA +0x64 (`0x893a68c0`, W+0x6e8); the note's M at +0x68 (made by `0x893a5db0`).
     Their parameters are cached by ID, per part and zone, by the eleven readers of `0x8939b170` (messages through
     `0x8939b050`).
   - **What step 4.2 left.** G's channel word (`0x893a5050`, G+0x840): Part 1 Receive Switch (0x47) off, the word at
     `0x8999738c`, which no code seen writes (0 after the part's init, so G's word is 0: version 9 must follow what the
     Matrix's sources do with that); on, the per-channel state of Part 1 Receive Channel (0x48), from the table
     `0x8950d6e0` by the source byte at `0x8950d6d9`, + 2. **G's allocation** `0x893a5030` has no caller: G is static at
     `0x89997398`. **The static M** at `0x89997108` is the part's own, for the part mixer's levels: `0x893a3020` (the
     init table's) builds its update node at +0x170 and its two vtables (`0x898cd448`, `0x898cd460`, each's +0x0c being
     `0x893a2ee0`), `0x893a2fe0` (the part's init) sets its velocity 0, keys 60, part 0 and channel word and queues the
     node at `0x89989cc4`. **Event 0x200**: `0x893602e0` (a patch of Category 10 or 11, also `0x893607f0` at a patch's
     load, and `0x89394340` when the Part channel moves on those) raises it on the notes' task, which plays C4 at
     velocity 127 from source 3 on node 3's channel, unless `0x89981904` is set; the flag `0x899891d0` says so, and
     `0x89392c60`'s all notes off (event 0x80) ends it. It is a held note for the vocoder's carrier, with no key down.
   - **The pool can't run out** (settled; VC2_VOCAL_DESIGNER.md has the proof and the run). `Vc2CosmPools` needs no
     answer for an empty pool in the wiring.
   - **What each interface still needs** (ported routines are named by their version where it matters):

     | Interface | Has a port | No port yet |
     |---|---|---|
     | `Vc2ToneParts` | OSC voices, TVA, COSM object, note-on and adapters; update and DSP (v262…v264); the envelopes and LFOs (v268), the mix (v269), the free LFOs' manager (v271), the part's lists of tones and the mix's settings update (v273); the envelopes' and LFOs' settings and release (v315) | the vibrato's key-off `0x893b2b60` (its +0x34 to 0) |
     | `Vc2OscParts` | carrier and kind 6 on the voice (v266), the vibrato (v267), players' installs, seeks | kind 5's update `0x893afff0` and kind 6's `0x893b0150` still go by address (`vc2Kind5Update` and `Vc2Kind6::set` are their ports, for the part's wiring) |
     | `Vc2TvaParts` | the TVA itself; in the tone's parts' host from v317, its envelope and LFOs the tone's, the fade's step the part's word at `0x8993eb08` (`Vc2ToneCommon::fadeStep`, which C's TVA child writes, v285) | the event is the task's (the host's deriving class's `osEvent`) |
     | `Vc2MixerParts` | all nine | none |
     | `Vc2AutoNoteHost` | none | `0x89391dc0` (Auto Note's note + 12, posted when no key holds it), `0x89391e20` |
     | `Vc2MatrixParams` | `Vc2Params`' descriptors | the readers `0x89320760`, `0x89320790`, `0x893207b0`, `0x89320a50`, `0x89320f60` on the getter `0x89324350`: in the plugin, the host parameters |
     | `Vc2ToneShared` | `Vc2LibMath` for log and pow | a non-interpreter velocity `0x893a7fb0` and exp2 `0x893a8340`: only `RomShared`, the checks', implements it |

   Split by those, bottom-up, each against rom.4's with what it calls hooked:
   1. The voices and their lists: V's words, the init (`0x89391270`, `0x89391ba0`), take and steal, the three moves, the
      reserve (`0x893911b0`), the ended scan (`0x89390ee0`), stop all (`0x89390f40`), the cost sum and budget.
      - done (v272): `Vc2PartVoices`, with the heads' static initializer `0x89391450`, the budget's setters and getter, a
        voice's cost and the prepared list's test; `xt_vc2voices` (HARDWARE_FIRMWARE.md, "The VC-2 part's voices and
        their lists"). Stop all, the two costs and the steal's two parts are out-of-line copies that nothing calls: the
        assigner's start `0x89390860` holds them inline, with the splice of the prepared list onto the sounding list and
        the budget's loop, which item 5 ports on these.
   2. done (v273): the tone's remaining methods: the block setters `0x893b3110…0x893b3350`, key-off `0x893b3ad0`, the
      two truncations `0x893b3bc0` and `0x893b3c20`, the cost `0x893b4020`, the position getters (`0x893b3f80` and
      `0x893b3fd0`: `0x893b3fc0` and `0x893b4010` are their first cases), the start `0x893b1cd0` with the part's two lists
      of tones (`Vc2ToneLists`: `0x893ada10`, `0x893ada30`, `0x893ad9a0` and the join `0x893ada90`), and the mix's
      settings update `0x893add60`, against rom.4's own in xt_vc2tone (HARDWARE_FIRMWARE.md, "The VC-2's tone: its
      remaining methods"). What they call stays hooked: the OSC voice's settings, legato and key-off, the COSM's and the
      TVA's new blocks and truncations, and the envelopes' and LFOs' settings and release (`0x893ac110`, `0x893acff0`,
      `0x893ac470`, `0x893ac8c0`), which the real modulators (v268) don't have yet: open, for C's frame (version 7).
   3. The note W: constructor `0x89396520`, note-on `0x893965e0`, end `0x89396590`, the settings pass `0x89396650`, the
      update pass `0x893967a0`, the release `0x893968b0`, `0x89396910`, `0x89396950`.
      - done (v275): `Vc2Notes`, with the placement `0x893964f0`, the start `0x89396630`, the two routines a tone is
        given (`0x893964d0`, `0x893964e0`) and the hook word's initializer `0x89396960`; `xt_vc2notes`
        (HARDWARE_FIRMWARE.md, "The VC-2 part's notes"). The two passes are one code; `0x89396640` is a copy of the start
        nothing calls. The notes' task sets the hook word to `0x893901a0` (version 10's).
   4. The key table: `0x89392190`, `0x893920c0`, `0x89391e90`, `0x89391ef0`, `0x89392670` and the held-key queries, held
      to `XtKeys.h` where rom.2's layout matches (step 1b found key assign equal).
      - done (v276): `Vc2Keys`, the 66 key tables and the 129 portamento keys with every routine of theirs (the reset,
        the pedals, the clear, the velocity factor, the five queries, the portamento keys' constructor, reset, note-on,
        note-off and two initializers), the portamento keys' laws `XtKeyState`'s; their tables `Vc2KeysTables`;
        `xt_vc2keys` (HARDWARE_FIRMWARE.md, "The VC-2 part's keys"). Left to the assigner: the voice's held bits
        (`0x89391760`, `0x89391820`, `0x89391880`) and who aims a portamento key at its own channel's table.
   5. The assigner: `0x89396a80`, `0x89396c90`, `0x893969e0`, `0x89390860`, mono's `0x89390b20` and `0x89390b90`, the
      release path, the pedals, on faked voices' virtuals.
      - first half done (v277): `Vc2Assigner`, the start `0x89390860` (the partner, the voice's +0x3c, the note-on,
        the cost against the budget, the start, the prepared list spliced onto the sounding list's front), the class
        builder `0x893969e0`, the partner `0x89390720`, the matching test `0x893913a0`, mono's retunes, the truncation
        of a part's voices `0x893907d0` and a voice's `0x89391a50` with `0x89391a90` and `0x89391aa0`; xt_vc2voices'
        second part (HARDWARE_FIRMWARE.md, "The VC-2 part's assigner, its first part").
      - second half done (v278): the release path (`0x89391760`, `0x89391820`, `0x89391880`; over a key table's
        voices `0x89390c00`, `0x89390cd0`, `0x89390d10`, `0x89390d50`, `0x89390da0`, `0x89390e20`) and the part
        node's note start `0x89396a80` and note-off `0x89396c90` on v276's keys; xt_vc2voices' third part
        (HARDWARE_FIRMWARE.md, "The VC-2 part's assigner, its second part"). Item 5 done.
   6. The voice's virtuals (`0x89397130`, `0x893970b0`, `0x893971f0`, `0x89397280`, `0x893972d0`, `0x893973a0`) and the
      update queue at `0x89989cc4` (`0x89394f30…0x893951b0`).
      - done (v279): `Vc2VoiceClass`, the six virtuals and the words they set from a portamento key (`0x89391ae0`,
        `0x89391b40`), C's routines and the note's through `Vc2VoiceClassParts`; `Vc2UpdateQueue`, the one queue word
        for word with its constructor, initializer, link, unlink, enqueue, move on, pass on, take off, waits and move
        (a node's constructor `0x89394f30` is v272's `vc2ConstructNode`'s); xt_vc2voices' fourth part
        (HARDWARE_FIRMWARE.md, "The VC-2 part's voices' virtuals and update queue").
   7. C's frame: `0x893a76d0`, `0x893a7a20`, `0x893a7b20`, `0x893a7c50`, `0x893a7cc0`, `0x893a7df0`, `0x893a7f10`, and
      the note's M (`0x893a5db0`, `0x893a5fe0`, `0x893a6010`, `0x893a61f0`), its children behind an interface.
      - done (v280): `Vc2Sets`, C's 24 sets word for word and its seven routines (and the placement), each a pass over
        the children by a table of kinds behind `Vc2SetParts`; `Vc2NoteMatrices`, the notes' 24 M word for word, the
        constructor and update (the end and copy are bare returns); xt_vc2sets (HARDWARE_FIRMWARE.md, "The VC-2 part's
        conversion sets and the notes' Matrix"). The children of item 8 are the common, mix, envelopes, LFOs and TVA;
        the OSC blocks' and COSM inputs' are behind the same interface, their converters v251's, v256's and v235's.
   8. C's children with no port, a version each: the common's (`0x893a1940…`), the envelopes' (`0x8939a620…`), the LFOs'
      (`0x8939b690…`), the mix's (`0x8939bd90…`), the TVA's (`0x893a67d0…`).
      - the common's done (v281): `Vc2SetCommons`, its 24 words, constructor, conversion, update, copy and its twin
        `0x893a1870` (the block at W+0xc); xt_vc2sets' second part (HARDWARE_FIRMWARE.md, "The VC-2 part's sets'
        common").
      - the envelopes' done (v282): `Vc2SetEnvelopes`, the 312 words, constructor (the tempo), conversion, update,
        copy, release and the block writer `0x8939a3d0` with the Matrix's sums, the time curve, its quarter and the
        key's factor; their tables `Vc2SetEnvelopeTables`; xt_vc2sets' third part (HARDWARE_FIRMWARE.md, "The VC-2
        part's sets' envelopes").
      - the LFOs' done (v283): `Vc2SetLfos`, the 120 words, constructor, conversion, update, copy and the block writer
        `0x8939b590` (the waveform, the rate by note value or by the rate table with the Matrix's sum `0x8939b450`, the
        two depths squared, two switches, the key sync); what they share with the envelopes in `Vc2SetChildHead`;
        xt_vc2sets' fourth part (HARDWARE_FIRMWARE.md, "The VC-2 part's sets' LFOs").
      - the mix's done (v284): `Vc2SetMixes`, the 24 words, constructor, conversion, update, copy and the block writer
        `0x8939bcc0` (off, the level table by type, the crossfade's two pairs, 1.0, or 0.7); on `Vc2SetChildPool` with
        the voice's bytes at +1; xt_vc2sets' fifth part (HARDWARE_FIRMWARE.md, "The VC-2 part's sets' mix").
      - the TVA's done (v285): `Vc2SetTvas`, the 24 words, constructor and conversion (the velocity curve's level),
        update, copy and the block writer `0x893a6260` (the level with CC 7 and 11, two depths, the pan with CC 10,
        the outputs by a route, two sends with CC 93, the part's gain, the global step); xt_vc2sets' sixth part
        (HARDWARE_FIRMWARE.md, "The VC-2 part's sets' TVA"). Item 8 is done.
      - the COSM inputs' done (v390, another session's, beside 4.3's from v329): `Vc2SetCosmInputs`, the 48 inputs, the
        constructor `0x89399b10` (its 42 bytes placed by the kind's and index's whole words), conversion, update and copy
        on the dispatcher `0x89399920` and its converters (v235…v241's); xt_vc2sets' seventh part (HARDWARE_FIRMWARE.md,
        "The VC-2 part's sets' COSM inputs"). v314's other child: the OSC blocks' (`0x8939fe00` and its kin).
      - the OSC blocks' done (v391): `Vc2SetOscBlocks`, the 48 inputs, constructor, conversion, update, copy and refresh,
        Random Pitch's seed, the step counts and the start key; `Vc2SetOscKinds`, the dispatch `0x8939fb40` with the
        analog's, EXTIN's and the VOCODER's starts and the converter `0x8939fbd0` with the vibrato's words and the analog's
        and VOCODER's converters (v251, v256, v222); xt_vc2sets' eighth part (HARDWARE_FIRMWARE.md, "The VC-2 part's sets'
        OSC blocks"). Both of v314's children are done. Left, which the OSC blocks call by rom.4's address through
        `Vc2SetReads::unportedOscKind`: PCM's start `0x8939e120` (its wave lookups `0x89394b30`, `0x89394c20`,
        `0x89394d10`, `0x89351840`, `0x89351ad0`, `0x89351ff0`, `0x893520a0`, `0x8939cd30`, `0x8939cba0`), the PCM
        converter `0x8939e300` and EXTIN's `0x8939e9e0` (its own level law `0x8939c490` on a .data table), a version each.
      - EXTIN's converter done (v393): `vc2OscExtinConvert`, v251's head with its own gain, its kind 6 or 7 and its words,
        the formant law `0x8939c490` on rom.4's .data rows; xt_vc2sets' eighth part (HARDWARE_FIRMWARE.md, "The VC-2's
        EXTIN converter"). Left: PCM's start `0x8939e120` and the PCM converter `0x8939e300`.
      - PCM's converter's own part done (v394): `vc2OscPcmConvert`, the Time law `0x8939c310`, the Formant law
        `0x8939c410`, the depth curve and the gain; xt_vc2sets' eighth part (HARDWARE_FIRMWARE.md, "The VC-2's PCM
        converter"). Left: what it calls by the wave's type, LITE's words `0x8939efa0` and VariPhrase's `0x8939f2a0`
        (about 1,100 instructions), and PCM's start `0x8939e120` with its wave lookups.
      - LITE's words done (v395): `vc2OscPcmLite`, the pitch10s and key as v251's head makes them, the cents against
        the wave's root, the start sample held below the length, the switches (Time Trip under way through the part
        table, `Vc2SetReads::tripMoving`) and the wave's bytes; xt_vc2sets' eighth part (HARDWARE_FIRMWARE.md, "The
        VC-2's LITE words"). Left: VariPhrase's words `0x8939f2a0` and PCM's start `0x8939e120`.
      - VariPhrase's words done (v396): `vc2OscPcmVari`, the Playback Mode's ways (STEP's and EVENT's events from the
        wave's events' table through `Vc2SetReads::waveHalf`, EVENT's key by periods about a moved centre), the pitch
        about that centre on v251's key sum, which takes the centre now, the cents, the level and the flags;
        xt_vc2sets' eighth part (HARDWARE_FIRMWARE.md, "The VC-2's VariPhrase words"). PCM's converter is whole. Left:
        PCM's start `0x8939e120`.
      - PCM's start done (v397): the starts' words, LEGATO's flag and key, the key Pitch KF gives, the kind by the
        wave's type; the wave store's four routines it calls (`0x89351840`, `0x8939cd30`, `0x893520a0`, `0x8939cba0`)
        through `Vc2SetReads::waveStore`; xt_vc2sets' eighth part (HARDWARE_FIRMWARE.md, "The VC-2's PCM start"). Every
        routine the OSC blocks call is the port's now but the wave store's, which `Vc2Synth` must give (the wave
        memory's lookups, a port of their own).
      - The wave store's lookups (v398): `Vc2WaveStore`, rom.4's store at `0x8997c52c` (its key and 128 lists) with
        the find `0x89353130`, whether a record is loaded `0x89351840`, a set's index `0x893520a0` and the wave's words
        `0x8939cd30` with the getters it calls, on the wave memory by rom.4's address (`Vc2WaveMemory`, through
        `Vc2SetReads::waveMemory`, which VariPhrase's words read their events from too); xt_vc2sets' eighth part
        (HARDWARE_FIRMWARE.md, "The VC-2's wave store"). Left: the words by a set's index `0x8939cba0` and its three
        sets (v399). `Vc2Synth` must give the store and the memory; the store's set-up and insert, which its loader
        runs, aren't ported.
      - The wave sets (v399): `Vc2WaveSets`, the velocity's, key's and alias's sets (counts `0x8993448c`, tables
        `0x89973084`, buffers `0x89989754`) with their routines `0x89394b30`, `0x89394c20`, `0x89394d10` and the words
        by a set's index `0x8939cba0`; xt_vc2sets' eighth part (HARDWARE_FIRMWARE.md, "The VC-2's wave sets"). Every
        routine PCM's start calls is the port's now; `Vc2Synth` must give the store, the sets and the wave memory, and
        the loaders that fill the store and the sets aren't ported.
      - The wave loaders, v500–v509, a block of their own (the coordinator's, 2026-11-23; map in the shared notes'
        v500-on-vc2-wave-loaders.md): the index's parse (v500), the store's pool, records and chunks, the library's
        records of the Wave Numbers, the bank, its wave reader and the start-up load of the samples.
        - The index's parse done (v500): `Vc2WaveIndex`, rom.4's `0x8934d120` and its three section readers, which
          fill the sets' counts, tables and the velocity's and key's data (now in `Vc2WaveSets`, with `dataAt`)
          through rom.4's file calls (`Vc2Files`); xt_vc2waves, a harness of its own (HARDWARE_FIRMWARE.md, "The
          VC-2's wave index, parsed"). Left: the store's side and the samples; where the plugin gets its files is the
          user's call.
        - The store's pool done (v501): `Vc2WavePool`, rom.4's pool at `0x8997c51c` that the records and chunks live
          in, its set-up, alloc, calloc, free, realloc, largest, compaction and the objects' wrappers, a moved
          block's object told through `Vc2PoolHost`; xt_vc2waves' second part (HARDWARE_FIRMWARE.md, "The VC-2's wave
          pool"). Left: the store's set-up, records and chunks (their classes' moved methods on the host), the
          library's records, the bank, its wave reader and the start-up load.
        - The store's records done (v502): `Vc2WaveRecords`, a record's and a chunk's making, a record's deletion with
          every class's destructor, the lists' insert and unlink, every class's moved method as the pool's host;
          xt_vc2waves' third part (HARDWARE_FIRMWARE.md, "The VC-2's wave store's records"). The block is v500–v549
          now (the coordinator's, 2026-11-23). Left: the store's set-up with its sampling record (v503), the loader's
          context, the Wave Number records, the bank, its wave reader and the start-up load.
        - The waves loaded by rom.4 itself (v503): from here rom.4's own boot loaders run in the interpreter at load
          time, and only what the player reads is ported (the coordinator's, 2026-11-23); the files the disk image's,
          the user's choice ("Image, FAT links"). `Vc2WaveBoot` runs the boot's stages with the kernel answered from
          `Vc2DiskBootFiles` and takes the store, the pool and the sets as `Vc2LoadedWaves`, the wave memory;
          xt_vc2waves' fourth part (HARDWARE_FIRMWARE.md, "The VC-2's waves, loaded by rom.4 itself"). v500's parse
          is the check's expectation, v501's pool the taken pool's container, v502's records unused.
        - [x] The load at the plugin's start (v504): the disk image sought beside rom.4 once rom.4 is in (PluginRom.cpp),
          the load once a process on a thread of its own, the waves published whole (`Vc2WavesInUse`); checked by
          VSynthEmuRomTest (HARDWARE_FIRMWARE.md, "The VC-2's waves at the plugin's start"). Left: the player's reads
          through it, when `Vc2Synth` comes into the sound (its `Vc2SetReads` given `vc2Waves()`).
   9. The part's settings by ID: `0x8939b170`'s readers, the system's `0x8939b0d0`, the mode row `0x89396e20`, the
      Category and budget `0x89392c60`, the nodes `0x893932c0`, `0x89393100`, with G's channel word, the static M and
      event 0x200.
      - the readers of item 8's tables done (v286): `Vc2PartSettings`, the common's, envelopes', LFOs', mix's and
        TVA's readers and their systems', on `Vc2PartParams`; xt_vc2settings (HARDWARE_FIRMWARE.md, "The VC-2 part's
        settings by ID"). Left: the mode row, the part's bytes, the OSC rows, the COSM inputs, `0x893a72f0`, the
        dispatchers, the Category and budget and the nodes.
      - the part's bytes and the COSM inputs done (v287): `0x893a8250` and `0x89399ed0` with their rows, the types'
        runs by `Vc2PartSettingsTables`; xt_vc2settings' second part. Left: the mode row (it sends messages to the
        part's channels), the OSC rows, `0x893a72f0`, the dispatchers, the Category and budget and the nodes.
      - the OSC table's readers done (v288): the systems' `0x893a1000` and `0x893a1060` and the part's `0x893a1410`,
        its header with rom.4's twelve IDs and each block's six velocity pairs; the OSC block's own reader
        `0x893a04e0` behind `Vc2PartParams::oscBlock`; xt_vc2settings' third part (HARDWARE_FIRMWARE.md, "The VC-2
        part's settings by ID: the OSC table"). Left: the OSC block's reader (v289), the mode row, `0x893a72f0`, the
        dispatchers, the Category and budget and the nodes.
      - the OSC block's reader done (v289): `0x893a04e0`, its 63 reads as rows in rom.4's order, Random Pitch Depth's
        switch as its law, the Wave Number's change clearing part 0's step count (`0x89989c84`), kind 0's Type 2 taking
        the Harm Formant's three; xt_vc2settings' fourth part (HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID:
        the OSC block's reader"). The VC-2 takes part 0 alone on this path (the handler `0x89392830`). Left: the mode
        row, `0x893a72f0`, the dispatchers, the Category and budget and the nodes.
      - the vocoder reader done (v290): `0x893a72f0`, the vocoder's settings struct (Formant Shift's ratio by powf, a
        template's offsets from rom.4's records when Auto Note is on with an EXTIN OSC), Unvoice Level into kind 6's
        global, the part mixer's Direct Sound and sends and its targets, Auto Note's switch and legato, robot human
        depth, the vocoder's mode and settings, through `Vc2SettingsParts`; xt_vc2settings' fifth part
        (HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID: the vocoder reader"). Left: the mode row, `0x89396f90`,
        the dispatchers, the Category and budget and the nodes.
      - the mode row and Auto Note's switch done (v291): `0x89396f70` (`0x89396e20`), the parts' settings at
        `0x89995390` (the key's offset, mono, Legato, Patch Tempo, the zones' upper keys by `0x89320870`, Vocoder Sw,
        Split Point, Modulator Key Mode), All Sound Off to the part's nodes' channels when the Mode moves or Vocoder Sw
        goes off, through `Vc2SettingsParts`; the nodes' words (`Vc2PartNodes`); `0x89396f90`, an EXTIN OSC on kind 0
        or 1 taking the part's Auto Note On Switch; xt_vc2settings' sixth part (HARDWARE_FIRMWARE.md, "The VC-2 part's
        settings by ID: the mode row and Auto Note's switch"). Left: the dispatchers, the Category and budget and the
        nodes.
      - the dispatchers and G's channel word done (v292): `0x8939b170` (every reader of a part in rom.4's order, the
        Matrix's settings v247's), `0x8939b0d0`, `0x8939b110`, `0x8939b140`, and `0x893a5050` through
        `Vc2ChannelStates`; v247's record now tests flag 0 after its reads too, as rom.4's does; xt_vc2settings' seventh
        part (HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID: the dispatchers and G's channel word"). Left: the
        nodes, the Category and budget, the static M and event 0x200.
      - the nodes done (v293): `Vc2PartNodes`' routines, the channel's move `0x89393100` on the lists at `0x899890d0`,
        the class installs `0x893932c0` and `0x89393310` on the part objects at `0x8998246c` with the class bytes, the
        voices' stop `0x89393360` through `Vc2NodeParts`, and `0x893904d0` on `Vc2Keys`; xt_vc2keys' second part
        (HARDWARE_FIRMWARE.md, "The VC-2 part's nodes"). The init `0x89393370` is item 10's. Left: the Category and
        budget, the static M and event 0x200.
      - the Category and budget done (v294): `0x89392c60`, the budget less 400 for every Category 0…11 but 6, the held
        note's word `0x899891d0` for 10 and 11, All Notes Off to part 0's node of source 4's channel, the node's word's
        voices truncated, Auto Note's switch, all notes off with no note held, through `Vc2SettingsParts`;
        xt_vc2settings' eighth part (HARDWARE_FIRMWARE.md, "The VC-2 part's settings by ID: the Category and budget").
        Left: the static M's initializers and event 0x200 (the notes' task's: item 10).
      - the static M done (v295): `Vc2StaticMatrix`, its 0x290 bytes at `0x89997108` (M, links, its queue node and
        vtables, the levels, its own channel state, the unreceived channel word, three floats), the static initializer
        `0x893a3020` and the part's init `0x893a2fe0`, the channel state's set-up and the enqueue through
        `Vc2StaticMatrixParts`; xt_vc2settings' ninth part (HARDWARE_FIRMWARE.md, "The VC-2 part's static M"). Item 9
        is done but for event 0x200, which the notes' task raises and handles: item 10's.
   10. The front and the tasks: the MIDI dispatcher `0x8938fed0`, the controllers `0x89392f90` and `0x89394440`, the
       mailbox, the three tasks in one block order, the part mixer, vocoder, tracker and Auto Note joined: `Vc2Synth`,
       with `xt_vc2tone` playing rom.4's whole tone.
       - the system readers the parameters' messages run done (v296): Time Trip's assigns `0x893943f0`, Patch Remain
         `0x89391520`, the voices' counts `0x89391550` (each against its last read at `0x8993d02c`, to v272's reserve
         and setters), and the budget `0x893937b0`, the Category routine's second half; xt_vc2settings' tenth part
         (HARDWARE_FIRMWARE.md, "The VC-2 part's system readers and budget"). Next: the parameters' messages
         themselves (`0x89392890` by kind, the part's `0x89392830`, `0x89392b10`, `0x89392720`, `0x89392750`,
         `0x8939b0c0`, `0x89394370`), the receive channel's nodes `0x89393fd0`, then the controllers.
       - the parameters' messages done (v297): `0x89392890` by kind as a table of ways and the part's `0x89392830`,
         with `0x89391500`, `0x89392750` and `0x89394370`, on `Vc2PartWorld`; the reload task's request `0x8939af40`
         and run `0x8939af10`, the receive channel `0x89393fb0`, the routing's reset `0x89393e30` and every voice's
         truncation `0x89391120` through `Vc2SettingsParts`; xt_vc2settings' eleventh part (HARDWARE_FIRMWARE.md, "The
         VC-2 part's parameters' messages"). Next: the receive channel `0x89393fd0` and the routing's reset
         `0x89393e30` (the parameter store's read objects `0x893206f0`/`0x89320700`, `0x89318a20`/`0x89318a90`), the
         reload task (`0x8939af10`, `0x8939af40`, `0x8939ae50`), then the controllers.
       - the receive channel done (v298): `0x89393fd0` for a part (its Receive Switch and Channel, the store's record
         IDs 0, 1, 2 and 52 read to no effect, its four sources' nodes moved and glides aimed, part 0's change heard
         by controllers 121, 123, 120 and all notes off or the held note's event `0x89391e50`) and part 0's
         `0x89393fb0`, each answering 1; the event through `Vc2SettingsParts::heldNoteOn`; xt_vc2settings' twelfth
         part (HARDWARE_FIRMWARE.md, "The VC-2 part's receive channel"). Next: the routing's reset `0x89393e30`, the
         reload task (`0x8939af10`, `0x8939af40`, `0x8939ae50`), then the controllers.
       - the routing's reset done (v299): `0x89393e30` (ChordMemory Switch's change against `0x8993d03c`, all notes
         off; with its flag every node to no channel, every glide on the spare table and reset, each channel's
         Hold-press byte at `0x899893e0` set and its key tables reset, part 0's receive channel, part 0's node of
         source 1 the held class), answering 1; xt_vc2settings' thirteenth part (HARDWARE_FIRMWARE.md, "The VC-2
         part's routing reset"). Next: the reload task (`0x8939af10`, `0x8939af40`, `0x8939ae50`), then the
         controllers.
       - the reload objects done (v300): `Vc2PartReloads`, rom.4's 16 OS timer objects at `0x89996120` and the three
         floats after them, the static initializer `0x8939b350` with the constructor `0x8939b260`, the cancel
         `0x8939ae50`, the run `0x8939af10`, the request `0x8939af40`, the fire `0x8939b020` and the notes' message
         `0x8939b050`; the queue's add and remove and the part's read through `Vc2ReloadParts`; xt_vc2settings'
         fourteenth part (HARDWARE_FIRMWARE.md, "The VC-2 part's reload objects"). Next: the controllers
         (`0x89392f90`, `0x89394440`), then the part's init (`0x8939b200`, `0x89393370`) and the MIDI dispatcher.
       - the controllers done (v301): `Vc2PartControllers`, the controller `0x89392f90` (the number's object's virtual,
         then each node on the source's channel's list through the part's controller) and the part's controller
         `0x89394440` (Time Trip Assign X and Y on the part's trip object, Reset All Controllers its rest); the number's
         objects, a word's first byte, the part table and the trip object's `0x893b0610` and `0x893b0460` through
         `Vc2ControllerParts`; xt_vc2settings' fifteenth part (HARDWARE_FIRMWARE.md, "The VC-2 part's controllers").
         Next: the part's init (`0x89393370`, which makes the 128 number objects, and `0x8939b200`), the Time Trip
         object (`0x893b0610`, `0x893b0460`), then the MIDI dispatcher `0x8938fed0`.
       - the controller numbers and the part's init done (v302): `Vc2PartNumbers`, the 128 number objects at
         `0x89988cd0` and their table `0x899891e0`, made by class (twelve, as rows), the controller's virtual by the
         object's class through `Vc2NumberParts`, and the init `0x89393370` (part 0's nodes, the keys' and glides'
         static initializers, the lists cleared, the numbers); v301's call out given the value it left out;
         xt_vc2settings' sixteenth part (HARDWARE_FIRMWARE.md, "The VC-2 part's controller numbers and init"). Next:
         the classes' handlers (`0x893938d0` with the listeners' `0x89390ea0`, Hold `0x89393980`, `0x89393a10`,
         `0x89393a70`, `0x89393ac0`, `0x89393ba0`, `0x89393c10`, `0x89393c80`, `0x89393ce0`), then `0x8939b200`, the
         Time Trip object and the MIDI dispatcher.
       - the number classes' handlers done (v303): each a class row's way on `Vc2PartNumbers` (the base's listeners,
         Hold by the channel's Hold-press byte, Sostenuto, CC 84's key, Reset All Controllers, All Sound Off, Mono,
         Poly, the notes off) and the glides' held flags of a channel's nodes `0x89393000`; the listeners
         `0x89390ea0`, the vocoder's pedal and v278's release path through `Vc2NumberParts`; v299's byte the Hold's
         press, not its release (`holdPresses`); xt_vc2settings' seventeenth part (HARDWARE_FIRMWARE.md, "The VC-2
         part's controller handlers"). Next: the listeners' run `0x89390ea0` (the list at `[0x899826e0]`), `0x8939b200`,
         the Time Trip object (`0x893b0610`, `0x893b0460`), then the MIDI dispatcher `0x8938fed0`.
       - the part's start-up done (v304): `vc2PartStartUp`, rom.4's `0x89392ab0`, the notes' task's first call: Time
         Trip's assigns and the two bytes after them −1, the routing object's bytes and floats (π, π/2 by acosf's
         constant), the settings object's +0x14, then the part's init, v272's voices' and lists' inits and `0x8939b200`
         (part 0's reload object idle, eight bare returns, the static M's part init); its words `Vc2PartStartWords`;
         xt_vc2settings' eighteenth part (HARDWARE_FIRMWARE.md, "The VC-2 part's start-up"). Next: the listeners' run
         `0x89390ea0`, the Time Trip object (`0x893b0610`, `0x893b0460`), then the MIDI dispatcher `0x8938fed0`.
       - the listeners' run done (v305): `Vc2Assigner::notify`, rom.4's `0x89390ea0`, each sounding voice whose key
         table's channel state is the one given, its +0x48 virtual (the note's change test) with its +0x3c, through
         `Vc2AssignParts::changed`; xt_vc2voices' fifth part (HARDWARE_FIRMWARE.md, "The VC-2 part's listeners' run").
         Next: the Time Trip object (`0x893b0610` the set, `0x893b0460`, `0x893b04d0`, `0x893b0540` the moves, the
         polar table `0x89543f18` through `0x893b0690`, the steps `0x893b0790`, `0x893b1780`), then the MIDI dispatcher
         `0x8938fed0`.
       - the Time Trip done (v306): `Vc2TripRecords`, the 16 part records at `0x89989408` and their static
         initializer `0x89394570`, the trip object's constructor `0x893b0400`, rate `0x893b05b0`, set `0x893b0610` and
         X-Y move `0x893b0460` with the position `0x893b0690` (the polar table), the steps `0x893b0790` and the
         broadcast `0x893b1780` to the sounding tones' +0x48 through `Vc2TripParts`; v301's front the trip object and
         the pad; xt_vc2settings' nineteenth part (HARDWARE_FIRMWARE.md, "The VC-2 part's Time Trip"). Next: the MIDI
         dispatcher `0x8938fed0`, the mailbox and the three tasks, then Vc2Synth.
       - the channel messages done (v307): `Vc2PartChannel`, the note-on `0x89392da0` and note-off `0x89392e70` (Auto
         Note's source 4 on source 3's tables through the pitch source `0x89989748` and its On Switch, the key
         tables', the nodes' part objects' +0x0c and +0x18, v278's release over a key table) and the three that tell
         the listeners (`0x893930a0`, `0x893930c0`, `0x893930e0`, one code); the program change a bare return; v303's
         key table, channel state and node walk shared; xt_vc2settings' twentieth part (HARDWARE_FIRMWARE.md, "The VC-2
         part's channel messages"). Next: the dispatcher `0x8938fed0` (the status classes `0x8938fe50`, the parameters'
         message `0x89390230`, the 0x41 messages, 0x70), the mailbox and the three tasks, then Vc2Synth.
       - the MIDI dispatcher done (v308): `vc2MidiDispatch`, rom.4's `0x8938fed0` by the status's class `0x8938fe50`:
         the channel messages to their handlers (a note-on of velocity 0 the note-off at 64, none while `0x89981904`
         is set), the parameters' messages 0x40 and 0x41 (a kind-3 word's Out DC cut for TG into `0x893955e0` first,
         then v297's routines) and 0x70's reset `0x89390340`; the calls out through `Vc2MidiParts`; xt_vc2settings'
         twenty-first part (HARDWARE_FIRMWARE.md, "The VC-2 part's MIDI dispatcher"). Next: the mailbox and the three
         tasks (to be read: the routines before the dispatcher, `0x8938fc00`…`0x8938fec4`, which set event flags at
         `0x8994fec8` through `0x89301830`), then Vc2Synth.
       - the notes' task's pass done (v309): `vc2NotesTaskPass`, rom.4's loop body `0x8938fc72`…`0x8938fdac` on the
         events its wait gives (the MIDI queue drained, the ended scan, the reset while `0x89981904` is set, the
         schedule once per count, all notes off on source 3 with Auto Note's key let go, Auto Note's note-off and
         note-on, the aftertouch, a held C4), each on source 4's part 0 node's channel; through `Vc2NotesTaskParts`;
         xt_vc2settings' twenty-second part (HARDWARE_FIRMWARE.md, "The VC-2 part's notes' task"). Next: the mailbox
         (the queue's drain `0x893110b0` and what posts to it), the update task `0x893952b0` and the block task
         `0x89395ac0`, then Vc2Synth.
       - the update task's pass done (v310): `vc2UpdateTaskPass`, rom.4's loop body `0x89395316`…`0x89395414` on its
         wait's events (the timer's: with ticks elapsed, each node on v279's update queue given its +0x0c under the
         queue's lock and taken off on an answer of 0, then the ticks measured into the time struct `0x8993d084`; the
         update's: the pitch source and the voices' update `0x893b16a0`); through `Vc2UpdateTaskParts`; xt_vc2voices'
         sixth part (HARDWARE_FIRMWARE.md, "The VC-2 part's update task"). The queue's drain `0x893110b0` is the OS
         library's MIDI byte reader: the plugin's host gives parsed messages, as XtSynth's `tgNoteOn` takes them for
         rom.2, so the wiring may hand them to v308's dispatcher directly. Next: the block task `0x89395ac0`, then
         Vc2Synth.
       - the voices' update and schedule done (v311): `Vc2PartMixer::updateTones` and `scheduleTones`, rom.4's
         `0x893b16a0` (the blocks since its last in 10 ms ticks, at least 1, to each tone's +0x0c, then the vocoder's
         glue on the carried word) and `0x893b1710` (the blocks since its last in seconds, a block's when none, to the
         free LFOs' clock and each tone's +0x30 and +0x3c), on v227's words; xt_vc2mixer's second part
         (HARDWARE_FIRMWARE.md, "The VC-2 part's voices' update and schedule"). The block task `0x89395ac0` is the DMA's
         and interrupts' plumbing around the part mixer (its input frames' pointer, then `0x893b1820`), which
         processBlock is in the plugin: the wiring's, not a port. Next: Vc2Synth.
       - the update queue with the static M's node done (v312): v279's queue given the static M's node at
         `0x89997278` (`Vc2UpdateQueue::setNode`), every routine and v310's walk reaching a node's words through one
         view, `slotsOf`, the head's, the node given's or a voice's; the walk runs M's init `0x893a2ee0` on the static
         M (its class's +0x0c −0x170, +0x14 the routine); xt_vc2voices' seventh part (HARDWARE_FIRMWARE.md, "The VC-2
         part's update queue with the static M's node"). The update task's timer is the OS's, registered by the task's
         start `0x893952b0` with a period of 10 (the kernel's unit taken to be ms, as rom.2's note-on takes its
         clock), its routine `0x893952a0` a tick added to `0x8993d080` and event 2 posted; so Vc2Synth runs the update
         task's timer pass every 10 ms, 441 samples. Next: Vc2Synth.
       - rom.4's MIDI channel states done (v313): `Vc2OsMidi`, the OS library's MIDI task's handler `0x89314e90` and
         its channel-state module, the 32 states and 48 port objects with their initializers `0x89317d00` and
         `0x89317910`; xt_vc2midi (HARDWARE_FIRMWARE.md, "rom.4's MIDI channel states"). It puts right v310's "the
         wiring may hand parsed messages to the dispatcher directly": the part only reads the channel states (the
         controllers, bend and pressures), and only this handler writes them, on the XT most likely before the part's
         dispatcher sees the message, so Vc2Synth runs it on each message first. The OS's part objects, its listeners,
         turn some messages into parameter edits: a patch change by Bank Select and Program Change, Portamento Time and
         Switch by CC 5 and 65, Mono and Poly by CC 126 and 127, the bend range and tunes by RPN 0…2, the scale tunes
         by SysEx. They are not ported. They are the plugin's side's to decide with the parameters as a host sees them
         (step 5), and Vc2Synth gives them through `Vc2OsMidiParts`. Next: Vc2Synth.
       - Vc2Synth's frame and start done (v314): `Vc2Synth`, every port's object of the part at its address, started
         as rom.4's static initializers leave them in its table's order, with the .data and .bss words and the rows
         no port runs; xt_vc2synth (HARDWARE_FIRMWARE.md, "The VC-2 part as one object: Vc2Synth's start"). C's
         `Vc2LfoWords` and `Vc2MixWords` renamed `Vc2SetLfoWords` and `Vc2SetMixWords`: they shared their names with
         the tone's. Next: the tones' own parts in Vc2Synth (24 tones' two OSC voices, two COSMs, TVA, mix and
         modulators), C's OSC-block and COSM-input children (unported: 0x8939fe00 and its kin, Random Pitch's seed),
         then the tasks' wiring: the OS model of events and waits, the three tasks in one block order, MIDI through
         v313's handler, then v308's dispatcher.
       - The envelopes' and LFOs' settings and release done (v315): `0x893ac110`, `0x893ac470`, `0x893acff0` and
         `0x893ac8c0`, rom.2's word for word, as `Vc2ToneEnvelope::settings` and `release` and `Vc2ToneLfo::settings`
         and `release` on XtEnvelope's and XtLfo's laws, their hooks out of xt_vc2tone (HARDWARE_FIRMWARE.md, "The
         VC-2's envelopes' and LFOs' settings and release"). The tone's parts' host needed them.
       - The tones' parts' host done (v316): `Vc2ToneHost`, `Vc2ToneParts` for the parts the port has whole (a tone's
         envelopes, LFOs and mix, the free LFOs, the lists, the bindings' flags) on what every tone shares
         (`Vc2ToneCommon`), the code xt_vc2tone's port side joined them with, moved into the plugin; the parts not
         joined yet a deriving class's (HARDWARE_FIRMWARE.md, "The VC-2 tone's parts' host"). Next: the TVA joined,
         then the COSMs, then the OSC voices; with the three the host is whole and Vc2Synth holds 24.
       - The TVA joined (v317): the host's `Vc2Tva`, its envelope and LFOs the tone's own through the modulators, its
         fade's step the part's word 0x8993eb08 (`Vc2ToneCommon::fadeStep`), the OS's event the deriving class's; the
         tone's input 0x738 bytes, the TVA's block whole (HARDWARE_FIRMWARE.md, "The TVA in the tone's parts' host").
         The fade step's entry in the interfaces' table is closed. Next: the COSMs, then the OSC voices.
       - The COSMs joined (v318): the host's two COSM objects, a `Vc2CosmDsp` each and the note-on frame, the host their
         bound (`Vc2ToneModulators::wordAt`); `Vc2ToneCommon` gains the pools and the three words past buffer c; a
         sound takes its note at the install the tone's note-on ends with, and keeps that mode (HARDWARE_FIRMWARE.md,
         "The COSMs in the tone's parts' host"). Found: rom.4's note-on holds the install pass
         inline, nothing reaching `0x893b4060`, so a COSM's mode never changes mid-note in the part. Next: the OSC
         voices, then Vc2Synth's 24 hosts.
       - The OSC voices joined (v319): the host's two `Vc2OscVoice`s, every part a tone has; each voice's parts a
         `Vc2ToneOscLink`, its address and the tone's own envelopes' and LFOs' words the host's, the rest (a kind's
         routines by rom.4's address, the OS's clock, the wave's records, the mic ring, the unvoiced decision) the
         deriving class's `oscOutside`; `Vc2ToneCommon` gains the shared routines (HARDWARE_FIRMWARE.md, "The OSC voices
         in the tone's parts' host"). Found: the tone drops the install's answer; an install writes no render for a
         type or code it has no player for; the analog's install reads the code's copy at +0x248; only a note-on
         writes a voice's kind and flag. Next: Vc2Synth's 24 hosts, what a voice calls beyond the tone the part's.
       - The shared routines for the plugin (v320): `Vc2ToneMath`, a `Vc2ToneShared` on XtOsc's velocity curve and exp2
         (rom.2's ports of rom.4's `0x893a7fb0` and `0x893a8340`, equal over what the part gives) and Vc2LibMath's
         logf and powf; xt_vc2libm's second part (HARDWARE_FIRMWARE.md, "The VC-2's shared routines for the plugin").
         The checks keep rom.4's own (`RomShared`). Next: Vc2Synth's 24 hosts on it.
   11. The OSC kinds' routines the voice still reaches by rom.4's address (the user's choice, 2026-11-20: these before
       Vc2Synth holds the 24 tone hosts, so that each kind sounds once the hosts are in). The voice calls them through
       `Vc2OscParts` (`kindCall`, `kindSettings`, `renderBy`, `seek`, `objectCall`, `eventPosition`); xt_vc2osc hooks them
       (`Vc2OscFakes.cpp`'s table, 59 rows at v320). Each joins the voice as v238…v248 joined the installs and note-ons: a row's
       way in `Vc2OscVoiceKinds.cpp`, its hook out of the table, rom.4's own running in xt_vc2osc and xt_vc2tone. By
       what each stands on (VC2_VOCAL_DESIGNER.md, "The VariPhrase players and key assign in rom.4"):
       1. Kinds 5's and 6's updates, `0x893afff0` and `0x893b0150`: the first ported as `vc2Kind5Update` (v222, on the
          input stage's peak at `0x899d9a5c`, which `Vc2OscParts` doesn't give yet), the second the note-on's kind-6
          laws ending in `Vc2Kind6::set`. With them kinds 5 and 6 are whole but for kind 6's first routine, below.
       2. The EXT IN player, kinds 4 and 7 and kind 6's first update: `0x893c6260` (update), `0x893c62e0` and
          `0x893c61e0` (renders), on the effect chip's returned words, against rom.2's (`0x895097d0`, v203's
          `XtOscVoice::extIn`).
       3. Vari off's renders joined (`0x893c63c0`, `0x893c66e0`, v261's ports), with the wave's samples through the
          voice's parts.
       4. LITE: its update `0x893c8280` and settings `0x893c8390`, its object routine `0x893cbf40`, its heads' renders
          `0x893c6e80`/`0x893c6e8c` on `XtLitePlayer` (rom.4's backward reads the right way up: a switch), kind 1's mode 1
          (`0x893c9f60`, its set-up `0x893cc760`), its seeks `0x893c6360`, `0x893cc830`, `0x893c6de0`.
       5. VariPhrase: its update `0x893c88b0` and the routine after it `0x893c8670`, its settings `0x893b0280`, the event
          lookup `0x893cc0d0`, its seeks `0x893cc630`, `0x893cd090`, `0x893cd440`; the players' renders on rom.2's ports
          given rom.4's object layout (the insert at +0x94): BACKING `0x893cadc0` and ENSEMBLE `0x893ca1f0` on
          `XtMbPlayer`, SOLO `0x893c9450` on `XtSoloVoice` (Robot Voice rom.4's own, open), the period-jump player
          `0x893ca8b0`.
       6. done (v248's installs, v337's renders): the analog OSC's 19 renders, its installs' (`0x893c2ad0`,
          `0x893c3e60`), against rom.2's `XtAnalog`.
       Then Vc2Synth's 24 hosts, what is left beyond the tone (the OS's clock, the wave's records, the mic ring, the
       unvoiced decision, the input stage's peak, the chip's words) the part's.
       - Kinds 5's and 6's updates (v321): `0x893afff0` by v222's `vc2Kind5Update` on the voice's own words, the
         input stage's peak through `Vc2OscParts::inputPeak` (the tone host's link the deriving class's), and
         `0x893b0150` (the ratio on its own pool, the formant, the gain and kind 6's level into P, then
         `Vc2Kind6::set`), rows `Vc2OscUpdate::carrier` and `kind6`; their hooks out of xt_vc2osc, which now draws
         the peak and P's growl, balance and Dynamics words as a note gives them (HARDWARE_FIRMWARE.md, "The VC-2's
         kinds 5 and 6 updates on the voice"). Found: those words lie over the block's record pointers a player's
         note-on reads; the pitch ramp's gain factor is 1 in the part, nothing calling the restart that gives it a
         depth. Next: the EXT IN player.
       - The EXT IN player (v322): its update `0x893c6260` (the ratio and gain into the object; kinds 4 and 7, and
         kind 6's first, a row's flag `extInFirst` where an address was) and its renders `0x893c62e0` and `0x893c61e0`
         on v203's `XtExtInPlayer`, found by the address the install writes (`renderExtIn`), the frames through
         `Vc2OscParts::inputFrames` (`[0x8993eadc]`, the part mixer's input); their hooks out of xt_vc2osc
         (HARDWARE_FIRMWARE.md, "The VC-2's EXT IN player"). Found: rom.4's stereo render is rom.2's player's code on
         the frame's words 0 and 1, and the mono one the same on word 4; the player's output must stay within full
         scale for the tone's delay mix, which moves its delay by it. Next: Vari off's renders.
       - Vari off's renders joined (v323): `0x893c63c0` and `0x893c66e0` (v261's ports) on the wave the object's +0xf4
         points into, through `Vc2OscParts::waveSamples`; the voice's renders one table by address (`renderOwn`, the
         EXT IN two its rows too), any other still out; their hooks out of xt_vc2osc, v261's draws of the wave and the
         words shared (HARDWARE_FIRMWARE.md, "The VC-2's Vari off renders on the voice"). Next: LITE.
       - LITE's update and settings (v324): `0x893c8280` (the targets into the object by the wave's type, type 2's
         filter rate) and `0x893c8390` (the object's three bits and their side effects, then type 0's Vari off side
         or type 2's heads' loops through the record the note-on's head names), kind 1's row's ways; the parts they
         share with the note-ons factored out of them (HARDWARE_FIRMWARE.md, "The VC-2's LITE update and settings").
         Next: LITE's object routine, heads' renders, mode 1 and seeks.
       - The players' beat and seeks (v325): LITE's object routine `0x893cbf40` and the reset `0x893cbfe0` its seeks
         end in are rom.2's beat start and reset (`0x8950f860`, `0x8950f900`), run as XtBeat's on the object's words;
         Vari off's seek `0x893c6360` and the heads' `0x893c6de0` (rom.2's `0x89509840`, `0x8950a780`, four bytes up)
         the voice's own, its seeks one table by address; mode 1's and the band players' seeks still out
         (HARDWARE_FIRMWARE.md, "The VC-2's players' beat and seeks"). Next: LITE's heads' renders on XtLitePlayer;
         mode 1 with VariPhrase's band players.
       - LITE's heads (v326): `0x893c6e80` and `0x893c6e8c` on XtLitePlayer (rom.2's, its words four bytes up from
         +0x94, its backward reads upright), after rom.4's beat block `0x893c8990` (XtBeat's, its steer reaching four
         steps, then the tail's step word by byte 3, the loop's positions through the +0xe0 record), the voice's own
         renders' rows; XtLitePlayer runs on the object's words by its table of them (HARDWARE_FIRMWARE.md, "The VC-2's
         LITE heads"). Next: mode 1, with VariPhrase's band players.
       - Mode 1's render (v327): `0x893c9f60` (rom.2's `0x8950d880` on rom.4's object, mono and stereo) the voice's
         own, on the players' event walk it runs: the event load `0x893c8f00`, the next crossing `0x893c9380`, the
         crossings `0x893c8f50` and `0x893c9140` with the events' ways byte 2 opens, mode 1's two jumps `0x893ca190`
         and `0x893ca1a0`, a player's index a row (`Vc2WalkWay`) (HARDWARE_FIRMWARE.md, "The VC-2's mode 1"). Found:
         mode 1's cached jump writes through the render's mask, an address error on the XT at every One Way or hold
         loop point; the walk's searches step by an event's own offset. Next: mode 1's set-up `0x893cc760` and seek
         `0x893cc830`, with SOLO's start `0x893cc1a0` and the lookup `0x893cc100`, which SOLO's walk shares.
       - Mode 1's set-up (v328): `0x893cc760` the voice's own, with SOLO's start `0x893cc1a0` and its lookup
         `0x893cc0d0` by a player's row (its seek and jump two more columns of `Vc2WalkWay`) and the loops' rates
         `0x893ccf50`. Found: the set-up and LITE's settings read the records through the object's head, which a
         legato's take brings from the earlier tone's voice, so a legato sets the new note up on the earlier note's
         wave; both read through it now, the link answering a pool tone's voice's words from its host
         (HARDWARE_FIRMWARE.md, "The VC-2's mode 1 set-up"). Next: mode 1's seek `0x893cc830` with the find
         `0x893cc100`.
       - Mode 1's seek (v329): `0x893cc830` the voice's own with the find `0x893cc100` (`Vc2OscWalkStart.cpp`), a row of
         the voice's seeks, each row now given the word as its callers give it. With the range in samples: the
         position held to 0…the last event's, the event found for it, mode 1's jump, the position. By events: the
         number's low half held to the last, the jump. Then the beat's position the position × 32 and the beat's reset
         (HARDWARE_FIRMWARE.md, "The VC-2's mode 1 seek"). Found: LITE's note-on of a type 1 wave seeks on the object
         as the voice's last set-up left it, before this note's install pass sets the new wave up; that set-up writes
         again every word the seek wrote. By events the seek takes a position as an event's number (the note-on's seek
         point × 16, the re-seek's beat position over 32), its low half held to the last; the re-seek's stands. And
         from the words a start leaves, the find never reaches its stop at the last event. With it LITE is whole.
         Next: VariPhrase's update, settings, lookup and seeks.
       - VariPhrase's update, start and settings (v330): `0x893c88b0` (five words 0.7 of the way to their targets),
         `0x893c8670`, the players' start in its note-on (the gain's and level's targets, the player's set-up by the
         wave's type, the band seek to the block's seek point) and `0x893b0280` (the flags, the object's bits and
         their side effects), the voice's own in `Vc2OscVari.cpp`; the last of a kind's routines by address, so
         `kindCall` and `kindSettings` are gone; the players' set-ups out by address on the object again
         (`objectCall`) until the players come (HARDWARE_FIRMWARE.md, "The VC-2's VariPhrase update, start and
         settings"). Found: the start gives a band seek the right units, a number in events and the point × 16 in
         samples. Next: the event lookup for VariPhrase's note-on and the band seeks.
       - The event lookup (v331): VariPhrase's note-on's range by events reads `0x893cc0d0` as the voice's own (v328's
         lookup, `vc2OscEventPosition`), so `Vc2OscParts::eventPosition` is gone and the lookup's hook with it
         (HARDWARE_FIRMWARE.md, "The VC-2's VariPhrase event lookup"). Next: the band seeks, which call the players'
         own routines, with each player.
       - The band players' renders (v332): BACKING's `0x893cadc0` and ENSEMBLE's `0x893ca1f0` the voice's own
         (`Vc2OscBands.cpp`): rom.4's beat block, then XtMbPlayer's block on a copy of the object's words (the state
         moved into `XtMbState.h`, its words' places the VC-2's map), its crossings by the voice's walk with the band
         row (index 16) and its wrap at an end the VC-2's (HARDWARE_FIRMWARE.md, "The VC-2's band players' renders").
         Found: the walk list's wrap under byte 0's 0x08, which the rom.2 port had left out; a range past the last
         event, which rom.4's set-up leaves for the wrap to read past the table; ENSEMBLE below 0 after a backward One
         Way from below the loop, its frames' index read unsigned. Next: the band players' set-ups (`0x893cd1d0`,
         `0x893cc8f0`) and their seek `0x893cd440`, then SOLO and the period-jump player.
       - The band players' set-ups and seek (v333): BACKING's `0x893cd1d0` and ENSEMBLE's `0x893cc8f0` (with their
         tail `0x893cd370`) the voice's own on the object's words (`Vc2OscBandSetUp.cpp`), each band's words from
         rom.4's six tables, the walk's start with the band row, the loops' rates and the beat's start; their seek
         `0x893cd440` mode 1's seek's code with the band row, then rom.2's `startAt`'s bands' part
         (`State::startBands`) on a copy (HARDWARE_FIRMWARE.md, "The VC-2's band players' set-ups and seek"). Found:
         neither set-up writes band 0's shift and stride; the set-ups' gains and byte 3's 0x40 and the seek's clear of
         the position show in no word. Next: SOLO (`0x893c9450`, `0x893cc2e0`/`0x893cc310`, `0x893cc630`) and the
         period-jump player (`0x893ca8b0`, `0x893ccff0`, `0x893cd090`).
       - SOLO (v334): its render `0x893c9450`, set-ups `0x893cc2e0`/`0x893cc310` and seek `0x893cc630` the voice's
         own, rom.4's code on the object's words as mode 1's was (`Vc2OscSolo*.cpp`; `XtSoloVoice`'s model of records by
         index can't be laid back onto the words), its row of the walk `kVc2SoloWalk`; the band players' tail and its
         set-ups share `vc2WalkSetUp` (HARDWARE_FIRMWARE.md, "The VC-2's SOLO"). Found: type 1 and type 6 differ only in
         byte 3's 0x80, which gates the next record's length, the period's glide and the second tap (type 6 rom.2's
         SOLO); Robot Voice is rom.4's own, the grains at the pitch ratio × 440 Hz, its swing × the +0x94 word that
         VariPhrase's note-on writes 0 (open what sets it). Next: the period-jump player (`0x893ca8b0`, `0x893ccff0`,
         `0x893cd090`).
       - The period-jump player (v335): its render `0x893ca8b0`, set-up `0x893ccff0` and seek `0x893cd090` the voice's
         own (`Vc2OscJump*.cpp`), its row of the walk `kVc2JumpWalk`; SOLO's record block code and the frames' steps
         made one for the players (`Vc2OscRecordBlock.cpp`, `Vc2OscFrameSteps.cpp`); `Vc2OscParts::seek` and
         `objectCall` gone, every player's seek and set-up the voice's own (HARDWARE_FIRMWARE.md, "The VC-2's
         period-jump player"). Found: after a walk it compares the new crossing; it glides the voice's gain words, not
         the object's its set-up writes; its read can lead or lag the walk far and read past the wave (open what the XT
         holds there for real waves), the port's sample reader by the physical address as the SH-4's. Next: the
         analog oscillator's 19 renders.
       - XtAnalog.h's split (v336): the MOD and the tone's render order into `XtToneMod.h`, the knobs and the block
         into `XtAnalogBlock.h`, so `XtAnalogOsc` can open its state to the analog's renders by a word table
         (HARDWARE_FIRMWARE.md, "XtAnalog.h's split"); XtAnalog.cpp left whole. Next: the 19 renders on it.
       - The analog OSC's 19 renders (v337), item 6's renders: rom.2's code four bytes up from +0x94 and HQ's tables
         through rom.4's own pointers (rom.2's byte for byte), so rom.2's `XtAnalogOsc` on the object's words
         (`XtAnalogWords`, `Vc2OscAnalogRender.cpp`), HQ's tables from rom.4 (`Vc2OscAnalogPool::hq`); their hooks gone
         (HARDWARE_FIRMWARE.md, "The VC-2's analog renders"). Found: FM's delay lines read beside a ring when OSC 2
         passes full scale, as an analog OSC's Impact takes it, into the rings around it (rom.4's A and B rings end to
         end): `Vc2ToneMixRings` one array, a read beside a ring as rom.4's (the port's had left its array). Item 6's
         installs were v248's, so item 6 is done. Next: Vc2Synth's 24 hosts.
       - The band waves' segments distinct (v338): the checks' band wave draw (v332's) no longer gives a segment no
         records when two drawn starts fall alike, which an AddressSanitizer build reached (HARDWARE_FIRMWARE.md, "The
         band waves' segments distinct").
       - VariPhrase's formant target finite before its update (v339): the check's formant depths took exp2 past its
         table to a NaN that FSUB and xtf's subtraction carry otherwise; no XT reaches it, so a draw
         (`oscSteerVariFormant`; HARDWARE_FIRMWARE.md, "VariPhrase's formant target finite before its update"). Next:
         Vc2Synth's 24 hosts.
       - Vc2Synth's tones (v340), Vc2Synth whole's first version: the 24 hosts the part's (`Vc2SynthTone`), their
         OSC voices' outside from the part and the waves it is given; xt_vc2synth makes the 24 on both sides
         (HARDWARE_FIRMWARE.md, "Vc2Synth's tones"). Next, as the shared notes plan it: the note's reads
         (`Vc2SetReads`, `Vc2NoteParts`, `Vc2VoiceParts` from the part), then the tasks' OS model, the block, and the
         whole check against rom.4's three tasks.
       - C's reads of the part's memory in the plugin (v341): SetPortParts' reads by address moved as
         `Vc2SetMemoryReads`, the first half of the note's reads (HARDWARE_FIRMWARE.md, "C's reads of the part's memory
         in the plugin"). Next: the part's reads on its own words (`Vc2SynthMemory`), C's children's routines, the
         notes' and voices' parts.
       - The part's memory by address (v342): `Vc2SynthMemory`, xt_vc2synth's region table the plugin's
         (HARDWARE_FIRMWARE.md, "The part's memory by address"). Next: `Vc2SynthReads` on it, the Matrix's reads by
         their readers' groups (`vc2MatrixRead`), C's children by their rows to the part's pools.
       - C's reads in the part (v343): `Vc2SynthReads`, the children by row to the part's pools (the OSC blocks' and
         COSM inputs' in Vc2Synth now), a Matrix group by its reader on M by address, `nextOscStep`, the waves given;
         xt_vc2synth's reads part (HARDWARE_FIRMWARE.md, "C's reads in the part"). The mixer's offsets are the static
         M's levels, one memory to give it when the block is wired. Next: `Vc2NoteParts` and `Vc2VoiceParts` from the
         part, the tasks' OS model, the block, the whole check (which runs the children's other routines).
       - The part's voices' stops and truncations (v344): rom.4's 0x89390f70, 0x89390fd0 and 0x89391120 in
         `Vc2Assigner`, the +0x48 virtual's answer through `Vc2AssignParts::changed`; xt_vc2voices' eighth part
         (HARDWARE_FIRMWARE.md, "The part's voices' stops and truncations"). Next: the part's patch load (its parameters
         by read object, the defaults with no store; the settings' readers and calls out on the part).
       - The part's settings and voices wired (v345): the notes' task's start, the parameters by read object (the
         defaults with no store), the readers and messages, and the voices', notes' and controllers' parts on the
         part's ports; xt_vc2synth's load part (HARDWARE_FIRMWARE.md, "The part's settings and voices wired"). Next:
         the notes' task's pass on the part (MIDI through the dispatcher and channel to the assigner, the notes, C and
         the tones), then the update task, the block and the whole check.
       - The part's MIDI note path (v346): `Vc2SynthMidi`, a message through the OS MIDI's handler and the dispatcher on
         the part's ports, a part object's note-on and note-off by its class, the audio task's object (row 16's start,
         DC cut's word); xt_vc2synth's notes part (HARDWARE_FIRMWARE.md, "The part's MIDI note path"). No note reaches a
         voice yet: Part 1 Receive Switch is a system ID, read as its default (off) with no system store. Next: the
         system store (rom.4's store object 0x89961250 and its read objects), so a note reaches the assigner, then the
         update task, the block and the whole check.
       - The part's system store (v347): `Vc2System` (rom.4's three system areas, its defaults run once in an image by
         `vc2LoadSystem`), the part given it and reading the system's blocks from it, the OS's 16 part objects (the OS
         row 0x89318140's); xt_vc2synth's notes reach the voices (the assigner, C, the tones' note-ons, voices stolen,
         the fade's wait rendered at once) on rom.2's and rom.4's tables as the plugin loads them (HARDWARE_FIRMWARE.md,
         "The part's system store"). Next: the update task on the part, the block (mixer, renders, vocoder, tracker,
         Auto Note) with the waits rendering in place, the OS model, and the whole check.
       - The checks' region compare on a part word (v348): `vc2FirstDifference` rounds a region's bytes up to words,
         so xt_vc2sets' sets' counts (6 bytes) are no longer written past and their third halfword is compared (the
         v550 sweep's finding; HARDWARE_FIRMWARE.md, "The checks' region compare on a part word"). The update task on
         the part, planned as v348, is v349.
       - The part's update task (v349): `Vc2SynthUpdate` (a node's update by what it is: the static M's M's init
         answering 1, a voice's by its class answering 0; the pitch source; the voices' update on the sounding list,
         then the vocoder's glue), `Vc2Synth::updatePass`; xt_vc2synth's notes part with an update pass one event in
         13 (HARDWARE_FIRMWARE.md, "The part's update task"). Next: the notes' task's events (the ended scan, the
         schedule), the block (mixer, renders, vocoder, tracker, Auto Note), the OS model and the whole check.
       - The part's notes' task (v350): `Vc2Synth::notesPass` on v309's pass, its parts the part's MIDI parts (the
         queue's drain none, as the part dispatches a message as it comes; the ended scan; a source's reset by the
         dispatcher's 0x70 way; the voices' schedule on the sounding list; Auto Note's release); xt_vc2synth's notes
         part with a notes' task's pass one event in 13 (HARDWARE_FIRMWARE.md, "The part's notes' task"). The ended
         scan frees nothing yet: a tone's TVA ends in the renders. Next: the block (mixer, renders, vocoder, tracker,
         Auto Note), the OS model and the whole check.
       - The part's block (v351): `Vc2Synth::block`, the part mixer's block on the part, its voices the sounding
         tones (`Vc2SynthTones`, shared with the update and schedule), its parts the tracker's input stage with Auto
         Note, the mic ring, the detector and the vocoder's driver (`Vc2SynthBlock`, with Auto Note's host
         0x89391dc0 and 0x89391e20); the audio task's start, the mixer's reset (`Vc2Synth::audioStart`);
         xt_vc2synth's notes part with a block one event in 13 (HARDWARE_FIRMWARE.md, "The part's block"). Next: the
         OS model (the three tasks in their order a block, by the events each is posted) and the whole check.
       - rom.1, the kernel (v352). **The user's decision (2026-11-24): "Load rom.1".** The OS model goes by the XT's
         kernel: rom.1 read at run time from `firmware/unpacked/V-SynthXT.rom.1.bin` (none of its bytes, listings or
         tables committed), its event services run in the interpreter as the reference, the part's model of them (the
         event set, the wait, the cyclic handlers and alarms, the tempo timers) the hand-port, checked against them at 0
         different; the task table's order a priority only as the kernel's scheduler reads it. What rom.1 is, from its
         bytes (`kernelmap.py`): it runs at 0x88000000, and every slot word rom.4 loads and v503's 27 answers are its
         service table's slots; v503 answered them without rom.1, which still stands. Its scheduler reads a lower level
         as a higher priority; the update task's 0x1 is its own 10-tick alarm (HARDWARE_FIRMWARE.md, "rom.1, the
         kernel"). Next: the kernel's event services ported and checked against rom.1's in the interpreter, then the OS
         model on the part and the whole check.
       - The kernel's event services (v353): `Vc2Kernel`, rom.1's scheduler as the part runs on it, against rom.1's own
         in the interpreter (xt_vc2kernel, rom.1 beside rom.4, its switch hooked; HARDWARE_FIRMWARE.md, "The kernel's
         event services"). Next: the OS model on the part (its tasks' passes on Vc2Kernel at their levels, the 1 ms
         tick with the update task's alarm and cyclic handler and the clock's tempo ticks, the block's DMA), then the
         whole check against rom.4's three tasks on rom.1's kernel.
       - The part's clock and tempo timers (v354): the clock object built as rom.4's row 20 builds it (the part's copy
         was 0), its ms and the clock task's pass (`Vc2SynthClock`, XtTempoTicks' counting), the tempo timers' ring
         (`Vc2TempoTimers`, the ring one template with the arpeggio's, `XtRing.h`) with the notes' schedule on it;
         xt_vc2synth's notes part with clock events (HARDWARE_FIRMWARE.md, "The part's clock and tempo timers"). Next:
         the part's tasks on Vc2Kernel (the audio, clock, notes and update tasks at their levels; the 1 ms tick with
         the update task's alarm and cyclic handler and the clock's ms; the block's DMA), then the whole check.
       - [x] The part's tasks on the kernel (v355), Vc2Synth whole done: the part on its OS model
         (`Vc2Synth::runOnOs`; `Vc2SynthOs`, the four tasks, the MIDI input task, the OS MIDI task and the front's
         stand-in on Vc2Kernel at their levels with the idle below, a post's woken task run at once, a sender's pass
         ending at a post that switches, an interrupt taken with the idle running, a wait inside a pass taking the next
         outside event; `Vc2SynthOsTasks`, their prologues' and passes' parts, the cyclic handlers, TMU0's handler with
         the OS clock, the DMA's interrupts, a MIDI message as its bytes' receive interrupts or the front's); the XT's
         MIDI input path on both sides by the coordinator's call (a) under the 0-different rule (`Vc2MidiReceive`, the
         receive objects and MIDI IN's 2048-word ring with its full marker; `Vc2OsQueue`; `Vc2SynthOsMidi`, the input,
         OS MIDI and front tasks, the senders' waits for empty and for room, the drains' wakes); the clock task's MIDI
         listener and its 0x40 pass (Clock Source, Clock Out, the patch's tempo); the update's walk of the sounding list
         as it stands, since the notes' task preempts it; a channel state's lookup on a channel of -1. Checked whole
         against rom.4's own tasks on rom.1's kernel in one interpreter, their prologues and interrupts rom.4's and
         rom.1's own (`Vc2MachineRig`; xt_vc2synth's whole part, with bursts that fill the notes' queue and floods that
         fill MIDI IN's ring; HARDWARE_FIRMWARE.md, "The part's tasks on the kernel"). Next: step 5's second half, the
         plugin's side, which needs it, with the step 4.5 [gap]s below.
       - [x] The merge's gate (v356): the --tone list's update-osc-order, missed from v334, shown equivalent on rom.4
         (its update in both orders leaves the same memory) and taken out (HARDWARE_FIRMWARE.md, "A tone's two OSC
         updates: their order shows nowhere").
       - [x] The merge's gate (v357): the --osc list's heads-count, missed from v334 as the check's wave memory grew, its
         reach back by a draw of LITE's heads on SOLO's waves (HARDWARE_FIRMWARE.md, "LITE's heads past the wave
         memory's first quarter").
       - [x] The plants the merge's whole lists still missed (v358 on): the --kernel list's kn-rr-freeze, caught by
         the words compared between a tick and the watchdog's interrupt (v358; HARDWARE_FIRMWARE.md, "The round
         robin's tie with a put-off dispatch"); the --synth list's plain voice's five and its drain's flag, shown out
         of reach on rom.4 and taken out (v359; "The plain voice and the drain's flag: out of the part's reach");
         seven more of the --synth list's caught by draws and compares of their own (v360; "The part's reach: seven of
         xt_vc2synth's misses caught").
       - [x] tns-count (--synth): an OSC voice's count moves only where a VariPhrase player asks it, caught once the
         part has waves (v362; HARDWARE_FIRMWARE.md, "The part given the disk image's waves"). Both sides get the disk
         image's waves, the port through `giveWaves` and rom.4's machines by rom.4's own load (v503's), read at run time
         beside rom.4, and rom.4's reads past them are given zeros and counted on both sides. The renders showed two
         faults, each fixed red first in v362: the samples `giveWaves` didn't give (step 4.5's v455 lines) and
         `boundFloat`'s word for an address in no tone (Tempo Sync's beat). No steer was needed.
       - [deferred] The VOCODER's balance NaN's sign (v349; the user, 2026-11-24: "Defer it"). A patch that changes a
         sounding VOCODER-OSC note's OSC Type to PCM (a patch change, or an OSC Type edit under the note) has the next
         update pass convert the note's OSC block by the new type (C picks the converter by the record's type at each
         convert): PCM's converter writes −1 (0xffffffff) at P+0x48, the block's +0xf8, while the block's head kind
         stays 5, and kind 5's update `0x893afff0` reads it as the balance envelope depth, so the carrier's balance and
         noise gain are NaN. The two sides then differ in the NaN's sign alone: the interpreter's FSUB keeps it
         (0xffffffff), `xtf::sub` (sum2(a, −b)) flips it (0x7fffffff), and a real SH-4 gives a third answer, 0x7fbfffff
         (the sNaN quieted). Deferred because only a NaN's sign differs, and matching the interpreter would change
         `xtf::sub`, which rom.2's ports share; XtFloat.h is unchanged and there is no VC-2-only subtract. Meanwhile
         xt_vc2synth's notes part keeps a sounding VOCODER OSC's Type in a new patch (`vocoderTypesKept`,
         Vc2SynthNotesCheck.cpp). What the NaN does to the audio: step 4.5's "A NaN sample to the host" (closed by v454).
       - [gap] `ftrc` of a NaN (seen in v364, not its cause): XtFloat's `xtf::ftrc` gives 0, the interpreter's FTRC
         (`ftrcF`, Sh4Interp.cpp) 0x80000000. XtFloat is shared with rom.2's ports, so it is left as it is. What a real
         SH-4's FTRC gives for a NaN is to be read from its manual; not decided (the coordinator's call, 2026-11-25).
4. The effects: rom.4's driver on `XtFxChip` (XtFxControl on rom.4's programs), the patch EQ with them. It needs nothing
   of step 4.3: the driver reads parameters by ID and writes the chip's bus, and no tone is in its way.
   The map (2026-11-17), run on rom.4's driver a message flag at a time:
   - **The driver** is rom.2's, recompiled and moved: the object `0x89980f24`, the message's flags `0x89980f84`, its start
     `0x89368280`, `0x8935f9a0`, `0x8935fa80`, the reader `0x8935fc40`, the clock's poll `0x8935fde0` on the clock's object
     `0x8995cb40`, the bus's primitives `0x89362490`, `0x89362610`, `0x893626d0`, `0x893627f0`, the sleep `0x89365e50`, the
     reads `0x89324350`, `0x89324370`.
   - **The programs** are rom.4's own, loaded by its driver from its image (VC2_VOCAL_DESIGNER.md, "The effect chip's
     programs"): program A rewritten, program B 23 words apart, the 42 MFX and the V-Synth's reverbs with registers
     renumbered, seven new stereo reverbs.
   - **The message's flags** mean what they mean on rom.2: second byte 0x20 system, 0x10 Attenuation; fourth byte 0x80
     patch, 0x40 switches, 0x20 MFX, 0x10 Chorus, 0x08 Reverb. No other bit reads or writes anything.
   - **What each reader reads** (every message also reads IDs 0…2, and on rom.4 0x34):

     | Flag | rom.2's IDs | rom.4's IDs |
     |---|---|---|
     | System | 0x3e, 0x41, 0x65…0x70, 0x76 | 0x40, 0x43, 0x67, 0x68, 0x6f…0x72, 0x78, 0x7f |
     | Attenuation | 0x51e, 0x178 | 0x52b, 0x1b8 |
     | Switches | 0x11d, 0x120…0x122 | 0x13a, 0x13d…0x13f, **0x1af…0x1b4 (the patch EQ)**, 0x12c |
     | MFX | 0x177…0x17a, the type's | 0x1b7…0x1ba, the type's |
     | Chorus | 0x1fc…0x1fe, the type's | 0x23c…0x23e, the type's |
     | Reverb | 0x230, 0x231, the type's | 0x270, 0x271, the type's |

     A patch reads those and, on rom.4 alone, 0x195 and 0x2e0, 0x2e2, 0x2e5. The Category (0x12c) moves no chip word at any
     value, but a patch of Category 10 or 11 posts event 0x200 to another task (`0x89391e50`, on the object `0x8994fec8`),
     which step 4.3 has to place. The system's EQ has no mid bands on rom.4: EQ 1 and EQ 2 are the patch's, read with the
     switches.
   - **The types:** 42 MFX and 9 Chorus as on rom.2, 15 Reverb Types against 11, each type's IDs from tables at
     `0x895164ec`, `0x895165b8`, `0x895165e4`.
   - **Not mapped, so not run:** rom.4's pair of the Matrix's MFX-PRM routines (`0x89361d50`, `0x89361de0`) has no literal or
     branch reaching it, though MFX-PRM1…3 are Matrix destinations (v401: rom.4 sends those to the part mixer's levels);
     ~~and no reader reads External In Type (0x7a), where
     rom.2's EXT IN request reads its own~~ (v400: its EXT IN request `0x8935b050` reads it, outside the driver as rom.2's
     is, and moves no chip word).
   - **XtFxControl** needs only where these are: `XtFxImage` holds them, one row of addresses an image, rom.2's as the
     class held them.

   Split by those:
   1. done (v259): the driver by image: `XtFxImage`, XtFxControl on rom.4, the patch EQ's IDs, against JE-8086's chip and
      against the driver run by hand (HARDWARE_FIRMWARE.md, "rom.4's effect driver").
   2. done (v274): the frame's way through on rom.4: `XtFxUnit` on an image, rom.4's frame into program A's slots 0x20…0x2e
      as rom.2's (its truncation `0x89396200` is rom.2's `0x894db2b0`), at a gain of 1 where rom.2's is Out Gain for TG's
      sixteenth, and out as the mix, DIR going nowhere; against rom.4's truncation and JE-8086's chip
      (HARDWARE_FIRMWARE.md, "rom.4's frame through the unit"). For step 4.3: the VC-2's tones take rom.4's Out Gain
      for TG (ID 0x533, −24 dB by default) in the TVA's block, as rom.4 does (`0x893a670c`), since the unit doesn't.
   3. done (v400): the outside input (HARDWARE_FIRMWARE.md, "rom.4's outside input"). External In Type is read by rom.4's
      EXT IN request `0x8935b050`, which moves no chip word (its event `0x893620e0` only stores it), now routeInput's on
      rom.4; and by `0x893a1000` into the tone generator's system words (+0x10203), whose STEREO sets the EXTIN
      converter's block +0xb0 (`0x8939ee30`): step 4.3's. USB External In Type is the system reader's (v259), its law on
      the chip checked; the unit gives the plugin's input to rom.4's analog pair alone, as rom.4 adds its USB pair on top.
      For step 4.3 and step 5: the part mixer reads frame words 1 and 5, and by v204's slot mapping word 1 is slot 0x92,
      which no input reaches on rom.4; which slot the mic comes back on wants a capture, and `returned()` hands the part
      nothing yet.
   4. done (v401): the Matrix's MFX-PRMs (HARDWARE_FIRMWARE.md, "rom.4's Matrix MFX-PRMs"). rom.4 sends them to the part
      mixer's levels through `0x893a2e50` (Direct Sound's and MFX Send's offsets and the consonants' level, ported in
      v209, v227, v247), not to the effects, so `setMatrix` doing nothing on rom.4 is rom.4's way: checked, no MFX type
      answering and the driver's object untouched. The MFX object's take and apply (`0x89361d50`, `0x89361de0`) have no
      caller.
   5. done (v402): the MFX's sends by CC 91 and 93 (HARDWARE_FIRMWARE.md, "rom.4's MFX sends by CC 91 and 93"). rom.4's
      driver's receiver (`0x89360b90`) keeps CC 91 and 93 (`0x893608a0`) and its event 0x10 (`0x89360930`) scales the
      MFX's Reverb and Chorus Sends by them, rom.4's own. `XtFxControl::controller(number, value)` runs both, checked
      against the receiver run by hand. For step 4.5: which of the host's controllers reach it is the plugin's to wire,
      as which MIDI the front hands the receiver is step 4.3's.
5. The plugin: the mode switch, rom.4's IDs as parameters appended, the session, a `.p02`'s load, the tab of cards, the
   plugin test, verify.sh lines and plants. Split in two (2026-11-21), the first half in its own number block, v450–v499,
   beside step 4.3 and the effects; the second needs step 4.3's Vc2Synth whole and the pages the user's look.
   The parameters' scope, the user's decisions (2026-11-21): a patch's zones 1 and 2 (the common, the effects' types
   and levels, Beam, Pad and the arpeggio, and each zone's tone, 13 envelopes, 5 LFOs and both COSMs by type; zones 3–16
   and the arpeggio pattern the patch's data, the effect types' own parameters edits by ID, as the V-Synth's), the
   system settings the V-Synth's are (the controller assigns, Master Level, Output Gain, Clock Source, External In
   Type), held apart in `VSynthEmuEditorTest` and counted until the VC-2 pages come, their IDs by rom.4's hex ID.
   1. done (v450): a `.p02` read by rom.4's own reader and its store by rom.4's pointing routines, `Vc2PatchFile` and
      `Vc2PatchStore`, against rom.4 in `xt_vc2patch` (HARDWARE_FIRMWARE.md, "rom.4's patch store and a .p02").
   2. done (v451): rom.4's agreed IDs, 1,332, as host parameters after the 763, by rom.4's names, ranges and text, the
      set fixed in the source and rom.4 brought in before the layout (HARDWARE_FIRMWARE.md, "rom.4's parameters as the
      plugin's").
   3. done (v452): a `.p02`'s load into them, `loadVc2Patch`, and the session's state for them, append-only: the
      parameters by the processor's state as every other, the patch's store and name beside them (HARDWARE_FIRMWARE.md,
      "A .p02 onto the VC-2's parameters"). With it the first half is done.
   4. The second half: the mode switch's audio, the wiring to Vc2Synth, the VC-2 pages and cards, the plugin test of a
      played block.
      - Before it, done (v550): the VC-2's firmware checks under AddressSanitizer, UBSan and libc++'s bounds
        (`XT_SANITIZE=1 build.sh`), the 21 step 4.3 isn't changing at 40 and 400 cases, three seeds: the voices' noise
        drawn as an XT's keys; the check rig's compare passed to step 4.3 (HARDWARE_FIRMWARE.md, "The VC-2 checks under
        the sanitizers"). Step 4.3's own three (osc, tone, synth) are its to run so.

   Open for the second half, found in step 4.3 (v349):
   - [gap] Part 1 Receive Switch off, among the system settings. rom.4's system defaults (`0x89328d60`, run by
     `0x89319e10`) set it on (ID 0x47 = 1); a user's own system setting turns it off. With it off, G's channel word is
     rom.4's `0x8999738c`, which nothing writes (0), so M's init on the static M (`0x893a2ee0`, on the update task's
     pass) gives M a channel state of 0, and the Matrix sources (`0x893a1df0`) read [0 + 0x84 + a byte]: low addresses,
     on an XT the boot flash. rom.0 isn't the bytes there (it starts with 0xff). The interpreter faults on them and the
     part reads zeros (its step fails on a read no region holds); xt_vc2synth's notes part draws Part 1 receiving always.
     Step 4.5 decides between starting the plugin with Receive on and finding the boot flash's bytes; the read isn't
     ported.
   - [x] A NaN sample to the host. In step 4.3's [deferred] state (the VOCODER's balance NaN), the carrier's noise
     gain is NaN on both sides, and `vc2Clamp01` passes a NaN through (both compares false). So the carrier's render
     (Vc2Carrier.cpp) gives a NaN noise scale, its noise low-pass state goes NaN and stays so until the carrier's next
     init (a note-on), and every sample the OSC gives is NaN, the saw's added to it; with them the tone's mix, COSMs
     and TVA, and the part's bus. Whatever rom.4 does, the plugin must never hand the host a NaN sample: step 4.5 puts
     a guard where the VC-2's audio leaves the part, and the plugin test holds it. Not fixed in v349. Closed by v454
     (the user's "ok, fix the NaN at the output", 2026-11-24): the guard is at the plugin's output rather than the
     part's, so it holds for the V-Synth's audio and the VC-2's alike. `OutputFinite` (PluginOutputFinite.h), made at
     processBlock's top, sets every sample that isn't a finite number to 0 on every way out; `VSynthEmuOutputTest`
     (in verify.sh) plays a float wave holding NaN and ±infinity with no image: 410 samples out weren't numbers
     before the guard, 5/5 checks after (HARDWARE_FIRMWARE.md, "The plugin's output always numbers"). When the
     VC-2's audio reaches processBlock (this step's second half), it passes the same guard; the plugin test of a
     played block then also holds it on the VC-2's own path.
   - [gap] The MIDI input's real-time bytes (v354; v355). v354 took `0x89313cb0` for their handler; it is the clock
     task's listener on the OS MIDI's hub, of the parameters' messages, and v355 ports it with the clock task's 0x40
     pass (`0x89313d50`, `0x89313e10`: Clock Source, Clock Out, the patch's tempo). With Clock Source MIDI the clock
     object goes on the MIDI IN ring (`0x899598a0`), whose listeners the input task tells each message it takes (its
     F8's the object's +0x40 path, ported in the ms but fed nothing; start, continue and stop the class's own, not
     traced); the port's parser drops the real-time bytes, SysEx and system common (v355), and the part's MIDI takes
     none. Step 4.5 decides whether the plugin's host transport stands in for them, as XtSynth's does for the V-Synth.
   - [gap] The MIDI out side (v355): USB-MIDI Thru (MIDI IN's messages to USB out instead of the part, USB's to MIDI
     out as well), the MIDI out task and its flush (cyclic record 8). Nothing drains USB out or MIDI out on the rig,
     so the whole part keeps Thru off; step 4.5 decides what the plugin does with Thru (most likely nothing: a host
     has no MIDI out of the part's).
   - [gap] The front (v355): the whole part's front is a stand-in on entry 12's TCB sending each message the check
     gives it as the front's are (`0x89314d60`); the front's own task (its panel, its patch loads) is step 4.5's.
   - [gap] A wait inside a pass, in the plugin (v355). On its OS model the notes' task's wait for a fade (0x1000, the
     TVA's render posting it in a later block) takes the part's next outside events until the fade ends: the update
     task's passes, the ticks and the blocks run meanwhile, as on an XT. The plugin hands the part a host block's
     events and input at once, so a wait reaching past them needs the next host block before the part can go on.
     Step 4.5 decides: run the part a host block behind the host (the plugin's latency), or let such a wait take
     the blocks the host block still holds and end the fade at the host block's end. The host's MIDI never makes the
     host wait (v355): each message goes, as its bytes, into MIDI IN's or USB's receive ring (2048 words,
     preallocated, as rom.4's row 15 builds it), and a full ring drops its backlog for the full marker, as rom.4's does
     (`0x89311e90`); the input task's wait for room is the task's own state, not the audio thread's.
   - [gap] A player's interpolated sample loads past its wave (rom.4's 46 sample-load pcs) read rom.4's own memory on
     an XT: its .text (e.g. 0xa93220b0) or .bss (e.g. 0xa9d5a518), as sample values. Factory patches reach it: 8 reads
     in the 2024 factory run, 32 in 31337's. The port plays 0. The .text part could match by reading rom.4's image at
     those addresses; the .bss part would need the part's memory as rom.4 lays it out. Not done: the check gives rom.4
     zeros there (v363; HARDWARE_FIRMWARE.md, "Sample loads past the waves"). The walk past the wave reaches unmapped
     memory too (physical 0x07959e08, 0x08704e30, 0x09085420, 0x0ca1e0e8; 31337's factory notes case 69 reads
     0xaca1e0e8), where an XT reads whatever is there, and the port plays 0 (v362). And a tone mix delay index driven
     far out (an OSC overflowing to ~−3e18) wraps in 32 bits past the rings into the free LFOs; the port plays 0 there
     (v364; HARDWARE_FIRMWARE.md, "The tone mix's delay index driven far out").
   - [gap] A LITE voice reused across kinds reads its loop record through a stale pointer (0x893c859a); on an XT that
     read lands in area 7's on-chip registers (P4), whose values the port doesn't model; the port reads 0. Reached by
     seed 3's noise draws (xt_vc2synth's notes case 28, through 0x3ea7ffcb), by no factory patch in the runs at 2024, 7
     and 31337. xt_vc2synth gives rom.4 a zero record there and counts both sides (v362).

### 5. Against the machine

Recordings of the user's XT in VC-2 mode with a known signal into the mic input: for what the firmware can't show (the input's
levels, the effect chip).

Prepared 2026-11-22 (tools and the guide, no number): RECORDING_GUIDE.md, "VC-2 captures", for the user at the XT. Nothing is
recorded yet, and none of it has been tried on an XT. **What the recordings settle**, each with where it was asked and the takes
that answer it (`vc2takes.py`, `vc2vsynth.py`; every take's settings in the panel's words in `vc2_takes.csv`):

1. **The input's level into the VC-2.** Asked here (above: "the input's levels"), and by VC2_VOCAL_DESIGNER.md, "The part mixer,
   run" (the direct sound at Direct Sound 127 is 0.025 of the frame's full scale, its target ÷127, eased 3% a block) and
   HARDWARE_FIRMWARE.md, "rom.4's frame through the unit" (rom.4's frame at a gain of 1, "nothing here is checked against one").
   Run 1's `main`, `direct_64`, `direct_16`, `master_100`, `gain_plus6`, and the input's 1 kHz steps, which say where the input
   clips. Holds the plugin's input scale into the mic word and the part mixer's Direct Sound law to the XT, in dB against the
   input (in volts too, if the interface's output level is known: question d-4).
2. **Which slot the mic comes back on.** Asked at step 4.4.3 above ("which slot the mic comes back on wants a capture, and
   `returned()` hands the part nothing yet") and HARDWARE_FIRMWARE.md, "rom.4's outside input" (by v204's mapping the part
   mixer's word 1 is slot 0x92, which no input reaches). Run 1's direct sound against the input's left-only and right-only clicks;
   the EXT IN tone at each External In Type (`extin_*`); and V-Synth mode's EXT IN OSC at each External In Type (`vc2_vsynth`),
   held to v204's mapping, as that section proposes. Holds `XtFxUnit::returned()` on rom.4, and v204's mapping on rom.2.
3. **The EXTIN flag.** HARDWARE_FIRMWARE.md, "The part mixer" (v227: zone 0's or 1's OSC 1 Type, or OSC 2's too, by how
   `0x89320a50` reads its arguments; unsettled). Run 1's `direct_pcm` and `direct_osc2`.
4. **The tracker's input and Auto Note.** VC2_VOCAL_DESIGNER.md, "Open" (which inputs the frame's words at +4, kind 6's ring,
   and +0x14, the tracker, are) and "Kind 6's pitch source, run" (Auto Note's mode 1 against 2 in the tone generator, read and not
   run). Run 1's `autonote` and `autonote_retrig` on the input's sides (a vowel at 220 Hz left, 330 Hz right) and its pitch
   staircase (with steps 30 and 50 cents off a note), and `extin_key67` (the key's move, kind 6's P).
5. **The vocoder engines and the Direct Sound at known settings.** Steps 3 and 4 above hold HIRESO and LEGACY to rom.4 bit for
   bit; VC2_VOCAL_DESIGNER.md, "Open", leaves the engines' delays against HIRESO's 128-sample input latency and the vocoder's
   output scale. Run 2: each engine at its defaults and repeated, then each of Formant Shift, Formant Template, Vocoder Depth and
   Sound Level, Unvoice Level and Detect, Env Attack and Release, Stereo Process, Unvoice Spread, Direct Sound and the key against
   its base. Each of its takes is marked: Direct Sound Level 127 for the input's first 11.5 s (its clicks and sweep), then the
   take's own, and the key down only then, so every take is placed by its own clicks and the vocoder is measured on the input's
   voice, from 17.5 s. Holds Vc2Synth's vocoder path end to end, through the real input and chip, once step 4.3 renders it.
6. **The carrier.** VC2_VOCAL_DESIGNER.md, "The VOCODER OSC's IDs, run" (left: the Dynamics' law, which reads the input's
   peak). Run 2's `carrier_analog`, `carrier_vocosc`, and `carrier_vocosc_quiet` (the same with the input silent).
7. **rom.4's program A.** VC2_VOCAL_DESIGNER.md, "Program A's routing" (rewritten: the system EQ's defaults, the patch's EQ 1 and
   EQ 2 on MULTI alone, the Reverb's stereo input) and HARDWARE_FIRMWARE.md, "rom.4's frame through the unit". Run 3's `main`
   (the EXT IN tone through MFX Thru), `feed_off`, `sys_eq_off`, `peq1_plus15`, `peq2_plus15`, `direct`, `peq1_direct`. DIR
   isn't recorded.
8. **rom.4's 41 MFX.** VC2_VOCAL_DESIGNER.md, "The effect chip's programs" (rom.2's programs with their registers renumbered;
   "not run: rom.4's MFX types loading their programs"). Run 3's `mfx01`…`mfx41` at their defaults, against `XtFxChip` on
   rom.4's programs.
9. **The Chorus and the seven stereo reverbs.** VC2_VOCAL_DESIGNER.md, "The new reverbs on XtFxChip" ("that the VC-2 sounds so
   is not checked against a real one"; Room Size, Repeat Time, the pre-EQ and the Pre Delay's law not measured) and "The Chorus's
   sides" (the other Chorus types). Run 4's `cho01`…`cho08` and `rev01`…`rev14` at their defaults, wet.

**The tools** (`research/tools/`): `vc2capture.py` writes the input signal (`vc2probe.py`: fxprobe's 16.8 s, then a synthetic
voice), the runs and the take list into `analysis/test_files/vc2/` (gitignored: the runs hold rom.4's typed defaults); fxplay
plays a run's input at its cues beside its MIDI (`--audio`, `fxaudio.h`) and prints what the XT sends (`--in`, `fxmidiin.h`);
`vc2cut.py` checks a recording and cuts it, on the shared parts it now has with `fxcut.py` (`caprec.py`, `capbase.py`,
`capclick.py`, `caplayout.py`, `capmeasure.py`, `cappath.py`, `capreport.py`); `vc2cut_test.py` holds it to a simulated XT
(`vc2sim.py`, on `capsim.py`) with faults planted; `fxplay_test.sh` holds fxplay's input to the take list's own.

**The address map is inferred** (`vc2map.py`): the V-Synth's map on rom.4's own IDs. Nothing in rom.4 that receives or sends
SysEx has been read. Block by block, rom.4's IDs from each block's first are rom.2's name for name: the system's 72 but the two
rom.4 gave to MIDI MODE for VariOS and USB External In Type, the common's 114 with the vocoder block appended at +0x73, the MFX,
Chorus and Reverb heads, zone 1's 128. The guide's step "Learn the map" asks the XT itself (Transmit Edit Data, `fxplay --in`,
`vc2capture.py --learn`) before any long run. The VOCODER OSC's own IDs, past a zone's 128, aren't written.

Not done: the recordings (the user's); the comparison against Vc2Synth's renders (needs step 4.3's Vc2Synth and step 4.5's
second half); the comparison against `XtFxChip` on rom.4's programs (needs a recording made at 44.1 kHz on an exact path).

**The user's answers so far** (2026-11-22): the interface is a **Ferrofish Pulse 16, over MADI** to the Mac, and the XT can be
**wired back to back** to it, direct inputs and outputs. So d-1 is answered; d-2 is the rear INPUT jacks from the Pulse's
analog outs; d-3 is analog, the Pulse's converters both ways (its optical ports are ADAT only, so the XT's digital outs aren't
used); d-4 is the Pulse's own level setting (−8 to +20 dBu at 0 dBFS in 1 dB steps; recommended +8 dBu out, the
sweep at the XT's −10 dBu nominal, +20 dBu in), written down. With 16 inputs, DIRECT OUT and a loopback of the signal can be
recorded too. d-5: **a MIDI interface**, both ways. RECORDING_GUIDE.md, "VC-2 captures", step 1, has the wiring. All five
answered. With no digital path, the comparison against `XtFxChip` sample by sample waits on USB audio from the XT, if a
current Mac sees it; the analog recordings settle the rest.

The questions as asked, each with the recommendation:
- **d-1. The audio interface**: its name, its line outputs and inputs, whether it has an S/PDIF (coaxial or optical) input,
  and whether it runs at 44.1 kHz. Recommended: two line outputs into the XT's INPUT jacks and two inputs back, at 44.1 kHz and
  24 bits; with an S/PDIF input, the XT's COAXIAL or OPTICAL OUT recorded there as well.
- **d-2. How the input is fed**: the rear INPUT jacks, L and R, from the interface's line outputs (recommended: both sides at a
  fixed gain, INPUT Jack Gain 0 dB, and the slot question needs L and R apart); the front MIC jack (one side, at the INPUT
  knob's unmarked gain); or USB audio from the Mac (an exact level, if a current Mac sees the XT as an audio device at all).
- **d-3. How the XT is recorded**: USB, the digital outs, or analog MAIN OUT. Recommended: what the effects captures use. Analog
  is enough for runs 1 and 2 (levels, sides, pitches, the vocoder's settings); runs 3 and 4 want USB or digital to be held
  sample by sample to `XtFxChip`.
- **d-4. The input's level in volts**: the interface's output at 0 dBFS, in dBu (its spec, or a fixed line-level setting).
  Recommended: a fixed or marked output level, written down; without it the input's level is known only against the input.
- **d-5. MIDI both ways**: USB-MIDI, or a MIDI interface's IN and OUT. Recommended: USB-MIDI if the Mac sees the XT, else a MIDI
  interface both ways: the map check (the guide's step 4) needs the XT's own messages.
