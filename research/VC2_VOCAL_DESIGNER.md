# The VC-2 "Vocal Designer": rom.4, first map by its own names (2026-11-12)

The XT runs three applications on one OS (`rom.1`): the V-Synth (`rom.2`), the VC-1 "D-50" (`rom.3`) and the VC-2 "Vocal
Designer" (`rom.4`, header `VAPP2 0.17`, 9.6 MB). The [V-CARD] button switches between them without a restart (XT owner's manual,
"Starting up V-Card"). This file is the VC-2's, kept apart from HARDWARE_FIRMWARE.md because nothing in it touches the V-Synth's
ports. Names-first pass only: no routine has been disassembled yet.

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
  map, so they can't run or read rom.4 as they stand; `ramrd.py` reads it ("The tools").
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

## The vocoder's code (2026-11-12, second pass)

Found from what loads the vocoder block's IDs, then followed by hand in `sh4dis.py` listings (`ROM=` rom.4). rom.4 is compiled
differently from rom.2: matching rom.2's routines instruction by instruction (pool displacements and branch offsets masked) finds
none of the tone code, so nothing here can be taken from a rom.2 address. A Ghidra project of rom.4 is in `build/tools/vc2/ghidra`
(analysed, not yet used). Every address is rom.4's.

**How rom.4 reads a parameter.** As rom.2 does, with its own routines: a pointing routine aims a read object at the part's block
(`0x893207b0(obj, part)`) or at one OSC's (`0x89320a50(obj, part, osc)`), then the getter `0x89324350(obj, id)` returns the value.
An OSC's IDs are OSC 1's, read once per OSC with the object pointed at it. The IDs are built in registers (`mov #2; shll8;
add #-97` is 0x19f), so a search for literal IDs misses them; `idbuild.py` ("The tools") tracks those constructions.

**The converter, `0x893a72f0(?, part)`.** It reads the vocoder block in this order and hands the results on:

- Formant Shift (0x19f), 0 taken as 1, as (v − 64)/63 into a library call at `0x89506e54` with 2.0: by its arguments 2^((v−64)/63),
  an octave each way. Not confirmed which library routine that is.
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
`0x89393980` that compares a 0…127 value with 64: by that, a pedal or switch, probably the Hold pedal. Not proven.

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
template's record, 12 by default), with separate branches for ratios over 1.05, under 0.95 and between. The exact warp is not
read. `0x893f07a0` (from the glue, `0x893b16f4`) calls it four times, for each side.

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
parameter lands in is not traced, nor the branch taken when the field at +0x3c is positive (`0x893ec240`, which calls
`0x893f3280` and `0x893f3210`, likely the growl). Kind 6 is written inside the EXT IN path (`0x8939ee5e`); its render is
`0x893ec730(voice + 0xafc, out)`. Not read.

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
  the third (LEGACY, HIRESO). Still open: HIRESO's exact formant warp (slot 4's three branches); whether HIRESO's Depth routine
  `0x893f22d0` has a computed caller; the per-block value `0x893f07a0` multiplies the consonant gain by, and the glue that calls
  it; which controller drives the hold (slot 6; a handler at `0x89393980` compares it with 64); the analysis and synthesis
  groups' delays against HIRESO's 128-sample input latency; the levels the part mixer ramps (`0x8999f4d0…`) beyond Direct
  Sound; the carrier's growl branch; kind 6; and the pass the VOCODER OSC's own IDs (0x354…0x361) take into the voice's fields.
- **Nothing here has been run.** Every formula is a reading of the code, checked against it by eye and by computed filter
  responses. The project's way is a harness: the firmware's routines in `Sh4Interp` against a port. That needs rom.4's memory
  map in the interpreter first (its own .data copy and .bss, "Sources").
- The 64 factory patches' values, read with rom.4's descriptors (the `.p02` store's layout, as `XtPatchStore` reads a `.p00`'s).
- The `WLF08` library's format, and whether it is the V-Synth's preset area's format too.
- rom.4's sections (code, .data, .bss), and how much of its code is rom.2's.
