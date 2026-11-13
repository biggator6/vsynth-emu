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
  address − 0x21a64 in the file; read in place it is something else. `Sh4Interp::loadRom` and `build/tools/bin/rd` hold rom.2's
  map, so they can't run or read rom.4 as they stand; `ramrd.py` reads it ("The tools"), and `vc2rom.*` maps it in the
  interpreter by this header for the research runs ("rom.4 run in the interpreter").
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
- `diskls.py [filter]`, `diskls.py get <path> <out>`: the disk image's FAT16 files (`$DISK` for another image).
- `likeat.py <A> <addr> <n> <B>`: code in B shaped like A's at addr, with what moves when code moves masked out.
- `vc2run <rom.4> <run>` (C++, build line at the top of `vc2run.cpp`; run through `.claude/tools/buildq.mjs`): rom.4's
  routines in `Sh4Interp`, on `vc2rom.*`'s map; runs `boot`, `warp`, `ratio`, `hold`, `growl`, `kind6`, one file each
  ("rom.4 run in the interpreter"). `warp check` and `growl` hold a model of a routine (`vc2warplaw.cpp`, `vc2growllaw.cpp`)
  to the firmware bit for bit, and fail when any word differs; `ratio` fails past 2 ulp from powf; `kind6` fails when an
  output's period is more than a sample from the reading's.
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
  over their shapes), its scale in "The analog OSC in rom.4". `romfile.option` runs a tool's `--option`s and
  `romfile.images` reads a two-image tool's arguments (codepairs.py's and codealign.py's).
- `vc2run <rom.4> <block> <rom.2>` (`lfo`, `env`, `cosm`, `osc`, `analog`): a block's rom.2 routines and their rom.4 pairs on
  two interpreters, its object compared after every call, and the port held beside rom.4's where one is linked (`vc2twin.*`,
  a run per block, `vc2twin<block>.cpp`; `lfo` and `env` link their ports from plugin/Source, `cosm`, `osc` and `analog`
  none). Fails on any difference: `cosm` does, at the Comb ("COSM in rom.4"), and `osc` at the pitch ramp ("The OSC block in
  rom.4").

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
COSM's voice side is too for 14 of its 15 types (below), its converters and the Comb are not; the OSC block's arithmetic
and key state are, its wiring and portamento are not; the analog OSC's waveforms are, for the 11 the VC-2 has (no SUPER-SAW,
no FEEDBACK-OSC), its note-on, install, update and converter are not.** Still to run: the band and SOLO players, Vari off and
LITE, key assign.

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

**Run (`vc2run <rom.4> cosm <rom.2>`, `vc2twincosm.cpp`).** No port is linked: xt_cosm holds `XtCosm` to rom.2, so rom.2
equal to rom.4 is the port equal to rom.4. For each of the 15 types, 200 notes: a block made by rom.2's converter on a random
input (each word that indexes a table held in its range, as xt_cosm holds it) and given to both images, then the note-on
with the slot finder hooked in each to hand out four buffers of junk, six updates with new envelope, LFO and pitch values,
and for the TVF and Dynamic TVF 40 filter blocks of signal; the object, the four buffers and the signal compared after every
call (`build/tools/vc2/cosm_run.txt`):

| Codes | Types (by xt_cosm) | Result |
|---|---|---|
| 2, 3, 4, 6, 7, 8, 12, 15, 16, 18, 19, 20 | among them the Side Band Filters (7, 8), the Compressor (16), Lo-Fi (19) | 200 notes each, 0 different |
| 13, 14 | TVF, Dynamic TVF | 200 notes each with 40 filter blocks, 0 different |
| 9 | Comb Filter | 200 of 200 different at note-on |

So **14 of the 15 types' note-on, update and (for the TVFs) filter are the V-Synth's**, the note-on and update included,
though rom.4 compiled them anew and 0x40 and 0xc0 bytes smaller. The Comb is not: rom.2's note-on sets it up in place
(the case at `0x894ee300`: the octave byte at +0x14 through the octave table to +0xf4, then the delay's whole and fraction),
rom.4's sends code 9 to a call of `0x893b9d30(object)`; given rom.2's block, rom.4's Comb leaves another word above the
octave byte at +0x14 and, at +0x2c, a whole number where rom.2 has a ratio near 1 (−40.0 against 1.017). So the VC-2's Comb
reads its block otherwise, and is settled with rom.4's own Comb converter. (The other types' cases have the same sizes in
both note-ons' jump tables, `0x894ee118` and `0x893aa288`; only code 9's case moved.)

Two wrong first runs, kept here so they're not made again: random input words past a table's index range (the TVF's filter
type, an octave) make both images read past the table, into neighbours each image holds its own (the octave table is
copied for 198 bytes, the filter's kernel table is followed by code), which gave 612 notes "different" and a fault in rom.2's
own filter. Held in range, they all agree. Seen red: one word of rom.4's copy of the block nudged sets 13 types apart on
every note (the first Side Band Filter doesn't read that word).

Not yet run: the types' own DSP routines but the TVFs' (copies by shape, at the common distance), the stereo copies, the
new-block routine (`0x894ed390` → `0x893a9500`) and the Dynamic TVF's targets apart from note-on.

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
| Rewritten, not run | the VariPhrase converter `0x894e2f20`, the pitch target `0x894f3a00`/`0x894f3ba0`, the gain target `0x894f3dc0`/`0x894f3e20` (rom.2's seven routines 0x894f3a00…0x894f3ec0, 0x5c0 bytes, are three of 0x470 in rom.4 at 0x893afff0…), the voice update's 70% step `0x8950c1b0`, the portamento settings `0x894e66e0`, the note's setup `0x894e4c30` and random seed `0x894e5010`, the bend `0x894e9fe0`, the velocity curve's patch converter `0x894ebed0` | no alike run of more than 12 instructions within 0x400 of their place |

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
  from rom.4, its law read from those three; the start, update and value are the V-Synth's.

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

So the note-on, install, update and the converter (rom.4's `0x8939ced0`, "The carrier: the VOCODER OSC"; rom.2's `0x894e1920`
has no pair by shape) are the wiring, ported from rom.4 as COSM's converters are; only the waveform routines are run.

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
places them at +0x88). What rom.4 keeps there is not read yet. First written as inserted at +0x80, from the `--pools` line
0x80 → 0x84 of `0x89509360` (SQUARE with the sub oscillator): that routine loads its pool word 0x80 twice, as the base of
+0x80 and +0x84 and later to step from +0x84 to +0x104, and rom.4's steps by a word of its own, 0x84, to +0x108; `--pools`
pairs pool words by place, so the second use read as the first. The run with the insert at +0x80 gave 68,320 of 88,000
blocks different.

Seen red: the insert at +0x84 (68,320 different); no insert (rom.4's NOISE routine `0x893c2230` runs to the step limit on the
misplaced fields, and the run fails); SAW paired with rom.4's SQUARE routine (3,890 different); one bit of rom.4's sub-object
flipped at the eighth block (4,077 different).

**So the VC-2's 11 analog waveforms are the V-Synth's, and `XtAnalog`'s routines serve them as they stand, given rom.4's
sub-object; its note-on, install, update and converter are its own.** Not run: how rom.4's converter maps OSC 1 Analog
Waveform (ID 0x2f1, 0…10) to the input's code (the names' order says one for one).

## rom.4 run in the interpreter (2026-11-12, fourth pass)

plugin/Source is unchanged: `Sh4Interp::loadRom` keeps rom.2's map. `research/tools/vc2rom.*` maps rom.4 through the
interpreter's public `mapRam` and `write` by its own header: the image at 0x89300000, .data [0x8992e1bc, 0x8994fc20) copied from
0x8990c758, .bss zeroed to 0x8a1f11c4 (refused unless the source ends where the copy begins), with XtFwMachine's stack and heap
and operator new (`0x89506df4`) answered from the heap. Where a run stands in for the OS it says so; nothing else is hooked but
the probes named. `boot`: the vocoder's builder `0x893f0490` on its static object, 31,748 steps; both engines' vtables and their
eight slots are the table under "The vocoder object".

### HIRESO's formant warp, run (`warp`)

Slot 4, `0x893f15d0(engine, side, ratio)`, writes the side's map and nothing else (a diff of the whole engine): 32 floats at
the engine's +0x2924 (side 1's at +0x2b24), for each synthesis band the fractional analysis band it reads. Its law, read and then
held to the firmware bit for bit (`warp check`, `vc2warplaw.cpp`: 40,352 cases, both sides, pivots p = 0, 1, 3, 6, 9, 12, 18,
20, 22, 24, 40, 63 and 127, ratios 0.5…2 every 1/1024 and the floats either side of 0.5, 0.95, 1, 1.05 and 2; 0 bands
different, and 26,476 with an error of 1e-7 planted in one blend's weight):

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

What the glue gives slot 4 (`0x893f07a0`, read, not run): it calls slot 4 twice a block, side 0 then side 1. With the vocoder's
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
hook at `0x893ec310`; `vc2growllaw.cpp`; 256,000 samples at 55 Hz…10 kHz and growl 0.05…1, 0 different; planted errors in the
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

Where g comes from (read, not run): the kind 5 update `0x893afff2(voice)` builds P at voice + 0x104 and gives P+0x20 =
clamp₀¹(P+0x30 + m₁·P+0x34 + m₂·P+0x38) and P+0x2c = clamp₀²(P+0x44 + m₁·P+0x48 + m₂·P+0x4c), m₁ and m₂ two modulators the voice
points at (+0xa50, +0xa60): the shape of Growl Level and Saw-Noise Mix Balance with their LFO and Envelope Depths. Which ID lands
at P+0x30 (and where Growl Keyfollow goes) is not traced: the converter hasn't been run.

### Voice kind 6, run (`kind6`)

Kind 6 (written in the EXT IN path; render `0x893ec730(voice + 0xafc, out)`) is a pitch-synchronous (TD-PSOLA) pitch shifter of
the mic:

- the block mixer writes each block of the mic into a 4096-sample ring (`0x893ec820(input, 32)`: `0x8a0f31d0`, write index
  `0x8a0f31cc`) and the pitch tracker's period for that block (`0x893ee010`, `0x899d9bd0`, in samples; 551.25, 80 Hz, at the
  tracker's init) into a 128-block history (`0x8a0f71d4`, which `0x893ec7d0` resets to 44100/440);
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

**Mode 1 is an interval (a harmonizer), mode 0 a pitch: the voice sung at a played note, whatever is sung.** Not traced: which
parameter selects the mode (`0x893ec890` and the voice's start have no pool reference, so they are called through something not
yet found, and the mode word's .data value, 2, is one `0x893ec890` never writes), and where the voice's P comes from (the
keyboard's note, Auto Note). A pulse train in can leave grains empty: the epochs are period-spaced from where the voice started,
not placed on pulses (the run first used one and got silence at 300 samples).

## The vocoder's code (2026-11-12, second pass)

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
  `0x8999f4e0` (setter `0x893b1eb0`). Where the others go has not been traced.
- Then, if either OSC's Type (0x2e5) reads 2, Auto Note On Switch, Legato Switch and robot human depth (÷127, to the setters
  `0x893ebc20`, `0x893ebc60`, and the Formant Template's table at `0x89540904`, 16 entries of 140 bytes, offsets the formant).
  Otherwise those are set to 0. So Auto Note and robot human depth belong to a mic played through an EXT IN-type OSC, not to the
  vocoder. (Type 2 is EXTIN by rom.4's text table; why the test is 2 and not 3 is not read yet.)
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
per sample with levels ramped across the block from `0x8999f4d0…0x8999f4fc`.

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
- the block's peak after the highpass, and a running maximum over the last five blocks; 1/256 of it (at least 0.002) is a
  hysteresis;
- a count of the third filter's output crossing zero, Schmitt-trigger style (it must swing past ± that hysteresis): a
  zero-crossing rate. The last four blocks' counts plus 0.75 of the previous total is compared with the threshold (`0x899d9984`:
  Unvoice Detect + 24 from the settings, and 29 once a block has been called unvoiced, so it holds); over it, the block is
  unvoiced;
- that 0/1 goes into a 20-block ring (`0x8a0f87dc`) and is read five blocks later, so the decision lags 160 samples. It is
  smoothed per sample into 32 floats (`0x899d99dc`, one a sample of the block), rising at 0.002 and falling at 0.001 a sample;
- beside it, the peak, a peak that decays 1% a block (`0x899d9a68`), and a level with its own attack and release
  (`0x899d9a6c`) set a gain stored in `0x899d99c4`.

Its state is reset by `0x893ecf60` (from the glue's init, `0x893b1688`): Unvoice Detect's store at 50, the rings cleared.

**The output stage, `0x893ed510(output, fr4, fr5)` (mono) and `0x893ed690` (stereo): the mic's consonants.** It keeps the mic
input in a 640-sample ring (`0x8a0f73dc`) and reads it 448 samples back, takes the unvoiced flag five blocks back × fr5 (slot 7's
result), smooths it per sample (toward 1 at 0.002, toward 0 at 0.01), gates the delayed mic with it, and passes that through a
two-state highpass (coefficients 0.35 and 1.65). With Unvoice Spread on it goes through two delay stages of gain 0.6 on the
buffers at `0x8a0f882c` and `0x8a15a2ac`. Then per sample:

out = engine's output × level + consonants × fr4,

where level is Vocoder Sound Level (`0x899d9980`) plus `0x89997298` (a modulation, not traced), within 0…1, gliding 1/32 a block
(`0x8993fe5c`), and fr4 is the vocoder's +0x14 × 2^31. So Unvoice Level and Depth set how much of the mic's own unvoiced sound is
added (with HIRESO only: +0x14 stays 0 with LEGACY, see HIRESO's "Vocoder Depth"); Unvoice Detect sets how readily a block counts
as unvoiced; Unvoice Spread diffuses it. The stereo stage does the same into both channels.

### LEGACY: a 10-band channel vocoder

Read from the listings by a forked agent, then checked here: the band count, both coefficient tables, the band gains, the
response of every band (computed again), the loop bound and the follower's constants. Nothing here has been run.

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

The 32 band centres (`0x8994069c`) are 100 Hz to 1400 Hz in steps of 130, then steps widening from ×1.097 to ×1.184, up to
11874 Hz: linear where the voice's formants are, wider above.

**Analysis** (slot 1 → `0x893ed930`, .bss globals). Per block: frame count mod 14; the input decimated to half and quarter
rate; per group a first difference (pre-emphasis) and per band two resonators, one at each edge of the band, with gains of
opposite sign (`0x893eda90`); then per band an energy, scale × Σ (band sample × gate)², into a 14-frame ring. The gate is the
shared detector's: `0x893ed3e0` gives "voiced" when neither the flag 1 block back nor 5 back says unvoiced, and it glides at
0.001 a sample rising, 0.01 falling. So unvoiced blocks drop out of HIRESO's band envelopes. The envelope (`0x893edd10`, called
by the synthesis) is the square root of the largest energy in the last N frames, N from `0x8956141c`: 14 for bands 0–25, then
13, 12, 11 and 10 for the top three: about 10 ms.

**Synthesis** (slot 2 mono `0x893f0c60`, slot 3 stereo `0x893f1240`). The output buffer's carrier is decimated the same way and
run through the carrier bank (`0x893f1a10`): 2n + 1 resonators a group, a centre one per band and edge ones shared by
neighbours, band b = edge(b − 1) + centre(b) + edge(b), so overlapping bands sum flat with alternating signs. Per band and block:
the target is the analysis envelope at a fractional band index (the formant map, linear between neighbours), through the hold,
ramped across the block, followed with the attack coefficient when rising and the band's release coefficient when falling
(scaled by the group's rate), and out += carrier band × follower × template gain × W_b, where W_b = (−1)^(b+1) · A_b · B_b ·
50/(A_31 · B_31) from `0x8994059c` and `0x8994061c` (built in the constructor). The groups are interpolated back up and summed.
Stereo does the same per channel, each with its own formant map, template curve and follower state.

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
ANALOG and VOCODER both go through the analog converter `0x8939ced0`, which writes kind 3; VOCODER then writes **kind 5**. PCM is
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

That is the Saw Wave LPF, Noise LPF and HPF and Saw-Noise balance of the OSC's own parameters, by shape. Which field each
parameter lands in is not traced~~, nor the branch taken when the field at +0x3c is positive (`0x893ec240`, which calls
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
field from 0x193 to 0x1a5 starts on the bit after the last one ends, and each range matches its text's count but two (below). rom.2's second descriptor bank (8 bytes per ID
from 0x55d) has not been looked for in rom.4.

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
64 factory patches and the code that reads the field to say.

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
OSC rather than one played from a wave. That is a reading of names, not of code.

### System

Chord Memory: Switch and Hold (0x37, 0x38) and 12 keys × 4 notes (0xc0…0x11e, every other ID). External In Type (0x7a):
STEREO, MONO-L+R, MONO-L, MONO-R.

Screens named in rom.4 and not in rom.2 include `VC-2 MODE MENU`, `Go V-SYNTH!`, `SYSTEM MIC Setting`, `MIC Setting Name`,
`MIC Sets`, `PATCH Chord Memory`, `VOCODER    MIX` (in the list after the Matrix's destinations, beside the MFX knob
assigns) and `Vocoder/Natural`. Where the MIC settings are stored, and whether they are the V-Synth's sampling templates (IDs
0x4f7…0x525 in rom.4, with the pre-effect chain), is not known.

## The files

`Internal.vc2` in the disk image holds:

- `VC-2 Palette.s00` (304 bytes), `VC-2 Template.s00` (280), `VC-2 System.s00` (136). The manual's figure also shows a
  `VC-2 Setup.s00`; the image has none.
- `Patch/`: 128 `.p02`, 5060 bytes each. 065–128 are byte for byte 001–064 (all 64 pairs compared). A `.p02` has the `.p00`'s
  file layout: `00 1e`, a type tag (`VC20Rev008` where a `.p00` has `SVD0`), then `PATa`, a 4-character tag (`VC_2`; a `.p00` has
  `KATE`), 0x20 and a size (0x13a4; a `.p00` 0x14fa).
- `Wave/`: 58 `.w02`, 36 bytes each: a `link` chunk holding a preset wave's number (`VP-330 Saw` is 42) and a 16-character `name`.
  No audio.

The audio is a library after the V-Synth's 416 preset waves: at file offset 0x356d200 of the disk image, header `WLF08`, then
`1.01VC2W20050316` and `1.00VC2P20050314`, three section entries, and a count of 0x3a = 58; its data ends at 0x38154d0
(about 2.7 MB). Not decoded. rom.4 names the drives it is reached by: `V:\Preset.vc2\Patch\`, `V:\Preset.vc2\Wave\`,
`A:\Internal.vc2\`, `Z:\Demo.vc2\`, `Z:\TestMode.vc2\`, and `VC-2 Patch.bin`, `VC-2 Preset.bin`, `VC-2 Wave.bin`,
`VC-2 Project.ini`.

The disk image's root also holds the VC-1's files: `VC1.S10` and `VC1_I1.P10`…`VC1_I8.P10`, 36944 bytes each.

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
  period; the analysis and synthesis groups' delays against HIRESO's 128-sample input latency; the levels the part mixer ramps
  (`0x8999f4d0…`) beyond Direct Sound; the pass the VOCODER OSC's own IDs (0x354…0x361) take into the voice's fields (P+0x30's
  ID among them: run the OSC converter `0x8939ced0` on a patch's block); kind 6's mode parameter and where its P comes from;
  whether the pedal jack reaches the hold as CC 64.
- ~~**Nothing here has been run.**~~ Four things have ("rom.4 run in the interpreter"), two of them held to a model bit for bit;
  the rest is still a reading of the code, checked against it by eye and by computed filter responses. rom.4's memory map is in
  the interpreter for research (`vc2rom.*`), so the project's way, the firmware's routines against a port, is open to the rest.
- The VC-2 as its own mode (the user's decision, 2026-11-13): its steps and their state are in `research/VC2_MODE_PLAN.md`.
  Next there, step 1b: each tone block's rom.2 routine run beside its rom.4 counterpart ("The V-Synth's tables and code in
  rom.4" gives the pairs by the tables they read).
- The 64 factory patches' values, read with rom.4's descriptors (the `.p02` store's layout, as `XtPatchStore` reads a `.p00`'s).
- The `WLF08` library's format, and whether it is the V-Synth's preset area's format too.
- rom.4's sections (code, .data, .bss), and how much of its code is rom.2's.
