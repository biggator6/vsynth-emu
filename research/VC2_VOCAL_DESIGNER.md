# The VC-2 "Vocal Designer": rom.4, first map by its own names (2026-11-12)

The XT runs three applications on one OS (`rom.1`): the V-Synth (`rom.2`), the VC-1 "D-50" (`rom.3`) and the VC-2 "Vocal
Designer" (`rom.4`, header `VAPP2 0.17`, 9.6 MB). The [V-CARD] button switches between them without a restart (XT owner's manual,
"Starting up V-Card"). This file is the VC-2's, kept apart from HARDWARE_FIRMWARE.md because nothing in it touches the V-Synth's
ports. ~~Names-first pass only: no routine has been disassembled yet.~~ That was the first pass; the second and third read the
code ("The vocoder's code"), and the fourth ran parts of it ("rom.4 run in the interpreter").

Every address below is a CPU address in rom.4's own map unless it says "file offset". Every name and range here comes from one
source, rom.4's own tables `[FW]`. The second source, the "V-Card owner's manual" that the XT manual defers to for the VC-2, is not
in the repo (see "Open").

## Sources

- `firmware/unpacked/V-SynthXT.rom.4.bin` (gitignored). It loads at **0x89300000**, like rom.2: both start
  `mov.l @(disp,pc),r0; jmp @r0` to 0x89300040. Its header words differ from rom.2's (0x8994fc20, 0x89add9a8, 0x8a1ea1c0,
  0x8a1f11c4 against rom.2's 0x8987cb00, 0x8a015eb0, …), so its sections are its own; its .data/.bss reach past 0x8a1f0000, where
  rom.2's stop below 0x8a020000. Its C startup copies .data from ROM [0x8990c758, 0x8992e1bc) to RAM [0x8992e1bc, 0x8994fc20)
  and zeroes .bss [0x8994fc20, 0x8a1f11c4): the rule `Sh4Interp` uses for rom.2 (the source ends where the copy begins), which
  for rom.4 lands on its header's own word 0x8990c758. So a table at a CPU address in 0x8992e1bc…0x8994fc20 is read at that
  address − 0x21a64 in the file; read in place it is something else. ~~`Sh4Interp::loadRom` and `build/tools/bin/rd` hold
  rom.2's map, so they can't run or read rom.4 as they stand~~ (from v197 `Sh4::loadRom` lays out any image by this header,
  rom.2's map unchanged: HARDWARE_FIRMWARE.md, "A firmware image's own header"); `ramrd.py` reads it ("The tools"), and
  `vc2rom.*`'s `Machine` loads it through `Sh4::loadRom` for the research runs ("rom.4 run in the interpreter").
- The disk image's `Internal.vc2` folder (FAT16 partition), and a preset wave library after the V-Synth's preset waves (below).
- `research/V-SYNTH_XT_OM.txt`: the VC-2's file names (fig. 07-06) and its one-line description. Nothing on its parameters.

## The tools

In `research/tools/`, run as the others are (`.venv/bin/python3 research/tools/<tool>`, not `-I`, which hides `romfile.py`
from them); `ROM=firmware/unpacked/V-SynthXT.rom.4.bin` points the image ones at rom.4.

- `romfile.py`'s `ram32`, `ramf32`: a word as the CPU sees it after C startup, by the image's own header (.data through its
  copy, .bss 0). `ramrd.py <addr>[:n] …` prints them. Checked on rom.2 against `build/tools/bin/rd`, which runs the startup in
  the interpreter: three nonzero .data words, at its start, middle and end, the same.
- `idbuild.py <lo> <hi> [from] [to]`: where a register comes to hold a value in [lo, hi], following the constant building the
  firmware uses for parameter IDs. How the converter and the OSC dispatch were found.
- `vc2params.py <lo> <hi>` (rom.4 only): an ID's name, descriptor and value text from rom.4's own tables.
- `diskls.py [filter]`, `diskls.py get <path> <out>`: the disk image's FAT16 files (`$DISK` for another image); importable
  (`all_files`, `read`) since `wlflib.py`.
- `wlflib.py`: the disk image's two `WLF08` wave indexes read whole and held to their link files and samples, the VC-2's to
  its sample bank; exits 1 on a difference ("The wave libraries, read").
- `likeat.py <A> <addr> <n> <B>`: code in B shaped like A's at addr, with what moves when code moves masked out.
- `vc2run <rom.4> <run>` (C++, build line at the top of `vc2run.cpp`; run through `.claude/tools/buildq.mjs`): rom.4's
  routines in `Sh4Interp`, on `vc2rom.*`'s map; runs `boot`, `warp`, `ratio`, `hold`, `growl`, `kind6`, `track`, one file
  each ("rom.4 run in the interpreter"). `warp check` and `growl` hold a model of a routine (`vc2warplaw.cpp`,
  `vc2growllaw.cpp`) to the firmware bit for bit, and fail when any word differs (v221: `growl`'s model went into the
  plugin as `Vc2Carrier`, held by xt_vc2carrier, and `growl` now prints the dividers alone; v213: `warp check` and its model went
  into the plugin, `vc2HiresoMapOf` held to slot 4 by xt_vc2hireso; `vc2run warp` keeps `diff` and `map`); `ratio` fails past 2 ulp from powf; `kind6`
  fails when an output's period is more than a sample from the reading's; `track` (`vc2track.cpp`, its settings and state
  section `vc2trackstate.cpp`, sharing `vc2track.h`) measures the pitch tracker against no model and fails only on a fault
  ("The pitch tracker, run", "The pitch tracker's analysis"); `vocosc` (`vc2vocosc.cpp`) follows the VOCODER OSC's IDs
  through the builder, the converter and the carrier and fails on a fault or a store write its reader doesn't read back
  ("The VOCODER OSC's IDs, run"). `patches` shares its store helpers through `vc2patches.h`. `autonote`
  (`vc2autonote.cpp`, sharing `track`'s tones through `vc2track.h`) runs kind 6's mode and Auto Note on sung tones, with the
  tone generator's calls answered and logged, and fails only on a fault ("Kind 6's pitch source, run"). `mixer`
  (`vc2mixer.cpp`) runs the part's level routine and its live block mixer on known input frames, the vocoder's block call
  answered, and fails on a fault or a store write its reader doesn't read back ("The part mixer, run"); `vc2patches.h`'s
  `setStoreValue` is the store writer `vocosc` and `mixer` share, `vc2runs.h`'s `initVocoderModule` the init `autonote` and
  `mixer` share. `typed <file.p02>…` (`vc2typed.cpp`) reads the MFX, Chorus, Reverb and COSM blocks through the second bank
  by rom.4's own readers, with `vc2patches.h`'s `p02Data` and `spreadOf` shared with `patches` ("The second descriptor bank,
  read"). `fxprog <rom.2>` (`vc2fxprog.cpp`) runs rom.4's reverb types and holds each program they load to rom.2's ("The
  effect chip's programs"); `vc2runs.h`'s `fileContents` is the file reader it and `p02Data` share, and its `answerKernel`
  the kernel's stubs answered. `fxroute <rom.2> [scan|flags]` (`vc2fxroute.cpp`) maps where program A takes each input on
  both images, on `XtFxChip` ("Program A's routing"); `vc2fxdriver.h` is rom.4's effect driver on an `XtFxChip`, with
  the chip's side of a measurement (`silence`, `settled`, `tracesOf`) and the Reverb Types' names (`reverbTypeName`), and
  `vc2patches.h` has the second bank's `typedDescriptorOf`. `fxreverb [envelope <type> [time] | damping]`
  (`vc2fxreverb.cpp`) measures the Reverb Types on `XtFxChip` ("The new reverbs on XtFxChip"), its decay by `vc2decay.h`;
  `vc2runs.h`'s `intArg` reads a run's number arguments. `fxchorus <rom.2> [lfo]` (`vc2fxchorus.cpp`,
  `vc2fxchorusphase.cpp`, sharing `vc2fxchorus.h`) holds rom.4's Chorus to rom.2's, and with `lfo` its LFO's phase on
  both images and whether a reload sets it ("The Chorus's sides"); `vc2fxdriver.h`'s `rom2With` and `loadRom2Fx` set up
  rom.2's effects through `XtFxUnit` for it and `fxroute`, and its `runOn` runs a chip on in silence. `splits <index>` (`vc2split.cpp`) runs rom.4's wave index parser
  and its split readers at every velocity and key ("A VC-2 wave's split, chosen"); `wlflib.py get VC2 <out>` writes the
  index it reads, and `wlflib.py choices VC2` what the rule gives, to diff with it. `splitbytes <index>`
  (`vc2splitbytes.cpp`, its laws in `vc2splitlaws.cpp`) follows the bytes a split passes on into the PCM converter ("The
  split's bytes, followed"). Shared by them: `vc2index.h`, the index read by rom.4's parser and its readers over every
  wave, velocity and key; `vc2oscinput.h`, the default patch's store, the OSC input builder's record and the note input
  the dispatch and converter take (also `vocosc`'s); and `vc2watch.h`, a read-watch: every load in a stretch of code
  hooked, and each read of a window of memory kept by the instruction that made it. `bank <index> <bank>` (`vc2bank.cpp`)
  runs rom.4's bank loader and start-up load and finds each sample's stream by rom.4's own lookup ("The sample bank,
  loaded"); `wlflib.py bank VC2 <out>` writes the bank it reads, and `wlflib.py samples VC2` the same from its own
  reading (the bank's table in `wlfbank.py`), to diff with it. `vc2index.h`'s `hookFiles` answers the file calls from
  several files, one of them read as memory. `wavein <rom.2> <index> <bank>` (`vc2wavein.cpp`) runs the start-up load
  with rom.4's wave reader and store for real and holds what the store keeps of each sample to the plugin's reading
  (`vc2wavecmp.h`: `xtWaveRead`, whole RDAC blocks, `xtMbBuildBacking`) ("The wave reader, run"); `vc2bankload.h` is the
  start-up load both it and `bank` run. `cosmgrowl` (`vc2cosmgrowl.cpp`) shows COSM type 8's laws, the Growl's
  ("COSM type 8, the Growl, run"); its model, which it held to the converter, update and DSP bit for bit, went into
  the plugin at v234 (`Vc2CosmGrowl`, held to rom.4's routines by `xt_vc2cosm`).
- `romtables.py <rom.2> <rom.4> [--window N] [addr …]`: rom.2's tables found in rom.4 by their bytes, with the code in each
  image that loads them; with no addresses, every address constant in plugin/Source's ports. `poolrefs.py`'s search is
  `loads(img, value)` for it, and `romfile.py`'s `image`, `sections` and `offsets` take any image (both tools' old output
  checked unchanged).
- `codepairs.py <rom.2> <rom.4> [addr …]`: rom.2's routines found in rom.4 by the shape of their code, with the distance
  each moved; with no addresses, every rom.2 code address in plugin/Source (an address given outside rom.2's code,
  0x89300000…0x89540000, is said and skipped). `--pools <rom.2> <rom.4> <a2> <a4>`: for one pair, the literals that differ,
  paired by their place in the pools (so a pool word used twice reads as the first use; "The analog OSC in rom.4").
  `--callers <rom.2> <rom.4> <a2> …`: a routine rom.4 compiled anew, through the literals its callers load. `likeat.py`'s
  `shape`, `shapes` and `find` are importable for it (its output checked unchanged).
- `codealign.py <rom.2> <rom.4> <lo2> <hi2> <lo4> <hi4>`: a region's routines lined up by order and size, for code rom.4
  compiled anew; `--near <rom.2> <rom.4> <distance> <a2> …`: the longest alike run near where a routine should be;
  `--similar <rom.2> <rom.4> <a2> <a4> <n2> <n4>`: the share of two routines' instructions alike in order (difflib's ratio
  over their shapes), its scale in "The analog OSC in rom.4"; `--fields <rom.2> <rom.4> <a2> <a4> <n2> <n4>`: the same with
  field offsets, immediates and literals masked too, and the values that differ where the two line up, counted, with where
  each first is: an object's field map read from the code ("The VariPhrase players and key assign in rom.4").
  `romfile.option` runs a tool's `--option`s and `romfile.images` reads a two-image tool's arguments (codepairs.py's and
  codealign.py's).
- `vc2run <rom.4> patches [--stores <dir>] <file.p02> …` (`vc2patches.cpp`): the VC-2's patches read by rom.4's own routines,
  section by section, with each ID's spread over the files; `--stores` writes the stores ("The factory patches, read by
  rom.4").
- `vc2run <rom.4> <block> <rom.2>` (`lfo`, `env`, `cosm`, `osc`, `analog`, `band`, `solo`, `lite`, `tva`): a block's rom.2
  routines and their rom.4 pairs on two interpreters, its object compared after every call, and the port held beside rom.4's where
  one is linked (`vc2twin.*`, a run per block, `vc2twin<block>.cpp`; `lfo` and `env` link their ports from plugin/Source,
  the rest none; `band`, `solo` and `lite` share `vc2twinplay.*`, and `band` and `solo` link XtFirmware for rom.2's
  encoders and band load). An object rom.4 lays out otherwise is copied and compared by segments (`Twin::copyMapped`,
  `differsMapped`); every call starts both machines' FPU registers at 0. Fails on any difference: `cosm` does, at the Comb
  ("COSM in rom.4"), `osc` at the pitch ramp ("The OSC block in rom.4"), `solo` under Robot Voice, `lite` walking
  backward, and `tva` at its ramp ("The TVA in rom.4").

## The V-Synth's tables and code in rom.4 (2026-11-13, the VC-2 mode's step 1a)

The first step of bringing the VC-2 in as its own mode (`research/VC2_MODE_PLAN.md`): which of the V-Synth's firmware the VC-2
shares. `romtables.py` over the 156 address constants of the ports (output in `build/tools/vc2/romtables.txt`): 82 have a
copy in rom.4, 45 none, 7 too plain to search (zeros, a ramp) and 22 are .bss. Where a table is copied, the code loading it in
each image is listed, so each block below has its counterpart routine in rom.4 by the table it reads, not by its code.

**The tone's tables are all there, byte for byte, with their neighbours** (the equal run goes on past each table, often for
kilobytes), each loaded from as many places in rom.4 as in rom.2 or close:

| Block | Tables (port) | A loader in rom.2 → rom.4 |
|---|---|---|
| Envelopes | time, sustain, attack, shape, clocks, depth (`XtEnvelope`) | `0x894df7b8` → `0x8939a978` (time), `0x894ef808` → `0x893ab898` (attack) |
| LFOs | rate, sine (.data), clocks (`XtLfo`) | `0x894e02dc` → `0x8939b5d2`, `0x894f1152` → `0x893ad202` |
| OSC, pitch, TVA | exp tables, velocity sensitivity, level, pan, the pad's angles, Matrix values (`XtOsc`) | `0x894ec09c` → `0x893a834c`, `0x894ebd90` → `0x893a7fe0`, `0x894e6aa2` → `0x893a1df2` |
| COSM | cutoff, cap, octaves, resonance, sine, Hilbert, waveshaper, speaker, Lo-Fi, limiter (`XtCosm`) | `0x894ec164` → `0x893a8414`, `0x894ffa2e` → `0x893bd36e`, `0x8950131c` → `0x893bec5c` |
| Analog OSC | tone key follow, super saw detune, the HQ tables (`XtAnalog`) | `0x89505a56` → `0x893c24f8`, `0x894e50b6` → `0x893a3c12` |
| Band players | tree, final, oscillator constants, shifts, masks, offsets, cosine, kernel (`XtMultiBandPlayer`) | `0x8950c668` → `0x893c8d88`, `0x8951018e` → `0x893cc94e` |
| Rate converters | the 2× and 4× decimators and interpolators | `0x89522b52` → `0x893df3d2` |

The descriptors' first bank is at `0x8950f0b0` (found before by its first 48 entries), and rom.2's parameter getter
`0x8947d310` is `0x89324350` in rom.4, its first 42 bytes equal: the getter already named in "How rom.4 reads a parameter".

**Some code is a copy.** Where a constant is a routine's address, its bytes were looked for too, and these are in rom.4 from
their first byte for the run given: the SOLO encoder and its setup (`0x8951e120` → `0x893da9a0`, 368 bytes; `0x8951dc40` →
`0x893da4c0`; the import's `0x89516400`, `0x895164c0`, `0x8952dec0` and on), the clock object's routines (`0x894710e0` →
`0x89313000`, 484 bytes; `0x894710b0`, `0x894712f0`, `0x89471040`), the OS's allocator and UI calls (`0x89527910` →
`0x893e4190`, `0x895113a0` → `0x893cdc20`), the reverb time routine (`0x894b2bf0` → `0x893663a0`, 592 bytes), and parts of the
arpeggio (its event set and clear, note-off, stop, shuffle, tick and pedal check). ~~The tone's routines are not
("The vocoder's code"): the same tables, compiled anew.~~ Withdrawn the same day: matched by the shape of their code rather
than their bytes (pool displacements and branch offsets masked), most of the tone's routines are in rom.4, moved; the
converters that read parameters by ID are not ("The V-Synth's routines in rom.4, run"). Whether they compute the same is step
1b, by running them.

## The V-Synth's routines in rom.4, run (2026-11-13, the VC-2 mode's step 1b)

**Pairs by code shape.** `codepairs.py` looks for each rom.2 routine named in plugin/Source (982 code addresses) in rom.4 by
the shape of its first instructions up to its return (`likeat.py`'s shape: what moves when code moves masked out; output in
`build/tools/vc2/codepairs.txt`): 416 are found once, 76 several times, 477 not, and 11 are too short to look for. The
distances (rom.2 − rom.4) of those found once cluster in a narrow band, 0x1426c0…0x144cb0 (the most common 0x143920,
0x143780, 0x143f50, 0x1426c0, 0x143e90): rom.2's tone code is in rom.4 in large pieces moved whole, with code inserted between
them. The misses are of two kinds: addresses inside routines (hook points, branches written in comments), and the routines
that read parameters by ID (the COSM types' converters `0x894dc8b0`…, the getter `0x8947d310` past its first 42 bytes), which
rom.4's own IDs ("The parameter tables") change. `codepairs.py --pools` lists, for one pair, the literals that differ: the
globals, callees and tables that moved, as the routine itself sees them.

**Run side by side.** `vc2run <rom.4> <block> <rom.2>` runs a block's rom.2 routines and their rom.4 pairs on two
interpreters with the same arguments and memory (`vc2twin.*`: both have the harness RAM at one address), compares the block's
whole object between them after every call, and holds the port beside rom.4's as its own check holds it beside rom.2's. A word
that holds a moved address in each image (a vtable, an installed routine) counts as the same when the pair is one `--pools`
or `codepairs.py` gives.

| Block | rom.2 → rom.4 | Run | Result |
|---|---|---|---|
| LFO (`lfo`) | constructor `0x894f0b40` → `0x893acbf0`, note-on `0x894f0cf0` → `0x893acda0`, tick `0x894f1050` → `0x893ad100`, note-off `0x894f0810` → `0x893ac8c0`, clock `0x894f0fd0` → `0x893ad080` (all 0x143f50 apart) | 20 cases: every wave, rates 0…127, note values with moving clocks, offsets, delay and fade modes, note-offs | 13,920 states, 0 different; the port equal to rom.4 throughout |
| Envelope (`env`) | constructor `0x894efdb0` → `0x893abe40`, note-on `0x894f0130` → `0x893ac1c0`, tick `0x894efdf0` → `0x893abe80`, note-off `0x894f03e0` → `0x893ac470` (0x143f70 apart) | 10 cases: segments fast to slow, sustain at both ends, both curves, a release from each stage | 11,270 states, 0 different; the port equal to rom.4 |
| Envelope's converters (`env`) | Time KF scale `0x894df760` → `0x8939a920`, time `0x894df7b0` → `0x8939a970`, pitch depth `0x894e0ce0` → `0x8939c1a0` | xt_env conv's ranges | 1,051,135 returns, 0 different |

What moved, by `--pools` and the runs: the LFO's chaos seed `0x89f91c48` → `0x89999668`; its object's +0x24, `0x89833ef8` →
`0x898cd490`; the envelope's installed output routine (+0x64), curve 2's `0x894efca0` → `0x893abd30` and curve 0's
`0x894ef7d0` → `0x893ab860`; the envelope's global `0x89f91c44`, which rom.2's constructor and note-off share, is
`0x8999965c` in rom.4's constructor and `0x89999660` in its note-off (not yet read why).

Each run was seen red: the LFO with rom.4's chaos seed written a word off (the images part at the output, +0x38) and with the
port skipping a tick (the port parts from rom.4 at that tick); the converters with rom.4's other time converter in place of
its pair (782,308 of 1,051,135 returns part).

**rom.4 has a second time converter.** rom.2's `0x894df7b0` has two look-alikes in rom.4: `0x8939a970`, which reads the same
time table (`0x8953fb90`, rom.2's `0x897cec24`), and `0x8939aa40`, which reads a table of its own 0x100 bytes after it
(`0x8953fc90`, in 0x120 bytes rom.2 doesn't have before its LFO rate table). What reads through `0x8939aa40` is not traced;
it is not the V-Synth's envelope time.

**So far: the LFO and the envelope (and its converters) of the VC-2 are the V-Synth's, and the ports serve it as they stand;
COSM's voice side is too for 15 of its 16 types (below: note-on, update, new block and every DSP routine, mono and stereo), its converters and the Comb are not; the OSC block's arithmetic
and key state are, its wiring and portamento are not; the analog OSC's waveforms are, for the 11 the VC-2 has (no SUPER-SAW,
no FEEDBACK-OSC), its note-on, install, update and converter are not; BACKING's, ENSEMBLE's and SOLO's players are, given
rom.4's object layout, but for SOLO under Robot Voice; LITE's is walking forward, and walking backward but for its read the
right way up; key assign is; Vari off is not; the TVA's render is the V-Synth's gains on one stereo pair, with two changes
in its ramp, and no buses, sends or glide.** Not run: Vari off (rewritten; read from the code), kind 1's mode 1, the
period-jump player (type 8).

### COSM in rom.4 (2026-11-13)

xt_cosm's 77 addresses (`build/tools/vc2/cosm_pairs.txt`) part three ways.

- **The DSP is copied.** The filter and the types' routines are found by their shape, most twice in each image (rom.2 holds
  two copies too, 0xf80 apart: the TVF's `0x89502890` and `0x89503810`), at the common distance 0x1426c0: the TVF
  `0x89502890` → `0x893c01d0`, the Dynamic TVF `0x89502920` → `0x893c0260`, and on.
- **The voice side is recompiled, mostly to the byte.** Listing the routines' starts in each image (16-aligned, a prologue
  after nop padding) and lining them up by order and size, rom.2's 0x894ecce0…0x894edf70 and rom.4's 0x893a8e50…0x893aa0e0
  run one for one with equal sizes (the block targets `0x894ed8f0` → `0x893a9a60`, 266 instructions alike; the Dynamic
  TVF's `0x894eda10` → `0x893a9b80`; new blocks `0x894ed390` → `0x893a9500`; the slot finder `0x894ec2b0` → `0x893a8560`;
  the start values `0x894ec340` → `0x893a85f0`, by `codepairs.py --callers`). Two differ in size: **the note-on**
  (`0x894edf70` → `0x893aa0e0`, 0x730 against 0x6f0 bytes) **and the 10 ms update** (`0x894ee6a0` → `0x893aa7d0`, 0x8d0
  against 0x810). Their code is the same instructions scheduled otherwise (a `mov #-84` moved up, a `shll8` down), so a shape
  match can't find them; their order and size can.
- **The converters are rewired.** rom.4's dispatcher `0x89399920` has rom.2's (`0x894de3b0`) switch, the same 15 cases in the
  same order, 7 by literal and 8 by `bsr`: rom.2's `0x894dc8b0`, `0x894dcbf0`, `0x894dcd80`, `0x894dcf00`, `0x894dd0a0`,
  `0x894dd2c0`, `0x894dd4f0`, `0x894dd690`, `0x894dd870`, `0x894dda40`, `0x894ddc50`, `0x894ddd20`, `0x894ddde0`,
  `0x894ddf80`, `0x894de110` are rom.4's `0x89397980`, `0x89397cf0`, `0x89397eb0`, `0x89398080`, `0x89398260`, `0x893984e0`,
  `0x89398780`, `0x893988a0`, `0x89398ae0`, `0x89398d40`, `0x89398fd0`, `0x893990d0`, `0x893991d0`, `0x893993f0`,
  `0x89399620`. Before its switch rom.2's dispatcher calls `0x894ddf40`, rom.4's nothing. And the converters' input block is
  laid out otherwise: rom.2's first converter reads its words from +0x12, +0x14, … and adds the word at +0x40 (the Multi Step
  Modulator's term, v193) to its field; rom.4's reads the same words from +0x10, +0x12, …, adds no such term, and where rom.2
  has a velocity float at +0x38 it takes a pointer and calls `0x893a2960` with it (which reads the object's +0x1c and +0x94:
  by its shape the Matrix read in the converter, where rom.2 is given it). So the VC-2's COSM converters take another input:
  they are to be ported from rom.4 with the converters that read by ID, and compared once their input path is read.
  (Read and ported from v234 on: the input whole by its builder `0x89399b10`, the dispatcher, the Matrix's COSM
  readers `0x893a2960`/`0x893a2aa0` and getter `0x893a2350` on M, which is the note's Matrix object, the Growl's and
  the TVF's converters; HARDWARE_FIRMWARE.md, "The VC-2's COSM converters' frame". v237: the Amp Simulator's, Dual
  Filter's, Dynamic TVF's, Compressor's, Limiter's and TB Filter's, "The VC-2's COSM converters, the first batch"; the
  seven left reach rom.4's powf and logf. v239: OD/Dist's, the Wave Shaper's, the Resonator's, the Frequency
  Shifter's and Lo-Fi's, "The VC-2's COSM converters on rom.4's libm". v241: the two Side Band Filters', the last, "The
  VC-2's COSM Side Band Filters", with rom.4's whole map of the converters.)

**The DSP routines by block code** (all 21 codes, mono and stereo) are in a table in each image's .data, the chooser
`0x894eef70`'s: rom.2's at `0x8987c7ec` and `0x8987c840`, rom.4's at `0x8993fb50` and `0x8993fba4` (its image's `0x8991e0ec`
and `0x8991e140`, found where the words hold its OD routine's address). Each of rom.4's is rom.2's at 0x1426c0, 0x142c40 or
0x142c10, but DS's mono (`0x894fc570` → `0x893b9ec0`, 0x1426b0) and the Comb's: rom.4 has **one Comb routine for mono and
stereo** (`0x893b9b40`), where rom.2 has two (`0x894fa8d0`, `0x894faa00`). Output in `build/tools/vc2/cosm_tables2.txt` and
`cosm_tables4.txt`.

**Run (`vc2run <rom.4> cosm <rom.2>`, `vc2twincosm.cpp`).** No port is linked: xt_cosm holds `XtCosm` to rom.2, so rom.2
equal to rom.4 is the port equal to rom.4. For each of the 16 types (the 15 the dispatcher's switch calls and the Speaker
Simulator's own converter `0x894dcef0`, which makes its block as the dispatcher's inline case does; nothing calls it, in
either image: HARDWARE_FIRMWARE.md, "The VC-2's COSM converters, the first batch"), 100 notes mono and 100 stereo: a block made by rom.2's converter on an input whose
words are the type's parameters, each drawn in its descriptor's range (the second bank, `0x897a56d8`), and given to both
images; the note-on with the slot finder hooked in each to hand out four buffers holding a last note's junk; then 40 blocks
of the type's DSP routine, each image's from its own table, on 32 frames at one of xt_cosm's levels (30,000 to 2.1e9, and 1),
with an update (new envelope, LFO and pitch values, the pitch in fr4 as well) twice in 14 blocks and a new block
(`0x894ee4a0` → `0x893aa5d0`, 1.000 alike by field shape) at the ninth one time in two, as xt_cosm's stereo section runs
them; the output, the object and the four buffers compared after every call (`build/tools/vc2/cosm_run.txt`):

| Codes | Types | Result |
|---|---|---|
| 1, 2, 3, 4, 5, 6, 7, 8, 12, 13, 14, 15, 16, 18, 19, 20 | OD and DS, Wave Shape, Amp Simulator, Speaker Simulator, Resonator, both Side Band Filters, Dual Filter, TVF, Dynamic TVF, Compressor, Limiter, Frequency Shifter, Lo-Fi, TB Filter | 100 notes each, mono and stereo, 0 different: 120,000 DSP blocks |
| 9 | Comb Filter | 200 of 200 different at note-on |

Every type's DSP changes its block (each changed 400 of 400 blocks in a count of 10 notes, but the stereo Resonator's,
Frequency Shifter's and Lo-Fi's 379, 379 and 368: blocks they leave as they were). Seen red: the TVF's mono routine paired with
rom.4's stereo one (the TVF's notes all differ), a bit of rom.4's DSP state (D + 0x1c) flipped at the 21st block (234 of 320
notes, every type), and a bit of rom.2's signal alone (317 of 320).

So **15 of the 16 types are the V-Synth's whole on the voice side: note-on, update, new block and DSP, mono and stereo**, the
note-on and update included, though rom.4 compiled them anew and 0x40 and 0xc0 bytes smaller. The Comb is not: rom.2's
note-on sets it up in place
(the case at `0x894ee300`: the octave byte at +0x14 through the octave table to +0xf4, then the delay's whole and fraction),
rom.4's sends code 9 to a call of `0x893b9d30(object)`; given rom.2's block, rom.4's Comb leaves another word above the
octave byte at +0x14 and, at +0x2c, a whole number where rom.2 has a ratio near 1 (−40.0 against 1.017). So ~~the VC-2's Comb
reads its block otherwise, and is settled with rom.4's own Comb converter~~ code 9 in rom.4 is not a Comb at all: it is the
GROWL, its own type ("COSM type 8, the Growl, run", 2026-11-13). (The other types' cases have the same sizes in
both note-ons' jump tables, `0x894ee118` and `0x893aa288`; only code 9's case moved.)

Wrong runs, kept here so they're not made again: random input words past a table's index range (the TVF's filter type, an
octave) make both images read past the table, into neighbours each image holds its own (the octave table is copied for 198
bytes, the filter's kernel table is followed by code), which gave 612 notes "different" and a fault in rom.2's own filter;
the first run held a list of such words in range, and the run now draws every word in its descriptor's range, which also
keeps the Amp Simulator's type (0 to 2) from jumping its DSP to junk. The update takes the pitch in fr4: without it, once
both machines' FPU registers start at 0, the Side Band Filters asked for an endless delay and rom.2's ran to the step limit
(which rom.2's twin calls didn't have, 20e9 steps, so the first DSP run seemed to hang: they now stop at rom.4's 200e6). Seen
red, the first run: one word of rom.4's copy of the block nudged sets 13 types apart on every note (the first Side Band Filter
doesn't read that word).

Not yet run: ~~the Comb's DSP (rom.4's one routine, on rom.4's own Comb object)~~ (run, "COSM type 8, the Growl, run"),
the Dynamic TVF's targets apart from note-on, and codes 10, 11 and 17, which have routines in both tables but no converter
writes.

### COSM type 8, the Growl, run (2026-11-13, the VC-2 mode's step 1c)

**What it is.** rom.4's list of COSM type names has `["GROWL"]` where rom.2's has `["COMB FILTER"]`, between the 2nd Order
Side Band Filter and the Dual Filter, and rom.4 has no "Comb Filter" string. Its parameters are named Intencity (sic) and
Speed, then five names the Comb's list had (Detune LFO and Env Depth, Tone and its two depths). It keeps the Comb's block
code, 9. So the VC-2 replaced the V-Synth's Comb Filter with a growl: the same idea as the VOCODER OSC's growl ("The
growl, run"), a subharmonic near 50 Hz, done to any signal.

**The routines.** rom.4's COSM converters are called by a dispatcher that switches on the input's +7 (`0x89399982`;
rom.2's switches on +9); its case 7, type 8, is `0x89398780`. The note-on's code-9 case is `0x893b9d30`, the update's `0x893b9d40`, the DSP
`0x893b9b40` (one routine for mono and stereo). What each does, all held to the model below:
- **The converter** reads only Intencity and Speed (the input's +0x10 and +0x12, each plus its Matrix term). It writes
  code 9; +4 the intensity I/127 (held to 0…127 first); +8 the speed 2·S/128 − 1 (held to 0…128); and 0 in every other
  field. So the five other parameters do nothing.
- **The update** sets the DSP's depth (its state's +0xa4) to the intensity, held to 0…1, and its divider factor (+0xa8) to
  440 ÷ ((s + 1) × 50), the speed s held to −0.9…1. Their LFO and envelope terms take the block's depth fields, which are
  0. The note-on sets the phase and step to 1.
- **The DSP**, each sample:
  - The phase runs −1…1. Each time it reaches 1 it wraps by −2, and its step becomes pitch × 880/44100 ÷ n, with
    n = trunc(factor × pitch), at least 2, plus 1 when the line's write position is odd. The pitch is the COSM object's
    +0: the voice's pitch ratio against the wave's root, as the COSM objects of rom.2 and of rom.4's 15 matching types
    hold it. So the growl runs at 440 × pitch ÷ n ≈ 50(s + 1) Hz. That is a subharmonic of the note only for a wave whose
    root is A4.
  - The input goes into two delay lines (L, R) of 3267 samples.
  - The first tap, 512 samples back, is scaled by 1 + depth · w(0.5 − |phase|), where w(x) = x(3 − 4x²), a sine shape
    peaking at ±1.
  - The second tap, at the same place, is moved by w(the phase folded to ±0.5) × depth × 15.95 ÷ pitch samples. It is
    read with linear interpolation between the sample before the position and the one at it, so at rest it is 513 back.
  - The output is the sum of the two taps.

**What it does.** At Intencity 0 the output is the input 512 and 513 samples late, summed: 11.6 ms late, +6 dB at low
frequencies and a null at Nyquist. As Intencity rises, the first tap swings in level from 0 to twice, and the second in
position by up to ±16 samples (0.36 ms) at pitch 1, less at higher pitches. Both move on the growl's phase, so the sound
is gated and wobbled at a subharmonic. The divider swaps between two neighbours by the write position's parity, so the
growl's period jitters between two lengths. Speed sets the growl's frequency:

| Speed | Pitch 0.5 | Pitch 1 | Pitch 2 |
|---|---|---|---|
| 0 | 5.0, 5.1 Hz | 5.0 Hz | 5.0 Hz |
| 32 | 24.4, 27.5 Hz | 24.4, 25.8 Hz | 24.4, 25.1 Hz |
| 64 | 44.0, 55.0 Hz | 48.8, 55.0 Hz | 48.8, 51.7 Hz |
| 96 | 73.3, 110 Hz (n = 3, 2) | 73.3, 88.0 Hz | 73.3, 80.0 Hz |
| 127 | 73.3, 110 Hz (n = 3, 2) | 88.0, 110 Hz | 97.7, 110 Hz |

(the dividers the firmware chose over 200 blocks at Intencity 127, from its step after each). The 12 factory zones on
type 8 vary Speed alone ("The second descriptor bank, read").

**Run** (`vc2run <rom.4> cosmgrowl [notes] [plant]`, `vc2cosmgrowl.cpp`, the model in `vc2cosmgrowllaw.*`, its constants
read from rom.4's pools `0x893b9c80` and `0x893b9e94`; `build/tools/vc2/cosmgrowl.txt`). A COSM object is laid as the
COSM twin lays one, the buffers handed out by the slot finder hook and the Matrix reads answered with zero terms. Over
200 random notes (Intencity and Speed 0…127, pitch 0.03…4, moved every 9 blocks, signals ±1), 40 blocks each:

| | |
|---|---|
| The converter's block, the update's depth and factor, against the model | 0 different |
| 8000 DSP blocks: output, position, phase, step and both 3267-sample lines, bit for bit | 0 different |
| Planted: the divider's odd-position step dropped (`plant`) | 132 of 8000 blocks differ |
| Planted in a copy: the position tap's depth doubled | 4867 of 8000 differ |
| An impulse at Intencity 0 | 1.0 at frames 512 and 513, both sides, nothing else |

For the port (step 3): a COSM type of its own, `vc2cosmgrowllaw.cpp` held to the firmware, with the plugin's type list
and parameters named as rom.4 names them. Ported (v234, HARDWARE_FIRMWARE.md, "The VC-2's COSM type 8, the Growl"):
`Vc2CosmGrowl`, the converter, the note-on's, update's and new block's cases and the DSP held to rom.4's own routines
word for word by `xt_vc2cosm`; the model went into it, and `vc2run cosmgrowl` keeps the laws alone (the run above was
the model's, its `[notes] [plant]` gone with it).

### The OSC block in rom.4 (2026-11-13)

xt_osc's routines (`build/tools/vc2/osc_pairs.txt`): 13 pair by shape, 2 several times, 17 not. `codealign.py` (new) lines a
region's routines up by order and size, and `--near` finds the longest alike run near where a routine should be; they place
the rest, and show which were rewritten:

| Part | rom.2 → rom.4 | How |
|---|---|---|
| exp2, powf | `0x894ec090` → `0x893a8340`, `0x8952ff9c` → `0x89506e54` (the converter's powf, "HIRESO's formant warp, run") | shape; run |
| Velocity curve, sensitivity | `0x894ebd60` → `0x893a7fb0`, `0x894ebe20` → `0x893a8070` | shape |
| pitch10, key-follow key | `0x894e1250` → `0x8939c8a0`, `0x894e12c0` → `0x8939c910` | shape |
| Key state | reset `0x894d6420` → `0x893923c0`, note-on `0x894d6330` → `0x893922d0` (144 alike), note-off `0x894d63c0` → `0x89392360` | shape |
| The Matrix's getters | `0x894e7010` → `0x893a23b0`, `0x894e70b0` → `0x893a2440`, `0x894e7320` → `0x893a26b0`, `0x894e7520` → `0x893a28b0` | shape |
| Pitch ramp | start `0x894f4890` → `0x893b0e10` (41 alike), legato update `0x894f48b0` → `0x893b0e30`, value `0x894f4c00` → `0x893b1160` | shape, `--near` |
| Pitch ramp, rewritten | constructor `0x894f4860` → `0x893b0dc0` (37 instructions against 22), restart `0x894f4920` → `0x893b0b10` (rom.4's update branches to it), tick `0x894f4b00` → `0x893b0f30` (0x230 bytes against 0x100) | order, run |
| Rewritten, not run | the VariPhrase converter `0x894e2f20`, the pitch target `0x894f3a00`/`0x894f3ba0`, the gain target `0x894f3dc0`/`0x894f3e20` (rom.2's seven routines 0x894f3a00…0x894f3ec0, 0x5c0 bytes, are three of 0x470 in rom.4 at 0x893afff0…; **withdrawn, v232:** rom.4's targets are twelve routines at 0x893af9f0…0x893aff60 with their pools to 0x893affe3, two of them copies of the cents nothing calls, and 0x893afff0 is kind 5's update: HARDWARE_FIRMWARE.md, "The VC-2's OSC voice's note-on and targets"), the voice update's 70% step `0x8950c1b0`, the portamento settings `0x894e66e0`, the note's setup `0x894e4c30` and random seed `0x894e5010`, the bend `0x894e9fe0`, the velocity curve's patch converter `0x894ebed0` | no alike run of more than 12 instructions within 0x400 of their place |

The rewritten ones are the wiring: what reads parameters, the Matrix and (in rom.2) the Multi Step Modulator's terms. They
are ported from rom.4, as COSM's converters are.

**Run (`vc2run <rom.4> osc <rom.2>`, `vc2twinosc.cpp`, no port linked: xt_osc holds `XtOsc` to rom.2;
`build/tools/vc2/osc_run.txt`):**

- exp2 over xt_osc's grid, powf at 2 over its 200,000 exponents, the velocity curve over every velocity, sensitivity −210…+210
  and curve 0…8, the sensitivity converter, pitch10 over keys, Coarse, Pitch KF and Fine, and the key-follow key: **988,032
  returns, 0 different.**
- The mono/poly key state over 400 sequences of 300 key events, the system clock hooked in each (rom.2's `0x89301940` is
  rom.4's `0x893018d0`, by the note-on pair's literals): **120,000 events, 0 different.**
- The pitch ramp over 4000 sequences of starts, legato updates with and without a restart, and ticks: **1,759,632 values,
  18,485 different**; while rom.2's glide is under way (its value not 0), 955,339 values, 12,917 different. Traced glide by
  glide, they agree for most of a glide (0 → 1000 cents over 500 ms steps 24 cents a tick in both; the type 1 glide traced
  agrees throughout) and part in two places. Near the target, where rom.2 steps linearly to exactly 0, rom.4 eases in by a
  ratio of about 0.6 to 0.7 a tick (0 → 100 cents over 500 ms: rom.2 −100, −76, −52, −28, −4, 0; rom.4 −100, −60.0, −33.4,
  −16.8, −7.3, −2.6, …). And a fast, wide glide starts at ±500 cents in rom.4 where rom.2 starts at the whole offset (276.2 →
  −770 cents over 114 ms: rom.2 1046, 941, 835, …; rom.4 500, 448, 397, … in about as many ticks; −3584 → 4632 over 7 ms:
  rom.2 −8216, rom.4 −500). **So the VC-2's portamento is its own** (the constructor, restart and tick rewritten): ported
  from rom.4, its law read from those three; the start, update and value are the V-Synth's. (v236, ported and read:
  under 100 ms the span is held to ±500 cents and type 0 eases in with x⁴, which gives this run's −60.0, −33.4; and a
  gain factor of its own over the first ticks; HARDWARE_FIRMWARE.md, "The VC-2's portamento". The start's shape is
  the V-Synth's, but it copies the six words it is given and branches into rom.4's restart.)

Seen red: the sensitivity paired with rom.4's velocity curve instead (201 returns part); rom.4's clock a millisecond late
(every sequence parts, at the key state's time, +0x10).

One correction this brought: `0x893018d0`, which the carrier's init seeds its start phase from, is the system clock in ms
("The growl, run", corrected in place), not a random number.

Not yet run: the Matrix's getters (paired by shape; xt_osc hooks them), and anything of the rewritten wiring.

**What rom.4 lacks.** Not in it: the Multi Step Modulator (its coarse and grid tables, its object's routines, its terms), the
arpeggio's note-on, pedal, step, enable and constructor, the MIDI out of the clock, the effect driver's MFX, Chorus and Reverb
ID tables and its value-text routine, the parameter names at rom.2's address (rom.4's are its own, "The parameter tables"),
and the velocity curves' pointer table (its pointers would differ even with the curves equal). A miss alone means the bytes
differ, not that the VC-2 has no such thing: a table changed within itself, or code compiled anew, reads as a miss too. For
the Multi Step Modulator a second source agrees: rom.2 holds the string "Multi Step Modulator" (and "Step Modulator" twice),
rom.4 none of them. So the VC-2 has no Multi Step Modulator; its arpeggio's and its effects' IDs are its own.

### The analog OSC in rom.4 (2026-11-13)

**Its waveforms: the V-Synth's first 11.** rom.4's install (`0x893c2ad0`, rom.2's `0x89505170`) loads 12 routine literals
where rom.2's loads 15: the sub oscillator's install (`0x895069d0` → `0x893c3e60`) and the 11 waveform routines, in rom.2's
order, each 0x1435a0 from rom.2's. rom.2's other three are not there: SUPER-SAW (`0x89507520`), FEEDBACK-OSC (`0x89507900`)
and X-MOD (`0x89507b60`, HARDWARE_FIRMWARE.md, "X-MOD"). A second source agrees: rom.4's waveform names are SAW, SQUARE,
TRIANGLE, SINE, RAMP, JUNO, HQ-SAW, HQ-SQUARE, NOISE, LA-SAW, LA-SQUARE (11, xt_analog's codes 0…10 in order), and it holds
no "SUPER-SAW" (rom.2 twice), "FEEDBACK-OSC" (once) or "X-MOD" (13 times). A third: the super saw's shared counter
(`0x8987c894`) is not in rom.4's .data ("The V-Synth's tables and code in rom.4"). So the VC-2's analog OSC has 11
waveforms and no X-MOD.

**Pairs.** The 11 waveform routines and the sub oscillator's 8 pair by shape at the block's distance 0x1435a0 (19 pairs,
`kWaveforms` in `vc2twinanalog.cpp`); their literals differ only in the analog sub-object's offsets from +0xec on (4 more in
rom.4, below) and their own pools' addresses. The note-on, install and update were compiled anew, and the voice is laid out
otherwise: rom.4's install clears at +0xa40 what rom.2's clears at +0x960 (0xe0 further). `codealign.py --similar` puts them
between its scale's ends:

| Pair | Instructions | Alike |
|---|---|---|
| COSM note-on `0x894edf70` / `0x893aa0e0` (runs equal) | 0x398 / 0x378 | 0.918 |
| COSM update `0x894ee6a0` / `0x893aa7d0` (runs equal) | 0x468 / 0x408 | 0.761 |
| Analog install `0x89505170` / `0x893c2ad0` | 0x128 / 0x128 | 0.605 |
| Analog update `0x89505400` / `0x893c2ce0` | 0x1f8 / 0x1f0 | 0.450 |
| Analog note-on `0x89504af0` / `0x893c22e0` | 0x488 / 0x500 | 0.425 |
| Pitch ramp's tick `0x894f4b00` / `0x893b0f30` (differs, "The OSC block in rom.4") | 0x80 / 0x118 | 0.368 |
| Unrelated: COSM note-on / analog note-on, analog update / COSM note-on | | 0.096, 0.080 |

So the note-on, install, update and the converter (rom.4's ~~`0x8939ced0`~~, which only starts kind 3's block: the conversion
is `0x8939fbd0`, "The VOCODER OSC's IDs, run"; rom.2's `0x894e1920` has no pair by shape) are the wiring, ported from rom.4
as COSM's converters are; only the waveform routines are run.

**Run (`vc2run <rom.4> analog <rom.2>`, `vc2twinanalog.cpp`, no port linked: xt_analog holds `XtAnalog` to rom.2;
`build/tools/vc2/analog.txt`).** A voice is made in rom.2 alone, as xt_analog makes one: the calibration from the
descriptors' defaults, the converter on a random input (key, velocity, gain, pitch, level and its key follow, Pulse Width's and
Fat's groups, the sub oscillator on or off), the note-on and the install. Its analog sub-object (voice + 0xd4, what a waveform
routine is given) is copied into rom.4 in rom.4's layout, and each image's routine renders 40 blocks of 64 samples, rom.2's
update moving the targets every fifth block (the sub-object copied again) and a random sync mask one block in eight; the
samples, the returned mask and the sub-object compared each block. 100 notes for each waveform, with and without the sub
oscillator: **88,000 blocks, 0 different**, all 19 pairs reached. 544 of the 2,200 notes are silent in both images, every one
with its level key follow taking the level down at its key; the rest sound.

The sub-object's layout, by the run: rom.4's waveform routines read +0x80 and +0x84 where rom.2's do, and every field from
+0xec on 4 bytes further (+0xec, +0xf0, +0xf4, +0xf8, +0x104, +0x10c in rom.2). So rom.4 has 4 bytes more somewhere in
(+0x84, +0xec]; no waveform routine reads between, and the run is equal with them placed at +0x88, +0xc0 or +0xe8 (the run
placed them at +0x88). The band players' run pins them at +0x94 (the run uses that now, `kOscInsert`, and is equal there
too; "The VariPhrase players and key assign in rom.4"). What rom.4 keeps there is not read yet. First written as inserted at
+0x80, from the `--pools` line
0x80 → 0x84 of `0x89509360` (SQUARE with the sub oscillator): that routine loads its pool word 0x80 twice, as the base of
+0x80 and +0x84 and later to step from +0x84 to +0x104, and rom.4's steps by a word of its own, 0x84, to +0x108; `--pools`
pairs pool words by place, so the second use read as the first. The run with the insert at +0x80 gave 68,320 of 88,000
blocks different.

Seen red: the insert at +0x84 (68,320 different); no insert (rom.4's NOISE routine `0x893c2230` runs to the step limit on the
misplaced fields, and the run fails); SAW paired with rom.4's SQUARE routine (3,890 different); one bit of rom.4's sub-object
flipped at the eighth block (4,077 different).

**So the VC-2's 11 analog waveforms are the V-Synth's, and `XtAnalog`'s routines serve them as they stand, given rom.4's
sub-object; its note-on, install, update and converter are its own.** Not run: how rom.4's converter maps OSC 1 Analog
Waveform (ID 0x2f1, 0…10) to the input's code (the names' order says one for one). (The note-on from v242: it runs to
0x893c2ac2, about 1,000 instructions, past the table's 0x500; rom.2's laws on the words at rom.2's +0x8c, but the
Pulse Width's target, which the VC-2 narrows by waveform; HARDWARE_FIRMWARE.md, "The VC-2's analog note-on".)

### The VariPhrase players and key assign in rom.4 (2026-11-13)

**Where they are.** `codepairs.py` finds most of the band players' routines at 0x143920 (63 of 75 addresses
`XtMultiBandPlayer` names) but few of SOLO's (26 of 55), Vari off's (3 of 24) or LITE's (3 once, 14 several times; output
in `build/tools/vc2/player_pairs.txt`): their shapes hold the object's field offsets, and the object is laid out otherwise
in rom.4 (below). The installs place them. rom.4's voice setup (`0x893ae0f0`, rom.2's `0x894f2010`) switches on the same
kinds, with the kind at the voice's +0x54 where rom.2 has it at +0, and three more (5 the VOCODER OSC, 6, 7; 7 is kind
4's install, update and render but for its render routine, `0x893c61e0` for `0x893c62e0`: v230, HARDWARE_FIRMWARE.md,
"The VC-2's OSC voice"; it is the install, not a setup: kinds 1 to 3 run their own in its place); its kind 1
install (`0x893af7a0`, rom.2's `0x894f3590`) and kind 2 install (`0x893af870`, rom.2's `0x894f3660`) load the players in
rom.2's order. `codealign.py --fields` (new) then lines each pair up with the values masked and lists what differs; on pairs
that run equal it reads 0.90 (COSM's note-on) to 0.98 (SAW). Output in `build/tools/vc2/player_fields.txt`:

| Player | rom.2 → rom.4 | Alike by field shape |
|---|---|---|
| BACKING | block `0x8950e6e0` → `0x893cadc0`, note-on `0x89510980` → `0x893cd1d0` | 0.94, 1.00 |
| ENSEMBLE | block `0x8950db10` → `0x893ca1f0`, note-on `0x89510130` → `0x893cc8f0` | 0.93, 1.00 |
| SOLO | block `0x8950cd90` → `0x893c9450`, start `0x8950fa60` → `0x893cc1a0`, event jump `0x8950d580` → `0x893c9c40`, body `0x8950cde0`, `0x8950d0f0`, `0x8950d380` → `0x893c94a0`, `0x893c97b0`, `0x893c9a40` | 0.93, 1.00, 1.00; 0.96, 1.00, 0.99 |
| Period-jump player (type 8) | `0x8950e1d0` → `0x893ca8b0` | 0.96 |
| LITE | `0x8950a7f0` → `0x893c6e80`, `0x8950a7fc` → `0x893c6e8c`, a kernel `0x8950a808` → `0x893c6e98` | 0.95, 0.97, 0.98 |
| LITE's note-on, update | `0x8950b5b0` → `0x893c7c30`, `0x8950bbb0` → `0x893c8280` | 0.85, 0.93: the voice's offsets differ |
| Kind 1's mode 1 | `0x8950d880` → `0x893c9f60`, `0x8950ffd0` → `0x893cc760` | 0.96, 1.00 |
| Vari off | `0x89509870` → `0x893c63c0`, `0x89509f52` → `0x893c66e0` | 0.60, 0.55 over 0x180 instructions: rewritten |

**The OSC object** (rom.2's voice + 0xd4, rom.4's voice + 0x15c past its vibrato's 0x54 bytes and the block's copy:
v230, HARDWARE_FIRMWARE.md, "The VC-2's OSC voice"; the analog OSC's and the players') has 4 bytes in rom.4 at +0x94 that rom.2's has not:
every offset from +0x98 on is 4 more in rom.4's code (+0x98, +0xa8, +0xe8 in the players, +0xec on in the analog OSC's) and
+0x84 is not. The band run is equal with the insert at +0x94 and differs with it at +0x88, +0x8c or +0x90 (all 9,680
calls) or +0x98 (4,800, all of BACKING's); the analog run is equal with it there too (`kOscInsert` in `vc2twin.h`).

**SOLO's state** past R10 (rom.2's +0x124, rom.4's +0x128) is laid out otherwise again. rom.4 has no word at R10+0: rom.2's
`0x8950c7e0` writes 256 there, or 0 under Robot Voice (byte 0 bit 0x10), and rom.4's copy `0x893c8f00` drops both the test
and the write. R10+0x24 (the note-on's "no cutoff" 0) is at rom.4's R10+0x44, so rom.4 has R10+4…+0x20 4 lower, +0x28…+0x48
8 lower and +0x4c on 4 lower. And rom.4 keeps a word of its own at +0x188 (its R10+0x60), written at the start (0.337 in the
first case), which no word of rom.2's holds. Read from the body routines' offsets (`--fields`), and checked by finding each
value of rom.2's object in rom.4's after the start and jump.

**Runs** (`vc2run <rom.4> band <rom.2>` and `solo`, `vc2twinband.cpp`, `vc2twinsolo.cpp`, `vc2twinplay.*`; no port linked:
xt_fw `bandcmp` and `solocmp` hold `XtMbPlayer` and `XtSoloVoice` to rom.2; output in `build/tools/vc2/band_run.txt`,
`solo_run.txt`). Each test wave is a tone of six harmonics struck one to three times, 6,000 to 20,000 frames, one in three
stereo. A band wave is encoded by rom.2's multi-band encoder (one segment, or the attack detection at Encode Depth 30 or 90)
and its bands rebuilt by rom.2's load; a SOLO wave's records are rom.2's SOLO encoder's, in a one-segment vpdt. Each note
has a pitch ratio from ¼ to 4, a walk forward, backward or still and moved as it plays, the gain moved, one note in three
looped (points and modes at random), and for SOLO a formant ratio from ½ to 2, an energy and, one note in four, Robot Voice.
The player is set up in rom.2 as `xtFwBandPlay` or `xtFwSoloPlay` sets it up and copied into rom.4's layout; then each
image's note-on (SOLO's start and event jump, then `xtFwSoloPlay`'s rest of the note-on) and 120 blocks of 32 frames, the
output, the player and its loop cache compared after each call (not r0, where the players leave a code address or an
offset):

- BACKING 4,840 calls and ENSEMBLE 4,840: **0 different.** All 9,600 blocks sound (peak 0.50 of full scale).
- SOLO 4,392 calls: **0 different** (5,725 of 5,760 blocks sound, peak 0.65). Under Robot Voice, 1,427 of 1,464 calls
  differ, first at the event jump (R10+0x40): **rom.4's SOLO under Robot Voice is its own.** rom.4 still names OSC 1 and 2
  Robot Voice Sw, so the VC-2 has the switch; what its own word at +0x188 does, and how Robot Voice plays there, is read
  from rom.4 when SOLO is ported.

Seen red: the insert at +0x90 (9,680 of 9,680 different); BACKING's block paired with rom.4's ENSEMBLE block (4,800); a bit of
rom.4's pitch (+0x70) flipped at the 51st block (band 5,600 different, SOLO 3,947); SOLO's map without R10+0x24's move
(5,800); SOLO's block paired with rom.4's ENSEMBLE block (a fault, which fails the run). Two plants showed nothing, for a
reason: a bit of the pitch's copy at +0x98, which the players write before they read it, and `xtFwSoloPlay`'s rest of the
note-on left out, which is input to both images alike.

**LITE** (`vc2run <rom.4> lite <rom.2>`, `vc2twinlite.cpp`; no port linked: xt_tone holds `XtLitePlayer` to rom.2; output
in `build/tools/vc2/lite_run.txt`). Its note-on is a stretch of the voice's (`0x8950b5b0`, at `0x8950b8d0` on the voice's
registers) and can't be called alone, so each note's state is set in rom.2's player as `XtLitePlayer`'s note-on leaves it
(the fields `XtLitePlayer.h` lists: heads, windows, period, filter, the loop as floats; the beat's rates and cached loop
events), copied into rom.4's layout (`kOscMap`), and each image's block routine run 120 times on test waves as the band and
SOLO runs, the wave between zeroed margins in both machines (the heads read below its start walking backward). Its
per-mode routines and forward kernels are the same code with no value changed (`--fields`, 1.000). LITE's routine tables
(`0x8950a870`, `0x8950a890`; rom.4's `0x893c6f00`, `0x893c6f20`) hold three loop modes a way and 0 in the fourth slot, in
both images, so mode 3 jumps to 0: the run draws 0 to 2.

- Walking forward throughout: **2,400 blocks, 0 different.**
- Walking backward or still at any point (a step of 0 or less takes the backward kernels): **4,794 of 6,240 blocks
  differ.** rom.2's backward kernels (`0x8950b1d4` mono, `0x8950b41a` stereo) read each sample negated, FNEG then FADD onto
  the difference to the next (−x[i] + frac·(x[i−1] − x[i])); rom.4's (`0x893c7864`, `0x893c7aa6`) FSUB with the sample as
  it is (x[i] + frac·(x[i−1] − x[i])): six places, one for each head, and each channel's in the stereo kernel. **The VC-2's
  LITE plays backward the right way up; the V-Synth's reads it inverted.** The run's second pass makes rom.2's six reads
  rom.4's in rom.2's machine (the FNEG a NOP, the FADD an FSUB) and is then equal throughout (8,640 blocks, 0 different).
  So that is all of LITE's difference.

All 17,280 blocks of the two passes sound (peak 0.47 of full scale). Seen red: the insert at +0x84 (8,640 of 8,640 in both
passes), the mono routine paired with rom.4's stereo one (7,782 and 6,480), a bit of rom.4's pitch flipped at the 51st
block (6,738 and 5,040: the forward notes too), four of the six reads patched (the second pass 1,302 different, the stereo
right channel's). The insert at +0x90 doesn't show (4,794 and 0, as without it): LITE reads nothing in rom.2's +0x90…+0x93
(the beat's integral, used under Tempo Sync), so only the band run places it.

**Vari off is the VC-2's own; not run** (run and ported in v261: HARDWARE_FIRMWARE.md, "The VC-2's Vari off"). rom.4's `0x893c63c0` and `0x893c66e0` (paired by the installs' order) are 0.60 and
0.55 alike by field shape. rom.4's mono routine, read as far as its mode paths: no test of the step's sign before it
switches on the mode alone, where rom.2's picks a way by the sign; it steps by
the pitch (+0x70) alone, where rom.2's `0x89509870` steps by the pitch × the walk (+0x7c), and under Tempo Sync or Time Trip
(byte 0 & 6) by the beat's factor (`0x8950c280`, `XtBeat`), which rom.4's doesn't call; it reads the forward way's fields
where the OSC object's insert puts them (fraction +0xec, position +0xf0, samples +0xf4, last sample +0xf8, loop +0xfc to
+0x108's rate, mode +0x10c), ~~its fade at +0x124 where rom.2's is~~ its fade at +0x128, four bytes up from rom.2's like the rest (withdrawn, v261: +0x124 is the stereo render's switch to fold its channels; HARDWARE_FIRMWARE.md, "The VC-2's Vari off"), and switches on the mode three ways (its table at
`0x893c6410`) where rom.2's has six paths by mode and direction. Its mode means something else: run on a state rom.2's way
(walk 1, forward), its mode 0 branches to the end, so the output stays 0 and the position where it was; with rom.2's mode
+ 1 its mode 1 wraps the position by the loop's length, and with no loop (length 0) never ends (the step limit); mode 3
jumps into its table's data. So its state is set by rom.4's own converter and note-on, and it is ported from rom.4 rather
than run beside rom.2's. rom.4 still names OSC 1 and 2 Tempo Sync Sw, Time Trip Sw and Time Trip Beat Keep, so how they
reach a Vari-off wave in the VC-2 is open.

LITE's note-on and update (`0x8950b5b0` → `0x893c7c30`, `0x8950bbb0` → `0x893c8280`) read the voice by other offsets (the
voice's layout, as the analog install's +0x960 → +0xa40): wiring, ported from rom.4. (The note-on from v240: in rom.4 a
routine of its own, not a stretch of the voice's; its two heads' words are rom.2's player's four bytes up, and its Vari
off words VariPhrase's; HARDWARE_FIRMWARE.md, "The VC-2's LITE note-on".)

**Key assign** (`XtKeys.h`). The reset, note-on and note-off ran equal in the OSC run (120,000 events, "The OSC block in
rom.4"), and the highest held key (`0x894d66b0` → `0x89392670`, by shape) ran within them: both images' note-offs call it.
rom.4 calls it from two more places (`0x89390c9c`, `0x89391de8`) rom.2 has not.

**So BACKING's, ENSEMBLE's and SOLO's players of the VC-2 are the V-Synth's, and `XtMbPlayer` and `XtSoloVoice` serve them
as they stand, given rom.4's object layout; SOLO under Robot Voice is not; LITE's is, with its backward read the right way
up (six instructions; for the port, a switch in `XtLitePlayer`); key assign is the V-Synth's; Vari off is the VC-2's
own.**

### The TVA in rom.4 (2026-11-13)

**Pairs.** Of the patch TVA's routines (HARDWARE_FIRMWARE.md, "The patch TVA's level and pan") only the render pairs by
shape: `0x894f5720` → `0x893b2870`, at 0x142eb0. The converter (`0x894ea380`, `0x894eab40`) and its helpers (`0x894ea990`,
`0x894eaa90`, `0x894eb170`, `0x894eb260`, the controllers' copy `0x894e9fe0`) have no pair by shape: wiring, as the other
converters. The object's routines don't line up by size (rom.2's 0x600, 0x500 and 0x170 bytes from `0x894f4f70`, rom.4's
0xe0, 0x2b0, 0xc10 and 0x150 from `0x893b2020`), so each was scored by field shape (`codealign.py --fields`' masking, as
difflib's ratio) at every place in rom.4's 0x893b2000…0x893b3300: the end test `0x894f54b0` → `0x893b2560` and the new
block's caller `0x894f5de0` → `0x893b32d0` at 1.000; the update `0x894f5360` at 0.72 at best, the new block `0x894f5310`
0.45, the truncated-tone dispatcher `0x894f5570` 0.50 and the note-on `0x894f50e0` 0.35: rewritten. The end test's offsets
are 0x10 further in rom.4 (+0x40, +0x68, +0x6c, +0x94 at +0x50, +0x78, +0x7c, +0xa4): **rom.4's TVA object holds rom.2's
words 0x10 further on.**

**rom.2's render is in rom.4, and nothing calls it.** No `bsr` reaches `0x893b2870` and no word in rom.4 holds its address;
its pool (32, 1/32.5, 0.0009375) is the only place rom.4 holds the bus levels' glide step. rom.4's voice routines
(`0x893b3f80`, `0x893b4060`, `0x893b4140`, `0x893b4220`) call `0x893b2640` instead, rom.4's own render:

- With +0x50 and +0x58 clear (rom.2's +0x40, +0x48: no truncation), it ramps the two gains (+0x78, +0x7c) toward their
  targets (+0x70, +0x74) by their steps (+0x68, +0x6c), a step that would pass the target in the block shortened to
  (target − gain)·1/32.5 as rom.2's is, and adds the signal × the gain into **one stereo pair, 8 bytes a frame**. rom.2's
  adds it into eight buses (MAIN, the Reverb and Chorus sends, MULTI, DIR) through levels that glide 3% a block. **So the
  VC-2's TVA has no Output Assign pairs, no sends and no bus glide: one pair a voice.** Where its pan goes is not read
  (rom.2's is the bus levels' pan law; rom.4's would have to be in the gains, which its own update forms).
- Truncated, it fades by a global step (`0x8993eb08`) and at the end sets +0x54 and calls the OS (`0x89301830` with
  `0x8994fec8` and 0x1000). Not run.

**Run (`vc2run <rom.4> tva <rom.2>`, `vc2twintva.cpp`; no port linked: xt_tone holds XtSynth's TVA to rom.2;
`build/tools/vc2/tva_run.txt`).** rom.2's render with its MAIN pair's levels at 1 (the block's +0x20, +0x24 and the gliding
+0x70, +0x74), where it is the gains alone, beside rom.4's live render, each object holding the same state (rom.4's 0x10
further on): targets and gains within ±1, a quarter of them at rest near 0, steps toward the targets, the envelope released
or not. 2000 cases of 40 blocks, the signal at one of xt_cosm's levels added into a pair the other voices have filled,
the targets and steps moved every tenth block as an update moves them, then each image's end test; the pair, the targets and
gains and the end test's answer compared after each:

| Pass | Render | End test |
|---|---|---|
| As the images are | 46,395 of 80,000 blocks different, the gains at most 1.3e−5 apart | 0 of 80,000 |
| rom.2's 1/32.5 made rom.4's (`0x894f5844`: 0x3cfc0fc0, a unit low; rom.4's `0x893b2844` 0x3cfc0fc1, to nearest) | 6,874 different | 0 |
| That, and rom.4's steps set to rom.2's before each block | **0 different** | 0 |

The second difference: rom.4's render writes a shortened step back into the object, where rom.2's keeps it in a register
and shortens afresh from the stored step each block. Near a target a stale step can then point the wrong way, and under
the SH-4's rounding to zero a step under half a unit of the gain still moves it a whole unit toward zero: rom.4's gain
creeps away from its target by a unit a sample (2.4e−7 a block at a gain of 0.097) until the next update sets the step.
The end test's answers are the same throughout (13,938 "ended" in each pass).

Seen red (200 cases a pass, the third pass clean without them): the object 0x14 further on (9,317 of 16,000 calls), a bit
of rom.4's gain flipped at the 21st block (2,730), rom.4's dead copy of the render in place of its live one (9,703), the
envelope's state changed in rom.4 alone (2,744).

**So the VC-2's TVA renders the V-Synth's gains onto one stereo pair, with its last-step constant rounded to nearest and
its shortened step kept; its eight buses, sends and glide are not there, and its note-on, update, new block, truncated
path and converter are its own.** For the port: ~~`XtSynth`'s TVA gains serve it given those two changes; its frame does
not~~ (withdrawn in v233: rom.4's note-on and update multiply the gains and targets by its block's +0x48 and +0x4c when
its +0x40 is above 0, which this run never set; the port, `Vc2Tva`, forms them itself, the whole object held to rom.4,
HARDWARE_FIRMWARE.md, "The VC-2's TVA").

## rom.4 run in the interpreter (2026-11-12, fourth pass)

~~plugin/Source is unchanged: `Sh4Interp::loadRom` keeps rom.2's map.~~ (From v197 `Sh4::loadRom` reads the header and
`vc2rom.*` calls it, its own copy gone: HARDWARE_FIRMWARE.md, "A firmware image's own header".) `research/tools/vc2rom.*`
mapped rom.4 through the interpreter's public `mapRam` and `write` by its own header: the image at 0x89300000, .data [0x8992e1bc, 0x8994fc20) copied from
0x8990c758, .bss zeroed to 0x8a1f11c4 (refused unless the source ends where the copy begins), with XtFwMachine's stack and heap
and operator new (`0x89506df4`) answered from the heap. Where a run stands in for the OS it says so; nothing else is hooked but
the probes named. `boot`: the vocoder's builder `0x893f0490` on its static object, 31,748 steps; both engines' vtables and their
eight slots are the table under "The vocoder object".

### HIRESO's formant warp, run (`warp`)

Slot 4, `0x893f15d0(engine, side, ratio)`, writes the side's map and nothing else (a diff of the whole engine): 32 floats at
the engine's +0x2924 (side 1's at +0x2b24), for each synthesis band the fractional analysis band it reads. Its law, read and then
held to the firmware bit for bit (`warp check`, `vc2warplaw.cpp`: 40,352 cases, both sides, pivots p = 0, 1, 3, 6, 9, 12, 18,
20, 22, 24, 40, 63 and 127, ratios 0.5…2 every 1/1024 and the floats either side of 0.5, 0.95, 1, 1.05 and 2; 0 bands
different, and 26,476 with an error of 1e-7 planted in one blend's weight; v213: the plugin's port, `vc2HiresoMapOf`,
equals slot 4 on the same 40,352 maps in xt_vc2hireso, and the model is gone; HARDWARE_FIRMWARE.md, "HIRESO's formant
warp"):

- the pivot P = 500 + 172.265625·p Hz (172.27 = 44100/256), p the side's int at +0x2cb4/+0x2cb8 (slot 5's `ftrc` of the
  template record's second float: 1…24 in the 32 records, 12 in record 0, so P = 2567.2 Hz there);
- a warp in Hz, piecewise linear, that holds 100 Hz and 11874 Hz (the first and last band centres) and moves P to P·r. Synthesis
  band b, at centre c_b, reads the analysis at x = (c_b − (A − A·s))/s, with A = 100 when P·r > c_b and 11874 otherwise, and
  s = (P·r − A)/(P − A);
- x to a band index by a cubic (`0x893f18ec`…, a0…a3 = −0.80590, 0.0087785, −9.3079e-7, 3.5423e-11): a fit of the centres, not
  the table, so 100 Hz reads as 0.0627 and 11874 Hz as 31.499;
- three branches: for r ≤ 0.95 and r ≥ 1.05 that map; between them b + (map − b)·|r − 1|/0.05, so r = 1 is the identity exactly
  (the cubic alone would read band 0 as 0.0627) and the warp comes in continuously by 0.95 and 1.05.

So with template 0 at r = 2 the bands up to 5.1 kHz read their own frequency over 2.04 (formants up an octave) and the rest are
squeezed between 5.1 and 11.9 kHz; at r = ½ the bands up to 1.3 kHz read theirs × 1.9 and the rest are stretched to 11.9 kHz.

The ratio, run (`ratio`): the converter's `0x89506e54(2.0, (v − 64)/63)` (v = 0 read as 1) is powf: on all 128 values within 1 ulp of the
host's powf but for two (v = 31, 87) 2 ulp low, as round-to-zero gives; other bases agree to 7 digits. It wraps the routine
`0x89509b80` and sets an error word (`0x89add990`, 1100 or 1101) on an infinite or NaN result. So Formant Shift 64 is 1 exactly,
60…68 (0.957…1.045) fall in the blend, and 0 and 1 both give ½.

What the glue gives slot 4 (`0x893f07a0`, read, not run; run and ported bit for bit in v220, HARDWARE_FIRMWARE.md, "The
vocoder object"): it calls slot 4 twice a block, side 0 then side 1. With the vocoder's
+0xc and +0x10 (the EXT IN path's template offsets) both 0, a side gets +4 or +8, both the Formant Shift ratio. Otherwise q =
`0x899d9d90` over the pitch tracker's period (`0x893ee010`), d = min(q, 2) − 1 for q ≥ 1 and max(2q − 1, 0) − 1 below, and the
sides get +4 + (+0xc)·d and +8 + (+0x10)·d, each held to [0.5, 2]. Before that it glides the consonant gain, +0x14 += 0.3·
(+0x18 × its float argument − +0x14).

### The hold's controller, run (`hold`)

The controllers' handler table is built by its own routine (`0x89393370`: an object per controller number, pointers at
`0x899891e0` + 4·cc), then every controller is sent through the dispatcher `0x89392f90(port, channel, cc, value)`, which calls
the number's handler through its vtable and then the listeners at `0x899890d0` + 64·(port − 1) + 4·channel. The base handler
tells a listener list (`0x899826e0`, walked by `0x89390ea0`) that the OS fills; the run gives it an empty one. On both engines:

- CC 64 reaches the engine's slot 6 with its value: every value 0…127, ports 1 and 2, all 16 channels;
- CC 121 (Reset All Controllers) reaches it with 0;
- no other controller does.

CC 64's handler (vtable `0x898cd238`, which the builder gives number 64; 66 and 7, 10, 11, 91, 93 get classes of their own)
lets a value below 64 through only while a per-channel byte at `0x899893e0` is set; every write found sets it to 1, so here all
pass and slot 6's own threshold at 64 decides. **The hold is the Hold controller, CC 64.** Not run: whether the pedal jack reaches
it as CC 64, and whether a receive-channel test comes before the dispatcher.

### The growl, run (`growl`)

The carrier (kind 5, `0x893ebfa0`), with +0x3c above 0. Run on a carrier made by its own init (`0x893ebed0(carrier, P)`, which
zeroes +0x3c and its phase, and starts the saw at 2·(3.997·x(1 − x)) − 1 for x = |t|/32767 with t the call `0x893018d0`
returns, ~~the OS's rand~~ the system clock in ms, as short (2026-11-13: it pairs with rom.2's `0x89301940`, which the key
state calls for the time and xt_osc hooks as the clock; so a note's saw starts where the clock and a logistic step put it),
answered here with a fixed number), set by its own update (`0x893ec4d0(carrier, P)`: P+0 gain, P+4 pitch, P+0x20 to +0x3c, P+0x2c the
balance), after the carriers' rate setter (`0x893ec6a0`, from the init table at `0x898c60a8`: 880/44100 at `0x899d9964`, 44100/440
at `0x899d9960`, so P+4 is the pitch as a ratio to 440 Hz). The law, held bit for bit to the saw the branch hands its filter (a
hook at `0x893ec310`; `vc2growllaw.cpp`, in the plugin from v221 as `Vc2Carrier`, held word for word by xt_vc2carrier;
256,000 samples at 55 Hz…10 kHz and growl 0.05…1, 0 different; planted errors in the
divider's noise test, its rounding and the cosine give 242,948, 127,960 and 256,000):

- a growl oscillator, a phase γ in [−1, 1) at +0x40, whose step (+0x44) is set each time it wraps: the saw's step over n, n =
  trunc(ratio·440/50), at least 2, and one more when the noise generator's word +0x64 is positive, about half the cycles. The run:
  2 or 3 at 55 and 110 Hz, 4 or 5 at 220, 8 or 9 at 440, 17 or 18 at 880, 199 or 200 at 10 kHz (trunc(f/50 Hz), less one where
  the product rounds down: 79 or 80 at 4 kHz). **So the growl is a subharmonic of the note near 50 Hz, f/n, between 50·n/(n + 1)
  and 50 Hz from 100 Hz up (f/2 or f/3 below), its divider jumping by one at random each cycle: a rough, irregular modulation
  locked to the saw;**
- the saw handed on is ½·[wrap(φ + (g/π)·S(γ)) + (1 + g·C(γ))·φ], g at +0x3c, φ the saw's phase, S and C a cubic sine and cosine
  of πγ (`0x893f3210`: γ folded into ±½, then γ(3 − 4γ²); `0x893f3280`: the same on ½ − |γ|). Half the saw phase-modulated by the
  growl, half amplitude-modulated by it; at g = 0 it is φ;
- the noise generator steps after the saw in each sample: +0x64 ← −250·(+0x64) + 127·(+0x68) + 30, then +0x68 ← 252·(+0x68) +
  (+0x64), 32-bit;
- the init sets +0x44 to 50 × the rate (0.998), so the first growl cycle starts at the second sample.

Where g comes from (read, not run; run and ported in v222, HARDWARE_FIRMWARE.md, "The VOCODER OSC's converter and
kind 5's update", where the Matrix sums are its OSC Time and Formant destinations): the kind 5 update `0x893afff2(voice)` builds P at voice + 0x104 and gives P+0x20 =
clamp₀¹(P+0x30 + m₁·P+0x34 + m₂·P+0x38) and P+0x2c = clamp₀²(P+0x44 + m₁·P+0x48 + m₂·P+0x4c), m₁ and m₂ two modulators the voice
points at (+0xa50, +0xa60): the shape of Growl Level and Saw-Noise Mix Balance with their LFO and Envelope Depths. ~~Which ID
lands at P+0x30 (and where Growl Keyfollow goes) is not traced: the converter hasn't been run.~~ Run: Growl Level, with its
Keyfollow added by octave from key 60, and m₁ the envelope, m₂ the LFO ("The VOCODER OSC's IDs, run").

### Voice kind 6, run (`kind6`)

(v225: the module ported bit for bit, every instruction read: HARDWARE_FIRMWARE.md, "Voice kind 6". What the run below
left out: the "filter set from the period" is an SVF high-pass at the output's pitch, and below r′ 1 a lift by level
adds where the grains thin.)

Kind 6 (written in the EXT IN path; render `0x893ec730(voice + 0xafc, out)`) is a pitch-synchronous (TD-PSOLA) pitch shifter of
the mic:

- the block mixer writes each block of the mic into a 4096-sample ring (`0x893ec820(input, 32)`: `0x8a0f31d0`, write index
  `0x8a0f31cc`) and the pitch tracker's period for that block (`0x893ee010`, `0x899d9bd0`, in samples; ~~551.25, 80 Hz, at the
  tracker's init~~, withdrawn 2026-11-13: run, it is 0 after the init table's setters and 220.5, 200 Hz, after the tracker's
  reset, which selects mode 0 and nothing else; 551.25 is mode 1's start, which nothing selects: "The pitch tracker, run")
  into a 128-block history (`0x8a0f71d4`, which `0x893ec7d0` resets to 44100/440);
- a voice starts 512 samples behind the write point (`0x893ec8d0`, which sets the flag `0x8a0f73d4` the render needs; a mode
  change, `0x893ec890`, clears it). An epoch marker steps through the ring by the period recorded for the block it is in. Every
  T_out samples a grain starts at the latest epoch, two heads in turn, each read with linear interpolation at a rate (+0x64, from
  P+4) under a window (`0x893f32a0`) twice the shorter of T_in/rate and T_out. The sum goes through a filter set from the period
  (`0x893ecde0`); where the input stage calls the mic unvoiced its own sample at the read point is mixed in by Unvoice Level
  (`0x899d9968`); the same value goes to both channels;
- T_out (`0x893ecd50`, at each grain) is T_in/r, T_in interpolated between the last two epochs and r the glided P+0 (+0x60).
  With the flag `0x8a0f73d8` set (mode 0 to `0x893ec890`) r is P+0 · T_in · 440/44100 (`0x899d996c`), so T_out =
  44100/(440·P+0), whatever the input.

Run on the firmware's routines (the globals' setters `0x893ec6a0` and `0x893ecef0` from the init table, the ring's reset and
mode, a voice by its setter `0x893ec9a0(voice, P)` and start), a two-harmonic wave of period 200 or 300 samples in, the output's
period by autocorrelation:

| Mode | Input period | P+0 | Output period | The reading |
|---|---|---|---|---|
| 1 | 200 | 1, 1.5, ½ | 200.00, 133.33, 400.00 | input/P+0 |
| 1 | 300 | 2 | 150.03 | 150 |
| 0 | 200, 300 | 1 | 100.21, 100.24 | 44100/440 = 100.23 |
| 0 | 200 | ½ | 200.49 | 200.45 |
| 0 | 300 | 2 | 50.11 | 50.11 |

**Mode 1 is an interval (a harmonizer), mode 0 a pitch: the voice sung at a played note, whatever is sung.** ~~Not traced: which
parameter selects the mode (`0x893ec890` and the voice's start have no pool reference, so they are called through something not
yet found, and the mode word's .data value, 2, is one `0x893ec890` never writes), and where the voice's P comes from (the
keyboard's note, Auto Note).~~ Settled, "Kind 6's pitch source, run": nothing selects mode 1 (`0x893ec890` has no caller; the
ring's reset turns the .data 2 into mode 0), and P+0 is the note's pitch, Auto Note's or a key's. A pulse train in can leave
grains empty: the epochs are period-spaced from where the voice started, not placed on pulses (the run first used one and got
silence at 300 samples).

### Kind 6's pitch source, run (`autonote`) (2026-11-13, the VC-2 mode's step 1c)

(v226: the input stage, Auto Note's setters, the getter and the bend ported bit for bit, every instruction of the live
paths read: HARDWARE_FIRMWARE.md, "Auto Note and the pitch source". What the run below didn't show: a key held under
Switch 1 clears Auto Note's state with no note-off of its own, as the key's note-on has already asked for the release.)

**Kind 6 is always in mode 0, the pitch mode.** The mode setter `0x893ec890` has no caller: no `bsr`, `bra` or pool word
in the image reaches it. The mode word `0x8993fe54` has one other writer, the mic ring's reset `0x893ec7d0`, which, when the
word isn't 0, makes it 0 and sets the pitch flag `0x8a0f73d8`. The flag has no other writer. **Run**
(`vc2run <rom.4> autonote`, `vc2autonote.cpp`): after the vocoder module's init entries (`0x898c609c…0x898c6108`) the word is
2 and the flag 0; after the ring's reset, 0 and 1. So a kind 6 voice sings at 440 × P+0 whatever the mic sings ("Voice kind 6,
run": T_out = 44100/(440·P+0)), and mode 1, the interval, is dead code.

**P+0 is the note's pitch.** The voice routine's kind 6 case (from `0x893af2d0`; read) builds P at the voice + 0x104 as kind
5's does. P+0 is the voice's + 0x1cc: exp2 of the base pitch and its modulation terms with the portamento's value
(`0x893b1160`) and a further offset added, clamped, times the voice's +0x60 and +0xa6c (v232, ported: the ratio on the
kinds' note-on's pool, its lower bound kind 6's own word; the further offset is the wave's, +0xa70; HARDWARE_FIRMWARE.md,
"The VC-2's OSC voice's note-on and targets"). Then `0x893ec700(voice + 0xafc, P)`
copies P+0, +4, +8 and +0xc into the kind 6 object and starts it (a tail branch to `0x893ec8d0`). The voice's +0x60 is the
converter's block +0xc: the converters by kind (VOCODER's `0x8939db00` at `0x8939dc22`, and at `0x8939d168`, `0x8939e3ba` and
`0x8939ec1a` the other three, which kind each is not mapped) take powf(2, the pitch) and multiply it by **the pitch-source
getter `0x89394a40`'s ratio** (read). So the pitch comes from the note, a key's or Auto Note's, bent by that ratio.

**The getter** reads one object for the part, `0x89989748`: +0 the note Auto Note is sounding (−1 for none), +4 how many other
keys are down, +8 a depth. With a note at +0 it returns Auto Note's ratio, `0x8993fe4c`; with none, `0x893ebc70(+8)`, a bend
by the singer's movement: 1 − depth × clamp(P_tracked − P_smoothed, ±20)/P_C4, P_smoothed following the tracked period by 0.1
a call from C4's (all periods in samples). **Run** (the getter called each block, the tracked pitch stepping from 220 to
247 Hz): depth 0 gives 1 throughout; depth 0.5 gives 1.029 and depth 1 gives 1.058 twenty blocks after the step, back to 1.0
by the hundredth. So a key's note takes the singer's pitch movement (scoops, vibrato), not the singer's pitch. What sets +8
is not traced (robot human depth, by its name, is a candidate).

**Auto Note** runs in the input stage (`0x893ebb00`, after the tracker's block). Its On Switch's setter `0x893ebc20` keeps 0
to 2 at `0x8993fe50`, so the third, empty name is a third state. The state is the note at `0x8993fe3c` (−1), its period
`0x8993fe40` and the ratio `0x8993fe4c` (1). **Run**, the tone generator's note-on, note-off and keys-held calls (`0x89391dc0`,
`0x89391e20`, `0x89391db0`) answered and logged, half a second each of 220 Hz, 330 Hz and silence:

| Switch | Keys down | Notes posted | Ratio after 220 Hz, 330 Hz, silence |
|---|---|---|---|
| 0 | none | none | 1, 1, 1 |
| 1 | none | note-on 60 at the first block | 0.84090, 0.63068, 0.63067: C4 × it 220.00, 165.00, 165.00 Hz, the tracked pitch |
| 1 | one | none | 1, 1, 1 |
| 2 | one | note-on 60 at the first block | as with 1 and none |
| 2, then set to 0 | none | note-on 60, then note-off 60 at the next block | 1 after the off |

- **Auto Note plays C4 and bends it to the sung pitch**, with no step to a semitone: its ratio is C4's period (168.5615
  samples, set by the init entry `0x893ebe50`) over the tracked period, block by block, held through silence. The converters
  multiply it into the note's pitch, so the note sounds at the sung pitch.
- **The semitone path is dead.** The path that snaps the tracked period to a note (`0x893ebde0`, a binary search of the
  128 periods 44100/f(note) at `0x89561150`, note 0 at 5393.97) and posts it runs only when `0x893ef380` says the detector
  (`0x893ef400`) found a new note. The detector runs only when `0x8993fe48` is set, and nothing sets it: it starts at 0, and its
  one writer, `0x893ebc60`, the setter the vocoder converter hands the Legato Switch, stores 0 whatever it's given. The
  detector's flag `0x899d9d34` is written only inside the detector, and its reset `0x893ef3a0`, the note-60 starter
  `0x893ebda0` and the state reset `0x893ebd20` have no caller. So the Legato Switch does nothing here.
- **Switch 1 against 2** (read in the tone generator's note-on `0x89392da0(source, part, key, velocity)`, where source 4 is
  Auto Note's, and in the input stage; the run shows the input stage's half). With 1, the input stage posts nothing while a
  key is down, and a key's note-on releases a sounding Auto Note note. With 2 it posts regardless. Under either, the tone
  generator drops a source-4 note while a key is down, puts the key at the object's +0 otherwise, and counts the other keys at
  +4.
- The notes go to the tone generator through an OS mailbox (`0x89301830`, `0x8994fec8`), the note plus 12 in the message.
  What the receiver makes of the 12 is not read.
- In this run the tracker followed 220 → 330 Hz to **165 Hz**, an octave low, and the tracker's own run shows it too ("The
  pitch tracker, run"). So Auto Note sings what the tracker hears, octave errors included.

### The pitch tracker, run (`track`) (2026-11-13, the VC-2 mode's step 1c)

Found by what loads its period, `0x899d9bd0`: a module from `0x893edca0` to `0x893ef400`. Its two setters are in the init
table (`0x898c60a8`): `0x893edf90` (44100/440 at `0x899d9b70`, and the two objects at `0x899d9ae8` and `0x899d9b2c` built by
`0x893efc30`) and `0x893ef240` (the tables at `0x899d9b78…0x899d9b9c`, and 881 at `0x899d9ba0`, the length the block entry
counts its input to). The reset, `0x893ee040` (called by the glue at `0x893b1682`, beside the mic ring's `0x893ec7d0`), clears
the state and calls the mode setter `0x893ee100(mode)`. Its four modes each set two frequencies as 2πf/44100 (`0x899d9bc8` and
`0x899d9cb8`, `0x899d9bcc` and `0x899d9cb4`; mode 0 60 and 400 Hz, 1 20 and 100, 2 100 and 300, 3 200 and 800), a start period
(`0x899d9bd0`, `0x899d9bd4`: 220.5, 551.25, 441 and 88.2 samples, 200, 80, 100 and 500 Hz) and two tables (`0x899d9cd8`,
`0x899d9cdc`). **Only mode 0 is used:** the reset passes 0 (`0x893ee04a`), the setter's one call is the reset's `bsr` (every
`bsr` and `bra` in the image searched), and no word in the image is its address.

A block goes in through `0x893ee280(samples, n)`, called by the input stage `0x893ebb00` with 32 samples the glue makes
(`0x893b1880`): each frame's int32 word at +0x14 × 2^-31 (`0x30000000` at `0x893b1a88`), so 1 is full scale. The word at +4,
scaled the same, is what goes into kind 6's mic ring (`0x893ec820`); which inputs the two words are is not traced. The block
entry returns the period in fr0 and leaves it at `0x899d9bd0`, where `0x893ee010` reads it. When the word at `0x8993fe48` is
set, the input stage gives the block to `0x893ef400` first; not run.

`vc2run <rom.4> track` (`vc2track.cpp`, 4 s): the two setters, then for each case the reset, the mode, and a second (1400
blocks of 32) of a tone of four harmonics (each 0.6 of the one below; its peak about the level given), white noise or
silence, the period read after each block. A run that fails only on a fault: it measures, against no model. In mode 0:

- **Reach**, from the reset's 200 Hz at 0.5: every note from 65.4 Hz (C2) to 1480 Hz (F♯6) settles on the tone's pitch,
  within 0.02%. At 61.7 Hz and at 1568 Hz the period stays 220.5 exactly.
- **How soon**, to stay within 2%: 6–15 ms from 196 Hz to 1 kHz, 16–69 ms below 196 Hz (the longest at 65–78 Hz), and
  14–42 ms from 1.1 kHz up to 1480 Hz.
- **Level** (110, 220, 440 and 880 Hz, from −66 dB of full scale by 6 dB): the lowest level that settles is −36 dB at 110 Hz,
  −54 at 220 (where it ends 0.5% high), −48 at 440 and −18 at 880; a step below, the period doesn't move. The threshold depends on the pitch, so it is
  not a plain gate on the input's level; what sets it is not read.
- **Noise and silence** leave the period where it is: noise at 0.5 gives no pitch.
- **Holding**: after a second of 220 Hz, a second of silence, of noise, or of the same tone 54 dB lower (0.001) leaves it at
  220.00 Hz. The last pitch is held; it never goes back to the start.
- **Following**: 110 → 330 Hz in 48 ms, 330 → 110 in 73 ms, 220 → 440 in 20 ms, 440 → 220 in 24 ms. 110 → 880 Hz is not
  followed in a second (it ends at 110.91 Hz), where 880 Hz from the reset's 200 Hz settles in 7 ms: how far it reaches
  depends on where it is. Added with Auto Note's run: 220 → 294 Hz (a fourth) in 17 ms and 330 → 220 in 12 ms, but **220 →
  330 Hz, a fifth up, ends at 165 Hz, an octave low**. ~~Which of the estimator's rules picks it is not traced~~ Traced in
  v224 on the port (HARDWARE_FIRMWARE.md, "The pitch tracker's analysis"): not the estimator but the onset check, which,
  locked, refuses a crossing sooner than 0.7 P after the last of its direction; one period of 330 Hz (134 samples) is under
  0.7 × 200.45, so every other crossing goes and the candidate is two periods. An octave up escapes it because the band-pass
  leaves the new pitch a third of the old peak, and the loud last frame clears the lock.

Modes 1 to 3, which nothing selects, on the grid of nine pitches (55…880 Hz) at 0.05 and 0.5: mode 1 reaches 55–165 Hz and
not 220 up, mode 2 110–440 and neither 82.5 down nor 660 up, mode 3 330–880 and not 220 down; each holds its start on noise,
on silence and at 0.001. So the two frequencies a mode sets are not its limits: mode 0's are 60 and 400 Hz, and it reaches
65.4 to 1480. ~~The analysis under the block entry (`0x893ee570`, `0x893ee7d0`, `0x893eeb90`) is not read~~: read, "The pitch
tracker's analysis", which says why. The tracker's port (the plan's step 3.6) will be held to these routines in a harness,
and this table is what its first run should give.

### The pitch tracker's analysis (2026-11-13, the VC-2 mode's step 1c)

(v223: the frame, the band-pass and the per-sample part below, ported bit for bit: HARDWARE_FIRMWARE.md, "The pitch
tracker's frame". v224: the rest, the crossings, the onset check, the estimator and taking, read instruction by
instruction and ported bit for bit: HARDWARE_FIRMWARE.md, "The pitch tracker's analysis", which states every rule this
section gives in outline; two statements below are corrected by it.) Read from the listing (`sh4dis.py` on rom.4, `0x893ee280…0x893ef200`), and each part marked **run** held to the firmware by
`track`'s last section (`vc2trackstate.cpp`: the settings after the reset, and the state after nine feeds). Levels are the
input's units (1 full scale); the settings are 16-bit sample values over 32768. It is a time-domain tracker: zero crossings of
a band-passed signal, timed to a fraction of a sample, validated by their peaks, two half-periods making a period.

**The band-pass** (the block entry's first loop): two state-variable filters in series, each low += f·band, then band +=
f·(in − low − band), damping 1. The first's high-pass (in − low − band) feeds the second, whose low-pass is the signal the rest
reads. f is 2πF/44100 itself, not 2 sin(πF/fs). Each f glides toward a target once a block, by (target − f)·n·0.0005 (1.6% a
block of 32, a time constant of about 45 ms). **Run:** the reset starts them at 20 Hz and 170 Hz and sets the targets to the
mode's (mode 0: 60 Hz high-pass, 400 Hz low-pass). When a period P is taken and lies between 73.5 and 441 samples (600 to
100 Hz; `0x899d9b84`, `0x899d9b80`), both targets move to it: the high-pass to f0/2 (2π/P × 0.5) and the low-pass to f0 + 25 Hz
(2π/P + 0.0035619, which is 2π·25/44100). **Run:** after 110 Hz the targets are 55 and 135 Hz, after 440 Hz 220 and 465 Hz,
and after 1480 Hz (P 29.8, outside the band's range) still 60 and 400 Hz. The last filtered sample is kept (`0x899d9be0`) so a
crossing between blocks is seen.

**Per sample** (the block entry's second loop): the largest |x| since the last crossing (`0x899d9c78`) and in the current
frame of 881 samples (`0x899d9c80`, moved to `0x899d9c84` at each frame's end), counters of samples since the last valid
crossing of either direction (`0x899d9cc0`) and of each (`0x899d9cc8` up, `0x899d9ccc` down); an upward crossing calls
`0x893ee570(previous, this)`, a downward one `0x893ee7d0`. When the period has gone longer than the longest candidate without
a valid crossing (`0x899d9cc0` ≥ `0x899d9cdc`), `0x899d9bd4` takes the period.

**Noise** (after the loop): the raw input's zero crossings in the block whose sample passes 50/32768 are counted, and
`0x899d9d14` = ⌊count + ½·itself + ½⌋. Above 25, no crossing is analysed in the next block; the estimator also refuses some
candidates above 7 and 10. **Run:** noise at 0.5 leaves it at 30; tones at 1 to 3.

**A crossing** (each direction its own copy of the same state, `0x893ee570` and `0x893ee7d0`):
- its peak, the largest |x| since the last crossing, must pass 0.68 of a peak that decays by 0.68 at each crossing it
  doesn't pass (and at each crossing more than the longest candidate after the last), or the crossing is not valid;
- `0x893eea50`, only when locked (more than 3 agreeing estimates, below): a peak over 0.005 and over 2.3 times the recent
  peaks unlocks and clears the failures, as an onset; otherwise a crossing sooner than 0.7 P after the last of its direction
  and sooner than 0.8 of the last candidate is ignored, so a harmonic's extra crossings don't count (sooner than 0.7 P alone
  halves the lock); and a crossing whose peak failed counts after all if it comes at least 0.85 P after the last of its
  direction when the recent peaks are over twice its own, 0.9 P otherwise;
- a valid crossing is timed to a fraction of a sample, |previous| / (this − previous) after the sample before it;
- crossings alternate: a flag (`0x899d9c64`, 1 after an up, 0 after a down) sends a crossing after one of the other
  direction to the estimator (`0x893eeb90(0)` up, `(1)` down) and one after its own direction to a history of the half
  intervals instead (`0x899d9cfc…0x899d9d08`).

**The estimator** (`0x893eeb90`): the half interval since the last valid crossing (whole samples plus this crossing's fraction
less the last's) goes into the four-deep history, and the candidate is it plus the half before it. It returns 0 (no estimate)
when the recent peak (`0x899d9c7c`, the largest of the last peaks) is under 50/32768 (**run:** 220 Hz at −60 dB peaks at
0.00085 and never passes; at −54 dB it passes early and then holds), when the candidate is outside 29.4–678.46 samples
(1500–65 Hz; `0x899d9cd8`, `0x899d9cdc`, set by the mode: **run**, and the reach measured above, 65.4 to 1480 Hz, is exactly
it), or when this half and the one two back, of the same direction, differ by more than a factor of 0.75 to 0.9 (the factor
by level, noise count and lock). Past those, rules by level (200, 400, 600, 1000 and 3000 over 32768,
`0x899d9bb0…0x899d9bc0`), by period (63 and 245 samples, `0x899d9ba4`, `0x899d9ba8`) and by the up and down peaks' balance
refuse a jump the signal is too quiet for; ~~they are read in outline only~~ read in full in v224 (HARDWARE_FIRMWARE.md,
"The pitch tracker's analysis", the estimator's steps 2 to 7). One of them, read in full: unlocked, a candidate
longer than both the current period and 245 samples (below 180 Hz) is refused while the recent peak is under 400/32768
(−38 dB), which fits 110 Hz needing −36 dB from the reset's 200 Hz where 220 Hz needs −54 (not run in isolation). When the
two halves before this one disagree: locked for more than 30 estimates, the candidate stands if it and the last candidate
are within a factor 0.85 of each other and the last within 0.85 of P; unlocked, with the up and down peaks unbalanced and
the recent peak over 200/32768, the candidate if it lies in 0.85–1.3 P, or else whichever of it and twice either half is
nearest P; otherwise there is no estimate.

**Taking it** (`0x893ef110`, then the crossing routine): an estimate within ±10% of the last one counts toward the lock
(`0x899d9c68`, up to 256) and, locked, sets the smoothing (`0x899d9cd4`) to 1, a jump, and clears the failures. One that
isn't: unlocked, smoothing 1; locked, outside 0.25–1.5 P, smoothing ½ and the lock cleared; otherwise the smoothing stays.
Then P += (estimate − P) × smoothing, and the band's targets follow P (above). No estimate counts a failure (`0x899d9c6c`, up
to 256; locked, it sets the smoothing to ½); ~~at 256 failures and each one after~~ from the third failure on, each one
(withdrawn by v224's reading of `0x893ef110`: it halves at `failures >= 3`, and 256 is only the count's cap, past which it
halves too; the port's check holds it to rom.4), the lock halves, the high-pass's target goes back to the mode's own (and the
fallback takes P) and the low-pass's does not. **Run:** after 110 Hz then 880 Hz, 256 failures, the high-pass target
60 Hz and the low-pass's 136 Hz. Which direction's estimates move P is a state at `0x899d9c70`: both (2: the reset, and set
again by the estimator whenever it finds no lock), up only (0) or down only (1), taken from the first estimate after a 2; a
flag the estimator sets (`0x899d9c74`) hands it to the other direction. ~~That last is read, not run.~~ Run in v224's
check, and ported: the flag is set when, locked, the last candidate is within 0.96–1.07 P where this one is not.

So, measured against read:
- the reach, 65.4 to 1480 Hz, is the candidate's limits, 678.46 to 29.4 samples;
- the last pitch is held because nothing changes P but an estimate, and silence, noise (its count) and a quiet tone (the
  level) give none;
- 110 → 880 Hz is never followed because the low-pass stays at the old f0 + 25 Hz (136 Hz), where 880 Hz arrives at a peak of
  0.0065 and no estimate passes, and the fallback ~~after 256 failures~~ (from the third failure, v224) resets only the
  high-pass; from the reset the low-pass is
  at 400 Hz, and 880 Hz at 0.5 passes;
- the level a tone needs depends on its pitch through the band-pass and the quiet-jump rules.

### The part mixer, run (`mixer`) (2026-11-13, the VC-2 mode's step 1c)

**The mixer**, `0x893b1820(out)` (called once a block from `0x89395b0e`; the two call sites "The block driver" names,
`0x893b19b0` and `0x893b1b46`, are its two paths). Per block of 32 it:

1. reads the 32 input frames (`*0x8993eadc`): the word at +4 × 2^-31 into the mic buffer `0x8999f3d0`, +0x14 into
   `0x8999f450`;
2. clears two stereo buffers, `0x8999f2d0` (the vocoder's) and `0x8999f1d0`;
3. runs the pitch tracker's input stage (on +0x14), the mic ring (on +4) and, ~~with a vocoder engine~~ with none (v227,
   ported: the `movt`/`xor` pair inverts the test; with an engine the vocoder's driver runs it on the ring, so once a block
   either way; HARDWARE_FIRMWARE.md, "The part mixer"), the unvoiced detector;
4. walks the part's voices (`*0x899996d0`), each rendering into one buffer or the other by what its methods answer (read, not
   run). This is where rom.4's TVA, which renders one stereo pair ("The TVA in rom.4"), lands;
5. runs the vocoder (`0x893f0580`) on a modulator into the `0x8999f2d0` buffer, unless the bypass flag `0x89934550` is set;
6. writes each output frame, the bus's eight words (MAIN L, R, the Reverb and Chorus sends, MULTI L, R, DIR L, R, as
   XtSynth's frame), from three levels:

| Frame | Source | Level |
|---|---|---|
| MAIN L and R (the same) | the direct signal | MAIN's |
| Reverb send | the direct signal | Reverb's |
| Chorus send | 0 | |
| MULTI L and R (the MFX's input) | the vocoder's buffer | MULTI's |
| DIR L and R | not written | |

The direct signal and the vocoder's modulator depend on a flag `0x8993eacc`, set when zone 0's or zone 1's OSC 1 Type is
EXTIN (v227: or OSC 1's or OSC 2's, by how `0x89320a50` reads its arguments; unsettled, HARDWARE_FIRMWARE.md, "The part
mixer"). With the flag set, both are the raw mic, the input frame's +4 word. Without it, both come from the other buffer's
right channel (the modulator × 2^-31).

**The levels.** Three targets: MULTI's `0x8999f4d0`, MAIN's `0x8999f4dc` and Reverb's `0x8999f4e8`. Each has the level the
mixer has reached, at `0x8999f4d8`, `4e4` and `4f0`; each sample it adds (target − reached) × 0.0009375, the step worked out
once a block, so a level eases 3% of the way each block (a time constant of about 24 ms). The targets (`0x893b1ee0`, and the
tail of `0x893b1480`):
- MULTI = clamp₀¹(MFX Send + offset₂) × 0.025;
- MAIN = clamp₀¹(Direct Sound + offset₁) × 0.025, × 2^31 (2147483392) with the EXTIN flag;
- Reverb = Reverb Send × MAIN's target.

The settings are each ÷127: Direct Sound Level (0x195, the patch common) and **zone 0's MFX and Reverb Send Levels** (0x2e0,
0x2e2, the V-Synth's tone TVA sends, read here for the part). The vocoder converter (`0x893a72f0`) sets them through the setters
`0x893b1eb0`, `0x893b1ed0`, `0x893b1ec0` at each conversion, then recomputes the targets. Chorus Send (0x2e1) is not read. The
level routine `0x893b1480` (from `0x89360880`, which two routines call) reads the same three and also sets the EXTIN flag. It
is the flag's only writer: its setter and getter `0x893b1370` and `0x893b1380` have no caller. The offsets
`0x89997290` (Direct Sound's) and `0x89997294` (MFX Send's), with a third at `0x89997298` that no level reads, are three
16-bit sums × 1/127 (`0x893a2e50`, from `0x893a24d0`, beside the Matrix's getters; `0x893a2ee0` is in a handler table at
`0x898cd45c`). Which controls drive them is not traced.

**Suspend and resume.** `0x893b1390` (nested by a count, interrupts masked) sets the bypass flag, saves the three targets
(`0x8999f4f4…fc`) and zeroes them; `0x893b1410` puts them back and clears the flag.

**Run** (`vc2run <rom.4> mixer`, `vc2mixer.cpp`): the store set, the level routine, then the live mixer with an empty voice
list on input frames whose +4 word is 1/8 of full scale and +0x14 1/16, the vocoder's block call answered with L 1000, R −1000.
- **No EXTIN OSC; Direct 127, MFX Send 127, Reverb Send 64:** targets MULTI 0.025, MAIN 0.025, Reverb 0.0126. MULTI eases from
  0 (0.73 at the end of the first block, 7.1 by block 10, 24.996 by block 299, of 25); MAIN and Reverb carry the voices'
  buffer, empty here, so 0.
- **Zone 1's OSC 1 EXTIN; Direct 64, MFX Send 100, Reverb Send 127:** MAIN's target 0.50394 × 0.025 × 2^31 = 2.7055e7. MAIN L
  and R are 3.38186e6, the mic's 0.125 × that (the +4 word's, not +0x14's), and the Reverb send the same (Reverb Send 1).
  Chorus 0; DIR left as it was; MULTI eases to 19.685 (1000 × 0.7874 × 0.025).
- **Suspended:** the vocoder isn't called and MULTI is 0 at once (its buffer cleared); MAIN and Reverb ease out, 0.97 a block
  (×0.049 after 99). **Resumed:** the targets come back and the levels ease in from where they had got to.

So a full-scale direct sound at Direct Sound 127 sits at 0.025, −32 dB, in the frame's units (2^31 full scale); the vocoder's
output goes to the MFX's input at MFX Send × 0.025 of its own scale, which this run doesn't fix (the vocoder was answered here).


Found from what loads the vocoder block's IDs, then followed by hand in `sh4dis.py` listings (`ROM=` rom.4). ~~rom.4 is compiled
differently from rom.2: matching rom.2's routines instruction by instruction (pool displacements and branch offsets masked) finds
none of the tone code, so nothing here can be taken from a rom.2 address.~~ Withdrawn 2026-11-13: matched the same way over
every routine the ports name (`codepairs.py`), 416 are in rom.4 once, most of the tone's routines among them, moved whole by
about 0x143000; what differs is the code that reads parameters by ID. Which routines that first search tried is not recorded.
("The V-Synth's routines in rom.4, run".) The vocoder's own code below has no rom.2 counterpart. A Ghidra project of rom.4 is
in `build/tools/vc2/ghidra` (analysed, not yet used). Every address is rom.4's.

**How rom.4 reads a parameter.** As rom.2 does, with its own routines: a pointing routine aims a read object at the part's block
(`0x893207b0(obj, part)`) or at one OSC's (`0x89320a50(obj, part, osc)`), then the getter `0x89324350(obj, id)` returns the value.
An OSC's IDs are OSC 1's, read once per OSC with the object pointed at it. The IDs are built in registers (`mov #2; shll8;
add #-97` is 0x19f), so a search for literal IDs misses them; `idbuild.py` ("The tools") tracks those constructions.

**The converter, `0x893a72f0(?, part)`.** It reads the vocoder block in this order and hands the results on:

- Formant Shift (0x19f), 0 taken as 1, as (v − 64)/63 into a library call at `0x89506e54` with 2.0: by its arguments 2^((v−64)/63),
  an octave each way. ~~Not confirmed which library routine that is.~~ Run: powf ("HIRESO's formant warp, run").
- Vocoder Sound Level, Vocoder Depth, Unvoice Level and Unvoice Detect (+24), Unvoice Spread, Env Attack, Env Release, Formant
  Template, Hold Dump Time (÷127), Direct Sound Level (÷127). Unvoice Level ÷127 goes to `0x899d9968`, Direct Sound Level ÷127 to
  `0x8999f4e0` (setter `0x893b1eb0`). Where the others go has not been traced. (Since: it also reads zone 0's Reverb and MFX
  Send Levels into `0x893b1ec0` and `0x893b1ed0` and recomputes the part mixer's targets, "The part mixer, run".)
- Then, if either OSC's Type (0x2e5) reads 2, Auto Note On Switch, Legato Switch and robot human depth (÷127, to the setters
  `0x893ebc20`, `0x893ebc60`, and the Formant Template's table at `0x89540904`, 16 entries of 140 bytes, offsets the formant).
  Otherwise those are set to 0. So Auto Note and robot human depth belong to a mic played through an EXT IN-type OSC, not to the
  vocoder. (Type 2 is EXTIN by rom.4's text table; why the test is 2 and not 3 is not read yet.) Run since ("Kind 6's pitch
  source, run"): `0x893ebc20` keeps Auto Note's switch, 0–2; `0x893ebc60` stores 0 to the detector's word whatever it's
  given, so the Legato Switch does nothing.
- Last, Vocoder Sw and Vocoder Type: `0x893f0600(vocoder, (Type + 1) × Sw)`, and Stereo Process Sw to `0x893f06a0`.

**The vocoder object**, static at `0x89999758`, 0x5a78 bytes, built by `0x893f0490`. It holds two engines, each a C++ object with a
vtable, on a base class whose first five methods are pure virtual (`0x89506f24`):

| Engine | Vocoder Type | At | Size | Constructor | Vtable |
|---|---|---|---|---|---|
| HIRESO | 1 | +0x24 | 0x2d70 | `0x893f0a10` | `0x898cd664` |
| LEGACY | 0 | +0x2d94 | 0x2ce4 | `0x893f23c0` | `0x898cd73c` |

`0x893f0600` takes 0 (off: no engine), 1 (LEGACY) or 2 (HIRESO), puts the engine's address at +0x20, calls its method 0 (a
reset), and installs a hook at `0x899d9d8c` that the block driver calls on the input pointer. LEGACY's hook does nothing
(`0x893f0480` is a bare `rts`). HIRESO's (`0x893ed420`) moves the pointer 128 samples back in a 640-word ring (`0x8a0f73dc`, index
`0x899d9a70`): HIRESO's own latency, compensated on the input.

The methods, by vtable slot (a vtable entry is 12 bytes, the function at +8; slot k is called through vtable + 12 + 12k):

| Slot | HIRESO | LEGACY | Called by the block driver as |
|---|---|---|---|
| 0 | `0x893f0b70` | `0x893f24b0` | reset, on a mode change |
| 1 | `0x893f0c50` → `0x893ed930` | `0x893f2570` → `0x893ef740` | analysis of the input (the modulator) |
| 2 | `0x893f0c60` | `0x893f25a0` | synthesis, mono driver |
| 3 | `0x893f1240` | `0x893f2980` | synthesis, stereo driver |
| 4 | `0x893f15d0` | `0x893f2f60` | not called by the drivers |
| 5 | `0x893f13e0` | `0x893f2f90` | not called by the drivers |
| 6 | `0x893f1960` | `0x893f2f80` | not called by the drivers |
| 7 | `0x893f1a00` | `0x893f2fc0` | after synthesis; its float result scales the output stage |

Slot 1 jumps to a plain function on globals, not on the object: the analysis keeps its state in `.bss`. HIRESO's (`0x893ed930`)
counts frames mod 14, runs three filter calls (`0x893efd20`, `0x893efdb0`, `0x893eda90` ×3), smooths a level in 16 steps, and
for each band takes a weighted sum of squares over a 128-entry row of `0x8a1bc0cc`, kept in a 14-frame ring of 56-byte rows.
~~That reads like band energies from a spectrum.~~ Withdrawn the same day: the rows hold each band's own samples from a bank of
resonators, and the weight is the voiced gate; HIRESO has no FFT ("HIRESO", below). Its constructor sets a band range
(fields 0, 22, 28, 32 at +4…+0x10), four 68-byte filter objects at +0x2ba4 (`0x893efc30`), two on the table `0x895616d8` and two
on `0x89561700`.

**The block driver** (32 samples): `0x893f0510(vocoder, input, output)` and `0x893f0580(…)`, the second with null checks between
the calls. Each runs `0x893ed040(input)`, the hook, slot 1 (input), slot 2 or 3 (output), slot 7, then `0x893ed510` or `0x893ed690`
(output, the object's +0x14 level × a constant). Both are called from the part's block mixers at `0x893b19b0` and `0x893b1b46`
(in `0x893b1300…0x893b1bd0`): each copies the mic input, skips the vocoder when `0x89934550` is set, and mixes onto a 32-byte frame
per sample with levels ramped across the block from `0x8999f4d0…0x8999f4fc`. (Run since, "The part mixer, run": the two are
one mixer's paths, `0x893b1820`, chosen by whether an EXTIN OSC is in zone 0 or 1; the frame is the bus's.)

**The settings path (third pass).** The converter builds a settings struct on its stack (its sp + 16) and passes it to
`0x893b1be0`, which calls `0x893f06a0(vocoder, settings)`: +0x00 and +0x04 the formant ratio; +0x08, +0x0c 0, or in the EXT IN
path values offset by the Formant Template's table; +0x18 Vocoder Sound Level ÷127; +0x1c Unvoice Level ÷127; +0x20 Unvoice
Spread; +0x24 Unvoice Detect + 24; +0x28 Env Attack; +0x2c Env Release (0…100 each); +0x30 Formant Template; +0x34 Vocoder
Depth ÷127; +0x38 Hold Dump Time ÷127. `0x893f06a0` copies +0x00…+0x0c into the vocoder's +4…+0x10, writes Unvoice Level ×
Depth × (its +0x1c) to its +0x18, gives the input detector {Unvoice Detect + 24, Unvoice Spread, Vocoder Sound Level} through
`0x893ed030` (to `0x899d9978`, `0x899d997c`, `0x899d9980`), then calls the engine's **slot 5** with the struct: slot 5 applies
the settings.

**The input stage, `0x893ed040(input)`: the unvoiced detector.** It reads the block and writes nothing back to it:

- three biquads, two from `0x895613a0` (10 floats) and one from `0x895613c8` (5 floats), states in `0x899d9988…`. By their
  responses at 44.1 kHz the first two make a highpass near 150–200 Hz (−20 dB and more below 60 Hz), and the third a notch near
  1 kHz with +2.3 dB above it;
- the block's peak after the highpass, and a running maximum over the last eight blocks (this one and the seven before;
  "five", as this line said before v208, is withdrawn: the code takes the maximum over `0x899d99a4…0x899d99c0`, eight words,
  and `Vc2Unvoiced` equals rom.4 with eight in xt_vc2in, a plant of seven caught); 1/256 of it (at least 0.002) is a
  hysteresis;
- a count of the third filter's output crossing zero, Schmitt-trigger style (it must swing past ± that hysteresis): a
  zero-crossing rate. The last four blocks' counts plus 0.75 of the previous total is compared with the threshold (`0x899d9984`:
  Unvoice Detect + 24 from the settings, and 29 once a block has been called unvoiced, so it holds); over it, the block is
  unvoiced;
- that 0/1 goes into a 20-block ring (`0x8a0f87dc`) and is read five blocks later, so the decision lags 160 samples. It is
  smoothed per sample into 32 floats (`0x899d99dc`, one a sample of the block), rising at 0.002 and falling at 0.001 a sample;
- beside it, the peak, a peak that decays 1% a block (`0x899d9a68`), and a level with its own attack and release
  (`0x899d9a6c`) set a gain; "a gain stored in `0x899d99c4`", as this line said before v208, is withdrawn: `0x899d99c4` is
  the block's count scaled by that gain (by the gain × 0.1 over the peak, or by 1.22), the first of the four the vote sums.
  The whole law, word for word: HARDWARE_FIRMWARE.md, "The VC-2's unvoiced detector" (v208).

Its state is reset by `0x893ecf60` (from the glue's init, `0x893b1688`): Unvoice Detect's store at 50, the rings cleared. It
leaves the second biquad's states and the smoothed decision as they were (v208: a port that cleared them differed from rom.4
at the first reset after a sound).

**The output stage, `0x893ed510(output, fr4, fr5)` (mono) and `0x893ed690` (stereo): the mic's consonants.** It keeps the mic
input in a 640-sample ring (`0x8a0f73dc`) and reads it 448 samples back, takes the unvoiced flag five blocks back × fr5 (slot 7's
result), smooths it per sample (toward 1 at 0.002, toward 0 at 0.01), gates the delayed mic with it, and passes that through a
two-state highpass (coefficients 0.35 and 1.65). With Unvoice Spread on it goes through two delay stages of gain 0.6 on the
buffers at `0x8a0f882c` and `0x8a15a2ac`. Then per sample:

(v209, ported bit for bit, `Vc2Consonants.cpp`; three readings above withdrawn there. The ring is written by the part mixer,
not here: it copies each block of the mic, its frame's second word × a unit under 2^-31, to `0x893ed410`'s pointer, at the
ring's index;
the output stage reads 448 back and moves the index on a block. The gate glides toward the flag × fr5, its rate chosen once a
block: 0.002 when the target is above the gate, else 0.01, so "toward 1" and "toward 0" hold only for fr5 = 1. "Five blocks
back" is the ring's index less 5, after the vote has moved it on: the flag of four blocks before the last. The delay stages
are two Schroeder allpasses, delays 280 and 162 samples, gain 0.6, each on a buffer that keeps every sample twice, 50,000
apart. HARDWARE_FIRMWARE.md, "The VC-2's consonants".)

out = engine's output × level + consonants × fr4,

where level is Vocoder Sound Level (`0x899d9980`) plus `0x89997298` (a modulation, not traced; v209: the third of three ÷127
values `0x893a2e50` writes from `0x893a24d0`'s shorts, at `0x89997290…98`), within 0…1, gliding 1/32 a block
(`0x8993fe5c`), and fr4 is the vocoder's +0x14 × 2^31. So Unvoice Level and Depth set how much of the mic's own unvoiced sound is
added (with HIRESO only: +0x14 stays 0 with LEGACY, see HIRESO's "Vocoder Depth"); Unvoice Detect sets how readily a block counts
as unvoiced; Unvoice Spread diffuses it. The stereo stage does the same into both channels.

### LEGACY: a 10-band channel vocoder

Read from the listings by a forked agent, then checked here: the band count, both coefficient tables, the band gains, the
response of every band (computed again), the loop bound and the follower's constants. Nothing here has been run.
(v210: the analysis and the hold, run: `Vc2LegacyAnalysis` equals rom.4 in xt_vc2legacy, and what is written below of them
holds; the unvoiced path is a one-pole lowpass, its first difference into a leaky one-pole, that one's first difference,
then the three sections. HARDWARE_FIRMWARE.md, "LEGACY's analysis". The synthesis and the slots are v211's to run.)

**Bands.** 10: the .data word `0x89940250` holds 10 and the synthesis loop's bound is a literal 10 (`0x893f265a`). Each band is
a second difference of its input, u = x − 2x[n−1] + x[n−2], then two two-pole resonators in transposed form,
y = c0·u + c1·y1 + c2·y2 and z = y + c3·z1 + c4·z2: five floats a band, from `0x89940184` (analysis) and `0x89940744` (carrier),
two identical copies of 50 floats. At 44.1 kHz the bands peak at 152, 219, 312, 468, 687, 1034, 1518, 2195, 3128 and 5307 Hz,
each +12.5 dB, about 0.29 × its centre wide at −3 dB (the top band 0.25): roughly half-octave spacing, as an analog vocoder's.
The carrier's band gains (`0x8994071c`) are 1.0 but for the top band's 0.8.

**Analysis** (slot 1 → `0x893ef740`, state in .bss from `0x8a1bd9d4`, init `0x893ef6a0`). Per sample and band: the band filter,
then a peak follower on its output, not rectified (z ≥ e: e = 0.937477·e + 0.0568395·z, else e = 0.993973·e, constants in the
routine's pool: 0.35 ms attack, 3.75 ms release, fixed: no setting reaches them), then the hold (below), then a two-pole
lowpass of unity gain, −3 dB at 50 Hz. Each band's 32 samples go to a row at `0x8a1be198`, read through `0x893efa00(band, n)`.
Row 10 is the unvoiced signal: the input through a one-pole lowpass, two first differences and three (1 − z⁻¹)² sections, an
eighth-order highpass, −3 dB at 6.4 kHz and −95 dB at 1 kHz by the agent's computation. The raw signal, not an envelope.

**Synthesis** (slot 2 mono `0x893f25a0`, slot 3 stereo `0x893f2980`), in place on the output buffer, which holds the carrier:
per band, the first resonator on the carrier's second difference, ×0.5 × the band's envelope, then the second resonator, × the
band's gain. The carrier's level is followed too (0.998489·l + 0.000755287·|x|, 15 ms). Then

x = 12 · (sum of the bands) + 16 · (carrier level) · (unvoiced row),

so the mic's highs come back shaped by the carrier's loudness (LEGACY's own unvoiced path: the shared output stage adds none
for LEGACY, below), and the sum goes through a fixed output EQ of six sections:
−26 dB at 100 Hz, +5.5 dB at 1 kHz, +5.9 dB at 3 kHz, −22 dB at 15 kHz. Its first section's coefficients move with a value the
engine derives from the Formant Shift ratio (slot 4: p = (ratio − 0.0156)/1.9687; a glide toward 1/(p² + 0.4966) at 1% a
sample, `+0x2ce0`). By the agent's computation the chain then peaks at 733 Hz with the ratio at ½, 810 Hz at 1 and 1.9 kHz at 2.
**LEGACY's Formant Shift is a tilt of the output EQ: its bands never move.**

Mono writes the left channel only, buffering each band's 32 samples (rows at +0x004) before summing; stereo runs left and right
through their own states as it goes, and scales the carrier's level by slot 7's value every sample, which mono doesn't.

(v211: the mono synthesis, the object and its slots but the stereo synthesis, run: `Vc2Legacy` equals rom.4 in xt_vc2legacy,
and what is written here of them holds, but one term: "16 · (carrier level)" is 16 × the object's row 10, which holds the
level as it is plus as it was the sample before, so about 32 × the level. The constructor sets p to 0.5000254 and its glide
to 0.5; the reset (slot 0) sets the glide and leaves p. HARDWARE_FIRMWARE.md, "LEGACY's synthesis". The stereo synthesis is
v212's to run.)

(v212: the stereo synthesis, run: `Vc2Legacy::stereo` equals rom.4, and what is written above of it holds. Besides: its
row 10 gives both channels the left carrier's level, so the right's unvoiced term rides on the left's loudness; the
right's level is followed into its own word and nothing reads it. HARDWARE_FIRMWARE.md, "LEGACY's stereo synthesis".)

**Hold** (slot 6 → `0x893efa70(v)`, slot 7 → `0x893efbe0`). A 0…127 controller v: with f = (v − 64)/63 × Hold Dump Time/127,
below 64 nothing is held (slot 7 returns 1); above, the held envelope h = h·(1 − C) + e·C follows with C from the decreasing
table `0x89561538` at 50·f + 51 (time constants from about 45 ms to 16 s), freezing past f = 0.99, and slot 7 returns 1 − f.
Letting go glides back over 100 blocks. Slot 6 is reached from `0x893b1c10` → `0x893f0770`, called from a handler at
`0x89393980` that compares a 0…127 value with 64: by that, a pedal or switch, probably the Hold pedal. ~~Not proven.~~ Run: it is
CC 64's handler, and CC 121 sends it 0 ("The hold's controller, run").

**Slots.** 0 reset (`0x893f24b0`); 1 analysis; 2 and 3 synthesis; 4 the formant parameter p (`+0x2cdc`); 5 settings, which
reads only +0x00 (the formant ratio) and +0x38 (Hold Dump Time); 6 the hold controller; 7 the hold level. The base class's 5 and
6 do nothing and its 7 returns 1.0. **So with LEGACY, Env Attack and Release, Formant Template, Vocoder Depth and the three
Unvoice settings do nothing**: its own code doesn't read them, and the shared output stage's consonant gain is 0 for LEGACY
(see HIRESO's "Vocoder Depth"). Vocoder Sound Level still sets its output level there.

The object (0x2ce4 bytes): band outputs and the carrier level in 32 rows of 0x100 from +0x004, band filter states from
+0x2004 (32 × 24 floats), the carrier's two past samples and level at +0x2c04…+0x2c18, two sets of output EQ states at
+0x2c1c…+0x2cdc, p at +0x2cdc and its glide at +0x2ce0.

### HIRESO: a 32-band multirate channel vocoder

Read from the listings by a second forked agent, then checked here: the band centres, the three groups (counts, strides,
scales), the envelope windows, the formant templates, and the Depth reader's callers. Not run.

**No FFT.** Neither side has a butterfly, bit reversal or twiddle table: both are banks of two-pole resonators, in three groups
at three rates. The fields 0, 22, 28, 32 are the groups' band edges:

| Group | Bands | Rate | Samples a block | Analysis coefficients | Carrier coefficients |
|---|---|---|---|---|---|
| 0 | 0–21 | fs/4 | 8 | `0x8993fe84` | `0x89940278` |
| 1 | 22–27 | fs/2 | 16 | `0x89940094` | `0x89940494` |
| 2 | 28–31 | fs | 32 | `0x89940124` | `0x89940530` |

(.data tables, read through the copy; counts `0x8993fe60` {8, 16, 32}, strides `0x8993fe6c` {4, 2, 1}, energy scales
`0x8993fe78` {1.2, 0.7397, 0.4956}.) The rates change through 8th-order IIR decimators and interpolators, objects of 68 bytes
(`0x893efc30`; coefficients `0x895616d8` and `0x89561700`, 10 floats each): `0x893efd20` 32→16 and `0x893efdb0` 16→8 on the way
in, `0x893efef0` 8→16 and `0x893efe40` 16→32 on the way out, with stereo versions.
(v214, run: "8th-order" and "10 floats each" are withdrawn. The decimators are three cascaded biquads after an input gain,
ten coefficients; the interpolators the same and a one-pole after, eleven, so `0x89561700` runs eleven floats, to the
templates at `0x8956172c`. The analysis's two decimators are on a third table, `0x895613f4`, which its init gives them;
`0x895616d8` and `0x89561700` are the object's. The carrier's own decimator is `0x893efc80`, which mixes each frame's two
words × 0.5 first. HARDWARE_FIRMWARE.md, "HIRESO's rate filters".)

The 32 band centres (`0x8994069c`) are 100 Hz to 1400 Hz in steps of 130, then steps widening from ×1.097 to ×1.184, up to
11874 Hz: linear where the voice's formants are, wider above.

**Analysis** (slot 1 → `0x893ed930`, .bss globals). Per block: frame count mod 14; the input decimated to half and quarter
rate; per group a first difference (pre-emphasis) and per band two resonators, one at each edge of the band, with gains of
opposite sign (`0x893eda90`); then per band an energy, scale × Σ (band sample × gate)², into a 14-frame ring. The gate is the
shared detector's: `0x893ed3e0` gives "voiced" when neither the flag 1 block back nor 5 back says unvoiced, and it glides at
0.001 a sample rising, 0.01 falling. So unvoiced blocks drop out of HIRESO's band envelopes. The envelope (`0x893edd10`, called
by the synthesis) is the square root of the largest energy in the last N frames, N from `0x8956141c`: 14 for bands 0–25, then
~~13, 12, 11 and 10 for the top three~~ 13, 12, 11, 10, 10 and 10 for bands 26–31 (v215, the table read whole; the rest of this
paragraph run and held: HARDWARE_FIRMWARE.md, "HIRESO's analysis"): about 10 ms.

**Synthesis** (slot 2 mono `0x893f0c60`, slot 3 stereo `0x893f1240`). The output buffer's carrier is decimated the same way and
run through the carrier bank (`0x893f1a10`): 2n + 1 resonators a group, a centre one per band and edge ones shared by
neighbours, band b = edge(b − 1) + centre(b) + edge(b), so overlapping bands sum flat with alternating signs. Per band and block:
the target is the analysis envelope at a fractional band index (the formant map, linear between neighbours), through the hold,
ramped across the block, followed with the attack coefficient when rising and the band's release coefficient when falling
(scaled by the group's rate), and out += carrier band × follower × template gain × W_b, where W_b = (−1)^(b+1) · A_b · B_b ·
50/(A_31 · B_31) from `0x8994059c` and `0x8994061c` (built in the constructor). The groups are interpolated back up and summed.
Stereo does the same per channel, each with its own formant map, template curve and follower state.
(v217, mono run and held: as above. The carrier bank works on the carrier's first difference. Attack or release is
chosen by the last target against the follower. Group 2 joins the sum a sample late, and the frames' second words are
left as they were. HARDWARE_FIRMWARE.md, "HIRESO's mono synthesis". v218: the stereo run and held as well, each channel
the mono arithmetic on its own side's map, template gains and band words: "HIRESO's stereo synthesis".)

**Formant Shift moves the bands here** (slot 4, `0x893f15d0(this, side, ratio)`): each synthesis band reads the analysis
envelope from the band its frequency maps to under the ratio, warped around a pivot of 500 + 172.27 × p Hz (p from the
template's record, 12 by default), with separate branches for ratios over 1.05, under 0.95 and between. ~~The exact warp is not
read.~~ Run and held to a model bit for bit: "HIRESO's formant warp, run". `0x893f07a0` (from the glue, `0x893b16f4`) ~~calls it
four times, for each side~~ has four call sites, two paths of two: it calls it twice a block, once for each side.

**Formant Template** (slot 5): 32 records of 140 bytes at `0x8956172c`, each three floats (64, 12, 64 in record 0; the second is
the pivot p) and 32 band gains, record 0 flat. Template T gives the left channel record T and the right record T + 16; the two
are the same for T = 0–8 and 10, different for 9 and 11–15. Templates 16 and up would be curves in RAM (`0x899d9d98`,
`0x899d9e18`) written from an edit page (`0x893f2290`, called from `0x8947a604`…); the parameter stops at 15. The converter's
table `0x89540904` is records 0–15 again, byte for byte.

**Env Attack and Release** (slot 5): Attack 0…100 picks a coefficient from `0x895628d0` (101 floats, 0.2275 at 0 to 2.8e-5 at
100); Release picks from `0x89562a64` (101 floats falling about ×0.887 a step, 0 by about 90), but bands 20–31 never release
faster than a cap of their own from the same table: the high bands are held back. The constructor's defaults: Attack 6,
Release 18, Template 0, Depth 1.0, Hold Dump Time 1.0.

**Hold** (slots 6 and 7): as LEGACY's, on the envelopes: from 64 up the envelopes follow at 32 × the curve's value at
51 + 50x (x = (v − 64)/63 × Hold Dump Time), freeze past 0.99, and glide back over 100 blocks after; slot 7 returns 1 − x.
(v216, run and held with the Formant Template and Env paragraphs above: the curve is Env Release's, `0x89562a64`; below
64, x < 0, the level is 1 and the hold's coefficient 1; the glide back is the synthesis's, not yet run. HARDWARE_FIRMWARE.md,
"HIRESO's object".)

**Vocoder Depth.** HIRESO stores it (+0x2d44), and its own reader (`0x893f22d0`, averaging the envelopes over groups of
trunc((1 − Depth) · 31) + 1 bands) has no caller: no word holds its address and no branch in the module reaches it. Depth does
act through the vocoder object instead: `0x893f06a0` puts Unvoice Level × Depth × (+0x1c) at the vocoder's +0x18;
`0x893f0600` sets +0x1c to 1 for HIRESO and 0 for LEGACY; `0x893f07a0` glides +0x14 toward +0x18 × a per-block value each
block; and the output stage adds the mic's consonants at +0x14 × 2^31. **So Unvoice Level and Depth set HIRESO's added
consonants, and do nothing with LEGACY**, which has its own unvoiced path.

The object (0x2d70 bytes): band edges at +4…+0x10, the carrier coefficient pointers at +0x14…+0x1c, carrier band signals from
+0x24 (32 rows of 256 bytes), carrier filter states from +0x2024, the held, ramp and follower envelopes per side from +0x2724,
W_b at +0x28a4, the formant maps at +0x2924 and +0x2b24, the four rate filters at +0x2ba4…+0x2c70, the pivots at
+0x2cb4/+0x2cb8, attack at +0x2cbc, the 32 releases from +0x2cc0, then Depth, the template pointers, Hold Dump Time, the hold
value, gain and coefficients to +0x2d6c.

### The carrier: the VOCODER OSC

rom.4's OSC dispatch is `0x8939fb40(input, block)`: input +8 the OSC Switch, +9 the Type.
ANALOG and VOCODER both go through ~~the analog converter~~ `0x8939ced0`, which writes kind 3; VOCODER then writes **kind 5**.
(Withdrawn 2026-11-13, "The VOCODER OSC's IDs, run": `0x8939ced0` reads nothing of its input; it starts kind 3's block, the
kind, five gains of 1 and a time from a global at +0x60. The conversion is `0x8939fbd0(input, block)`, which the note-on
calls after the dispatch: the vibrato's fields, then a converter by kind, `0x8939db00` VOCODER's.) PCM is
`0x8939e120`, EXTIN `0x8939e940` (kind 4). The voice switches on its kind at +84 (0–6): setup `0x893ae0f0`, render
`0x893ae9f0`, update `0x893aeb20`. Kind 5's setup only stores a constant at voice + 0xa88; its update is the analog kind's
(`0x893aec40`, shared by 3, 5 and 6); its render is **`0x893ebfa0(voice + 0xa90, out)`**: 32 samples, the same value to left and
right,

- a naive saw: a phase in [−1, 1) stepped by an increment that glides halfway to its target each sample, times `0x899d9964`;
- through two two-state filter stages in series, each `s1 += g·s2; s2 += g·(x − s1 − s2)` with one coefficient g (+0x14): states
  +0x50/+0x4c, then +0x60/+0x5c on the first's s1, whose s1 is the saw's output;
- a noise from a linear congruential generator (state +0x64, +0x68), ×2^-31, through a one-pole (+0x48, coefficient +0x20), then a
  stage of the same form (+0x58/+0x54, coefficient +0x28) whose output is what is left after both states (a highpass, by shape);
- out = (noise + saw × the field at +0x1c) × a gain that glides to its target at 0.00756 a sample, × 2^31, the voice buffer's
  integer scale.

That is the Saw Wave LPF, Noise LPF and HPF and Saw-Noise balance of the OSC's own parameters, by shape. ~~Which field each
parameter lands in is not traced~~ (run: "The VOCODER OSC's IDs, run", each ID to its carrier field)~~, nor the branch taken when the field at +0x3c is positive (`0x893ec240`, which calls
`0x893f3280` and `0x893f3210`, likely the growl)~~. That branch, run: the growl, a subharmonic near 50 Hz ("The growl, run").
Kind 6 is written inside the EXT IN path (`0x8939ee5e`); its render is `0x893ec730(voice + 0xafc, out)`. ~~Not read.~~ Run: a
pitch shifter of the mic ("Voice kind 6, run").

One correction from the run: the carrier's pitch (+0x08, +0x0c, P+4) is a ratio to 440 Hz, and `0x899d9964`, which the step is
multiplied by, is 880/44100 (set by `0x893ec6a0`), not a modulation.

**The module.** The float-heavy code from about `0x893ea840` to `0x893f3000` (35 KB, 36 routines of 40 or more FPU operations) is
the carrier, the EXT IN kind 6, the pitch tracker's setters and both engines. The glue (converter setters, block mixers) is in
`0x893b1300…0x893b2100`.

## The parameter tables

rom.4 has rom.2's three per-ID tables, in rom.2's formats:

| Table | rom.4 | rom.2 | Form |
|---|---|---|---|
| Names | 0x898b5798…0x898b7e48, **2476 IDs** | 0x89790338, 2389 IDs | pointer per ID to a name with a leading space |
| Descriptors | 0x8950f0b0 | 0x897a167c | 12 bytes per ID: u16 byte offset, s8 width, u8 bit, u16 centre, default, min, max |
| Value text | 0x898b8154 | 0x89792ba0 | pointer per ID: names split by 1, a range by 2, the unit after a 3 |

How each was found: the names table from the pointer to " Vocoder Sw"'s string, walked both ways while the words point at names;
the descriptors as the place where rom.2's first 48 descriptors (576 bytes) occur byte for byte; the text table as the first pointer
after the names whose string is ID 0's `0\x02127`. The descriptors were checked by reading the VC-2's own block with them: every
field from 0x193 to 0x1a5 starts on the bit after the last one ends, and each range matches its text's count but two (below). ~~rom.2's second descriptor bank (8 bytes per ID
from 0x55d) has not been looked for in rom.4.~~ Found 2026-11-13: `0x895131a8`, 8 bytes an ID from 0x56a, right after the first
("The second descriptor bank, read").

**The IDs are not rom.2's.** rom.4 inserts ChordMemory Switch and Hold at 0x37, and more further on, so every ID from 0x37 up is
shifted. A `.p02` can't be read with rom.2's IDs or the plugin's descriptors.

rom.4 has 179 parameter names rom.2 lacks. Besides the reverbs' unnamed sub-parameters and some renamed system settings (MIDI MODE
for VariOS, USB External In Type, Pedal $ Assign, Volume Exp Level Ctrl), they are these:

### The patch's vocoder block, IDs 0x193–0x1a5 (patch common, after the Matrix)

| ID | Name | Range | Default | Values |
|---|---|---|---|---|
| 0x193 | Vocoder Sw | 0–1 | 1 | OFF, ON |
| 0x194 | Vocoder Type | 0–1 | 1 | LEGACY, HIRESO (a 4-bit field) |
| 0x195 | Direct Sound Level | 0–127 | 0 | |
| 0x196 | Vocoder Sound Level | 0–127 | 127 | |
| 0x197 | Vocoder Depth | 0–127 | 127 | |
| 0x198 | Unvoice Detect | 1–50 | 25 | |
| 0x199 | Unvoice Level | 0–127 | 40 | |
| 0x19a | Env Attack | 0–100 | 8 | |
| 0x19b | Env Release | 0–100 | 18 | |
| 0x19c | Auto Note On Switch | 0–1 | 0 | OFF, ON, and an empty third name (a 2-bit field) |
| 0x19d | Auto Note Retrigger | 0–1 | 0 | OFF, ON |
| 0x19e | Hold Dump Time | 0–127 | 100 | |
| 0x19f | Formant Shift | 0–127 | 64 | |
| 0x1a0 | Formant Template | 0–15 | 0 | |
| 0x1a1 | Split Point | 0–127 | 37 | C-1…G9 |
| 0x1a2 | Modulator Key Mode | 0–1 | 1 | NORMAL, ALWAYS-ON, HOLD |
| 0x1a3 | robot human depth | 0–127 | 0 | |
| 0x1a4 | Stereo Process Sw | 0–1 | 1 | OFF, ON |
| 0x1a5 | Unvoice Spread switch | 0–1 | 0 | OFF, ON |

0x192 is "Dummy" and 0x1a6…0x1ad "Reserv area". After them a two-band patch EQ, 0x1af–0x1b4 (EQ 1 and 2: Freq 0–26 as
50…20000 Hz, Q 0–5 as 0.5…8.0, Gain 0–30 as −15…+15 dB).

Two disagreements to settle on the corpus: Modulator Key Mode's text has three values but its descriptor stops at 1, so HOLD is
out of the descriptor's range; Auto Note On Switch has a 2-bit field and a third, empty name, its range 0–1. Which wins is for the
64 factory patches and the code that reads the field to say. (The patches: 0–1 both. The code: Auto Note's setter keeps 0–2, and
2 differs from 1, "Kind 6's pitch source, run"; so the third state is real in the code and out of the descriptor's range.)

### The VOCODER OSC type

OSC 1 Type (0x2e5; OSC 2's likewise) is ANALOG, PCM, EXTIN, **VOCODER** in rom.4, where rom.2's (0x2a5) stops at EXTIN. The
panel strings beside it read "Analog / PCM / Ext In / Vocoder", and the input choice "MIC IN / LINE IN". So the VC-2's voice is an
OSC type of the V-Synth's two-OSC patch, not a separate engine.

The parameters only rom.4 has, per OSC (OSC 1's IDs; OSC 2's follow each run):

- 0x354–0x361: Saw Wave LPF cutoff (default 90), Saw Wave Level (101), Noise LPF cutoff (12), Noise HPF cutoff (51), Noise Level
  (95), Dynamics Threshold (50), Dynamics Sensitivity (38), Growl Level (0), Growl Keyfollow (−200…+200), Growl LFO Depth, Growl
  Envelope Depth, Saw-Noise Mix Balance (64), Mix Balance LFO Depth, Mix Balance Envelope Depth. OSC 2's are 0x362–0x36f.
- 0x371–0x375: Harm Formant (−63…+63), its Keyfollow, LFO Depth and Envelope Depth, and Harm Formant Style (0–10). OSC 2's
  0x376–0x37a.
- 0x37c–0x37f: Vibrato Type (0–3), Vibrato Rate, Vibrato Depth, Wave start offset (0–15). OSC 2's 0x380–0x383.

A saw and a noise, each filtered, mixed by a balance, with growl, dynamics and vibrato: by the names, a carrier made inside the
OSC rather than one played from a wave. That is a reading of names, not of code. (Run since: "The VOCODER OSC's IDs, run".)

### The VOCODER OSC's IDs, run (`vocosc`) (2026-11-13, the VC-2 mode's step 1c)

`vc2run <rom.4> vocosc` (`vc2vocosc.cpp`, a few seconds; `build/tools/vc2/vocosc.txt`). rom.4's default patch made a store
(the `patches` run's helpers, shared through `vc2patches.h`), part 0's entry in the part table (`0x8950d8e0`: +0 a flag, 1
when the part has a store, +8 the store) pointed at it, both OSCs switched on. Each of IDs 0x354…0x383 is written into zone
0's tone block by its descriptor (big-endian bits, the value plus the centre), and read back by rom.4's own reader, which
agreed every time. It goes over its range in four steps, and rom.4's routines run as a note's do:

- **the OSC input builder** `0x893a04e0(out, part, zone, osc)` (called at `0x893a154e`, by part, zone and OSC): it aims a
  read object at the store's tone block (`0x89320a50(obj, part, zone)`: the part's store + 0xfe + zone·0x80) and reads the
  OSC's IDs through the getter (`0x89324350`) into a 128-byte record at out + 0x20 + zone·0x100 + osc·0x80 (the firmware's
  out is `0x89e371e0`, 0x1020 bytes a part). Each OSC's IDs reach its own record, at the same offsets for both;
- **the note-on** (`0x8939fe08`) copies the record to the converter's input +8, behind an 8-byte header (+3 key, +4
  velocity), then calls the dispatch `0x8939fb40(input, block)` and **the converter** `0x8939fbd0(input, block)`. The block is
  the voice + 0x54: the "+84" where the voice keeps its kind ("The carrier: the VOCODER OSC") is decimal, 0x54, and the
  dispatch writes 5 there for VOCODER;
- **the carrier's** init and update (`0x893ebed0`, `0x893ec4d0`, after the rate setter, the clock fixed as `growl` fixes it)
  on P = the voice + 0x104 = the block + 0xb0, with P+0x20 and P+0x2c as the kind 5 routine (`0x893afff0`) leaves them with
  no modulation (Growl Level's and the balance's own; read, not run) and gain and pitch 1.

What each reaches (OSC 1's IDs; OSC 2's are 14 on, 0x362…0x36f, and land at the same offsets in its own record, block and
carrier). v is the ID's value; the laws hold at all five values tried:

| ID | Name | Record | P | Law | Carrier |
|---|---|---|---|---|---|
| 0x354 | Saw Wave LPF cutoff | +0x64 | +0x0c | v/127 | +0x18 (v/127); +0x14, the saw filters' coefficient g (0 at 0, 0.970 at 127 with the default Dynamics) |
| 0x355 | Saw Wave Level | +0x65 | +0x10 | v/127 | +0x1c, the saw's gain in the sum: Level × min(1, balance) |
| 0x356 | Noise LPF cutoff | +0x66 | +0x14 | v/127 | +0x24 (v/127); +0x20, the noise one-pole's coefficient (0.266 at 31, 1 at 127) |
| 0x357 | Noise HPF cutoff | +0x67 | +0x18 | v/127 | +0x2c (v/127); +0x28, the high-pass stage's coefficient (0.0068 at 0, 1 at 127) |
| 0x358 | Noise Level | +0x68 | +0x1c | v/127 | +0x30: min(1, Level × (2 − balance)) |
| 0x359 | Dynamics Threshold | +0x74 | +0x24 | v/127 | +0x34; and +0x20 (0.086 at 0, 0.103 from 31 up) |
| 0x35a | Dynamics Sensitivity | +0x75 | +0x28 | v/127 | +0x38; and +0x14 down (0.709 → 0.638) and +0x20 up (0.094 → 0.123) over its range |
| 0x35b | Growl Level | +0x6a | +0x30 | v/127, 1 exactly at 127 | +0x3c, the growl's amount ("The growl, run") |
| 0x35c | Growl Keyfollow | +0x6c, ×10, 16 bits | into +0x30 | + Keyfollow% × octaves from key 60 | |
| 0x35d | Growl LFO Depth | +0x6e | +0x38 | sign·(v/63)² | |
| 0x35e | Growl Envelope Depth | +0x6f | +0x34 | sign·(v/63)² | |
| 0x35f | Saw-Noise Mix Balance | +0x76 | +0x44 | v/64, 0…1.98 | through P+0x2c, +0x1c and +0x30 |
| 0x360 | Mix Balance LFO Depth | +0x7a | +0x4c | sign·(v/63)² | |
| 0x361 | Mix Balance Envelope Depth | +0x7b | +0x48 | sign·(v/63)² | |
| 0x37c | Vibrato Type | +0x1c | block +0x80…+0x98 | a 0x50-byte row of `0x8953ffd0` by type; 0 leaves them 0 | |
| 0x37d | Vibrato Rate | +0x1d | block +0x84 | 2 + 6·v/127 | |
| 0x37e | Vibrato Depth | +0x1e | block +0xa8, +0xac | sign·(v/63)² × 300, and × 0.6 | |

- **Two sources agree on P.** The converter's writes, and the kind 5 routine's reads ("The growl, run"): it takes P+0x30 +
  m₁·P+0x34 + m₂·P+0x38 into P+0x20 and P+0x44 + m₁·P+0x48 + m₂·P+0x4c into P+0x2c, m₁ at its envelope's +0x4c and m₂ at its
  LFO's +0x38, where rom.2's envelopes and LFO keep their outputs (`vc2twinanalog.cpp`). So P+0x34 and P+0x48 are the
  Envelope Depths and P+0x38 and P+0x4c the LFO Depths, as the converter put them. P+0x30 is Growl Level, the open question
  "The growl, run" left.
- **Growl Keyfollow** (run at key 72, Growl Level 64): −50, −20, −10, +10, +20, +50% give 0.004, 0.304, 0.404, 0.604, 0.704
  and 1 (clamped) from 0.504; at key 60 it moves nothing. So the growl is clamp₀¹(Level/127 + Keyfollow/100 × (key − 60)/12)
  before its modulation.
- **The balance** (Noise and Saw Levels at their defaults, 95 and 101): the saw's gain is Saw Level × b up to b = 1 and Saw
  Level from there, the noise's min(1, Noise Level × (2 − b)) (b = 0, 0.48, 0.98, 1.48, 1.98 give 1, 1, 0.760, 0.386,
  0.0117). So a balance of 64 has both at their levels, and each end fades the other source; the noise's clamp at 1 is on
  the product, the saw's on the factor (five points each, the form fits them). (Read in v221: the update writes the saw's
  gain only below a balance of 1, clamped on the product too; from 1 up the gain keeps what it had, Saw Level from the
  settings, or the last Level × b when the balance rises during a note.)
- **The Dynamics** move the two lowpass coefficients from their settings (at velocity 100, gain 1); ~~what level they follow
  is not read~~ (read and run in v221: the input stage's largest peak of the last eight blocks, `0x899d9a5c`, the mic's;
  the whole law in HARDWARE_FIRMWARE.md, "The VOCODER OSC's carrier").
- **The vibrato** is read for every Type: the builder puts its three IDs in the record under ANALOG, PCM, EXTIN and VOCODER
  alike (+0x1c…+0x1e), and the converter's part that reads them comes before its switch by kind (`0x8939fbd0`; read). The
  block's fields were run under VOCODER only. Its depth through the Matrix's getters (`0x893a26b0`, `0x893a28b0`), clamped to
  ±63, is read, not run.
- **Harm Formant is the EXT IN's.** 0x371, 0x372 (×10) and 0x375 reach the record only under EXTIN (+0x4e, +0x50, +0x5e; OSC
  2's 0x376, 0x377, 0x37a the same), beside kind 6's pitch shifter of the mic ("Voice kind 6, run"). Its LFO and Envelope
  Depths (0x373, 0x374) and the Wave start offset (0x37f) reach no record under any Type; `idbuild.py` finds 0x373 built
  outside the builder (`0x8932de12`, `0x893304c2`, …) and 0x37f only in the store's code (`0x8932b1e6`, `0x8932b386`). Where
  they are read is not followed.
- **The builder lays the record out the same under every Type**: the VOCODER OSC's IDs and the vibrato land at the same
  offsets under ANALOG, PCM and EXTIN as under VOCODER (the record holds them all); only the Harm Formant IDs are taken by
  Type.

### System

Chord Memory: Switch and Hold (0x37, 0x38) and 12 keys × 4 notes (0xc0…0x11e, every other ID). External In Type (0x7a):
STEREO, MONO-L+R, MONO-L, MONO-R.

Screens named in rom.4 and not in rom.2 include `VC-2 MODE MENU`, `Go V-SYNTH!`, `SYSTEM MIC Setting`, `MIC Setting Name`,
`MIC Sets`, `PATCH Chord Memory`, `VOCODER    MIX` (in the list after the Matrix's destinations, beside the MFX knob
assigns) and `Vocoder/Natural`. Where the MIC settings are stored, and whether they are the V-Synth's sampling templates (IDs
0x4f7…0x525 in rom.4, with the pre-effect chain), is not known.

### rom.4's values as text (2026-11-14, v199)

The plugin reads rom.4's three tables at its start (`plugin/Source/Vc2Params.cpp`) and `xt_vc2params` holds them to rom.4's
own routines (HARDWARE_FIRMWARE.md, "rom.4's parameter tables in the plugin"). The value text is rom.4's routine
`0x894fa070(id, value, unit)`, which is rom.2's `0x89461a70` by rom.4's IDs; `0x894fa730(id, value)` is its wrapper with the
unit (exts.w the ID, then `bra 0x894fa070` with r6 = 1), and that wrapper is what a screen's parameter object calls
(`0x89505b70`, a slot in 21 vtables from `0x8990aa58`, 0xe4 bytes apart). With the unit, the routine adds the ID's string
after its 3, brackets and all, to any text it gives (1,254 of the 4,946 ends below), but for the ASCII characters' names.

**Where it has no format.** At each ID's lowest and highest value, against the ID's own string (names split by 1, a range by
2): of 1,714 IDs with a range, 1,269 read as their string's ends and 215 as the bare number where the string has another
scale; of 297 with names, 290 start at their first name. rom.2's routine on rom.2's IDs, the same way: 1,657 ranged, 1,259
as the string, 133 bare. Most of the bare ones are the system's (a part's channel 0–15 where the string says 1–16, Device ID),
in both. Those rom.4 has and rom.2 hasn't, by name:
- Split Point (0–127, the string C-1…G9).
- The patch EQ, both bands (Freq 0–26, the string 50…20000 Hz; Q 0–5, 0.5…8.0; Gain 0–30, −15…+15 dB).
- The pre-effect chain's EQ (Low, Mid 1 and Hi Freq, Gain and Q) and its Compressor Release (0–127, the string −50…+50).
- OSC 1 and 2 Wave Number (0–999, the string OFF, 1…999) and Harm Formant Style (0–10 against 0…127); Beam Type (0–4, OFF,
  1…4).
- Types that count from 1 in the string: Structure Type (1–3), and COSM's Type, Amp Type, Speaker Type and Filter Type.
- Chord Memory's 48 notes, 0–128, whose string ends in OFF: the top value reads 128.

The VC-2's screens must draw these some other way, or show them bare; which is for the VC-2 mode's panel to settle, as the
V-Synth's four reverb IDs with no format were (HARDWARE_FIRMWARE.md, "The seven on the manual's scale"). The plugin reads
them as the routine gives them, and its rom.4 description puts nothing right yet (Vc2Params.cpp's `kVc2Text`).

**Where it faults.** Without the unit it faults at five IDs: the four new reverbs' Room Size (0x827, 0x84a, 0x856, 0x862,
0–5), whose string is "22 26 32 37 43 48 [m]", one name with spaces where the names should be split by 1, so values 1–5
run off the list (value 0 reads the whole of it); and COSM's Sample Rate Down (0x999, 0–5) with four names (NOR, NOR1/2,
NOR1/4, NOR1/8), at 4 and 5. With the unit, Room Size reads its bare number and unit (1[m]…5[m]) and Sample Rate Down still
faults. On the XT a fault isn't one: the read is of address 1, through null. The plugin's text for a value the routine
faults on is its number, as for rom.2's (XtFxValueText).

**Note values** come back in the LCD font's codes, as rom.2's (`0x15` 2/1 at 144, an LFO rate's top): 111 of the check's
values, read by rom.2's glyphs (`xtNoteText`), which xt_fx control measures on rom.2's driver; rom.4's own driver doesn't
check them.

## The files

`Internal.vc2` in the disk image holds:

- `VC-2 Palette.s00` (304 bytes), `VC-2 Template.s00` (280), `VC-2 System.s00` (136). The manual's figure also shows a
  `VC-2 Setup.s00`; the image has none.
- `Patch/`: 128 `.p02`, 5060 bytes each. 065–128 are byte for byte 001–064 (all 64 pairs compared). A `.p02` has the `.p00`'s
  file layout: `00 1e`, a type tag (`VC20Rev008` where a `.p00` has `SVD0`), then `PATa`, a 4-character tag (`VC_2`; a `.p00` has
  `KATE`), 0x20 and a size (0x13a4; a `.p00` 0x14fa).
- `Wave/`: 58 `.w02`, 36 bytes each: a `link` chunk holding a preset wave's number (`VP-330 Saw` is 42) and a 16-character `name`.
  No audio.

~~The audio is a library after the V-Synth's 416 preset waves: at file offset 0x356d200 of the disk image, header `WLF08`, then
`1.01VC2W20050316` and `1.00VC2P20050314`, three section entries, and a count of 0x3a = 58; its data ends at 0x38154d0
(about 2.7 MB). Not decoded.~~ Withdrawn 2026-11-13 ("The wave libraries, read"): the `WLF08` at 0x356d200 is a 12 KB index,
"VC-2 Wave.bin"; the audio is the last 192 of the 416 streams, in "VC-2 Preset.bin"; 0x38154d0 is where the V-Synth's own
index, a second `WLF08` at 0x3810200, ends. rom.4 names the drives it is reached by: `V:\Preset.vc2\Patch\`, `V:\Preset.vc2\Wave\`,
`A:\Internal.vc2\`, `Z:\Demo.vc2\`, `Z:\TestMode.vc2\`, and `VC-2 Patch.bin`, `VC-2 Preset.bin`, `VC-2 Wave.bin`,
`VC-2 Project.ini`.

The disk image's root also holds the VC-1's files: `VC1.S10` and `VC1_I1.P10`…`VC1_I8.P10`, 36944 bytes each.

### The wave libraries, read (2026-11-13, the VC-2 mode's step 1c)

The preset area after the partition holds both instruments' waves, each as an index and its samples:

| | The V-Synth's | The VC-2's |
|---|---|---|
| Index, `WLF08` | at 0x3810200 (to 0x38154d4), `1.00VSYN2W050316`, `1.00VSYN2P050316`: "V-Synth Wave.bin" (rom.2's string beside `WLF0`) | at 0x356d200 (12 KB), `1.01VC2W20050316`, `1.00VC2P20050314`: "VC-2 Wave.bin" |
| Waves → maps → samples | 224 → 224 → 224, one to one | 58 (70 splits) → 43 (430 splits) → 192 |
| Samples | the 416 streams' first 224, each starting on a sector, from 0x178c200 | the last 192, none on a sector, in "VC-2 Preset.bin": `"TABLE"` at 0x2804a00, `"DATA"` at 0x280770b, streams 0x2807711…0x356d097 |
| Link files | 224 `.w00` in `Internal.prj/Wave` | 58 `.w02` in `Internal.vc2/Wave` |

**The index** (`WLF08`): a section count at +8, two version strings at +0x10 and +0x20, and a 16-byte entry a section from
+0x40 (u16 number, u16 1, u32 offset from the index, u32 size). A section is a u32 count, a u32 index offset (0xc) and a u32
entry size: 8 means an index of (u32 offset from the section, u32 size), anything else inline entries of that size. Section 1,
the waves: a 16-character name, then a 4-byte split each (~~the upper key~~ the upper velocity, a byte, 127 in 67 of the
VC-2's 70 and 82, 88 and
100 once each, and a u16 map number). Section 2, the maps: a name, then a 6-byte split each (the upper key, a byte of 100 to
127, a level by its values, a u16, 0 in 402 of the 430 and 12 in 28, which by its value may be an octave ~~but isn't read~~, and
a u16 sample number). Section 3, the samples: 36-byte paths `VPW\<file>.vpw`. Numbers count from 1. So a VC-2 wave is a
multisample: `LargeChoVS S` splits at ~~keys~~ velocities 90, 120 and 127 into maps 3, 1 and 2 (the three strikes withdrawn
2026-11-13, "A VC-2 wave's split, chosen": rom.4 compares a wave's split with the note's velocity, and passes the 0 or 12
on), and the map `LondonChoSt` spreads samples
52…65 over 14 splits from key 43. **The sample bank** ("VC-2 Preset.bin"): `"TABLE"`, the count in ASCII (`0192`), 60-byte
entries (a 52-byte file name, u32 size, u32 offset from the end of `"DATA"`), then `"DATA"` and the streams. The index finds
a sample in it by name: the bank's order is not the index's. **A link file** (`.w00`, `.w02`, 36 bytes): a `link` chunk
holding the wave's number in its index, and a `name` chunk, its 16 characters.

**Checked** (`research/tools/wlflib.py`, which reads the image through `diskls.py`; `build/tools/vc2/wlflib.txt`): in both
indexes the sections follow each other, every number is in range, every map and sample is used, and every split list rises
to key 127. All 282 link files name their wave by its number. Every one of the VC-2's 192 sample paths is in the bank, every
bank entry lands on a stream, the entries tile `"DATA"` with no gap, and the bank's streams are exactly the 192 not on a
sector. 0 differences. Seen red: one sample number set to 193 in a copy of the image gives "outside 1..192" and exit 1.
**The firmware's own reading agrees**: rom.4's parser (`0x8934d12a`) opens "D:\VC-2 Wave.bin", compares 4 bytes with
`WLF0`, copies the two 16-character version strings and checks each section entry's +2 is 1 and its number 1 to 3. The bank's
(`0x89350978`) opens "D:\VC-2 Preset.bin", compares 7 bytes with `"TABLE"` and reads the 4-byte count. rom.2 has the same
five `WLF0` readers (`0x8949a088…`) beside "V-Synth Wave.bin", so the format is the V-Synth's, and the VC-2 adds the split
maps' use of it (the V-Synth's own index has one split everywhere) and its own bank.

So **the 416 "preset waves" the project's tools read (xt_files, xt_tone) are 224 V-Synth waves and the VC-2's 192 samples**.
A VC-2 wave plays one of a map's samples by key, which the V-Synth's own waves never needed. How rom.2 finds its 224 streams
(they have no `"TABLE"`), ~~and how rom.4's player picks a split,~~ are not traced (the split since: "A VC-2 wave's split,
chosen").

### A VC-2 wave's split, chosen (2026-11-13, the VC-2 mode's step 1c)

**Where.** rom.4's PCM converter (`0x8939e120`, rom.2's `0x894e2d20` by shape) takes the OSC's Wave Number to the wave
store as rom.2's does (`0x89351840`, rom.2's `0x8949e5c0`, as 2999 + the number). A V-Synth wave the store holds goes on as
before (`0x8939cd30`). One it doesn't hold goes to the wave library (`0x893520a0`), and from there to a routine rom.2
hasn't (`0x8939cba0`), with the converter's input +4, which the per-note OSC setup fills with the note's velocity (rom.2's
`0x894e4c30`: the note's +0xf; HARDWARE_FIRMWARE.md, "The note's object M"), and the key-follow key the converter already
forms (`0x8939c910`, rom.2's `0x894e12c0`: the key with the Octave Shift, moved by the OSC's Coarse Tune and Pitch
Keyfollow; HARDWARE_FIRMWARE.md, "The key-follow key"). It calls three readers of the index:
- `0x89394b30(wave, velocity, out)`: the wave's record (by a table of each record's place, `0x89973084`, that the parser
  fills), its splits walked, the first whose first byte is at or above the velocity taken: out+6 its map, out+4 its second
  byte. out+0 is set when the first split covers 127 (one split), out+8 when the name has " M" at its 11th and 12th
  characters (`LargeChoVS M`, a mono wave beside each `… S`).
- `0x89394c20(map, key, out)`: the same over the map's splits by the key-follow key: out+6 the sample, out+4 the split's
  second byte, out+5 the low byte of its "0 or 12" word.
- `0x89394d10(sample, out)`: out+2 the id the store then takes, as rom.2's wave fetch takes 2999 + a wave's number.
The routine copies these into ~~the converter's block~~ the OSC's per-note input, the converter's first argument (withdrawn
2026-11-13, "The split's bytes, followed": the converter keeps the input in r13 and its block, the second argument, in r14;
the fetch is given the input + 0xa0) (its +0xa0 on: +4 the wave split's byte, +5 and +6 the map split's, +7 the sample
reader's out+0, +0 set when both the wave and the map have one split, +0x5c the " M") and fetches the sample's chunks from
the store as rom.2's `0x894e16f0` fetches a wave's.

**Run** (`vc2run <rom.4> splits <index>`, `vc2split.cpp`; the index written by `wlflib.py get VC2`;
`build/tools/vc2/splits.txt`). rom.4's own parser (`0x8934d12a`) reads the index, its file calls (open `0x89336c60`, read
`0x89336cf0`, seek `0x89336db0`, close `0x89336ca0`) answered from the index's bytes: it returns 1 and finds 58 waves and 43
maps. Then the wave reader at every velocity 0…127 for every wave, and the map reader at every key for every map, printed
as `wlflib.py choices VC2` prints the rule "the first split whose first byte is at or above the value" on the same bytes:

| | |
|---|---|
| The firmware's choices against the rule's, 58 waves × 128 velocities and 43 maps × 128 keys | the same, line for line |
| The rule with "above" for "at or above", planted | all 101 lines differ |
| The key-follow key (`0x8939c910`) against the port's `xtKeyFollowKey`, keys 0…127, Coarse −48…+48, Keyfollow −2000…+2000 | 139,392 cases, 0 different |
| The sample reader | sample n is the store's id n, for all 192 |

So **a VC-2 wave plays by velocity, then by key**: its splits choose a map by the note's velocity (the six `…VS` waves, the
Large, Gospel and Small Choirs in stereo and mono: velocity 0…90, 91…120, 121…127; the other 52 have one split), and the
map's splits choose a sample by the key-follow key: the key the V-Synth's PCM OSC already forms for its key follow, which
XtOsc ports. Each split list ends at 127, so every velocity and key finds one. A VC-2 sample is the store's id 1…192, its own
number in the index, where a V-Synth wave is 3000 on. The bytes the readers pass on are 127 on every wave split but the FM
waves' (`FMSynWave 1`…`3`: 88, 82, 100), 100…127 on the map splits, and 12 on some splits of the four bass maps
(`LindaJohnBs`, `PennyRobyBs`, `JohnAndBs`, `RobyAndBass`), each into ~~the converter's block: what the block does with them
(a level, an octave, by their values) is not followed here~~ the OSC's input (followed since, "The split's bytes, followed":
two levels on the OSC's gain and a shift of the sample's root key), nor how the store comes to hold the bank's samples under
those ids.

### The split's bytes, followed (2026-11-13, the VC-2 mode's step 1c)

**Where they go.** The fetch of a VC-2 wave (`0x8939cba0`) and the V-Synth's own (`0x8939cd30`, rom.2's `0x894e16f0`) fill
the same fields of the OSC's per-note input, from +0xa0 in rom.4 and from +0x80 in rom.2 (rom.4's input lays the same fields
0x20 later: its Wave Number at +0x44, rom.2's +0x40). A read-watch over the tone generator's code found every read of them
(below, "Run"); the code at each was then read:

| Input (rom.2's) | The VC-2's fetch writes | The V-Synth's fetch writes | Read by | What it does |
|---|---|---|---|---|
| +0xa0 (+0x80) | 1 when the wave's and the map's reader both set out+0 (one split each) | 1 | `0x8939f318`, the VariPhrase block routine (`0x8939f2a0`), under Playback Mode EVENT only | 1: EVENT plays from the key's event to the next, as every V-Synth wave does; 0: from the vpno's first event to its last |
| +0xa4 (+0x84) | the wave split's second byte | 127 | `0x8939e78e`, the PCM converter (`0x8939e300`) | a level on the OSC's gain (block +0x5c) |
| +0xa5 (+0x85) | the low byte of the map split's word (0 or 12) | 0 | LITE's block (`0x8939f0c4`, `0x8939f10c`) and VariPhrase's (`0x8939f894`, `0x8939f8fc`) | a shift of the sample's root key, signed (`mov.b`) |
| +0xa6 (+0x86) | the map split's second byte | 127 | `0x8939e79e`, the PCM converter | a level on the OSC's gain, as +0xa4 |
| +0xa7 (+0x87) | the sample reader's out+0 | the wave header's +1 | `0x8939e274`, `0x8939e8c4`, `0x8939f7da` | the kind: 0 LITE (block kind 1), 1, 3, 5 VariPhrase (kind 2), as before |

rom.2 has the same code: its converter multiplies the gain by the level curve at +0x84 and +0x86 (`0x894e3572…0x894e3594`),
and the port already names both. `xtOscGain` (XtOsc.cpp) has "the level curve at two further levels (+0x84 / +0x86, set
with the wave) taken as full here", and `xtPitchCents` "the Original Key less the wave's shift (+0x88, +0x85: 0 here)". So
the V-Synth always had these fields. Its fetch fills them neutral, and the VC-2's index is the first to set them.

**The laws**, each held to the firmware's block over every value (`vc2splitlaws.cpp`):
- The OSC gain, block +0x5c = (T[+0xa4]/32768 · g) · T[+0xa6]/32768, each product rounded toward zero. T is the level
  curve (rom.4's `0x89541f34`, byte for byte rom.2's `0x897cfcb8`, XtOsc's `kLevelTab`, 32768·(k/127)²), and g =
  2^(Wave Gain/6) is the gain with both at 127. This is `xtOscGain` with its two 127s made the split's bytes.
- The pitch, block +0x08 (from the input's +0x94) and +0x68 (from +0x98) = (pitch − (root key − shift)·1000)/10, within
  ±6000. The root key is the wave header's +0xa (the input's +0xa8) and the shift the signed byte. This is `xtPitchCents`
  with the Original Key less the shift.

**What the VC-2's waves get.** The FM waves' bytes are 88, 82 and 100: −6.37, −7.60 and −4.15 dB. The map splits' bytes run
100…127, −4.15…0 dB, per sample: 127 on 170 of the 430 splits, 120 on 91, 110 and 115 on 50 each (`wlflib.py`'s reading).
A 12, on 28 splits (keys up to 41 in the four bass maps), plays those bass samples an octave above their root key: block
+0x08 is 1200 cents at the root key.

**+0xa0 never reaches EVENT's other path.** 179 of the 192 samples are LITE by the index's kind byte (0; their streams
are SOLO-encoded, "The sample bank, loaded"); 12 are SOLO (132…137, 179, 180, 182, 183, 191, 192) and 1 is BACKING
(181). Every map holding a VariPhrase sample has one split, and so does every wave holding
those maps. Over every wave × velocity × key (950,272), the index's own readers give a VariPhrase sample +0xa0 = 1 every
time: LITE with 0 ×638,976, LITE with 1 ×98,304, SOLO with 1 ×196,608, BACKING with 1 ×16,384. LITE's block doesn't read
+0xa0. So the factory waves play EVENT as the V-Synth's do, and the 0 path is the format's, unused.

**Run** (`vc2run <rom.4> splitbytes <index>`, `vc2splitbytes.cpp`; `build/tools/vc2/splitbytes.txt`). rom.4's default
patch with OSC 1 on PCM and the index read by rom.4's parser. The wave library (`0x893520a0`) is answered with VC-2 wave n for
Wave Number n. The wave store is answered with one wave's chunks, laid as xt_tone lays them, for every sample, so only the
split's bytes differ between cases. The dispatch (`0x8939fb40`, the fetch inside it) and the converter (`0x8939fbd0`) ran
twice under a read-watch (`vc2watch.h`): every load and delayed branch in `0x89380000…0x89400000`, 58,642 instructions,
hooked to decode its address. An earlier static scan for the compiler's usual form of these offsets (`mov #imm,r0; extu.b
r0,r0`) found no reader of +0xa4…+0xa6 in either image. The compiler forms them by adding to a register (`0x8939e78e` reads
`0xa4` held in r6, then `add #2`), which only the watch saw.

| | |
|---|---|
| Reads of the input's +0xa0…+0xff, LITE cases (waves 1, 43, 34, 4) and a SOLO case (wave 47 at keys 60 and 64 under RETRIGGER, STEP, EVENT) | as the table above; no block word points into the input, so nothing after the converter reads it |
| Each byte set after the fetch, the converter run twice, the block against the case's | +0xa4, +0xa6: block +0x5c alone; +0xa5: +0x08 and +0x68; +0xa0: nothing on LITE, nothing under RETRIGGER and STEP |
| +0xa0 set to 0 under EVENT (the vpno laid: 8 events, 1000 samples apart; 2 does as 1) | key 60: +0xec 1000 → 7000; key 64: +0xe8 4000 → 0, +0xec 5000 → 7000, +0xf0 4000 → 0. So 1 plays the key's event (from C4) to the next, 0 the first event to the last |
| +0xa7 set to 1, 3, 5 on a LITE case (the control) | 5 block words move each time |
| The level law, Wave Gain 0, +5 and −7 dB (g 1, 1.7818, 0.4454), each byte at 0…127 alone and both together | 0 of 384 differ, each gain |
| The shift law, +0xa5 at 0…255, on a LITE and a SOLO sample | 0 of 512 differ, each |
| Planted: the map byte's factor taken as 127 | 253 of 384 differ |
| Planted: the product's order changed, (T·T)·g | 0 at g 1, 60 and 48 of 384 at +5 and −7 dB |
| Planted: the shift taken unsigned | 128 of 256 differ (the first run's own error) |

**What the port needs** for a VC-2 wave: `xtOscGain` given the two level bytes and `xtPitchCents` the shift, both by the
laws above. +0xa0 needs nothing for the factory waves. Not followed here: ~~how the store comes to hold the bank's samples
under ids 1…192~~ (followed since, "The sample bank, loaded"); what the second pitch word (+0x68, from the input's +0x98)
is for beyond following the same law.

### The sample bank, loaded (2026-11-13, the VC-2 mode's step 1c)

**The bank.** At start-up (`0x8933eea0`), before the index is parsed, rom.4 loads "VC-2 Preset.bin" into a static object
at `0x89e0f034` (0xf008 bytes, cleared by the static initializer `0x89350af0`). The loader (`0x89350970`) reads 7 bytes and
compares them with `"TABLE"`, reads the 4-digit ASCII count (at most 1024), and reads that many 60-byte entries to +8 on.
It sets +0 to 17 + 60 · count, where the streams start in the file (`"TABLE"`, the count and `"DATA"`, which it doesn't
check), and +4 to the kernel's answer for the file (`0x89336d60` jumps straight to `0x88008fc0`). +4 is the file's base in
memory: the reader copies a stream's chunks from the place it is given (`0x893476b0`), so the preset area is read as
memory. "D:\…" files go to the kernel; "Z:\" and "V:\" are rom.4's own (`0x89336c60`).

**The start-up load.** After the parse, `0x8934d2a0` and `0x8934d2d0` open the index again and find its samples' section.
They open the drive "D:\" (`0x893369c0`) and take the Wave Numbers 1…128 to the wave library (`0x893520a0`, as 2999 + n).
Each VC-2 wave the library names is loaded whole:
- `0x8934da70(state, wave)` reads the wave's record (16 + 128 · 4 bytes) and takes each velocity slot's map.
- `0x8934d9d0(state, map)` reads the map's record (16 + 128 · 6 bytes) and takes each key slot's sample.
- `0x8934d8d0(state, n)` does nothing if the store already holds id n (`0x89351840`). Otherwise it reads sample n's
  36-byte entry, shows the wave's name on the display (`0x8933a610`), prints `"D:\"` before the entry's path
  (`D:\VPW\<file>.vpw`), and calls the wave reader `0x893474d0(path, n, 0)`.
- The reader looks `path + 7`, the file's name, up in the bank (`0x89350a50`: the first entry that `strncmp` matches over
  the entry name's length). It keeps the stream's size (`0x8997ad28`) and place (`0x8997ad2c`: the entry's offset + the
  bank's +0 and +4), reads the stream's chunks from there, and makes the store's object with +0 = n.
- At the end it closes the index and the drive (`0x893369f0`); a progress word (`0x899344e6`) runs to 100.

So **a VC-2 sample is the store's id n, its number in the index, and its stream is found by its file name**. Every sample
of every wave the library names comes in at start-up, each once. The library is answered here from the card's link files:
`Internal.vc2/Wave/001…058`, file k naming wave k, so Wave Number k is VC-2 wave k. How the OS fills the library from them
(`0x89353130`) is not traced.

**The kind is the index's.** A sample entry is a 32-byte path and 4 bytes; its byte 32 is the kind the sample reader
gives (`0x89394d10`'s out+0), which the fetch puts at +0xa7 ("The split's bytes, followed"). The V-Synth's own fetch takes
the stream's header type there instead. The two differ for most of the bank:

| The index's kind byte | The stream's vpsc type | Samples |
|---|---|---|
| 0 (LITE) | 1 (SOLO) | 179 |
| 1 (SOLO) | 1 | 12 |
| 3 (BACKING) | 3 | 1 |

So the VC-2 plays 179 SOLO-encoded streams (the choirs, the basses, the synth and FM waves) as LITE waves: plain
samples through LITE's players, their vpdt unused. Only the 13 VariPhrase samples play as VariPhrase.

**Run** (`vc2run <rom.4> bank <index> <bank>`, `vc2bank.cpp`; the bank written by `wlflib.py bank VC2`, 14,059,159 bytes;
`build/tools/vc2/bank.txt`). The two files answered to rom.4's file calls (`vc2index.h`'s `hookFiles`), the bank's bytes put
in memory and its base given for `0x89336d60`. rom.4's own routines ran for the bank, the parse and the start-up load,
with these answered: the drive's open and close, the display's message, the library (as the link files have it), the
store's "held" (from what has been loaded), and the reader, whose path and id were taken. Then rom.4's find ran on each
path. Each sample's path, id, place in the file, size and kind byte is printed in the form `wlflib.py samples VC2` prints
from its own reading: the index's entry, then the bank's `"TABLE"` by the file's name.

| | |
|---|---|
| The bank's object after the loader | +0 0x2d11 (17 + 60 × 192), +4 the base given |
| The start-up load | returns 1; the drive opened once; the reader called 192 times, for 192 different ids |
| `vc2run … bank` against `wlflib.py samples VC2`, 192 samples | the same, line for line; every place holds a `vpfl` |
| Planted in wlflib's copy: the place without `"DATA"`'s 6 bytes | all 192 lines differ |
| wlflib's own check and `choices`, after its bank reading moved to `wlfbank.py` | as before, byte for byte |

**What the port needs** for a VC-2 sample: its stream by its file name in the bank's table, read as the plugin reads the
preset area's streams (`xt_files` already holds all 416, these 192 among them, to rom.2's whole-wave routines). Its kind
comes from the index's byte, not the stream's header. Not covered here: ~~the reader `0x893474d0` itself, answered rather
than run, and whether rom.2 has its like~~ (run since, "The wave reader, run").

### The wave reader, run (2026-11-13, the VC-2 mode's step 1c)

**Whose it is.** rom.4 has six routines that walk a wave's chunk tags (vpfl, vpsc, vplp, vpno, vpdt, data, LIST, name,
link, vmdl, rdmd). Four are rom.2's, by size and shape (`codealign.py`; 1.0 the same code, near 0 unrelated):

| rom.2 | rom.4 | Alike |
|---|---|---|
| `0x894939d0` | `0x89345d70` | 0.97 |
| `0x89497d40` | `0x8934af60` | 0.96 |
| `0x89499000` | `0x8934c220` | 0.97 |
| `0x89496590` | `0x89349750` | 0.64 |
| `0x89494620` | `0x893469c0` | 0.60 |
| `0x89494620` | `0x893474d0`, the bank's reader | 0.38 |

`0x89494620` is the routine HARDWARE_FIRMWARE.md's "The XT's load" left unread. Its rom.4 twin `0x893469c0` reads a
file through the file calls (`0x89336cf0` ×7, `0x89336db0` ×5). The bank's reader is the VC-2's own: it makes no file
call, and copies a stream's chunks from memory at the place the bank's lookup leaves (`0x8997ad2c`, size `0x8997ad28`).

**Where it puts a wave.** The store is rom.4's wave library (`0x8997c52c`): 128 lists by id & 127, each entry 88 bytes
and the wave's name, taken from the store's own pool (`0x8997c51c`). The store's setup (`0x89353ac0`) clears the lists and
gives the pool 32 MB from the OS's allocator (`0x89350c10`). The reader registers the wave there (`0x89356a90` →
`0x89353b40(library, id, …)`), which looks the id up in its list first and hands an entry it finds to `0x89354490`
(not read).

**What it stores** (the run, all 192 samples):
- **Chunks:** kinds 2 (vpsc), 3 (vpno), 4 (vplp), 5 (vpdt), 6, 7, 8 and 10 (the cached loop events), none at 0, 1 or 9.
  Kinds 6–8 aren't compared.
- **Fields:** the header, the loop record, the vpno and the vpdt equal the stream's as `xtWaveRead` reads them, field
  for field and byte for byte, and the frame count with them. So the load keeps a stored vpdt as it is.
- **Audio, band 0:** decoded over whole RDAC blocks from the data chunk's own bytes. When the frames end inside a block,
  that last block is decoded from the stream's real bytes past the frames. The decoder (`0x893eb9d0`, over
  `0x893e6740`/`0x893e6800`, which are rom.2's `0x89529c20`/`0x89529ce0` by `codepairs.py`) pads with zeros only past the
  bytes it is given (its flush, `0x8935ca10`).
  `xtWaveRead`'s decoder (`xtFwRdacDecode`) feeds zeros past the frames, so the plugin's audio differs in the last frames
  (up to 15) of 177 of the 180 samples that end inside a block.
- **Bands:** the BACKING sample's six bands (0, 2…6) are rebuilt from that audio and equal `xtMbBuildBacking`'s, band 0
  the full-rate residual in place of the audio. Bands 2…6 are held for that sample alone.

**Run** (`vc2run <rom.4> wavein <rom.2> <index> <bank>`, `vc2wavein.cpp` and `vc2wavecmp.cpp`;
`build/tools/vc2/wavein.txt`). The start-up load as "The sample bank, loaded" runs it (`vc2bankload.h`), with the reader
and the store's "held" now run, after the store's own setup. Answered:
- the kernel's file name for a path (`0x89338c60`), as the part after the path's last `\`; the reader hands it to
  `0x89345be0`, which drops a "NNN " or "NNN_" number from a name as the card's files have it;
- the OS's allocator, calloc and free (`0x89300770`, `0x89300910`, `0x89300840`), from a region of their own with
  nothing freed, as the interpreter's machine answers operator new.

Each sample's store answers (chunk `0x89351ad0`, band `0x89351ff0`, frames `0x89351a10`) were held to the plugin's
reading of the same stream:

| | |
|---|---|
| The start-up load with the reader run | 1; 610,011,895 steps; the allocator 12 calls, 32 MB and 72,512 bytes |
| The chunks' fields, vpno, vpdt and frames against `xtWaveRead`, 192 samples | 0 differ |
| Band 0 against `xtWaveRead`'s audio, 191 samples | 14 equal, 177 differ only in their last block of 16 frames, 0 before it |
| Band 0 against the plugin's decoder over whole blocks | 0 of 191 differ |
| Sample 181's bands against `xtMbBuildBacking`'s from that audio | equal, every band |
| Planted: the key read one byte on (+0xb) | all 192 differ |
| Planted: the bands built from the zero-filled audio | 1 of 1 differs |
| The bank, splits and splitbytes runs after the load moved to `vc2bankload.*` | as before, byte for byte |

**What the port needs** for a VC-2 sample: its stream read as `xtWaveRead` reads it, but its audio decoded over whole
blocks (the frames rounded up to 16 where the data holds them), and a BACKING sample's bands built by `xtMbBuildBacking`
from that audio. The V-Synth's side is left as it was: rom.2's whole-wave routine, as `XtFiles` calls it with the frame
count, gives the zero-filled tail the plugin has, and what count rom.2's own file reader passes isn't known (read since,
not settled: HARDWARE_FIRMWARE.md, "The V-Synth's wave load, read").

### The factory patches, read by rom.4 (2026-11-13, the VC-2 mode's step 1c)

**How.** `vc2run <rom.4> patches <file.p02> …` (`vc2patches.cpp`) reads a `.p02` the project's way, by rom.4's own routines in
the interpreter, as `XtFiles` and `XtFwPatchReader` read a `.p00` by rom.2's: rom.4's default patch (`0x89325300`, rom.2's
`0x8947e230`, 0.91 alike by field shape) laid out as a file (`0x89324b10`, rom.2's `0x8947dad0`, 1.00) under the file's
PATa data (offset +0x20, 0x13a4 bytes), rom.4's file-to-store conversion (`0x89324b20`, rom.2's `0x8947dae0` at 0.50:
rewritten, as a new file version, `VC20Rev008`, would need), and its reads of each ID (`0x89328580` the bits, `0x8932b050` the
value; rom.2's `0x894817b0`, `0x89484310`, the same code on rom.4's descriptors). The 64 files (`Internal.vc2/Patch/001`…`064`,
taken out with `diskls.py`; 065–128 repeat them) in `build/tools/vc2/p02/`, the output in `build/tools/vc2/patches.txt`.

**The store's sections.** As in rom.2, an ID's descriptor gives its place from its section's base, which the caller adds
(XtPatchStore.cpp's `kSections`). rom.4's sections were mapped to rom.2's by their first IDs' names, and each base found as
where every one of the section's IDs reads its default in rom.4's default store (`--stores` writes the stores):

| Section | rom.4 IDs | Base | Blocks |
|---|---|---|---|
| Common (rom.2's 0x103…, the vocoder block and EQ after it) | 0x120…0x1b5 | 0 | 1 (97 of its 133 IDs read their default there; the default patch sets the rest otherwise) |
| The tone: rom.2's 0x52 bytes, then the VOCODER OSC's (offsets 0x52…0x7b) | 0x2d3…0x383 | 0xfe | 8, 0x80 apart (rom.2: 16, 0x52 apart) |
| The envelopes | from 0x384 | 0x4fe | 16, 0x70 apart, to 0xbfe |
| The LFOs | 0x439…0x461 | 0xbfe | 16, 0x14 apart |
| Beam Type to the arpeggio's End Point | 0x4a6…0x4b3 | 0x117e | 1 |
| The arpeggio pattern's lines | 0x4b4… | 0x11a6 (or one line before) | 0x22 apart |

~~Between the common and the first tone block (0x6e…0xfc) are 16-entry tables (the zones' own, by their shape).~~ Withdrawn
2026-11-13: the firmware's pointing routines put the MFX (+0x6c), Chorus (+0xb0) and Reverb (+0xcc) sections there, and the
COSMs at +0xd3e and +0xf5e, 16 zones of 34 bytes each ("The second descriptor bank, read"). MFX, Chorus,
Reverb and the COSMs don't place this way: their parameters are read by type through a second descriptor bank (rom.2's at
`0x897a56d8`), ~~not yet found in rom.4~~ rom.4's at `0x895131a8`. rom.4 has no Zone Knob Sets and no Multi Step Modulator ("Master Switch" isn't among its
names). **Eight tone blocks against sixteen envelope and LFO blocks** is not settled: what a block is (a zone, as in rom.2)
needs the code that reads them.

**What the 64 patches use.** Only the first two tone blocks: OSC 1 is PCM in block 1 and EXTIN in block 2 in 42 patches
(PCM in both in 10; the rest EXTIN or ANALOG in one of them); blocks 3–8 differ from the defaults only in bits no descriptor
reads, but for the wave number (1 → 0). The envelopes' first two blocks (35 patches both, 27 the first alone); no patch
edits an LFO. **No factory patch uses the VOCODER OSC type** (OSC 1 and 2 Type take 0…2, never 3), and its parameters are at
their defaults in all 64; only Harm Formant, its Keyfollow and Style vary (in five patches each). So the factory sounds' carrier
is a PCM wave or the EXT IN, under the vocoder block, not the VOCODER OSC.

**The vocoder block (IDs 0x193…0x1a5) over the 64:**
- Vocoder Sw off in 5 (008 Shifter Mod, 017 PolyPShift, 027 monkeybiznes, 044 All Chorus, 057 cheap monks; each with Hold
  Dump Time 100). Vocoder Type HIRESO in 47, LEGACY in 17.
- The same in all 64: Vocoder Sound Level and Depth 127, Auto Note Retrigger 0, Split Point 37, robot human depth 0, Stereo
  Process Sw 1, Unvoice Spread 0.
- Varying: Direct Sound Level (127 in 031, 62 in 042, else 0), Unvoice Detect (16…50, none at the default 25; most 26),
  Unvoice Level, Env Attack and Release, Hold Dump Time (0, 4, 100), Formant Shift (0…127, 20 values), Formant Template (12 of
  the 16).
- **The two disagreements, settled by the corpus:** Modulator Key Mode is 0 or 1 in every patch (9 at 0), never the text's
  third value HOLD, as its descriptor says; Auto Note On Switch is 0 or 1 (1 in 042 Trioinfernl@ alone), never its field's
  third, empty value.

Not done: ~~the second descriptor bank (MFX, Chorus, Reverb, the COSMs' parameters by type)~~ (read, below), what the eight
tone and sixteen envelope blocks each are, ~~and what reads the zones' tables~~ (no zones' tables: those bytes are the MFX,
Chorus and Reverb sections). A second source for the names is the V-Card owner's manual, which the repo doesn't hold.

### The second descriptor bank, read (`typed`) (2026-11-13, the VC-2 mode's step 1c)

**The bank.** `0x895131a8`, 8 bytes an ID from 0x56a, right after the first bank (`0x8950f0b0` + 12 × 0x56a), as rom.2's
follows its own at 0x55d: a slot byte, then the default, min and max as s16 (rom.2's form, XtPatchStore.cpp's `typedOf`).
rom.4's getter (`0x89324370(section, id, slot base)`) sends an ID from 0x56a on to the typed reader
`0x8932b460(section, slot base, id)`, which reads the slot (an ID offset from the block's slot base) through the first bank.
The names table runs to 0x9ab, so the bank holds 0x442 typed IDs, against rom.2's 0x3f8 to its COSMs' last.

**The blocks** (each place from the firmware's pointing routines, beside the tone's `0x89320a50`; each type table loaded by
code: the first three in one routine, at `0x8935f536`, `0x8935f604`, `0x8935f6a2`, the COSM's by the tone at `0x89399cfa`):

| Block | Type ID | Slot base | Store | Types | Type table | Typed IDs |
|---|---|---|---|---|---|---|
| MFX | 0x1b7 (0–41) | 0x1bb | +0x6c (`0x893208d0`) | 42 | `0x895164ec`, 51 entries, 9 spare | 0x56a…0x7c4 |
| Chorus | 0x23c (0–8) | 0x23f | +0xb0 (`0x89320950`) | 9 | `0x895165b8`, 11 entries, 2 spare | 0x7ce…0x816 |
| Reverb | 0x270 (0–14) | 0x272 | +0xcc (`0x893209d0`) | 15 | `0x895165e4`, 17 entries | 0x819…0x8ed |
| COSM 1, 2 | 0x463 (0–16), Switch 0x462 | 0x464 | +0xd3e, +0xf5e, 34 a zone, 16 zones (`0x89320c12`) | 17 | `0x8953fb40` | 0x926…0x9ab |

A slot base is the block's Type ID + 4, 3, 2 and 1, as rom.2's (`kXtMfxSlots` and the rest), and slot 3 is each block's
Parameter 1 by the names (MFX 0x1be, Chorus 0x242, Reverb 0x275, COSM 0x467). Each block's section has rom.2's number of IDs.
Past the types: the Reverb table's last two entries (typed IDs 0x8ee…0x917, two more reverb types) are past Reverb Type's
0–14, so no patch can choose them; and 14 named typed IDs (0x918…0x925) are in no table. The names in that stretch include a
non-linear reverb's (NLR Type, Envelope T1–T4 and L1–L3) and a delay's (Delay Time, L-R Shift, Feedback). (Since, "The effect
chip's programs": the two are rom.2's Plate and Non-Linear, and the 14 are rom.2's Delay's parameters, name for name.)
~~Rom.4 has four reverb types rom.2 lacks (11–14, Reverb Type's range 0–14 against rom.2's 11 types).~~ Withdrawn
2026-11-13, "The effect chip's programs": rom.4's Reverb Types are Off, seven new stereo reverbs (1–7), then the V-Synth's
Room 1–3, Hall 1–3 and Garage (8–14), by rom.4's own list and the programs each loads; types 11–14 are Hall 1–3 and Garage,
not new ones. The reading took rom.2's numbering for granted.

**The 64 factory patches** (`vc2run <rom.4> typed <file.p02>…`, `vc2typed.cpp`; every block's Type by rom.4's reader, its
type's typed values by its typed reader; `build/tools/vc2/typed.txt`). **No typed value is outside its descriptor's range.**
- MFX: 18 of the 42 types: type 0, which has one unnamed typed ID, in 13 patches, then 19 (10) and 1 (9).
- Chorus: type 0 in 32, then 3 (12), 1 (11), 2 (5), 8 (2), 5 and 6 (1 each).
- Reverb: 13 of the 15 types (none on 9 or 10): type 4 (11), 1 (10), 7 (9), 6 (8), …, ~~and all four of rom.4's own (11, 12,
  13, 14: 2, 3, 2, 2 patches)~~ and 11–14 (2, 3, 2, 2 patches). By rom.4's own list ("The effect chip's programs"): 50 of
  the 64 patches are on the VC-2's seven new stereo reverbs (Stereo Plate 10, OperaHouse 1, StudioSpring 5, Echoes 11,
  Room 6, Cathedral 8, Church 9), 10 on the V-Synth's (Room 1, Hall 1–3, Garage; none on Room 2 or 3), 4 Off.
- COSM, by count over 64 patches × 16 zones: COSM 1 is off in 116 zones and on type 8 in 12, two zones a patch; COSM 2 is
  off in 128; the other 896 of each (14 a patch) are on at type 0, a type with one unnamed typed ID. Which zones the two
  are isn't read zone by zone; the tone blocks' first two (zones 0 and 1) are the patches' only edited ones ("The factory
  patches, read by rom.4").
- **COSM type 8 is not the V-Synth's Comb Filter as it stands.** (It is the GROWL: run since, "COSM type 8, the Growl,
  run".) rom.2's type 8 (0x90e–0x914) opens with Octave Select (0–4)
  and Detune (−100…+100); rom.4's (0x965–0x96b) opens with Intencity and Speed (0–127 each), the same five after them (Detune
  LFO and Env Depth, Tone and its two depths). That agrees with the COSM run, where the Comb's DSP is the one of 16 types that
  differs and rom.4 has one Comb routine ("COSM in rom.4"). The factory zones on it vary Speed (0…127, 5 values) and nothing
  else.

### The effect chip's programs (2026-11-13, the VC-2 mode's step 1c)

**The driver is the V-Synth's.** rom.2's effect driver pairs by shape in rom.4 (`codepairs.py`), moved by about 0x14cd00:
the message reader `0x894ac970` → `0x8935fc40`, the start (`0x894ac6a0` → `0x8935f9a0`, `0x894ac780` → `0x8935fa80`,
`0x894b4a60` → `0x89368280`), the bus primitives (`0x894af400` → `0x89362490`, `0x894af510` → `0x89362610`, `0x894af6a0` →
`0x893627f0`), the program loader `0x894afa10` → `0x89362b60`, the boot `0x894afc50` → `0x893631b0` and `0x894abcf0` →
`0x8935ee90`, the Reverb Time `0x894b2bf0` → `0x893663a0`, the reverb's init and apply (`0x894d09b0` → `0x893873d0`,
`0x894d0b00` → `0x89387520`), the chorus's init `0x894d3350` → `0x8938e420`. Two don't pair: the coefficient write
`0x894af610` and the sleep `0x894b26a0`. So XtFxControl's way (rom.2's driver in the interpreter, feeding `XtFxChip`)
carries over to rom.4 with these addresses.

**The programs** (rom.2's from `fxmap.cpp`'s table and boot; rom.4's from its boot's literals and a run; each compared word
by word, the differing words sorted by the word's fields, HARDWARE_FIRMWARE.md "The word, as I read it"):

| Program | rom.2 | rom.4 | Words equal | The rest |
|---|---|---|---|---|
| A: routing, sends, mix, EQ, Master, Output Gain (boot) | `0x897a851c` | `0x895167bc` | 110 of 768 | rewritten: 628 words used against 584, to word 0x2eb against 0x2c5; register, op, coefficient, shift and X differ, the memory field never |
| B: Chorus, all types (boot) | `0x897a911c` | `0x895173bc` | 745 of 768 | 23: 10 in the register alone, 13 in other fields too; registers renumbered one down (0xec→0xeb, 0xed→0xec, 0xee→0xed, 0xf0→0xef, 0xf1→0xf0; 0x75, 0x77, 0x79 rotated), 11 memory fields, 3 ops, a shift, a coefficient, an X |
| The 42 MFX types, 307 words each, in rom.2's order at rom.2's place less 0x291d64 | | | 276…307 of 307 (2 identical) | 623 words in all, **only the register field**: 0xea → 0xe9 (601), 0xeb → 0xea (22) |
| The reverbs rom.2 has, 350 words | eight | moved | 345…349 | **only the register field**: 0xf1 → 0xf0, 13 words in all |
| The reverbs' transition program, loaded before each type's | `0x897bd1a8` | `0x8952bac4` | 350 of 350 | |

Every register change in the programs rom.4 shares with rom.2 is one down, among 0xea…0xf1 (with the Chorus's 0x75, 0x77
and 0x79 rotated); what those registers hold, and whether program A's rewrite is why they moved, isn't read here. The MFX's
and the seven reverbs' arithmetic is otherwise the V-Synth's word for word, and the Chorus's but for 13 words. rom.4's MFX
names are rom.2's 42 in order but one (EP Multi is "E.Piano Multi").

**The reverb types, run** (`vc2run <rom.4> fxprog <rom.2>`, `vc2fxprog.cpp`; `build/tools/vc2/fxprog.txt`). rom.4's reverb
init, then each of its table's 17 types set (`0x89387490`), its parameters set to its typed defaults (`0x893874a0`) and
applied, the chip's registers as plain memory, the program loader hooked to log what it loads and the 42 stubs into the
kernel answered with 0. Each type loads the transition program, then its own:

| rom.4 Reverb Type (rom.4's list) | Its program |
|---|---|
| 0 Off | rom.2's Off |
| 1 Stereo Plate, 2 Stereo OperaHouse, 3 Stereo StudioSpring, 4 Stereo Echoes, 5 Stereo Room, 6 Stereo Cathedral, 7 Stereo Church | seven new programs (21…96 of 350 words like the closest of rom.2's) |
| 8 Room 1, 9 Room 2, 10 Room 3, 11 Hall 1, 12 Hall 2, 13 Hall 3, 14 Garage | rom.2's, shared as in rom.2 (Room 2 and Hall 2; Room 3, Hall 3 and Garage), every word equal or differing in the register alone |
| 15, 16 (in the table, past Reverb Type's 0–14 and rom.4's list) | rom.2's Plate and Non-Linear |

rom.2's Delay is in no type: the table ends at 17 entries (the 18th word is 0), though its 14 parameters (0x918…0x925, rom.2's
Delay's names in order), its class (a literal at `0x89384358`) and its program (`0x8952c76c`, 345 of 350) are all in rom.4.
The display's list (`0x89577ee0`, "00:Off" … "14:Garage") and the Reverb Type descriptor agree on 15. rom.4's long names
(`0x89575dfc`, 68 of 24 bytes, one pointer to it at `0x89923de0`) are the 42 MFX, the Chorus's 9 (Off first), then 17
reverbs with no Off: the seven stereo ones, Room 1 … Garage, Plate, Non-Linear and Delay, the table's order with Delay after.

So **the VC-2 runs the V-Synth's effect chip programs** for its 42 MFX, its Chorus and the seven V-Synth reverbs it offers,
their registers renumbered one down, beside a new program A, and adds seven stereo reverbs of its own; the V-Synth's
Plate, Non-Linear and Delay are there but unreachable from a patch. Not run: rom.4's MFX types loading their programs (the
bytes and the names are the two sources). Program A on `XtFxChip`: the next section; the new reverbs on it: "The new
reverbs on XtFxChip".

### Program A's routing (2026-11-13, the VC-2 mode's step 1c)

**How it was measured.** `vc2run <rom.4> fxroute <rom.2>` (`vc2fxroute.cpp`, with `vc2fxdriver.cpp`;
`build/tools/vc2/fxroute.txt`, `fxroute_scan.txt`). rom.4's effect driver runs as XtFxControl runs rom.2's: its static
constructor `0x89368280`, its starts `0x8935f9a0` and `0x8935fa80`, then the system's and a patch's message (the object
`0x89980f24`, the flags at `0x89980f84`, the reader `0x8935fc40`). Its bus primitives (`0x89362490` a word, `0x89362610`
a read, `0x893626d0` a coefficient, `0x893627f0` a memory offset) are hooked into an `XtFxChip`, each word at once, and its
parameter reads (`0x89324350`, `0x89324370`; rom.2's `0x8947d310`, `0x8947d330`) answered with rom.4's defaults or a
setting's values. The coefficient write doesn't pair by shape, as it was compiled anew, but its listing sends the same
three bytes from the same arguments. rom.2's side is `XtFxUnit` as the plugin runs it. Then, on copies of each settled
chip, an impulse into one GRAM word against a copy given silence: every word that differs, its first sample and its peak,
whether rom.4's word is rom.2's sample for sample, and the mix out (0xa0, 0xa2) of the two against each other, over 8192
samples. 17 settings, and a scan with each of the 256 words as the input.

**The routes**, rom.4's EQ Hi Gain set to rom.2's default so the two EQs are alike (below):

| From | rom.2 | rom.4 |
|---|---|---|
| MAIN L, R (0x20, 0x22) | the mix L, R (0xa0, 0xa2, again at 0xa8, 0xaa), through the system EQ | the same: 12 of 8192 samples differ, by −73 dB of the impulse at most |
| MULTI L, R (0x28, 0x2a), the MFX off | the mix | the same within −67 dB, and through the patch's EQ 1 and EQ 2 (below) |
| MULTI to the MFX | its input at 0x74, 0x76 | at 0x76, 0x78 |
| The MFX's out (0xf8, 0xfa) | the mix; to the Chorus at +6 dB (0x70); to the Reverb at +6 dB (0x72) | the mix, the same; to the Chorus the same, sample for sample; to the Reverb at −6 dB, its left to 0x72 and its right to 0x74 (and with a V-Synth reverb into 0x72 too) |
| The Chorus send (0x26) | the Chorus's input, 0x70 | the same, sample for sample |
| The Reverb send (0x24) | the Reverb's input, 0x72, at 0 dB | a stereo input, 0x72 and 0x74, at −12 dB each; with a V-Synth reverb the right is added into the left, so 0x72 at −6 dB |
| The Chorus's and Reverb's outs (0xf0…0xf6) | the mix | the same |
| DIR L, R (0x2c, 0x2e) | DIR out (0xa4, 0xa6) | nowhere: none of the 256 words reaches 0xa4…0xa7 |
| The outside input, read at 0x11 and 0x13 | left to 0x90, 0x94, 0x98, right to 0x92, 0x96, 0x9a, each a copy (and to 0x78, 0x7a) | left to 0x90, 0x94, 0x98; right to 0x9a alone, and nothing reaches 0x92 or 0x96 |
| The USB input, 0x18 and 0x1a, with USB External In Type STEREO | not tried | added into the same: left to 0x90, 0x94, 0x98, right to 0x9a |

Room 1's program is the same too: from its input word on, its outs are rom.2's sample for sample, so its output in the mix
is 6 dB down only by its input. Nothing in the chip reads 0x90…0x9a back (the scan), so they leave it; rom.2's sampling
pre-effects write 0x90, 0x92 (HARDWARE_FIRMWARE.md, "There is no output stage").

**What the driver reads.** At its start rom.4's driver reads Master Level, Clock Source, Output Gain (0x40, 0x43, 0x78),
the system EQ's Low Freq and Gain, Hi Freq and Gain, Total Gain and Switch (0x67, 0x68, 0x6f…0x72), USB External In Type
(0x7f), Sampling Template (0x34), Part 1's bank and program (0…2), the patch's Category and Tempo (0x12c, 0x13a), the three
switches, Direct Sound Level (0x195), EQ 1 and EQ 2's Freq, Q and Gain (0x1af…0x1b4), the MFX's, Chorus's and Reverb's own
IDs, the tone block's MFX Send Level, Reverb Send Level and OSC 1 Type (0x2e0, 0x2e2, 0x2e5), and 35 typed IDs. It doesn't
read the system EQ's Mid 1 and Mid 2 (0x69…0x6e), where rom.2's reads all of its EQ (0x65…0x70). The patch's EQ 1 and EQ 2
have their ranges and defaults (Freq 50…20000 Hz, 12 and 16; Q 1; Gain 0 dB).

**What the settings do.**
- The system EQ: switched off, rom.2's MAIN peaks at +6.0 dB of the impulse as rom.4's does, and rom.4's answers its Low
  Gain. Of their defaults only EQ Hi Gain differs: rom.2's 13 (−2 dB), rom.4's 15 (0 dB). So at the defaults rom.4's mix
  is 2 dB brighter (MAIN's peak +6.0 dB against +4.7).
- The patch's EQ 1 Gain at +15 dB moves MULTI's path (its peak +4.7 → +7.3 dB) and not MAIN's. So the VC-2 took the two
  mid bands off the whole mix and put them on MULTI, by patch.
- Direct Sound Level 127, OSC 1 Type EXTIN or VOCODER, External In Type STEREO or MONO-R, and the tone's MFX Send 0 with
  its Reverb Send 127: each is read, and none moves a route measured here, from the frame or the outside input. What they
  set is elsewhere: the SH-4's side, or words none of these inputs reach.
- The message's 32 bits one at a time (`fxroute <rom.2> flags`), the chip's words cleared before each: only rom.2's
  (system and Attenuation, a patch and the switches) write a word; the others write none. A weak check, as the driver
  skips a block it holds as loaded.

So **the new program A keeps the V-Synth's mix**: MAIN, MULTI, the MFX and the returns reach the outputs as on the V-Synth,
at its gains. What it changes: the Reverb's input is a stereo pair, for the seven stereo reverbs, fed 6 dB lower (12 dB
from the MFX); the MFX's input moved up a pair to make room; the system EQ's two mid bands are the patch's EQ 1 and EQ 2,
on MULTI alone; DIR goes nowhere; and the outside input's right side reaches one outgoing word of three. For the port, the
VC-2 mode's effects can go XtFxControl's way on rom.4's driver, as here, with the frame's eight words where they are.
Not settled: ~~the Chorus, whose output in the mix differs by up to +3.8 dB, its taps at 17 and 35 samples against rom.2's
36 and 16, with their LFOs' phases not lined up between the runs, so whether its sides are swapped isn't known (program B
differs in 13 words besides its registers)~~ (settled, "The Chorus's sides": the +3.8 dB and the taps were the two LFOs
at different points of one cycle; lined up, rom.4's Chorus is the V-Synth's to a unit, and crossing its sides is half a
cycle of its LFO, whose phase only the time since the chip started sets); what outside the chip reads 0x90…0x9a; what DIR out carries on the VC-2; what
the IDs read that moved nothing do.

### The new reverbs on XtFxChip (2026-11-13, the VC-2 mode's step 1c)

**How it was measured.** `vc2run <rom.4> fxreverb` (`vc2fxreverb.cpp`, `vc2decay.cpp`; `build/tools/vc2/fxreverb.txt`):
rom.4's driver on an `XtFxChip` (`vc2fxdriver.h`, as in "Program A's routing") set to each Reverb Type with its typed
defaults and its Pre Delay at 0; an impulse at half the chip's full scale into the reverb's left input word (0x73, what
program A wrote to 0x72 the sample before), then its right (0x75); the wet outs alone (0xf4, 0xf6, before the mix), 12 s.
The decay is the measure-reverb skill's: each out less its DC, Schroeder's backward integral, a least-squares line from
−5 to −35 dB. Then each type's time parameter over its range against the panel's text for it (rom.4's text routine,
`0x894fa730(id, value)`), once at the type's damping and once with Hf Damp and Lf Damp at 0 dB.

**The floor first.** The first run read T30s of 45 to 58 s. The tails don't fall to nothing: the impulse leaves a DC on
both outs (−236 LSB on Stereo Cathedral, about −640 on Stereo Plate), and on some types a residue around it as well, near
30 LSB RMS, that holds for the whole 12 s (`fxreverb envelope <type>`). The same chip with no impulse is exactly 0, so the
residue is the impulse's, through the programs' truncating arithmetic, as v126 found the V-Synth's tails end on DC. So the
integral is cut where the decay meets the floor, the floor taken off (Lundeby's way), and a reading is marked where the
tail is still falling at the end. On the V-Synth's Room 2 the T30 then reads its driver's table: 0.80, 1.00, 1.90, 4.25 and
11.00 s as 0.80, 1.00, 1.90, 4.19 and 10.75. Above about 10 s the 12 s isn't long enough to read.

**At the defaults** (Pre Delay 0; out levels are energy against the impulse's):

| Reverb Type | T30 | Onset (samples) | Left in: left out, right out | Left in against right in |
|---|---|---|---|---|
| 1 Stereo Plate | 1.64 s | 2 | −13.1, −25.3 dB | 0.43 |
| 2 Stereo OperaHouse | 1.99 s | 3 | −15.6, −21.7 dB | 0.64 |
| 3 Stereo StudioSpring | 2.59 s | 2 | −14.5, −25.7 dB | 0.46 |
| 4 Stereo Echoes | 4.51 s | 585 | −10.3, −17.2 dB | 0.64 |
| 5 Stereo Room | 1.08 s | 2 | −10.9, −22.2 dB | 0.47 |
| 6 Stereo Cathedral | 1.84 s | 3 | −14.1, −20.4 dB | 0.65 |
| 7 Stereo Church | 3.27 s | 3 | −13.8 dB, none | 0.00 |
| 8–14, the V-Synth's | Room 1 0.90, Room 2 1.00, Room 3 1.08, Hall 1 2.88, Hall 2 2.90, Hall 3 3.16, Garage 1.37 s | 310…1765 | equal on both outs | the right input reaches nothing |

The seven are **stereo in and out**. Each input comes out mostly on its own side, the other side 6 to 12 dB under it, and
the right input's response is the left's mirrored (each out's level the other's, within 0.3 dB). The two inputs' responses
are partly the same (0.43 to 0.65) but not mirror images of each other (−0.30 to 0.01). Stereo Church is two reverbs, one
a side: nothing crosses. With Pre Delay 0, six of the seven answer within 3 samples; Echoes at 585 (13.3 ms). The V-Synth's read only the left input word (program A adds the right into it for
them), give it to both outs alike, and start late, 310 to 1765 samples, as on the V-Synth.

**Rev Time is the undamped decay.** Its text is the driver's 80-entry time table's ("1.00[S]" at 12, "10.00[S]" at 41;
Echoes, Stereo Room and Stereo Cathedral from 0 or 6, "0.06[S]", "0.40[S]"), though no new type's code calls it. At the
types' damping (Hf Damp −5 to −20 dB, Lf Damp 0 to −6 dB, their defaults) the T30 is short of it, more so as it grows:
Stereo Plate 0.87 s at "1.00" to 7.53 s at "10.00". With Hf Damp and Lf Damp at 0 dB (36, "0[dB]", on every new type):

| Rev Time's text | 1.00 | 1.40 | 1.90 | 3.00 | 4.25 | 6.00 | 10.00 |
|---|---|---|---|---|---|---|---|
| Stereo Plate | 1.08 | 1.43 | 1.91 | 3.00 | 4.25 | 6.01 | 10.01 |
| Stereo OperaHouse | 1.07 | 1.41 | 1.90 | 3.00 | 4.24 | 5.99 | 10.00 |
| Stereo StudioSpring | 1.00 | 1.37 | 1.83 | 2.76 | 3.71 | 5.01 | 7.75 |
| Stereo Church | 1.41 | 1.59 | 1.96 | 2.96 | 4.18 | 6.04 | 9.95 |

(OperaHouse's 4.24 is one the run marks as still falling at the end.) Stereo Cathedral (from "0.40"): 0.61, 0.93 at "0.90", 1.52 at "1.50", 2.01 at "2.00", 3.78 at "3.75", 5.53 at "5.50",
9.97 at "10.00". Stereo Room: 0.40, 0.89, 1.30, 1.49 to "1.50", then short: 2.16 at "2.25", 3.34 at "3.75", 7.16 at
"10.00". Stereo Echoes doesn't follow it, as its decay is a train of repeats (Repeat Time): 0.91 s from "0.06" to "0.40",
then 1.52, 2.52, 4.43, 14.00 at "10.00". So on Plate, OperaHouse and Cathedral, and on Church from 1.9 s, Rev Time is
the decay's T60 within 1 to 2%, the damping taking the highs and lows off it; the shortest settings bottom out (Church
1.41 s at "1.00"); StudioSpring and Room run short at long settings.

So **the seven new reverbs run on `XtFxChip` as reverbs**, rom.4's driver loading and setting them, each decaying as its
panel says, given its damping. As with every program here, the chip is JE-8086's and the programs the firmware's; that
the VC-2 sounds so is not checked against a real one. Not measured: Room Size (four of the seven), Repeat Time, the
pre-EQ, the Pre Delay's own law, and the seven at sample rates other than 44.1 kHz.

### The Chorus's sides (2026-11-13, the VC-2 mode's step 1c)

**How it was measured.** `vc2run <rom.4> fxchorus <rom.2> [lfo]` (`vc2fxchorus.cpp`, `vc2fxchorusphase.cpp`, sharing
`vc2fxchorus.h`; `build/tools/vc2/fxchorus.txt`, `fxchorus_lfo.txt`): Chorus type 1 on both images, rom.4's on its own
driver over an `XtFxChip` (`vc2fxdriver.h`, as in "Program A's routing") and rom.2's on `XtFxUnit`, each settled; an
impulse into the Chorus send (0x26) and each word's response at the Chorus's outs (0xf0, 0xf2) and the mix (0xa0,
0xa2), less the trace with none, over 4096 samples, rom.4's words held to rom.2's sample for sample. Depth 0 first, so
the LFO moves nothing, with Pre Dly Time 50 and with Feedback 98 beside it; then the type's defaults (Rate 5, Depth 10).
With `lfo`, at the defaults: each side's delay through the LFO's cycle (an impulse every 64 samples for 192,000
samples, the delay the centre of the response's taps, so a read between two samples gives the fraction), the lag that
lines rom.4's track up with rom.2's with the sides as they are and crossed, the same to the sample with the whole
response there; and on each image the chip run on 51,200 samples, a setting reloaded and set back (Depth 0, the Chorus
Switch off, Chorus Type 2), and the track again.

**With Depth 0 the two are one Chorus.** Each out is rom.2's, 4096 of 4096 samples: the impulse at 5 samples on both
sides, at 224 with Pre Dly Time 50, and with Feedback 98 its repeats too. The mix carries it 0.012% lower on rom.4, by
program A's gains.

**At the defaults** the LFO is a triangle on both images, about 147,200 samples a cycle (3.34 s at 44.1 kHz), each
side's delay going from 5 to 48 samples with the other side's opposite (the two sum to 53.0 throughout). So the runs
that read taps at 17 and 35 against rom.2's 36 and 16, and a mix up to +3.8 dB apart, had the two LFOs at different
points of one cycle: rom.4's track is rom.2's 31,296 samples later, 0.0004 samples apart on average. Run on by that,
rom.4's outs are rom.2's for the first 96 samples on both sides, and 4082 and 4086 of 4096 samples over the whole
response (correlation +1.0000); the others are a unit apart, but for one sample of the left out, 81,800 against 89,980
where the taps reach 840,000 to 940,000 (about −40 dB). With the sides crossed, the best lag is rom.2 run on 42,287
samples, half a cycle (73,583 samples) from the first: 4081 and 4078 of 4096 equal, the others a unit apart. **So the
outputs can't say whether the sides are swapped**: for a mono send and two sides in antiphase on one triangle, crossing
them is half a cycle of the LFO, to a unit.

**Nothing but time sets the phase.** On both images the LFO runs on through a reload. rom.4's, run on 51,200 samples and
reloaded with Depth 0, the Switch off or Type 2 and each set back, stands exactly 51,200 samples on (its track 0.0000
samples apart); rom.2's 54,336 and 57,920 on, the run and about 3,100 and 6,700 samples that `XtFxUnit::settle` runs the
chip while the reloads' words go in. So a patch doesn't start the Chorus's LFO: it runs from the chip's start, and which
side leads at a note is the time since then, on both (HARDWARE_FIRMWARE.md, "The Chorus's LFO, run").

So **rom.4's Chorus is the V-Synth's, its sides as far as anything outside the chip can tell**, and the VC-2 mode runs
rom.4's program B as it stands. Not read: what program B's 13 differing words do (they leave Depth 0 exact and the LFO's
track the same to 0.0004 samples; whether they make the units' difference, or the LFOs' states differ by less than a
sample's step, isn't known), the panel's text for Rate 5 against the cycle measured, and the other Chorus types.


## The names sweep

`names-sweep.mjs` over rom.4 with the VC-2's 64 factory patch names, 58 wave names and the manual's file names
(`build/tools/vc2/names.txt`, from the disk image): 14 of 128 read, all as stored (260 wrappings tried). The four file names read
at 0x89515f0c…; 2 patch names and 8 wave names read, by single words ("Choir", "Female", "Male") in unrelated text. So the factory
names are in the files, not the ROM, and rom.4's text is plain: 35,295 distinct strings of four or more characters, 19,392 of them not
among rom.2's.

## Open

- **The V-Card owner's manual.** Every name above has one source. The manual is the second, and the one that says what a value
  means to a player (what LEGACY and HIRESO are, what the 16 formant templates are called).
- ~~What reads the vocoder block and the VOCODER OSC's IDs.~~ Found, second pass ("The vocoder's code"); the engines mapped in
  the third (LEGACY, HIRESO). Settled by running in the fourth ("rom.4 run in the interpreter"): ~~HIRESO's exact formant warp
  (slot 4's three branches)~~, ~~which controller drives the hold~~ (CC 64), ~~the carrier's growl branch~~, ~~kind 6~~.
  Still open: whether HIRESO's Depth routine `0x893f22d0` has a computed caller; the float argument `0x893f07a0` multiplies the
  consonant gain by, and the glue that calls it; `0x899d9d90`, which the EXT IN path's formant offsets divide by the tracked
  period; the analysis and synthesis groups' delays against HIRESO's 128-sample input latency; ~~the levels the part mixer ramps
  (`0x8999f4d0…`) beyond Direct Sound~~ (run, "The part mixer, run"; left of it: what drives the offsets `0x89997290…98`,
  which voices render into which buffer, the vocoder's output scale); ~~the pass the VOCODER OSC's own IDs (0x354…0x361) take into the voice's fields (P+0x30's
  ID among them: run the OSC converter `0x8939ced0` on a patch's block)~~ (run, "The VOCODER OSC's IDs, run"; left of it: where
  the Harm Formant's LFO and Envelope Depths and the Wave start offset are read, and the Dynamics' law); ~~kind 6's mode
  parameter and where its P comes from~~ (run, "Kind 6's pitch source, run": mode 0 always, P+0 the note's pitch bent by the
  getter's ratio; left of it: what sets the getter object's depth, Auto Note's mode 1 against 2 in the tone generator, read
  and not run, and what the mailbox's receiver does with the note's 12);
  whether the pedal jack reaches the hold as CC 64.
- ~~**Nothing here has been run.**~~ Four things have ("rom.4 run in the interpreter"), two of them held to a model bit for bit;
  the rest is still a reading of the code, checked against it by eye and by computed filter responses. rom.4's memory map is in
  the interpreter for research (`vc2rom.*`), so the project's way, the firmware's routines against a port, is open to the rest.
- The VC-2 as its own mode (the user's decision, 2026-11-13): its steps and their state are in `research/VC2_MODE_PLAN.md`.
  Next there, step 1b: each tone block's rom.2 routine run beside its rom.4 counterpart ("The V-Synth's tables and code in
  rom.4" gives the pairs by the tables they read).
- ~~The 64 factory patches' values, read with rom.4's descriptors.~~ Read by rom.4's own routines ("The factory patches, read by
  rom.4"); left: ~~the second descriptor bank (MFX, Chorus, Reverb, COSMs)~~ (read, "The second descriptor bank, read"; left
  of it: ~~what COSM type 8 is~~ (the Growl, "COSM type 8, the Growl, run"), ~~rom.4's four reverb types~~, ~~and the
  typed IDs no table lists~~; the reverbs and the
  untabled IDs settled, "The effect chip's programs") and what the store's eight tone and sixteen envelope blocks are.
- The pitch tracker ("The pitch tracker, run"): run in mode 0, the only one selected, on known signals; its analysis read
  ("The pitch tracker's analysis"), the band-pass, the noise count, the gates, the limits and the fallback held to the
  firmware by the run. Left: the estimator's rules by level, period and peak balance, read in outline only, and the
  direction state (`0x899d9c70`, `0x899d9c74`), read and not run: the port's harness will settle both; `0x893ef400`, which
  takes the block first when `0x8993fe48` is set; which inputs the frame's words at +4 (kind 6's ring) and +0x14 (the
  tracker) are.
- ~~The `WLF08` library's format, and whether it is the V-Synth's preset area's format too.~~ Read and checked, both ("The
  wave libraries, read"). Left: how rom.2 finds its 224 streams, ~~and how rom.4's player picks a map's split~~ (found, "A
  VC-2 wave's split, chosen", and what the split's bytes do, "The split's bytes, followed"), ~~and how rom.4's store comes
  to hold the VC-2's samples under ids 1…192~~ (run, "The sample bank, loaded"; left of it: ~~the wave reader `0x893474d0`
  itself~~ (run, "The wave reader, run"; left of it: chunk kinds 6–8, and the count rom.2's file reader gives its
  decoder, read and not settled: HARDWARE_FIRMWARE.md, "The V-Synth's wave load, read"), and how the OS fills the wave
  library from the link files).
- rom.4's sections (code, .data, .bss), and how much of its code is rom.2's.
