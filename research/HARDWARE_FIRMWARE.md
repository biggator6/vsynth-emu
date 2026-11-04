# V-Synth Hardware & Firmware — Reconnaissance (2026-10-10)

Goal: decide whether identifying the V-Synth's processors and examining its
firmware can supply what black-box testing and the patents cannot — exact
constants (knob curves, classifier thresholds, window/filter tables).

**Hardware in use: V-Synth XT** (rack version). All reference recordings in
`analysis/test_files/` come from the XT, and the firmware examined below is
the XT's own v2.02 update — so firmware findings apply directly to the
reference unit. The original-V-Synth service notes are background only.

## Findings

### Processors
- **V-Synth XT service notes** (block diagram, p. 32 viewer / p. 36 printed):
  main CPU **Hitachi SH7750R (SH-4), 266 MHz**; sub-CPU **H8/3061** (panel,
  encoders, LEDs); a "DSP bus / PCM bridge" block whose part number is not
  legible in the text layer. Main 1/5 circuit diagram lists **IC10 =
  HD6417297BP267** (text extraction; likely the SH-4 — verify on page image).
- **XT spec sheet**: "Sound generating section: 32 bits (floating point)";
  "Effects section: 24 bits (fixed point)".
- **Original V-Synth service notes** (2003; background — not our unit): text layer names memories
  (K4S561632 SDRAM, LH28F320 flash, K9F5608 NAND wave memory), DAC AK4393,
  codec AK4524, a 133 MHz clock — but no CPU/DSP part number in the text.
  Circuit diagrams pp. 37–39 need to be read from the page images.

### Interpretation (hypothesis — unverified)
The SH-4 is a strong single-precision float processor. A floating-point
sound generator plus a separate fixed-point effects section is consistent
with **VariPhrase running as SH-4 software** and the fixed-point DSP doing
effects only. Supporting evidence: the VC-1 (D-50) and VC-2 (VP-330) cards
boot a *different OS* on the same hardware (VC-1 boot 14 s vs 4 s), i.e.
the sound engine is reprogrammable from storage, not hard-wired.

If true, the VariPhrase encoder and player are ordinary SH-4 machine code
inside the OS update — disassemblable with Ghidra (SH-4 is supported), no
undocumented DSP instruction set involved. This is the opposite of the
JP-8000 case (JE-8086 emulator), where four custom Toshiba "ESP2" DSPs with
an undocumented ISA had to be reverse-engineered from silicon.

Must verify before investing: (1) the DSP block's part number from the
XT circuit diagram images; (2) the update file contains SH-4 code (not
compressed/encrypted) with float-heavy routines. → (2) confirmed below.

### Firmware availability
- V-Synth **System Update v2.00** (PC and Mac) was distributed by Roland;
  V-Synth GT and XT updates also exist. Install = copy an update folder over
  USB mass storage (hold button 1 at power-on) — so the update is a plain
  file set on disk.
- **VariOS** (same VariPhrase lineage) has a System Update v1.07 and
  **V-Producer 3** (Win/Mac). Reviews say all audio processing runs in the
  module, so V-Producer likely does *not* contain the encoder — low priority.
- Encoding on the V-Synth itself runs on-device (Sample → Encode → type
  Lite/Solo/Ensemble/Backing), so the encoder (grain cutting, pitch
  detection) must be in the V-Synth OS.

## Firmware triage — V-Synth XT v2.02 (2026-10-10)

Files in `firmware/` (gitignored — copyrighted, never commit). Unpacked
copies in `firmware/unpacked/`.

**No encryption.** All payloads are plain zlib streams (`78 DA`) behind
`MI004   ` headers (the XT's model ID).  `V-SynthXT.rom` is a section table
(entries mapping to SH-4 P2 addresses `0xA000_0000+`) followed by five
compressed blobs:

| Blob | Header | Size | Contents |
|---|---|---|---|
| rom.0 | MI004 0.09 | 112 KB | boot loader (zlib inflate 1.1.3, "Program Version") |
| rom.1 | MI004 2.02 | 992 KB | system/kernel — "V-Synth XT Ver.2.02", panel test strings |
| rom.2 | VAPP1 0.20 | 8.75 MB | **V-Synth synth application** — "SAMPLE Encode", "Encode Type :", "Encodeing Error !", "Encode Depth", LITE/SOLO/ENSEMBLE/BACKING |
| rom.3 | VAPP3 2.00 | 12.6 MB | VC-1 "D-50" application |
| rom.4 | VAPP2 0.17 | 9.6 MB | VC-2 Vocoder application (shares tables with rom.2) |

`MI004.BIN` = updater program; `V-SynthXTDiskImage.bin` = 65 MB FAT disk
image (MBR, partition type 0x04) — presets/waves, not yet examined.

**Code is little-endian SH-4.** Prologue opcodes (`sts.l pr,@-r15`,
`mov.l r14,@-r15`, `lds.l @r15+,pr`) are 4–5× more frequent read LE than BE
in every blob.  rom.2 begins `mov.l @(disp,pc),r0; jmp @r0; nop` jumping to
`0x8930_0040`, where a function prologue sits at file offset 0x40 →
**load base 0x8930_0000** (P1 cached RAM).  Ghidra target:
`SuperH4:LE:32:default`, base 0x89300000.

**"Encode Depth"** (`HID_SPL_ENCODE_DEPTH`) is a user parameter we have not
modelled — check the owner's manual.

**Candidate float32 tables in rom.2** (same tables duplicated in rom.4):
- `0x20ee00`: 384 floats = **64-phase × 6-tap polyphase interpolation
  kernel** (phase 0 = [-0.106, 0.123, 0.859, 0.123, -0.106, 0.004]; not an
  identity phase → band-limiting interpolator).  Likely the sample/grain
  read resampler — i.e. what pitch/formant read-velocity playback runs
  through.  High-value: directly comparable to our resampling.
- `0x4eebb0`: 129-tap symmetric lowpass FIR, sum ≈ 8.1 (×8 interpolator?).
- `0x20ddcc`: cosine table, step π/128.
- `0x4e38b3`–`0x4ebfb3`: monotonic ±9377 ramps (likely Hz/frequency maps).
Unresolved until disassembly cross-references show which routines read
them.

**Still open:** whether VariPhrase synthesis runs on the SH-4 or the
separate DSP block. Settled by Ghidra: find the code that reads the
interpolation kernel and the encode routine reached from "SAMPLE Encode".

## Ghidra pass 1 (2026-10-10)

Setup: Ghidra 12.1.4 (Homebrew formula, bundled openjdk@21 — set
`JAVA_HOME=/opt/homebrew/opt/openjdk@21/libexec/openjdk.jdk/Contents/Home`
for headless use). Project in `firmware/ghidra/` (gitignored). rom.2 imported
raw at 0x89300000, entry 0x89300040, aggressive instruction finder on →
**5,671 functions**, analysis ~8 min. Decompiler needs FPSCR_PR = 0 and
FPSCR_SZ = 0 assumed (single-precision mode), or float code decompiles as
garbage. Helper scripts (SetupSH4 / FindRefs / Explore / Decomp) live in
the session scratchpad — recreate from this description if needed.

**Answer to "SH-4 or DSP?": the SH-4 does real audio DSP in software.**
- Floating-point-heavy code clusters in 0x894D_0000–0x8953_0000.
- Top FPU function `0x894f8a70` (795 FPU ops) is a per-block audio
  processor: interleaved stereo float buffer, **32-sample blocks**,
  one-pole smoothed parameters, a cascade of filter stages with hard clips
  and a cubic waveshaper (x³·a + x²·b + x·c) — an analog-style filter or
  drive model (TVF/COSM-type). Called through a function pointer (no static
  callers), as expected of a DSP-chain slot.
- So the 32-bit float "sound generating section" in the XT spec is SH-4
  code. The VariPhrase engine itself is not yet located, but it is very
  likely SH-4 code in the same cluster.

**Resampler identified — `0x8952dc60` / `0x8952dd20`:**
16-bit sample input, 8.24 fixed-point read phase (integer advance = phase
>> 24), **8-phase × 16-tap polyphase FIR** using the 129-coefficient table
at rom.2+0x4eebb0 (= 0x897eebb0, offset +0x1e0 into a larger block), with
**linear interpolation between adjacent phases** (remaining fraction bits),
output clipped to int16. Callers 0x8952e4d0/e620/e740/eab0. Writes int16 →
likely the sample-import / sample-rate-conversion path (e.g. 44.1→48 kHz),
not the VariPhrase voice. Sibling variant `dd20` handles a second channel.

**Encode UI:** "Encode Depth" string is used by `0x89342150` — a UI event
dispatcher (switch on event id), not the algorithm. Other encode strings
are referenced from code Ghidra has not yet made into functions
(0x8943d828, 0x89440b1e, 0x893943e4…) — need function creation there,
then follow calls down from the "SAMPLE Encode" screen to the encoder.

**64×6 interpolation kernel (rom.2+0x20ee00):** no direct pointer found;
the surrounding region 0x8950e000–0x89510d00 is a dense cluster of small
functions with pointers into it — the kernel is probably addressed as
base+offset. Still to resolve.

## Ghidra pass 2 (2026-10-10)

**Internal sample rate is 44.1 kHz — not 48 kHz.**
- Float constant 44100.0 is referenced by ~20 audio functions (voice,
  filter, effect cluster); 48000.0 appears nowhere as a float, and
  1/44100 / 1/48000 reciprocals are absent (rates are divided at runtime).
- Test-mode strings describe the signal chain: `SRC Input -> Output(44.1K)`,
  `SRC Output(44.1K) -> Output(48K)`, `SRC Output(48K) -> Output(96K)`. The
  block diagram's CS8420 is a hardware sample-rate converter.
- **Consequence for us:** every 48 kHz reference capture in
  `analysis/test_files/` went through engine @ 44.1 kHz → hardware SRC →
  48 kHz. Our engine runs at 48 kHz. Sample-count constants (grain sizes,
  frame/hop sizes, onset windows) that we tuned against references may be
  off by 48/44.1 = 1.088; and the SRC's anti-imaging lowpass shapes the
  top octave of every reference. Worth testing: render at 44.1 kHz and
  resample to 48 kHz before scoring.

**Encode path:** "NOW ENCODING..." dialog = `0x89440ae0`; it starts encoding
through a C++ virtual call (`obj->vtbl[+0x14](obj + vtbl[+0xc])`, cfront-
style this-adjust), so the encoder can't be reached by static call graph.
"Encode Type :" sample-info screen = `0x893942b0`; sample-property screen =
`0x89441010` (Encode Type, Sampling Type, FWD/BWD loop points, Time
Signature, Original Tempo/Pitch/Fine Tune). "Encodeing Error !" lives in a
data table (pool word at 0x89834bb0), i.e. an error-message array.

**Pitch-math constants:** 2^(1/12) (1.059463) used by 0x89504af0,
0x89505400, 0x89518240; ln 2 / 1/ln 2 by 0x89531a28, 0x89531b78 (log2/exp2
helpers); 440.0 by 0x894f21c0, 0x89505950, 0x89506bf0; 1200 (cents) by
~15 small functions in 0x894dc–0x894e1 and 0x894e1920 (4 KB, many
callees — candidate per-voice pitch/parameter update). 0x89505950 decodes
as a filter-coefficient update (env + LFO → cutoff, pow2, type switch),
i.e. TVF, not VariPhrase.

**Decompiler blocker:** much of the audio code switches FPU mode
mid-function (`fschg` for 64-bit paired moves, `lds Rn,FPSCR` for double
precision). A single global FPSCR_PR/SZ assumption mis-decompiles these
(paired-register garbage). Fix: a script that walks each function
linearly, tracks `fschg` / `lds …,FPSCR` (constant-loaded) and sets the
FPSCR_PR/FPSCR_SZ register values per address range before decompiling.

## Ghidra pass 3 (2026-10-10) — decompiler fixed, DSP region mapped

**Decompiler fix** (scripts in `research/ghidra_scripts/`, see README):
`FpscrFix` tracks `fschg` / `lds rN,fpscr` per address; `ReadOnlyCode`
makes 0x89300000–0x8955FFFF read-only so literal-pool constants fold;
`DumpRange` decompiles with RespectReadOnly. Float code now reads cleanly.
`SweepRegion` added 262 functions in 0x894D0000–0x89540000 (hand-written
asm Ghidra had skipped).

**Hand-written DSP asm** sets FPSCR = 0x00040001: single precision,
round-to-zero, denormals flushed. Block size **32 samples**.

**Analog oscillators** — `0x89508800`, `…8aa0`, `…8c20`, `…8db0`,
`0x89509030`, `0x895094c0` (no static callers; function-pointer slots):
phase accumulator `phase += freq × 0.004988662` (= 220/44100 — confirms
the **44.1 kHz** engine rate), piecewise waveform with shape and per-sample
hard sync, one-pole parameter smoothing (×0.03125 per block), output ×2³¹,
written as stereo pairs. Dispatcher `0x8950bf70` switches on a mode field
(+0x80: cases 1/3/5/6/8) for setup / update / parameter routines; driven
from the pitch code at `0x894f3110` (which uses 1/1200 cents constants).

**PCM sample playback** — `0x8950eb60` (called by `0x8950eab0`): the
**64×6 interpolation kernel at 0x8950ee00 is the PCM playback
interpolator** (addressed PC-relative, hence no pointer). Read position is
fixed-point with 6 fractional bits → kernel phase (64 phases × 6 taps
over int16 samples; stereo = packed halves). Output = Σ·gain, then a
per-voice one-pole lowpass `y += (x·g − y)·c`. Caller handles loop
start/end, forward and backward loops, octave-shifted step. **No grain
windows → this is plain resampling (VariPhrase OFF), not the grain
engine.** For our engine: Vari-OFF playback should use this kind of
6-tap polyphase interpolator; the kernel's phase 0 is not an identity
(−0.106, 0.123, 0.859, 0.123, −0.106, 0.004), so it slightly band-limits.

**Probable VariPhrase phrase player — `0x8950e1e0`** (sibling
`0x8950d890` shares all helpers; likely another encode type's variant):
- two 12.20 fixed-point position accumulators: phrase position
  (+0x38 int / +0x3c frac) advanced by a time rate (+0x11c, halved per
  step), and a second accumulator (+0x110/+0x114) advanced by +0x30, which
  is 2²⁵ (unity) when a flag is clear or 2²⁵·x² from a parameter
  otherwise — consistent with the patent's separate walk-rate vs
  read-rate (not yet proven which is time / pitch / formant);
- crossing the next boundary (+0x2c) calls `0x8950c880` (forward) /
  `0x8950ca70` (backward); bounds +0x104 / +0x108 call `0x8950e560` /
  `0x8950e500`;
- a 0.6 limit (0x3f199999) appears in the grain-length arithmetic;
- 32-sample block loop.
**Event-table walker — `0x8950c880`:** index +0xc, count +0xe over
16-bit entries with flag bits (`& 0xb`); handles loop modes 1/2/3 and
direction reversal. Very likely the encoder's event/syllable marks (our
"onset stamps"). Parts of `e1e0` are still mode-garbled: there FPSCR is
loaded from r0 (not a constant) — needs FpscrFix to follow
`mov.l @(disp,pc),r0` / caller-set modes.

**Next:** fix the r0-loaded FPSCR case; read `e1e0` / `d890` /
`e500` / `e560` fully to recover grain length, window shape, the 0.6
limit's role, and how the two rates map to time / pitch / formant.

## Ghidra pass 4 (2026-10-10) — VariPhrase grain player decoded

Players `0x8950e1e0`, `0x8950d890`, `0x8950db20` take the FPU mode from the
caller in r0 (`AssumeR0.java`). With that, `e1e0` decompiles cleanly.

### Encoded phrase format (`0x8950e500` next / `0x8950e560` prev)
A stream of 4-byte **grain records** `{u16 length, u16 info}`:
- `length` = the grain's pitch period in **1/16-sample units**; 0xFFFF =
  "continue" (lengths accumulate across records, for long unvoiced spans);
- `info & 0x1F8` → a per-grain field (`(info & 0x1F8) << 6` stored at +4);
- the loader also reads the NEXT record's length, so the player can
  interpolate the period across the grain.
Positions throughout are 1/16-sample units with a 20-bit fraction (12.20).

### Grain player `0x8950e1e0` (per output sample, 32/block)
1. Phrase position `P` += time rate (struct +0x11c, halved per step).
   Crossing the segment end → load next record; crossing the next event
   boundary (+0x2c) → event walker `0x8950c880` / `0x8950ca70`.
2. Current period `L = cur + (P − segStart)·(1/cur)·(next − cur)` —
   linear interpolation from this grain's period to the next one's.
3. Read pointer `Q` += read rate (+0x30 = 2²⁵·f(+0x70)·f(+0x9c); 2²⁵ =
   unity when a flag at +0xec is clear). P is clamped to [L, end − L];
   if `Q > P` → `Q −= L`; if `Q + L ≤ P` → `Q += L` (forward-play case;
   mirrored for reverse). **Q stays within one period behind P, jumping
   by exactly one (interpolated) period.**
4. Output = `s(Q) + (s(Q+L) − s(Q))·w`, `w = 5·(clamp((P−Q)/L, 0.4, 0.6) −
   0.4)`: two taps one period apart, **linear crossfade across 20 % of a
   period centred on half-period lag**. Each tap is 2-point linear
   interpolation at 1/16-sample resolution (`(pos & 0xF)·0.0625`).
5. Gain: one-pole smoothed amplitude, output × g² × 65536; mono written to
   both channels, stereo samples packed as 16-bit halves.

**Interpretation.** A period-synchronous delay-line pitch shifter: the
read pointer resamples at the read rate (pitch AND formants scale
together) while period-exact jumps keep it locked to the phrase walk
(time). **No formant control** — matches the manual for ENSEMBLE/BACKING
(formant follows pitch) and Session 16's v29 change. The crossfade is
linear over 0.4–0.6 period, NOT the patent's two half-period-offset
triangular windows.

### Other players
- `0x8950d890`: plain resampling, single position at `rate·f70·f9c` with
  24-bit fraction and linear interpolation; walks the event table but no
  period jumps (pitch and time coupled) — likely an encoded-sample mode
  with VariPhrase time control off.
- `0x8950db20`: additive bank — 6 cosine-table oscillators per call
  (`0x8950dc50`, 256-entry table at 0x8950ddd0, 16-bit phases), ×5, then
  3 biquads (`0x8950c720`). Unidentified oscillator type.
- `0x8950eab0`/`eb60`: Vari-OFF PCM playback with the 64×6 kernel (pass 3).

### Implications for our engine
- ENSEMBLE / BACKING pitch: implement this player (resample at pitch
  ratio + one-period jumps + 0.4–0.6 linear crossfade, linear interp at
  1/16 sample) instead of the phase vocoder; time = phrase-walk rate.
- Grain periods are per-record and interpolated to the next record.
- The SOLO (formant-capable) player is still not found — it must
  re-trigger grains at a new period (PSOLA-style); search next.

## Owner's manual cross-check (V-SYNTH_XT_OM.pdf, Session 16)

Facts that bear on the firmware and our engine (paraphrased):
- **Encode types** (p. 119): LITE (default, simplest), SOLO (monophonic
  voice/wind; enables formant + robot voice; may make pitch "incorrect by
  one octave" and dislikes reverb → manual suggests re-encoding as
  BACKING/ENSEMBLE), BACKING (decay-type, clear attacks: drums,
  percussion, guitar chords), ENSEMBLE (sustain, smooth tone changes:
  choir, strings). → SOLO relies on pitch detection; BACKING/ENSEMBLE do
  not.
- **Formant (PCM)** (p. 92): ignored when Vari Sw is OFF or encode type is
  BACKING/ENSEMBLE — so **LITE and SOLO both have formant control** (the
  Session 8 table saying LITE had none was wrong; v29 matches the manual).
  **Formant knob range is −63…+63** (not semitones); "max" test cases =
  ±63. Firmware holds the knob→ratio mapping (open question since S15).
  **Energy** (SOLO): OFF/1–127, emphasises the fundamental.
- **Encode Depth** 0–127 (p. 120): density of auto-detected attack
  **events**; events drive Playback Mode STEP/EVENT.
- **PCM OSC** (p. 87): Vari Sw (ON = VariPhrase, OFF = "linear" playback),
  Playback Mode RETRIGGER/LEGATO/STEP/EVENT, Time Trip, Beat Keep, Loop,
  Robot Voice (holds pitch steady), Tempo Sync.
- **Analog OSC waveforms** (p. 86): SAW, SQUARE, TRI, SINE, RAMP, JUNO,
  HQ-SAW, HQ-SQUARE, NOISE, LA-SAW, LA-SQUARE, SUPER-SAW, FEEDBACK-OSC,
  X-MOD-OSC; plus PCM and EXT IN oscillator types.

## Ghidra pass 5 (2026-10-10) — second player is a grain re-trigger engine

`0x8950db20` (r0-mode player) =
1. **grain scheduler `0x8950e760`**: walks a list of 16-bit grain lengths
   (pointer at r9+0x20); when the playback position reaches the next
   grain start it starts a new grain (start r9+0x14/0x2c, record ptr,
   read offset 0, end = start + r9+0x30); read rate +0x30 =
   2²⁵·f(+0x70)·f(+0x9c) (same form as e1e0); forward/backward grain
   steppers `0x8950f400` / `0x8950f4c0`; 16 × 2 samples per block;
   samples read by `0x8950e980` with 2-point linear interp at 1/16 sample
   (mono int16 or packed stereo). → **grain re-triggering = the
   independent-formant mechanism → leading candidate for the SOLO player.**
2. **optional additive layer** (when a flag is set): `0x8950dbe0` ×5 →
   `0x8950dc50`, 6 cosine-table oscillators each (256-entry table at
   0x8950ddd0, 16-bit phases, increments scaled by the voice rate);
   per-frame data (12 values/frame) loaded by `0x8950dcb0` every 64
   position units. Candidate: SOLO "Energy" (fundamental emphasis) or
   robot voice.
3. **three cascaded biquad sections** `0x8950c570`→`c650`→`c720`.
Mapping of the dispatcher (0x8950bf70, cases 1/3/5/6/8) and the asm
oscillators to the manual's waveform list is still open.

**Next:** read e760/f400/f4c0/f5a0/f6a0 fully (grain length source,
window, read offset — compare with the patent's wl ≤ ppw clamp); find
where f(+0x70)/f(+0x9c) are computed (pitch vs formant vs sample-rate
ratio) — that is where the −63…+63 formant mapping lives.

## Ghidra pass 6 (2026-10-10) — player routing and the SOLO engine

**Oscillator setup `0x894f2010`** switches on oscillator type (1–4). For
type 2 (PCM), `0x894f3660` installs the per-voice player from a mode
field (voice +0x80, values 1/3/5/6/8 — encode type, plus a Vari-OFF
fallback). The pitch dispatcher `0x8950bf70` uses the same field:

| mode | setup | update | player entry → body | what it is |
|---|---|---|---|---|
| 1, 6 | `8950fb70` / `8950fba0` | `8950fed0` | `8950cd90` → `cde0`, `ce80`, `d0f0`, `d380` | **two-channel grain player with grain READ VELOCITY** (formant) — SOLO (and probably LITE) |
| 3 | `89510980` | `89510bf0` | `8950e6e0` → `e760` + `eab0`×4 (r12 = 8,4,2,1) | 5 band streams of decimated PCM, 64×6-kernel reads, per-band grain re-trigger — BACKING candidate |
| 5 | `89510130` | `89510bf0` | `8950db10` → `db20` | 5 bands × 6 sinusoids (amp/freq frames, 12 values/frame) + octave biquad cascade = US6564187 — **ENSEMBLE candidate** |
| 8 | `895107d0` | `89510870` | `8950e1d0` → `e1e0` | period-jump player (pass 4), no formant — type not yet pinned |
(Flag 0x20 at voice +0xb4 swaps in linear players `89509870` / `89509f52`
— likely Vari OFF.) Mapping of mode numbers to the manual's
LITE/SOLO/BACKING/ENSEMBLE is inferred from behaviour, not yet from the
code that writes +0x80.

**SOLO engine details found so far**
- `0x8950d380` (per sample): two grain channels; each has a window
  envelope (+0x44 / +0x48) ramping by +0x40 per sample, clamped at 1;
  each channel's age counter steps by 0x1000 (1/4096 units) and its read
  position is `start + age × f(+0x2c) / 4096` (+ 1/16-sample start
  fraction) → **grains are read at velocity f(+0x2c) = the formant
  ratio** (patent fsv). Reads use 2-point linear interpolation.
- `0x8950cde0` (per block): rate = f(+0x70)·f(+0x9c) (the f9c factor is
  skipped when flag 0x10 is set); with parameter g = f(+0x78)
  (formant/Energy?) it computes a window parameter: g == 0 → 1.2 (and a
  second value ×0.125); else piecewise over rate ∈ [0.5,1) and [1,2)
  around (1.2 − g); a cubic shaping `x·(3.0962 − 4.4208·x²)·2`; both
  smoothed (×0.125 per block) and capped `v ≤ 1.9 − w`. Likely the
  window-length clamp behind the measured upward formant saturation.
- `0x8950d0f0`: the per-sample loop (two channels, crossfaded taps,
  grain retrigger via `0x8950d420`) — partially read.

**Consistency checks:** ENSEMBLE ⇒ subband/sinusoidal (mode 5) agrees
with v30's move of ENSEMBLE pitch to our subband engine; SOLO ⇒ two-
channel read-velocity grains agrees with our patent-based granular SOLO
path (which we can now make firmware-exact).

**Next:** finish d0f0/d420 (trigger spacing ppw, window shape, read
offset); find who computes f(+0x2c) (formant ratio from the −63…+63
knob) and f(+0x70)/f(+0x9c); pin mode↔type by the encoder.

## Ghidra pass 7 (2026-10-10) — parameter converters and the formant ratio

**PCM voice note-on `0x8950b5b0`** fills the player's rate fields (player
struct = voice + 0xd4) from converters over a voice-parameter block V:

| converter | → player field | formula (V offsets) |
|---|---|---|
| `894f3a00` pitch | +0x70 | x = V8 + env·V1c + lfo·V34 + keyfollow + bend; clamp **±6000 cents**; ratio = 2^(x/1200) (× V+0xc) |
| `894f3ac0` time | +0x7c (signed → reverse) | x = V10 + env·V24 + lfo·V3c, × modulation; clamp **±5.0** |
| `894f3b30` formant | +0x74 | x = **V14 + env·V2c + lfo·V44 (octaves)**; ratio = **clamp(ctl·2^x, 0.5, 2.0)** — ±1 octave hard limit (`ctl` = modulation value from V+0x8c8, normally 1) |
| `894f3dc0` / `894f3e20` | +0x80 / +0x84 | clamp(±1)·V5c (level/pan-type modulators) |
(`894f3c90` / `894f3d40` / `894f3ba0` are update-time variants.)
SOLO grain read velocity (player +0x150/+0x154) is copied from +0x14c at
mode-6 setup (`0x8950fba0`), which also seeds the window parameter 1.2
(+0x174) used by `0x8950cde0`.

**Knob curves** (patch → voice conversion, 0x894dc8b0–0x894de110): depth
knobs (−63…+63) map as **sign(k)·(k/63)²·range** (signed square; range
constant per destination at +0x34/+0x38 of the source block). The main
Formant knob's store into V+0x14 is not yet pinned (converters write via
float-array indices).

**Interpretation of the Session 15 formant asymmetry:** knob ±63 → at most
±1 octave, then clamp [0.5, 2]. Formant-down measured ≈ 0.5 = the clamp;
formant-up saturated ≈ ×1.68 — plausibly the SOLO engine's window
mechanics (`0x8950cde0`: window param 1.2, cap 1.9 − w, rate-dependent
pieces over [0.5, 1) and [1, 2)), i.e. the patent's wl ≤ ppw clamp, not
the knob mapping. To confirm by finishing `cde0` / `d0f0`.

## Ghidra pass 8 (2026-10-11) — SOLO engine fully decoded and implemented

Chain (mode 6 / 1): `0x8950cd90` → `cde0` (per block) → per sample
`ce80` (walk/records/trigger) + `d0f0` (output) with `d380`/`d420`.
Units: 1/16 sample for positions and periods; 12.20 fixed point for the
phrase position; 1/4096 sample for the trigger timer and grain ages.

**Records** (`d4c0` next / `d530` prev): `{u16 length, u16 info}`,
0xFFFF = continue. `info` bits used by the player: **0x4 = patent path**
(voiced), 0x1 gates the pitch factor, 0x2 gates the formant factor,
`(info & 0x1F8) << 6` = a per-record repeat/hold count (+0x7E00 marks the
last record).

**Walk / record selection (`ce80`, forward):** P += time/2 (12.20). An
anchor advances at max(A,1)·16 per sample; when it passes the current
record's end: if P has not passed it, the anchor wraps back (record
REPEATS); else load the next record(s) until P is inside (SKIP).

**Trigger (`ce80` tail + `d5a0`):** timer += 1 sample; when timer ≥ ppw:
timer −= ppw; (skip if playback ended); compute grain params; assign to
the OTHER channel (bit toggled; confirmed at instruction level): age =
timer remainder, window phase e = remainder·slope, start = recordStart +
os, velocity, second-tap weight = frac, second-tap offset = C.

**Grain params (`d5a0`):** C, N = this/next record period (min 160/16 =
10 samples); frac = (P − recStart)/C (1 if past the end); Ci = C +
frac·(N − C). Patent path (info 0x4) with pitch ratio A:
`ppw = Ci/A`, **velocity = F**, `wl = Ci/F`; if wl > ppw: `os = Ci −
ppw·F`, `wl = ppw`. Robot voice (A = 0): `ppw = 1603.64/R` (A440·R
absolute, 1603.64 = 44100·16/440). Non-patent path: S = A·F (gated by
bits 0x1/0x2), ppw = Ci/S, velocity S, wl = ppw. Window slope = 0.5/wl
per sample.

**Per sample (`d380`/`d0f0`):** each channel: e = min(e,1);
w = 2·min(e, 1−e) (triangle spanning 2·wl); e += slope; pos = start +
age·v; s = lerp(pos); s += (lerp(pos + C) − s)·frac (second tap, if
weight > 0); age += 1. out = wA·sA + wB·sB → Chamberlin SVF, HIGHPASS
output: f = x·(3.0962 − 4.4208x²)·2 with x = f0_out/fs (Energy on) or
f0_out/(8·fs) (Energy off); damping q = 1.2 (off) or (1.2 − g) + p(r)
with p = g(2r−1) on [0.5,1), g(2−r) on [1,2), else 0; coefficients
smoothed ×0.125 per block, f ≤ 1.9 − q → then gain smoothed 1/64 per
sample, ×65536.

**Window placement implies epoch-aligned records:** the triangle peaks at
os + wl = Ci — exactly the next record boundary. Implementing the encoder
with cuts snapped to the waveform's positive peak (±period/4): firmware
player +2.1 on the vocal set (formant downmax 55.3 → 62.7, pitch +7
61.2 → 63.9, time 2× 67.7 → 71.8), while the patent-based engine LOSES
7.3 with the same cuts — the player and the record layout were designed
together.

**Implemented** as `VariphraseEngine::soloFirmwareResynthOffline`
(default SOLO path since v31; v32 adds epoch alignment). Suite v6:
v30 68.4 → v31 69.1 → **v32 69.8**. Energy implemented (decoded logic,
knob scaling unknown → off by default); sweeping g 0.25–1.0 moves vocal
scores ±0.6, so the residual low-harmonic deficit at pitch +7 is not
Energy. Remaining unknowns: the real encoder (epoch detection, voiced
flags, info bits), and the mode→encode-type mapping.

## V-Trainer encoder (VP-9000 V-Producer CD, Session 16)

Source: `Vsynth_software/V-Producer*.zip` (gitignored; both editions
unpack to identical files). Extract with `unshield` (Homebrew). The PC
encoder is **`vpenc.dll`** (x86, 139 KB, C++ exports) used by
`VTrainer.exe`; Ghidra project `Vsynth_software/ghidra_vt` (gitignored).
The code is integer/fixed-point (Newton–Raphson reciprocal divide etc.) —
evidently a port of the VP-9000's own encoder.

**Encode types (`Vpenc_GetAlgoCaps`):** 1 = "Solo" (monophonic pitch
sounds), 3 = "Backing" (polyphonic sounds 1), 5 = "Ensemble" (polyphonic
sounds 2); VTrainer also knows hidden types 2 "XFG" and 4 "POL". **These
are the V-Synth firmware's player-mode numbers** → mode 1 = SOLO (cd90
read-velocity grain player), 3 = BACKING (e6e0 5-band decimated PCM
grains), 5 = ENSEMBLE (db20 5-band × 6-sinusoid model). The V-Synth-only
modes 6 (shares the SOLO player) and 8 (period-jump) include LITE.

**Templates** (`Vpenc_SetTemplate(t, tbr, tsr, pmin, pmax, oru)`, read by
VTrainer from the profile section "Template", template 0 selected):
t0 = (3, 0, 22, 2000, 4), t1 = (8, 0, 22, 2000, 10), t2 = (1, 0, 22, 2000,
5), t3 = (8, 5, 22, 2000, 5) [VTrainer reads key "oru1" for every
template — a bug]. Encoder uses: period range 44100/pmax … min(44100/pmin,
2047) samples; thresholds tsr/10, tbr/10, oru/100 (Q15). Default
sensitivity (Vpenc_SetSens) = 127.

**SOLO encoder (`Vpenc_Encode` type 1 → `0x100079d0` setup, `0x10007c70`
main loop):** 44.1 kHz; 12288-sample buffers; two signal copies (×4
decimated for tracking, full rate for refinement); positions in
1/16-sample units, periods in 16.16.
- distance `D(lag) = Σ (x[i−lag] − x[i])²` over a window of ONE PERIOD
  (`0x1000a050`), energy `E = Σ x²` (`0x1000a0b0`); hill-climb from the
  previous period + 3-point parabolic refinement (`0x1000aac0`,
  `0x1000a3f0`); full re-search (`0x10009480`) when D > E/4;
- **voicing: D/E < (1 − tbr/10)/2 = 0.35** (template 0);
- **first mark of a voiced run** (`0x10008480`): centre of the max-energy
  window of length P/4 within one period (+P/8, wrapped) — energy epoch;
  later marks step by the tracked (sub-sample) period;
- onset search (`0x10008240`/`0x10008540`/`0x100085d0`/`0x10008730`):
  hop 421 samples, energy gate, candidate periods with octave checks
  (D(2L), D(3L)), full-rate refine stopping at normalised diff 0.5;
- **post-run octave correction** (`0x10008da0`): 32 sample points; P/k
  (k = 2…6) votes when D(P/k) ≤ D(P) or (D(P/k) − D(P))/E < oru/2; the
  largest k with votes·32 ≥ checks·9 splits every interval; also a
  2P/merge check and backward extension of the run;
- unvoiced regions: short records (~44 samples = 1 ms constant).

**Ported** as `encodeGrainCutsVTrainer` (experiment, `VSE_ENC=vt`):
voicing test, tracker, first-mark energy epoch, run-level octave
correction, 22–2000 Hz, 1 ms unvoiced records. Vocal set with the
firmware SOLO player: 64.2 vs **65.6** for the v32 positive-peak-snapped
cuts (without octave correction 56.0 — tracks locked an octave low;
re-anchoring every mark to the energy epoch 62.5). The v32 encoder stays
default; the VP-9000 encoder may not equal the V-Synth's.

## LITE investigation (2026-10-14)

**V-Synth encode codes** (wave-info name tables at 0x89854480 / 0x89840f40,
indexed by code): 0 = **LITE**, 1 = SOLO, 2 = XFG, 3 = BACKING, 4 = (POL,
shown "ENSEMBLE"), 5 = ENSEMBLE, 6 = FOR4, 7 = FORO, 8 = UWA. Codes 1–5 =
V-Trainer's algorithm numbers. So PCM player modes 6 and 8 are **FOR4** and
**UWA** (internal types, presumably preset waves), not LITE.

**Oscillator kinds** (`0x894f2010`, 1–4) — corrected: **1 = PCM, Vari
OFF** → `0x894f3590` (modes 0/1/2; mode 0 → `0x89509870`/`89509f52`: plain
linear-interpolation resampler, int16 reads, position += pitch × time,
gain smoothing 1/32 per sample, loop/end variants via a jump table at
0x89509950/58); **2 = PCM, Vari ON** → `0x894f3660` (mode − 1 > 7 exits,
so **mode 0 = LITE has no case** here); 3 → `0x89505170` (analog); 4 →
`0x895097d0` (EXT IN). The per-block renderer `0x894f2800` calls the
installed player (+0x964, or +0x968/+0x96c for kind-1 mode 0xd). All 14
sites that build offset 0x964 are in these switches — **no separate LITE
install exists**, so LITE (Vari ON) must run through an existing mode,
most plausibly mode 1 (the SOLO player, the only other formant-capable
one), differing only in its records' info bits (0x4 patent path, 0x1 pitch
gate, 0x2 formant gate). **Tested and rejected as implemented**
(`VSE_LITEINFO=b`, 2026-10-14): forcing each info pattern on the sine
cases, no pattern matches more than one or two cases, and all keep time 2×
perfectly pure where LITE adds h2 −18 / h3 −28 … — LITE's time-stretch
mechanism generates harmonics even at unity pitch and formant, which none
of our grain paths do. Needs an exported LITE sample (records, info bits,
vpsc/vpfl) from the XT.

**Correction (2026-10-18):** the sine references were never LITE. They
were played from a SOLO wave (user's XT wave 003). The firmware SOLO
player with that wave's own records reproduces all six if four were
recorded with formant at minimum (notes.txt, "SINE REFERENCES
EXPLAINED"). The harmonic series is the SOLO player's half-speed grain
content (F = 0.5) inside full-period grains, not a LITE mechanism. XT LITE
waves (005/006) store no `vpdt`, `vpfl` or `vpno` at all.

**V-Synth sample files use the VP-9000 RIFF layout**: chunk table at
0x897a7de4 = vpfl, vmdl, vpsc, vpno, vplp, vpdt, LIST, rdmd, data, link;
used by five file routines 0x894939d0 / 0x89494620 / 0x89495580 /
0x89497d40 / 0x89499000. → a sample exported from the XT can be parsed
with `research/vpdt_parse.py`-style code (and VP-9000 files import).

**LITE fingerprint (sine references, dry inputs are pure sines):** LITE
adds a pure harmonic series of the OUTPUT fundamental — h2 −18, h3 −28,
h4 −34, h5 −39, h6 −43, h7 −46, h8 −49 dB — identical for time 2×, time
×½ and formant −12; slightly stronger for pitch +7 (h2 −16, h3 −25);
**absent for pitch −12** (pure 220 Hz). None of the decoded players
reproduces this set: SOLO patent path matches formant −12 only (time cases
pure, pitch +7 far too strong, pitch −12 keeps 440 dominant); non-patent
path and period-jump stay pure where the reference has harmonics.
Experiment hooks: `VSE_LITE=solo-p|solo-np|pj`.

## P-code emulation (2026-10-27) — firmware vs our C++, executed

Ghidra's `EmulatorHelper` runs the SH-4 code directly (script
`research/ghidra_scripts/EmuD5A0.java`). The script lays out the player
struct as `0x8950cd90` sets it up: r8 = player, r9 = r8 + 0xe8 (record
state), r10 = r9 + 0x3c (grain). It sets FPSCR = 0x00040001, calls
`0x8950d5a0` with PR at a breakpoint sentinel, and reads back the grain
fields.
- **Grid:** C ∈ {5427, 3200}/16, R ∈ {1, 1.4983, 0.5}, F ∈ {1, 0.5, 2,
  1.25}, info ∈ {7, 3, 1, 2, 0, 4}, K = grain[0] ∈ {256, 0}.
- **Result:** 288 cases, no emulator failure. Diffed against the Pass 8
  formulas in float32: **0 mismatches**. ppw, os and the anchor step match
  exactly; the float fields match to print precision.

The run also pinned down several fields:
- **Pitch factor:** A = grain[0]·R/256. grain[0] = 0 is the robot voice
  (voiced) or A' = 1 (non-patent).
- **Voiced gate:** bit 0x4 alone selects the patent path; info = 4 and
  info = 7 behave the same.
- **Non-patent path:** S = A'·F' with the 0x1/0x2 gates (confirmed on every
  combination).
- **Clamps:** ppw ≤ 524287/16 samples; anchor step (player +0x30) =
  max(16·A_eff, 16), where A_eff = Ci/ppw on the robot path.
- **The highpass cutoff (grain +0x24 = min(1/ppw, 0.5)) is written only
  on voiced triggers.** A sentinel preset survives every non-voiced
  record. Note-on (`0x8950fba0`) zeroes it and sets q = 1.2.
- **`cde0` runs at block start**, before the block's 32 samples and their
  triggers (`cd90`: `c280` → `cde0` → `ccc0` → 32 × (`ce80`, `d380`)). The
  per-sample mono path in `cd90` is the Chamberlin highpass as transcribed,
  then gain smoothing, ×65536, written to L and R.

**Engine fixes (v36).** The cutoff now holds through non-voiced records
and the coefficients update before the block's triggers. The vocal
renders change at −64 to −81 dB relative; scores are unchanged (suite
73.9, vocal 77.41).

**Template.** Any leaf routine can be run this way: point the PR at a
sentinel, preset memory, step to the sentinel, read the fields back.

### Full SOLO player (2026-10-27, v37)

`EmuSolo.java` renders whole notes through `0x8950cd90`, one 32-sample
block per call, at about 1 ms per sample. Inputs are the XT's own `vpdt`
records (Emu_test.prj waves 003/004, terminator included) and the raw
samples. Note-on state is set by hand from the `0x8950fba0` decompile,
because that routine also touches global RAM through `fa60`. The output is
read before the gain stage (gain = 1/65536).

Thirteen cases were rendered (vocal ×7, sine ×6) and diffed against the C++
engine on the same records. Sample-by-sample traces at each `d380` entry
(engine: `VSE_TRACE`) located each difference. The firmware facts below
are all now in the engine:

| firmware behaviour | where | effect before the fix |
|---|---|---|
| P advances before scheduling/trigger | `ce80` | 1-sample lag |
| note-on: timer = ages = 0; A and B both read from the record start, A's window at 0 and B's at 0.5; first trigger goes to B | `fba0` | wrong first grain; double trigger at t = 1 |
| no clamp on `timer −= ppw` | `ce80` | — |
| window slope is ONE shared field (+0x40), set by the latest trigger | `d380` | wrong decay of the older grain |
| record scheduler = the anchor/budget scheduler | `ce80`, `d6f0` | time 2x +3.7 dB vs −74 dB |
| anchor step stored as trunc(16·rate)/16, min 1 | `d5a0` | pitch +7 −3 dB |
| only the period is floored to 160/16; extent, wrap and tap use the raw length | `d4c0`, `d5a0` | XT sine lead-in record (6.6 samples) |
| frac = 1 and Ci = N once P is past the record end | `d5a0` | — |
| ppw on a 1/4096 grid (truncated), os on 1/16 (truncated), ppw ≤ 524287/16 | `d5a0` | trigger drifts a sample in ~1700 samples |
| record logic sees P's whole-1/16 part (+0x38) | `ce80` | — |
| end of wave: P holds, no more triggers; primary read clamps to the last sample, second tap reads silence | `ce80`, `d380` | tail |
| after the last record the stream holds a (256, 1) terminator → last record's N = 16 samples | `d4c0` | last record |

**Result:** engine vs firmware, whole files: vocal formant −12/+12/+4
−68/−64/−68 dB, pitch −12/+7 −50/−56, time 2x/½ −77/−56; sine
formant −71/−64, pitch −110/−78, time −115/−70 dB. The residual is
float32 round-to-zero (FPSCR RM = 1) vs double: ppw differs by one 1/4096
step on some triggers. The peaks (≤ 4e-3) are in the last ~100 samples,
where the second tap reads across the end of the wave. The record
scheduler's decoded repeat cost (`d6f0`) was confirmed instruction by
instruction.

**Firmware player + XT records vs the hardware references** (metric v6):
vocal 78.0 / 71.9 / 80.9 / 87.6 / 79.5 / 74.6 / 82.4, mean **79.3**. This
is the ceiling for the player with the XT's records. Our own encoder's
cuts reach 78.29 (v37), so the encoder accounts for about 1 point. The
remaining ~20 points are outside the SOLO player: references, the output
chain and the metric. Sine (wave 003, settings as recorded): 78.2 / 78.7 /
49.6 / 71.8 / 56.4 / 62.2. The pitch −12 and time sine references again
don't fit SOLO at those settings (see the sine-reference note in
RESEARCH_LOG).

Next targets: the encoder's per-record budget field (done: next
subsection) and the LITE player.

### How the encoder sets the repeat budget (2026-10-27, v38)

Found in `vpenc.dll` (V-Trainer), whose code the XT shares:
- **Writer `0x10004fa0`:** each mark node holds a u16 value v (+0xc) and
  low bits (+0xe, 7 = voiced).
- **Info word:** `info = ((v >> 8) & 0xf8) | low`, so the budget field is
  `v >> 11`.
- **Pairing:** `0x10004fa0` and the converter `0x100045e0` give each
  record [mark_k, mark_k+1) the value of its **closing** mark, mark_k+1.
- **Where v comes from:** the SOLO main loop (`0x10007c70`) inserts each
  voiced mark through `0x1000aa70`. That routine stores
  `v = 0x10009fb0(0x8000 − 2q)` with q from the period tracker
  (`0x1000aac0` → `0x1000ad60`):
  - `q = min(32768·D/E, 32768)` (Q15, divide `0x1000af00`);
  - `D = Σ (x[i − lag] − x[i])²` over one period starting at the mark,
    i.e. the following period compared with the preceding one, with the
    lag hill-climbed from the tracked period (`0x1000a050`);
  - `E = Σ x²` over that window (`0x1000a0b0`).
- **Mapping `0x10009fb0`**, with s = 0x8000 − 2q: s ≤ 0 → 0; s ≤ 0x1999
  → 0x5999; else `0x5999 + (0xa666 − 0x5999)·(s − 0x1999)/0x6667`. The
  constants (set in `0x100079d0`) make the field **11…20**: 20 means D/E
  ≲ 0.033, and 11 means s ≤ 0.2.
- **Decimation:** the tracker works on a ×4-decimated copy (21-tap Q15
  FIR, `0x10008c90`; table at vpenc 0x1001a2b8). The voicing threshold is
  q < (0x8000 − tbr·0x8000/10)/2.

**Checked against the XT's own records** (Emu_test.prj wave 004, 461
records):

| D/E measured on | exactly right | within ±1 |
|---|---|---|
| decimated signal | 80 % | 99 % |
| full-rate signal | **94 %** | 99 % |

The sine (wave 003) matches 100 %. Pairing records with their opening
mark instead drops the match to 69 %, which confirms the closing-mark
pairing. So the XT computes the field with this rule; its quality value
appears to come from the full-rate refinement.

**Implemented** as `assignRecordBudgets` (default since v38;
`VSE_BUDGET=off` = unlimited). On our own encoder's cuts it matches the
XT's field on 96 % of the aligned marks exactly, and all within ±1.

**Effect.** A repeat costs 6400 at pitch +7, 4096 at time ×½ and 2048 per
repeat at ×¼ (`d6f0`). The budget is b·512 = 5632…10240, so it binds only
from about 3–4 repeats per record, i.e. slowdowns of ≥ 4×. There a record
whose budget is spent is left early: vocal at time ×4 with our records
differs by −3.7 dB from the unlimited version. Emulator check in this
regime (vocal ×4 slower, the XT's records): the engine matches the
firmware at **−54.3 dB** with budgets, against −28.3 dB with budgets
ignored. The suite has no such
SOLO case, so the score is unchanged (74.25; vocal 78.28; time ×½ 80.89 →
80.93).

## The XT's own SOLO encoder, emulated (2026-10-27)

**Location.** The XT encodes on-board with `vpenc.dll`'s algorithm compiled
for the SH-4. Its setup routine `0x8951dd70` repeats `vpenc 0x100079d0`
line for line:
- the same divide helper (`0x89522940` = `0x1000b060`);
- the budget constants lo = 0x5999, hi = 0xa666;
- the thresholds `(0x8000 − t) >> 1`, and 0x2000 for re-search.

**The one difference: float decimation.** The decimation filter is a float
table. At 0x897ee1e4 it holds a float version of `vpenc`'s 21-tap filter;
at 0x897ee198 a 19-tap one is used for pmax > 3000 Hz.
None of `vpenc`'s integer tables appear anywhere in the firmware.

**API (driver `0x89515e20`):**

| routine | does |
|---|---|
| `0x8951dc40(n, samples, nEvents, events)` | new document; defaults pmin 22, pmax 2000, thresholds 3/10, 200, 10 |
| `0x8951dbb0` | template (not called for these encodes) |
| `0x89522a30` | node-pool size factor `[0x8987caa0]` |
| `0x8951e120(2)` | encode, mono (3 = stereo) |

- **Events:** a u32 list of attack positions (1/16 sample); the last must
  be n·16, so a sustained note uses {n·16}.
- **Node pool:** 16-byte nodes allocated by `0x895222a0` from the chunk
  list `[0x8a015260]` (base `[0x8a015240]`, count `[0x8a015244]`).
- **XT node layout** (differs from vpenc's): {u16 low (7 = voiced), u16 v,
  i32 pos (1/16), u32 lower neighbour, u32 higher neighbour}.
- **Records:** [mark_k, mark_k+1) with info `((v_k+1 >> 8) & 0xf8) | low`.

**Harness:** `EmuEnc.java` sets up memory as the C startup and the UI would:
- `.data` is copied from its ROM image [0x89834018, 0x8985858c) to RAM at
  0x8985858c, and `.bss` [0x8987cb00, 0x8a01ceb4) is zeroed (until
  2026-10-28 the harness zeroed both, with the same SOLO records);
- the pool factor is the ROM default;
- `malloc` (`0x89527910`) and `free` (`0x89527950`) are stubbed with a bump
  allocator.

It runs the encoder and writes the record stream. Runtime is about 3
minutes per second of audio.

**Result: the XT's own encodes, reproduced from the raw files.**

| wave | records | result |
|---|---|---|
| 003, sine | 1761 | **bit-exact**: every start, length and info word |
| 004, vocal | 461 | info 100 %; starts 457/461 exact, the other 4 off by 1/16 sample |

The 1/16-sample misses are FPU rounding: the hardware runs round-to-zero
(FPSCR RM = 1). So the template defaults from `0x8951dc40` are the XT's SOLO
encode settings. This is the ground-truth encoder for any input. It is also
the reference against which to validate a native port of the encoder
(`vpenc`'s fixed-point algorithm with the float decimator; the existing
`encodeGrainCutsVTrainer` port is only approximate).

### Native port of the XT's SOLO encoder (2026-10-27, v39)

`plugin/Source/XtSoloEncoder.cpp` ports the encoder literally from
`vpenc.dll`'s x86 decompile, with every XT difference applied. The
differences were found by diffing against `EmuEnc.java`, using breakpoint
traces (inserts, tracker calls) and buffer dumps.

**Result.** With the SH-4's round-to-zero float mode, it reproduces the XT's
own stored records **bit-exactly**: wave 004 vocal 461/461 and wave 003 sine
1761/1761 (start, length, info). With round-to-nearest, it matches the P-code
emulator exactly. It runs in about 20 ms per file. It is the engine's default
SOLO encoder from v39 (`VSE_ENC=epoch` / `vt` select the older ones).

**Where the XT differs from vpenc** (rom.2 addresses; v112 found more: the fills, the lag search, the ends of the wave, in "The SOLO encoder against the firmware's"):

| | vpenc (V-Trainer) | XT |
|---|---|---|
| decimation filter | 21-tap Q15 low-pass, int16 output | 127-tap float FIR designed at run time (`0x8951f380`): the 19-entry low-pass table at 0x897ee1e4 ⊛ a Kaiser-windowed high-pass at (pmin + 307.95)/44100; float buffer `0x8a00f0f4` |
| decimated D/E sums | int16 | `ftrc` of the float values (`0x89521a00` / `a50`) |
| onset normalised distance | int squares | float squares, wrapped at 2³², truncated (`0x89521970`) |
| candidate lag table | step L/8, 64 entries | step L/12 (multiple of 4), up to 128 |
| first mark | strongest P/4 energy window + P/8 | largest \|band-passed\| sample, then the nearer zero crossing (`0x8951e900`) |
| drift correction | none | every 0x1b000 (6912 samples) of forward marks: zero-crossing phase drift between the segment's first and newest mark (`0x89520d30`), spread linearly over the segment (`0x89520f30`); tracking re-bases on the corrected mark |
| thresholds | template 0 | onset 11469, voicing 0x4000 (swapped roles), hop 200, energy gate (10·32)²>>8 and `≤`, octave 0.2 |
| end of wave | zero tail past n | past n + maxLag; event pointer not advanced (`0x89521fc0`) |

**Common to both, but my first reading was wrong:**
- the parabolic vertex sign (`0x89521d00`: (p1 − p3));
- the Newton–Raphson divide, which can come out one above floor
  (`0x1000b060` / `0x89522940`; ported literally);
- the end node's low bits are 0.

The single-step up-climb in the tracker is real in both, not a decompiler
artefact.

**Effect** (metric v6): suite 74.25 → **74.59**. Vocal 78.28 → **79.25**,
which is the XT records' ceiling: every vocal case rises by +0.1 to +1.6.

## Output chain (2026-10-27, v40)

**Method.** The vocal passthrough (`passthrough/vocal_aah_passthrough.wav`)
is the XT's output for the raw vocal with every VariPhrase parameter at zero.
Our identity render reproduces the XT's SOLO player exactly (v37–v39), so
comparing the two isolates everything downstream of the player.

**What the capture shows:**
- **Level and latency:** gain +9.4 dB; L = R (mono, centre); latency 60.3 ms.
- **Clock drift:** the lag drifts by +2.3 samples over 3 s, about 16 ppm,
  from the XT and the interface running on separate clocks.
- **Frequency response:** flat to ±0.3 dB from 150 Hz to 6 kHz, then −0.4 to
  −0.8 dB from 6 to 14 kHz.
- **Noise and effects:** a noise floor about 56 dB below the signal, and no
  reverb tail (the capture decays within 20 ms of the note's end).
- **High band:** the excess above 17 kHz is just that noise; a 44.1 kHz
  engine has nothing up there.

Modelling the droop (first-order low-pass at 14 or 20 kHz) changes the
suite by only +0.02, so the linear chain is effectively transparent for this
material.

**The low end is not the chain.** Every vocal reference has:
- its fundamental about 1.6–1.9 dB low;
- content below the fundamental cut by 4–9 dB.

At pitch −12 that pattern sits at the 65 Hz fundamental, so no fixed filter
explains it. The SOLO player's **Energy** high-pass does (`0x8950cde0`, manual
p. 92 "Energy: OFF, 1–127"). When Energy is on, the cutoff moves from f0/8 to
the output fundamental with damping q = (1.2 − g) + p(rate), giving 1/q =
−1.6 dB at F0 with no pitch shift.

The fundamental-band ratios fit g ≈ 0.02–0.05 at pitch −12 and ≈ 0.08 at
pitch +7. With g on, the time 2× mismatch at F0 drops from −1.9 to −0.2 dB
for any g. So **the references were recorded with Energy on**, at a small
amount.

**Implemented** as `VariphraseParams::soloEnergy` (renderer `--energy`;
default OFF, as on the XT). `render_suite.py` renders at 0.05. Result: suite
74.59 → **74.79**, vocal 79.25 → **79.83**, every vocal case up (formant −12
+2.3).

**Open:**
- ~~The knob → g mapping~~: g = (k/127)², from the VariPhrase converter's
  tail `0x894e4030` (v51). The references' g ≈ 0.05 is Energy ≈ 28.
- The EMU-TEST patch's actual Energy value. The patch file is bit-packed; the
  value is easiest to read on the XT (OSC → Formant (PCM) page).

## BACKING / ENSEMBLE: first findings (2026-10-28, in progress)

**Which type the chord references used.** The manual says formant is
ignored for BACKING and ENSEMBLE, and `chord_Cmaj_formant_max` (formant +12)
matches the raw chord's long-term spectrum to ±1.3 dB in every octave band.
So the chord was encoded as BACKING or ENSEMBLE, not LITE or SOLO. Which of
the two is still open: the players differ (mode 3 against mode 5), the
analysis does not. The drum's type is unknown.

**Multi-band waves in the user's project.** BACKING waves 376, 377, 380 and
381, and ENSEMBLE waves 362 and 415, each with its own audio. *Correction
(see "Factory waves" below): these are factory content written by an
original V-Synth, firmware 1.50, not by the XT. They show the format, but
they are not ground truth for the XT's encoder.*

**Multi-band analysis layout** (vpfl 0102, e.g. wave 376). A directory
header of (offset, size, …) entries and 28-byte sub-headers (at 384, 440,
468, 496, 524 and 552 in 376) leads to **12 record streams**: 6 bands × 2
channels. Each stream uses SOLO's record format, {u16 length (1/16 sample),
u16 info}:
- the first record is a lead-in with info 1;
- the other info words have low bits 1 (pitch gate only, which is why
  formant is ignored) and budget fields of 15–19 (121, 137, 153);
- five bands have records of about 336 samples, the lowest band about 672.

BACKING (376) and ENSEMBLE (362) share the layout.

**The XT's multi-band encoder** (driver `0x89516000`; `SUB_8987c940` = 3):
1. Setup: `0x89522af0` (sample pointer, frames, channels; a 7-node band
   chain), `0x8951ab00(44100, n, ch, events)` and `0x8951a2e0` (a 0x3f00
   work buffer at `[0x8987cab0]`; pool factor `[0x8987ca90]`, default 5).
2. Per 4096-frame chunk: `0x89522e20` → `0x89523050` (the filter bank;
   coefficient tables at 0x897ee24c, 0x897ee39c, 0x897ee44c and 0x897ee4fc),
   `0x8951ac60` (per-band analysis) and `0x8951a420` → `0x8951a6f0` →
   `0x8951bf70` (marks, run once per channel).
3. `0x89515960` collects seven band-buffer pointers into `[0x89ff0dd4]`.
   **The band PCM is written into the sample memory after the original
   audio**: band 0 = the original, band 1 null, bands 2–6 decimated. The wave
   file holds only the n original frames, so the XT presumably rebuilds the
   bands when it loads a sample.

**Emulated** (`EmuBand.java`: `.bss` zeroed; UI, event-list and allocator
stubs; stops at the writer `0x89512f10`; 405 s for 35730 stereo frames). It
produces **12 node lists** in SOLO's XT node layout, and five of the six
bands start at exactly the stored first marks (14121, 14214, 14641, 14404,
16370). But the lists are about 14× denser than the stored records, and the
node values (e.g. 0x1ebf) don't map to the stored info words through
vpenc's `v/625` rule.

**The step that turns the dense lists into the stored records has not been
found.** It isn't in `0x89512f10` (which rescales the marker list) or in
`0x89515960`, and nothing loads 625. It may run at sample save, where the
vpdt chunk is serialised. Or there may be no such step in the XT: wave 376
was encoded by V-Synth 1.50, whose SOLO encoder already differs from the
XT's (below), so its multi-band encoder may differ too.

**Next:**
1. Get the XT's own BACKING and ENSEMBLE encodes of the raw chord and drum
   (RECORDING_GUIDE.md). Then diff `EmuBand.java`'s lists against them; if
   they differ, look for the thinning step from the vpdt serialiser.
2. ~~Emulate the BACKING (`0x8950e6e0`) and ENSEMBLE (`0x8950db10`)
   players on a stored wave's records and band PCM~~ — done (next section).
3. Run the chord and drum through both and score them against the
   references to settle the encode types; then port what matches.

### BACKING and ENSEMBLE players, emulated (2026-10-28)

**What a loaded multi-band wave looks like in memory.** On load the XT
rebuilds the band data with `0x894a9940`. It calls the type's chunk routine
over 0x2000-frame chunks: `0x89529f20` → filter bank `0x89522e20` for
BACKING, the sinusoidal analysis `0x89525640` for ENSEMBLE (init
`0x89525360`, finish `0x895255c0`). It then fetches seven band pointers
through `0x8952a1c0`. The player's wave struct holds:
- +0: the vpdt chunk, read in place (all offsets in it are relative to
  its start);
- +4: band 0. The rebuild **overwrites the original audio in place** with
  the top band;
- +8 … +0x18: bands 1–5, i.e. loader slots 2–6. Slot 1 is null for BACKING,
  and the vpdt has an empty slot there too.
  - BACKING: int16 PCM decimated ×4, ×8, ×16, ×32 and ×64.
  - ENSEMBLE: frames of 12 int16 per channel (six value pairs). The frame
    count halves from band to band (wave 362: 424, 220, 118, 67, 65).
- +0x28: a tuning struct (two s16 read as x/12000; zero here);
- +0x2c: the loop/cache struct.

For wave 376 the load-time BACKING bands are **bit-identical** to the
encoder's (`EmuBand.java`), sample for sample.

**vpdt image** (stereo; mono halves the strides):
- The event table sits at byte 28. There are 88 u16 per event (46 mono),
  and the header's u32[2] high half is the event count. Each event is {u32
  flags, u32 position (1/16 sample)}, then per band one 24-byte pair of
  channel entries {u32 segment-list offset, u32 record-stream offset, u16
  first mark}. An empty 24-byte slot follows band 0.
- Segment lists are entries {u32 record-stream offset, u32 flags, u32
  start}, each starting a grain run at its own record stream (`0x8950f6a0`).
- Records are SOLO's {u16 length, u16 info} (`0x8950f5a0` / `0x8950f5d0`).

**Players.**
- **BACKING** `0x8950e6e0`:
  - band 0 through the grain scheduler `0x8950e760`, at full rate;
  - bands 1–5 through `0x8950eab0`, producing 8, 4, 2, 1 and 1 samples per
    32-sample block (×4, ×8, ×16, ×32, ×32). They read with the 64×6
    interpolation kernel (`0x8950eb60`) and re-trigger per segment.
  - The recombination tree `0x8950c650` / `0x8950c720` upsamples ×2 per
    stage through a 4th-order recursive interpolator. `0x8950c570`
    applies the output gain.
- **ENSEMBLE** `0x8950db10`:
  - band 0 through the same scheduler;
  - each other band through six cosine oscillators (`0x8950dbe0` /
    `0x8950dc50`). Their frames are reloaded every 64 band samples
    (`0x8950dcb0`) and interpolated linearly.
  - The same recombination tree.
- Note-on for both is the setup alone (`0x89510980`, `0x89510130`).
  `0x89510bf0` seeks only when the start offset is non-zero.

**Ghidra emulator bug.** Ghidra's little-endian SH-4 spec stores the float
registers pairwise swapped (fr1, fr0, fr3, fr2, …). Its `fipr` P-code
addresses FVn by offset, so it writes the dot product to fr2 instead of
fr3. The recombination tree's four `fipr`s then integrate, and the low
bands ramp without bound. `EmuBacking.java` executes those four itself.
`fipr`/`ftrv` appear nowhere else in the players (`0x8950c000`–`0x89511000`)
or the encoders (`0x89511000`–`0x89528000`), so the SOLO and encoder
emulations were unaffected.

**Results** (full-length renders; octave levels against the original,
62.5 Hz – 8 kHz):

| wave | setting | length | measured pitch | octave levels |
|---|---|---|---|---|
| 376 BACKING | identity | 1.000× | 1.000 | ±0.5 dB, same rms |
| 376 BACKING | pitch +7 | 1.000× | 1.498 (expect 1.498) | — |
| 376 BACKING | time ×2 / ×½ | 2.000× / 0.500× | 1.000 / 1.002 | within 1 dB / up to −3.6 dB at 8 kHz |
| 362 ENSEMBLE | identity | 1.000× | 1.000 | 0 to −2.8 dB, rms −1.6 dB |
| 362 ENSEMBLE | pitch +7 | 1.000× | 1.498 | — |
| 362 ENSEMBLE | time ×2 / ×½ | 2.000× / 0.500× | 1.000 / 1.000 | — |

At identity, BACKING correlates 0.77 with the original waveform at zero lag.
The ENSEMBLE envelope follows the original block by block. So both players
run correctly in the emulator: pitch and time are independent, and the
level is right.

**Next:**
- ~~Port both players to C++, validated against these renders~~ — done
  (next section).
- Scoring against the chord and drum references still needs the XT's own
  vpdt for those inputs (RECORDING_GUIDE.md), or the step that writes it.

### C++ port of the BACKING and ENSEMBLE players (2026-10-28)

`plugin/Source/XtMultiBandPlayer.cpp` ports both players operation by
operation from the SH-4 code. It uses float arithmetic with no
contraction, `fmac` as a product then a sum (as Ghidra's P-code does it),
and `fipr` as a double sum rounded once. The arithmetic-heavy routines are
transliterated register by register: `0x8950e980`, `0x8950eb60`, the
recombination tree `0x8950c650` / `0x8950c720`, the output stage
`0x8950c570`, and the oscillators `0x8950dc50` / `0x8950dcb0`.

Like the SOLO encoder, it reads its firmware tables from the user's own
rom.2 at run time: the 64×6 kernel, the tree coefficients, the per-band
shift/mask/offset/stride constants, the oscillator constants and the
cosine table. Nothing is copied into the source. The kernel phases sum to
about 0.9, so the band reads carry a gain of about −0.9 dB.

**Validation** against `EmuBacking.java`, with the emulator's
round-to-nearest (`XT_ROUND=nearest`):

| wave | settings | result |
|---|---|---|
| 376 BACKING | identity, pitch +7, time ×2, time ×½ | **bit-exact**, every frame |
| 380 BACKING | pitch −12, time ×4 (quarter speed), both | **bit-exact** (repeat budget exercised) |
| 362 ENSEMBLE | identity, pitch +7, time ×2, time ×½ | **bit-exact** |

By default the port runs in round-to-zero, as the SH-4 does (FPSCR RM = 1).
That moves the output by −118 to −120 dB relative, i.e. LSB-level. The
hardware's own `fmac`/`fipr` precision is not modelled beyond this.

**A firmware quirk, reproduced:** ENSEMBLE's note-on (`0x89510130`) packs
each band's first frame as {base, delta}. The frame loader and the
oscillators use {delta, base}. So the first frame of each band (64
band-1 samples, longer for the lower bands) plays with swapped values. The
emulator does the same.

**Ported:** forward playback from the start event, with no loop points and
default voice flags.

**Not ported:**
- reverse playback (`0x8950e8f0`, `0x8950f4c0`, `0x8950f5d0`) — ported in
  v52;
- loops and the event walker at the bounds (`0x8950c880` / `0x8950ca70`);
- event seeks (`0x89510bf0`);
- the voice flags 0x2 / 0x4 / 0x8 / 0x20;
- the loop struct's cache bounds.

**Not yet in the engine.** The player needs a loaded wave's band data and
its vpdt. The BACKING band data is now ported (next section). Still to
port: ENSEMBLE's analysis, and the multi-band encoder (`0x89516000` and
the step that writes the vpdt).

### The BACKING load-time band rebuild, ported (2026-10-28)

`xtMbBuildBacking()` (in `XtMultiBandPlayer.cpp`) rebuilds what
`0x894a9940` writes for a type-3 wave.

**Filter bank** (`0x89522af0` init, `0x89522e20` / `0x89523050` per
chunk). It is a Laplacian pyramid over the wave, each channel separately:
- stage 1 decimates by 4;
- stages 2–5 decimate by 2.

Each stage:
1. **Decimate.** The decimating FIR `0x89523aa0` computes
   c₀·x₀ + Σ cᵢ·(xᵢ + x₋ᵢ). Each pair sum is an integer add; the taps
   are accumulated in float, one after another. The result is truncated
   and clamped to int16.
2. **Zero-stuff** that low signal.
3. **Interpolate.** Stage 1 uses a full half-kernel (`0x89523c60`); the
   others use a half-band (`0x89523fd0`, 0.5·x₀ + Σ cⱼ·(x₂ⱼ₊₁ + x₋₍₂ⱼ₊₁₎)).
   The result is scaled by the decimation factor and truncated.
4. **Residual.** It is subtracted from the stage input, with int16
   clamping. That residual is the band.

The four tables are half-kernels whose first float is the length. The
port reads them from rom.2 at run time.

**Equivalent to whole-signal filtering of the zero-extended wave.** The
XT streams the wave in 0x2000-frame chunks (the encoder uses 0x1000) and
rounds the length up to 64 frames. It carries the decimated signals'
tails before time 0 as history. So the result equals filtering the whole
wave, extended with zeros, and keeping every stage's signal beyond both
ends.

**Memory layout:**
- band k holds (n rounded up to 64) >> {0, 2, 3, 4, 5, 6} frames;
- 256 frames of padding follow each band;
- bands 1–5 hold the stage input continued past the end (41 frames for
  band 1, 10 for the others);
- 128 frames on, they hold the stage input's ten samples from before time
  0, reversed;
- the bands follow one another in sample memory; band 0 overwrites the
  wave.

**Result.** With round-to-nearest, the rebuilt sample memory equals
`EmuLoad.java`'s, int16 for int16, for waves 376 and 380. That includes
the padding and the band addresses. It takes well under a second.
Together with the player port, a BACKING wave now goes from raw PCM and
its vpdt to the firmware's output bit-exactly.

ENSEMBLE uses a different filter bank: its band 0 differs from BACKING's
for the same wave. Its layout is n rounded up to 512 frames, then frames
of 12 values every 16 band samples. Its code is `0x89525360` / `0x89525640`
→ `0x89525840` (mono) / `0x89525ae0` (stereo); stage filters `0x89525e20`,
`0x89526090`, `0x895263a0`, `0x89526600`; frame analysis `0x89526a20` /
`0x89526b50`; integer block routines `0x89527f00` / `0x89528cb0`. It is
not ported yet.

**ENSEMBLE analysis, findings so far** (prototype against `EmuLoad.java`,
wave 362):
- **Filter bank.** Stage 1 is the same 81-tap ×4 decimator as BACKING (own
  copy at 0x897ee530), clamped to ±32767 in float. The ×2 decimator
  (`0x895263a0`) is a third-band design: centre plus taps ±1, ±2, ±4, ±5,
  … ±41, skipping multiples of 3, with the table stored without its zeros.
  Unlike BACKING, every stage starts from zero history, so decimated
  signals are 0 before time 0. The end is flushed with zeros
  (`0x895255c0`).
- **Band 0** (`0x89526090`) is the residual against a polyphase ×4
  interpolator (10 iterations × 6 coefficients, three phases plus the
  direct sample), clamped to ±32767, subtracted as wrapping int16.
- **×2 residuals** (`0x89526600`): in[2m] −= low[m];
  in[2m+1] −= clamp(Σ cⱼ·(low[m−j] + low[m+1+j])).
- **The ×4 low band goes through a lossy block codec first.**
  `0x89527f00` encodes 16-sample blocks into slot 1. That is the extra,
  non-null band, 16 bytes per 16 samples. `0x89528cb0` decodes them back
  in place: two 8-sample halves, each with a header nibble → a shift and
  one of 8 decoder modes (function pointers at `.data` 0x8987cac0). The
  values recovered from the XT's band 0 are all multiples of 4 and differ
  from the plain decimated signal by up to about 14. Band 0 and the later
  stages use the decoded signal.
- **Frames** (`0x89526a20` → `0x89526bc0`; last band `0x89526b50` →
  `0x89526db0`):
  - a 6-tap antisymmetric (quadrature) filter with sign alternation fills
    a 128-entry circular buffer, 8 pairs per frame;
  - a 57-tap window and 36-point cosine/sine tables correlate it at three
    frequencies;
  - the result is six amplitude/phase pairs, ordered by the index table
    at 0x897eea40.

### Running the firmware itself: an SH-4 interpreter (2026-10-28)

ENSEMBLE's load-time analysis has three parts: a filter bank with its own
tables and zero history, a lossy block codec for the ×4 low band, and a
sinusoid fit. Together that is about 4,000 instructions, and the
multi-band encoder is larger. So instead of porting them,
`plugin/Source/Sh4Interp.cpp` executes them from the user's rom.2.
`plugin/Source/XtFirmware.cpp` provides the harnesses. As with the tables,
no Roland code is in the source; the code runs from the user's image.

**The interpreter** covers:
- the SH-4 integer set, including division steps, MAC and the shifts;
- the FPU: single and double precision, both banks, SZ = 1 pair moves (their
  word order corrected in v85),
  `fipr`/`ftrv` writing the correct register, and the host rounding mode
  taken from FPSCR.RM;
- paged memory, with rom.2 at 0x89300000 and the C startup's `.data` copy
  and `.bss` clear;
- C++ hooks for the OS calls (malloc, calloc, free, UI).

It runs the SOLO encoder in about 0.7 s and a band rebuild in about 0.2 s.
Ghidra's emulator takes minutes for each.

**Validation:**

| test | result |
|---|---|
| SOLO encoder (`0x8951dc40`, `0x8951e120`) on the raw sine and vocal, FPSCR.RM honoured | **bit-exact against the XT's own stored records** (003: 1761, 004: 461) |
| BACKING load of wave 376, round-to-nearest | identical to `EmuLoad.java` (238,724 int16) |
| BACKING load, round-to-zero | identical to the native port `xtMbBuildBacking()` |
| ENSEMBLE load of wave 362 (codec and sinusoid fit included), round-to-nearest | identical to `EmuLoad.java` (74,112 int16) |
| raw wave + vpdt → interpreter bands → C++ player, 362 ENSEMBLE and 376 BACKING, identity / pitch +7 / time ×2 / time ×½ | **bit-exact against `EmuBacking.java`**, every frame |

**API:**
- `xtFwSoloEncode()` runs the SOLO encoder. Its records match
  XtSoloEncoder's since v112. When this was written that rested on waves 003
  and 004 only, and on most generated waves they differed ("The SOLO encoder
  against the firmware's").
- `xtFwLoadBands(type, pcm, …)` builds the band data for BACKING (3) or
  ENSEMBLE (5), ready for `xtMbAttach()` and `XtMbPlayer`.

**The multi-band encoder runs to its vpdt** (`xtFwEncodeMulti`). The driver
`0x89516000` (analysis, then the writer `0x89512f10`) is followed by
`0x89515a70`:
- `0x89511a20` sizes the chunk: header + band records + segment lists.
  It allocates it through the OS (`0x895113a0`, a function pointer at
  0x8987c8f0, served by a hook) and registers it with `0x89517320` as
  chunk kind 5, at 0x8987c9a4 / 9a8.
- `0x89511de0` writes the records (`0x89514d50` for types 3 and 5).
- `0x89511e90` writes the segment lists.
- `0x89511c10` writes the header and the event table from the marker
  list.

The marker list hangs off a head at [0x89ff0d04] (count [0x89ff0cfc]),
doubly linked at +0xbc / +0xc0. The harness gives it a start marker (type
1) and an end marker (type 2), both with +0xd = +0xe = 1. Those are the
flags wave 376's events carry, i.e. a sustained note with no attack
events. The harness stops at `0x89515b40`, the next (note) chunk, by which
point the vpdt is complete.

Encoding 376's audio gives the factory vpdt's exact layout: the same
header, event table and first band entries. The record streams are
longer (33,792 bytes against 5,680), because the XT's analysis keeps the
dense mark lists `EmuBand.java` showed. So the "thinning" question was the
2003 encoder's difference, not a missing XT step.

**Scored against the references** (raw → resampled to 44.1 kHz int16 →
firmware encode → firmware bands → C++ player → 48 kHz):

| case | v40 engine | XT BACKING | XT ENSEMBLE |
|---|---|---|---|
| chord identity (formant_max) | 81.5 | 77.9 | **89.8** |
| chord pitch +7 | 71.6 | 53.4 | **86.6** |
| chord time ×2 | 72.9 | 57.5 | **86.5** |
| drum pitch +7 | **80.8** | 75.3 | 75.8 |
| drum time ×2 | **78.2** | 74.1 | 70.8 |
| drum time ×4 | **75.3** | 71.7 | 65.8 |
| drum time ×½ | **76.0** | 45.9 | 65.3 |

So the chord references are ENSEMBLE encodes, and the firmware chain
reproduces them far better than the engine did. The drum scores lower
through either type. Its encode settings, such as attack events from
Encode Depth (markers), are not modelled yet.

**v41** routes ENSEMBLE content through this chain
(`VariphraseEngine::multiBandFirmwareOffline`, with its own windowed-sinc
resampling). The suite goes 74.79 → **76.56**: the chord cases score
89.84 / 86.60 / 84.98, and no other case changes.

**Attack detection and Encode Depth** (`xtFwEncodeMulti(…, depth)`). The
encode page runs the encode module's init (`0x89516400`), then
`0x895164c0(depth)`. That routine stores the depth at 0x8987c943, then:
1. `0x89512000` detects attacks (init `0x89519db0(44100, frames,
   channels)`, analysis `0x89519e70`, count `0x8951a090`);
2. `0x89512080` turns each attack into a ranked marker, plus an end marker
   (`0x89513390`);
3. `0x89512160` keeps the markers whose rank is below a threshold looked up
   from the depth (byte table 0x897edfe4, values 10…81 over depths
   0…127). The kept markers become the vpdt's events.

On the suite inputs:
- the drum gets one attack event at 577 ms, the hit after its leading
  silence, at every depth;
- the chord gets none.

Detection markers carry flags 4, not the 13/14 of the factory waves'
start and end markers. The encoder then places different first marks, for
example 19905 against 14133 for the chord's band 0.

The player port is **bit-exact on this three-event wave** too: drum pitch
+7 and time ×½ against `EmuBacking.java`. For forward playback with no
play-mode flags, crossing an event (`0x8950c880`) only advances the event
index and the limit. It never reloads the bands' records, so the unported
walker changes nothing there.

Drum through BACKING: 66.7 with the start/end markers, **76.5** with the
detected event (time ×½ 45.9 → 73.1). Through ENSEMBLE it stays at 69.3,
so the drum was likely a BACKING encode. Its references are suspect,
though: the pitch +7 take is untransposed (RECORDING_GUIDE.md).

**v42** makes detection (depth 64, `VSE_DEPTH`) the markers and sends
BACKING content through the firmware chain too. Chord 89.88 / 86.57 / 85.42;
drum 82.07 / 78.55 / 73.91 / 76.18 (mean 77.68 against 77.58). Suite
**76.60**.

### Factory waves: V-Synth 1.50 encodes, and the stereo reader (2026-10-28)

**Who wrote each wave.** The `LIST/ISFT` chunk names the writer:
- "V-Synth Ver.2.01" (the XT): waves 001, 003, 004;
- "VTrainer Ver1.00": wave 002;
- "V-Synth Ver.1.50" (an original V-Synth, © 2003): the 40 factory waves
  343–382 and 415, including every BACKING/ENSEMBLE wave;
- no `LIST`: LITE waves.

`vpsc` byte 1 is the encode type (0 LITE, 1 SOLO, 3 BACKING, 5 ENSEMBLE),
and byte 10 the original key.

**SOLO vpdt layouts.**
- *Mono, one segment* (003, 004, 345): count at byte 84, records from 88.
- *Stereo* (343–382): one record stream, count at byte 136, records from 140.
- *Several segments* (001: 10, 002: 13, 346: 7, 347: 11): the high half of
  u32[2] is the segment count. Each segment has its own directory entry
  holding its first record length and position. They are probably the
  attack events (markers or Encode Depth). The u32[2] high half is 2 for a
  single segment.

**The XT's stereo SOLO encode reads (L+R) >> 1.** The setup routine sets
the sample reader by channel mode:
- mode 2 (mono) → `0x8951f6e0`, which reads one sample, stride 1;
- mode 3 (stereo) → `0x8951f6f0`, which loads L and R, adds them and
  shifts right arithmetically by one (the floor of the half-sum), stride 2;
- modes 0 and 1 read one channel of a stereo pair, stride 2.

`xt_encode … stereo` and the engine's int16 conversion (floor, not round)
now do the same. Round-to-nearest of the half-sum breaks one wave (359).

**V-Synth 1.50's SOLO encoder is nearly the XT's.** I ran the native XT
port on the 24 single-segment stereo factory SOLO waves with (L+R) >> 1:
- **8 are bit-exact** — every start, length and info word (358, 359, 360,
  365, 374, 378, 379, 382);
- the rest diverge in small ways:
  - 1/16-sample steps on otherwise identical tracks (344, 364, 375);
  - unvoiced records: always 704 (44 samples) on V-Synth 1.50, against the
    XT's 702–710, or 32080 for long unvoiced runs at the start (361, 368,
    373);
  - voicing in decaying tails: V-Synth 1.50 keeps voiced records where the
    XT goes unvoiced (363, 367, 370, 372).

So the 2003 encoder is an earlier build of the same algorithm, with
different unvoiced-record and voicing details. The port's
`xt_encode … stereo` reproduces 003, 004 and the eight waves above
bit-exactly.

Mono factory waves: BassDrum1 (345) diverges from its first mark (1381
against the XT's 6816). The multi-segment waves need the event list; the
port only takes {n·16}.

### Stereo SOLO playback, and three player corrections (2026-10-29, v44)

**The stereo read path.** `0x8950d0f0` tests the player's byte 0 bit 0x40
and branches to `0x8950d1f0` for a stereo wave. That path is the mono path
run on packed frames:
- each frame is a 32-bit word of two int16s, L in the low half;
- both channels are read at the same index and fraction, and the second
  tap and its weight are shared;
- the window weights are shared, and so is the clamp `0x8950d420`;
- the highpass SVF runs once per channel, with shared coefficients
  (+0x4c/+0x50) and separate state (L at +0x54/+0x58, R at +0x5c/+0x60);
- the smoothed gain (+0x80…+0x88) multiplies both channels.

The record stream is the one the encoder writes from (L+R) >> 1. So the
port keeps one plan with a second channel, and `XtSoloVoice::render(L, R)`
reads both through the same grains. `EmuSolo.java` with `EMUSOLO_CH=2` sets
the flag and writes L/R pairs.

**Three corrections that the stereo check turned up** (on factory wave 358,
whose records are V-Synth 1.50's). They apply to mono as well:
1. *Unvoiced records keep their rate.* `0x8950d5a0` builds the
   non-voiced rate as S = (info & 1 ? A : 1)·(info & 2 ? F : 1), with
   A = 0 read as 1. The port used S = A whenever the info was known. Both
   the XT's encoder and V-Synth 1.50 write unvoiced records with low bits 0
   (wave 001 has 189 of them; 358's tail is all info 128), so noise grains
   play at S = 1. Voiced records (bit 0x4) use A and F ungated.
2. *Grain parameters in single precision.* `d5a0` works in floats on the
   1/16 grid:
   - Ci = frac·(N − C) + C, and ppw = Ci / rate (at most 524287);
   - +4 = trunc(ppw·256) (1/4096), and the anchor step
     +0x30 = trunc(max(16·rate, 16));
   - wl = Ci / v; if wl > ppw, os = trunc(Ci − ppw·v) and wl = ppw;
   - slope = 8 / wl, and f0n = min(16 / ppw, 0.5).

   In double, a value that is exactly on the grid comes out just below it:
   os = 5365·(1 − 0.8) = 1073 gave 1072, a 1/16-sample start error. That
   was the −50 dB residual on the mono cases, not round-to-zero noise.
   The port now does these operations in single precision, with the XT's
   round-to-zero (or the emulator's round-to-nearest under
   `XT_ROUND=nearest`). Each is done in double and rounded once, which is
   exact for one operation on two floats.
3. *The second tap is clamped at the end of the wave.* After adding the
   record period, the tap indices go through `0x8950d420` again: an index
   at or past n − 1 reads s[n − 1] with fraction 0. The port read silence
   there.

Result, port against the emulator:

| Material | Cases | Before | After |
|---|---|---|---|
| Wave 358, stereo | 4: identity, pitch ×1.5, formant 0.8 at 2×, Energy 0.6 | −22 to −75 dB | −103 to −111 dB on both channels |
| XT-made mono | 13 | −50 to −115 dB | −101 to −118 dB |

Seven vocal suite renders move by −55 dB or less; the suite average goes
from 76.600 to 76.601.

### The envelope generator (2026-10-29, v45)

**Where it runs.** The XT keeps a pool of 24 tones (0x2264 bytes each,
0x89fb1500, vptr at +0x18, vtable 0x89833fac). A tone's control update is
its virtual slot 1, `0x894f75f0(tone, n)`. It advances, all with the same
n:
- its envelopes: 14 objects of 0x70 bytes at +0x1780, those with +0x68
  set, through `0x894efdf0`;
- its LFOs: 8 objects of 0x98 bytes at +0x1da0, through `0x894f1050`;
- both oscillator voices (+0x20, +0x9d4), through `0x894f2090`. The
  voices' converters read the envelopes through pointers at voice
  +0x970…+0x97c (pitch, time, formant, amp; output at +0x4c) and the LFOs
  at +0x980…+0x98c (output at +0x38).

The tone renders through a pointer-to-member at +0x1774 (`0x894f6d40`:
OSC2, OSC1, mixer, the two COSM sections, then the output stage).

**The envelope object.**

| Offset | Field |
|---|---|
| +0x00 | stage: −1 idle, 0 attack, 1, 2 decay, 3 sustain, 4 release, 5 done, 6 hold at L1 |
| +0x08 | curve |
| +0x0c | time scale |
| +0x10 / +0x18 | tempo sync |
| +0x1c…+0x28 | stage times T (ms): attack, second stage, decay, release |
| +0x2c…+0x3c | levels L0…L4 |
| +0x40 | rate |
| +0x44 | 16-bit phase |
| +0x4c | output |
| +0x50 | release start |
| +0x54 | follower coefficient |
| +0x5c…+0x64 | output pointer-to-member |

**Routines.**
- **Rate** `0x894ef520`: 655360 / T, or 0x10000 when T ≤ 10. Every stage
  but the release is then scaled by +0xc.
- **Tick** `0x894efdf0(env, n)`:
  1. At phase 0, outside sustain, it moves to the next stage.
  2. It re-reads the stage's rate and adds n·rate to the phase.
  3. On overflow it sets the stage's end level (L1, L2, L3, L3, L4).
     Otherwise it calls the output function.
  4. It sets the follower coefficient: 1/32, or (1 − T/30)/32 when the
     stage is under 10 ms (`0x894efd00`).
- **Output**, chosen by curve type (pointers-to-member at RAM 0x8987af00,
  af0c, af18):
  - curve 0, `0x894ef7d0`: linear between the levels. The release runs
    from +0x50 to L4 and is done once |out| ≤ 2^-16.
  - curve 2, `0x894efca0` = `0x894ef7d0`, then `0x894ef960`:
    - the attack first follows a u16 attack table (0x897d1934, 257
      entries);
    - each segment is then reshaped as
      out = low + range·C(32767·(out − low)/range), with C the u16 table at
      0x897d1b36 (129 entries, 0…65535, via `0x894efcc0`);
    - the result is a slow-starting attack and exponential-like decay and
      release. Segments with both levels negative are mirrored.
- **Note-on** `0x894f0130`:
  - it copies the parameter block and installs the output function;
  - attack T = 0 starts in stage 1 at its end, at full level;
  - 0 < T < 10 sets the output to L1 at once, and the follower's
    (1 − T/30)/32 makes the ramp;
  - every path leaves with the phase equal to the rate.
- **Note-off** `0x894f03e0`: stage 4, with the release starting from the
  current output.
- **Live edits** `0x894f0080`: copies new times, levels and scale; in
  sustain the output moves to the new S at once.

**ADSR parameters** (`0x894df240`, one converter for every envelope kind:
0–3 OSC1 pitch, time, formant, amp; 4–7 OSC2; 8–11 COSM; 12 TVA):
- curve 0 for pitch, time and formant envelopes, 2 for amp and filter
  envelopes;
- times [A, 0, D, R] from the u16 table at 0x897cec24, through
  `0x894df7b0`. That routine also applies key follow, as (key − 64)·amount
  >> 8 clamped to 1…12000, and a 16.16 time scale;
- levels [0, 1, 1, S, 0], with S = the u16 table at 0x897cfcb8 / 32768
  (S 64 = 0.254, −12 dB);
- matrix modulation offsets the knob values (`0x894deb60`…).

The time table runs 0, 1, 2, … 2130 at knob 64 … 60000 at 127.

**Units.** T is in milliseconds. In tempo sync the converter writes
T = clocks/(24·tempo)·600000, with the clock counts from 0x897cfeb8 (3,
6, … 24 for a quarter, 96 for a bar). The manual gives the Patch Tempo as
20.0–250.0, so the tempo is stored ×10, which makes T = 60000·beats/BPM.

A stage takes T/10 ticks of n, so the tick is 10 ms. Two other facts agree:
- the LFO step (`0x894f1320`) is f·65535/100 per tick, with f from the LFO
  rate table (0.001–40, in Hz);
- stages under 10 ms skip the generator, which only makes sense if one
  tick is 10 ms.

The patch TVA settles it (v46, below): each update ramps its gain to the
new target over exactly 441 samples. It counts the samples since the
previous update in steps of the 32-sample block. So the tone update runs
between blocks once per 441 samples, 10 ms at 44.1 kHz. The scheduler
itself was located later ("The whole tone against XtSynth"), but nothing
about the clock was left to infer.

**Consumers.**
- *The oscillator amp envelope* (voice +0x97c) drives the player's gain
  target, V5c·clamp(V54·env·(1 + V58·LFO)) (`0x894f3dc0`). The player
  follows that target per sample. At note-on it starts from the envelope's
  output if the attack is 0, otherwise from L0 (`0x894f3e20`). A released
  oscillator voice is stopped once that gain is ≤ 2^-16 (`0x894f2090` →
  `0x894f2da0`).
- *The patch TVA* (tone +0x1650, envelope pointer at +0x94), the manual's
  amp envelope:
  - **Update** `0x894f5360`, from the tone update after a done check.
    1. The targets are level·env·(1 + LFO·depth)·(1 ∓ pan), clamped to ±1,
       one per channel (+0x60/+0x64).
    2. The ramp lasts 441 samples. When the stage is short, it lasts
       d = T − elapsed/44.1 ms instead: d·44.1 samples if 0 < d < 10, 0.1 ms
       if d ≤ 0. T is the envelope's stage time (+0x6c, −1 in sustain) and
       elapsed is +0x90.
    3. step = (target − gain)/ramp (+0x58/+0x5c); elapsed = 0.
  - **Render** `0x894f5720`, per 32-sample block. If 32 steps would pass the
    target, the step becomes (target − gain)/32.5, so the gain lands on the
    target without overshooting. Per sample the gain is applied, then
    stepped. After the block, elapsed += 32. (The same code smooths the
    effect sends.)
  - **Note-on** (`0x894f5160`…): the gain starts at level × the envelope's
    onset level (+0x58: L1 with no attack, else L0). The first ramp lasts
    min(attack T, 10) ms, or 0.1 ms with no attack.
  - **Done** `0x894f54b0`: the envelope is in stage 5 and both gains are
    ≤ 2^-16. The tone is then marked finished.
  - Its constants (1/44.1, 44.1, 2^-16, 1/32.5) are each one unit in the
    last place below the nearest float.

**Port.** `plugin/Source/XtEnvelope.cpp` is tick-for-tick identical to the
firmware routines run in the SH-4 interpreter (`xt_env`, notes.txt):
stage, phase, output and coefficient on 18 cases. The cases cover attacks
0, under 10 and long, D = 0, S = 0 and 127, R = 0, note-off in each stage,
knob 127 and curve 0. The tables are read from the user's rom.2.

The plugin runs one envelope per voice and applies it as the patch TVA
does (v46):
- updates come between blocks once per 441 frames, on a clock shared by
  all voices;
- the gain ramps linearly with the TVA's ramp-length rule and block
  landing;
- voices end at the TVA's done check.

The port also matches the firmware on the two fields the TVA reads, the
stage time (+0x6c) and the onset level (+0x58).

**Not yet:**
- the oscillator amp envelope beside the TVA (two envelopes in series on
  the XT), with the player's own follower (SOLO 1/64) and the multi-band
  players' squared gain (`0x8950e1e0`);
- velocity curves, envelope depth, A/D/R velocity sensitivity and Time KF;
- the TVA's level, pan and LFO terms (the plugin uses level 1, centre pan,
  no LFO; velocity stays a linear gain). (Done in v114.)

### Tempo sync and Time Trip (2026-11-04, v118)

The Patch Tempo, the note values on envelope times and LFO rates, each PCM OSC's Tempo Sync, and the Time Trip pad with its Switch and Beat Keep are ported, with parameters and rows in the editor. The tempo is the Patch Tempo, the firmware's Clock Source INTERNAL; following the host's tempo is left for the user to decide. At the defaults nothing moves: `ab.sh` 4/4 identical to v114.

**The tempo.** ID 0x11d is BPM × 10, 200…2500, default 1200. `0x89471ef0` (flag 0x40 of the clock's task) takes it to the object at `0x89979bfc`, whose constructor `0x894714b0` leaves +0x10 = 20 and +0x14 = 500.
- +0x14 = 3000·(+0x10)/(t/10), the milliseconds of a quarter, so the tenths are dropped.
- Readers go back from it: t10 = 30000·(+0x10)/(+0x14), and t10/10 as a float is the block's +0x60 (`0x894e2d20`, analog `0x894e1880`, EXT `0x894e3790`). So 233 BPM has 257 ms and reads as 233.4.
- The envelopes' input +0x18 takes t10 at a note's start only (`0x894df4e0`). Their update (`0x894df600`) and legato (`0x894df5e0`) do not read it.

**The clock.**
- `0x89471030` runs each millisecond: `0x894710e0` adds 24 at +0x1c, and +0x28 counts ticks, Δ·(+0x10)/(+0x14) with the remainder kept at +0x2c. So 480 ticks to the quarter. The other branch, with +0x40 set, runs from +0x46…+0x4a: the external clock, not followed here.
- Flag 2 of task `0x89471ce0` → `0x894710b0` → the OS's timers (`0x8930eb00`, in tempo ticks) → `0x894d4210` every 20 ticks → flag 0x10 of task `0x894d4020` → `0x894f4da0`. So 24 clocks to the quarter.
- `0x894f4da0`: dt = (blocks `0x8987c7a8` − last `0x8987c7a4`)·0.000726, and 0.000726 if that is 0. Then the free LFOs on its list (`0x894f17c0`), and each tone: slot +0x24 (`0x894f78a0` → `0x894f0fd0` on its eight LFOs), slot +0x30 (`0x894f78f0` → `0x894f2540(voice, 1)`: if voice +0xd4 & 2, the byte +0x114 + 1).
- `XtSynth::clockPeriod`, `clocksDue`, `clockTick`. The clock falls on a block's start, ahead of the tone update. On the XT it is a task of its own, so that order is a choice; see "Not covered".

**Note values.** Attack, Decay and Release run to 144 in the descriptors, and so does LFO Rate. From 128 they are note values, a count of clocks from the table at `0x897cfeb8` (3 to 192; 24 is a quarter).
- An envelope's time: T = clocks/(24·t10)·600000 ms (`0x894df230`), as "Units" in the envelope generator's section (v45) had it. So a note keeps the tempo it started with, while the clock moves on.
- An LFO's block has +0x14 set and the clocks as its rate. Note-on `0x894f0cf0` takes fr4, the tone block's +0x494, which is OSC 1's block +0x60: +0x74 the tempo, +0x84 = 65535/clocks, +0x88 = 0 (the target), +0x8c = 60/tempo/24, +0x94 = 0 (the count). With OSC 1 off nothing writes +0x494 (`0x894e4830` skips the converter), so it holds what the block last held. +0x90 is not set, and the constructor does not clear +0x84…+0x94.
- A clock (`0x894f0fd0`): the count + 1; at the clocks, the count 0 and +0x90 = 1. Then +0x88 = +0x84·count, and +0x8c = dt within 0.01…0.125.
- A tick: the rate (`0x894f1320`) is the distance to the target, across the wrap, over +0x8c·100, within 1…65535. If the phase has reached the target and +0x90 is 0, it stops there. A wrap clears +0x90. So the LFO goes at the speed that gets it to the clock's phase by the next clock, and it waits there if the next is late.
- An update (`0x894f0f40`) keeps the flag from the note-on (object +0x60), and with it set +0x84 = 65535/rate. It runs on all of a tone's LFOs at any tone update (`0x894f5c70`), those whose OSC or COSM is off too. The port updated an LFO only when its own knobs moved, which made no difference until now: a key-unsynced note starts with the +0x84 of the tone it follows, and its first update sets its own. Seed 4242 found that. `XtSynth::lfoUpdates`.
- The free object: +8 rate, +0x10 phase, +0x14 count of users, +0x18 flag, +0x1c step, +0x20 target (72 from the constructor), +0x24 seconds per clock (1/48), +0x28 wrap, +0x2c count, +0x30 tempo (120). `0x894f0a30` takes a note's rate, `0x894f08f0` is the rate, `0x894f09b0` the tick, `0x894f0af0` the clock. A note-on that takes it puts it on the clock's list or off, by its flag (`0x894f16d0`, `0x894f1750`). A note's end does not take it off, so it goes on following the clock.

**The OSC's block.** The updater `0x894e55d0` reads the IDs to its table T; the converter's input is T − 8.

| T | OSC 1, OSC 2 | | Default |
|---|---|---|---|
| +0x54 | 0x2d0, 0x304 | Tempo Sync Sw | 0 |
| +0x55…+0x5a | | Robot, Playback Mode, Loop, Vari, Time Offset, Energy | |
| +0x5b | 0x2d6, 0x30a | Time Trip Sw | 1 |
| +0x5c | 0x2d7, 0x30b | Time Trip Beat Keep | 1 |

The tail `0x894e4030` makes the block's +0xb4 bit 0x02 Tempo Sync, bit 0x04 the pad's +8 and the Switch, +0xb5 bit 0x80 Beat Keep. Voice note-on `0x894f3110` copies those to +0xd4, the player's first bytes, with the player's +0x4c = ftrc(+0x60·10000) and +0x48 = the wave header's +0x1c, its Original Tempo, BPM × 10000.

**The beat.** `0x8950c280` runs first in a player's block, the same for SOLO and the band players. `XtBeat`. Positions are in 1/512 sample, 0x4000 to a block.

| Player | |
|---|---|
| +0x40, +0x41 | clocks and Time Trip steps not yet taken, signed |
| +0x48, +0x4c | the wave's Original Tempo, the note's tempo |
| +0x50 | one clock at the Original Tempo: 5.6448e11/(+0x48) |
| +0x54 | what is left before the factor is formed again |
| +0x58, +0x5c | Tempo Sync's target, Time Trip's target |
| +0x60 | where the walk has got to |
| +0x64 | the beat position, which runs on regardless |
| +0x8c, +0x90 | the factor on the walk, the steer's integral |

In a block:
1. **Time Trip.** Without +0xd4 & 4 its target follows the walk. With a step waiting, the target moves by steps·(+0x50), +0x54 = 2·(+0x50), and the factor is formed.
2. **Tempo Sync.** Without +0xd4 & 2 its target follows the walk and the beat position moves a block (`0x8950c470`). With a clock waiting, target and beat position move by clocks·(+0x50); the first clock also sets the walk to that (target 0 means none yet). Under Time Trip the target then stands at the walk; otherwise +0x54 is set and the factor formed.
3. **The walk.** With Time Trip the factor is taken, and +0x60 += 16384·factor. +0x54 falls by a block, and past 0 the factor is formed again if the target is still ahead, else it is 0, so the wave stands still. With Tempo Sync alike, +0x54 falling by the walk + 0x400 and the factor always formed again. With neither the factor is 1.
4. The player's walk this block = factor·walk·2^25.

- The steer `0x8950c4f0` is proportional and integral on the error: e = error·6.1e-6, integral += 0.027·e, factor = integral + 0.08·e, within ±32. Past one clock on the far side of the target both go to 0, but what it returns, which this block walks by, is not limited. Which side is far goes by the sign of the byte.
- The beat position wraps in the loop. Going forward it takes off the region in 1/512. Going backward it adds the region unshifted, in 1/16, so a backward loop's beat drifts. Ported as it is.
- `0x8950f860` at a note's start: factor and integral = tempo/original, +0x54 = 2·(+0x50), targets and walk 0, the beat position from the start position. `0x8950f900` after a seek: the same, but the factor 0 with Time Trip on. A player's position is otherwise its own: this only ever steers its walk.

**Letting go.** Voice update `0x894f3ec0` (kind 2, VariPhrase) compares the block's bits with the player's:
- Sync coming on: +0x114 = 0. Time Trip coming on: +0x115 = 1, a step to start it.
- Time Trip going off with Beat Keep on: voice +0x9b0 = 1 and the gain's target +0x154 = 0.
- Voice tick `0x894f2090`, `0x894f20d2`: while +0x9b0 is set and the gain +0x158 is above 2^−16, the whole voice update is skipped. Then `0x894f2da0` seeks to the beat position (+0x138 >> 5; SOLO `0x8950fed0`, bands `0x89510bf0`, with byte 0 bit 0x80 set meanwhile) and the update goes on, its gain target coming back. So the sound fades, jumps to where the wave would have got to, and fades in. With Beat Keep off it goes on from where it stands.
- `XtSynth::oscBeat`, `OscVoice::chase`.

The older sections call the voice's ramps +0x8c8 and +0x910 Time Trip's. They are not: Time Trip goes through +0x8c as above, and those stay at rest in every case here.

**The pad.** One object per part, at `0x8987ae78`[part], set up by `0x894d8740`. `XtTripPad`.
- Controllers reach it by `0x894d8360(part, cc, value)`, from `0x894d6f10`, the Control Change case of the message dispatcher (`0x894d43e0`), for each part on that port and channel. That answers the question in `CONTROLLERS.md` as far as the code goes: an assigned CC reaching that dispatcher moves the pad, whatever the chart says. Whether MIDI IN feeds it was not traced. `0x89cf69a4`, `0x89cf69a5` are Time Trip Assign X and Y (IDs 0x91, 0x92), `0x89cf69a6`, `0x89cf69a7` Beam 1 Assign L and R (0x87, 0x88), with 0 and 32 off (`0x894d82e0`). The descriptors give 0, where the manual says CC 82 and 83.
- `0x894f4230` is touching and letting go: both X and Y above 0 touch it, either at 0 lets go. Note that X at 0 lets go without storing the 0, so a Y above 0 in the same breath touches it again.
- `0x894f4080` is a move, `0x894f42b0` the angle (a table at `0x897d1c8c`, 512 to a turn), `0x894f43b0` the change in whole steps. `0x894d8740` picks index 2 of the tables at `0x897d3d8e`, `0x897d3da0`: 48 steps to a turn. A direction once taken holds against a move back of up to 64, which counts when a second one comes; a jump of more than 64 is no move. The first move goes the short way, and half a turn onto angle 0 counts as forward.
- `0x894f4e10` → tone slot +0x3c (`0x894f7920`) → `0x894f25a0`: voice +0x115 = the steps, if +0xd4 & 4 (overwriting, not adding). The firmware's loop hands them out by ±127, but half a turn is 24, so it goes round once.
- Touching and letting go reach a tone only at its next update, through the input's +0x63 and the object's +8 (`0x894e477a`). Here it is at once.
- Clockwise is forward: +48 steps a turn, and the manual agrees. A turn is 48 clocks at the Original Tempo, two quarter notes.

**The check.** `xt_tone` builds the clock's object and the pads as at power-on, and the Patch Tempo through `0x89471ef0`. It draws:
- the Patch Tempo (1200 in a third of the cases, its two ends in a ninth, where the time between clocks reaches its limits) and each OSC's Tempo Sync (a third), Time Trip Sw, Beat Keep (three quarters), and the wave's Original Tempo, written in its header;
- a note value on one envelope time in eight and one LFO rate in four, for every envelope and LFO;
- the pad in a third of the cases: a finger round a circle or back and forth, or jumping across the pad, held still in half (so a walk's budget runs out), let go by both controllers or one, touched again in half, within the fade or after. A first message may have one controller at 0, which is no touch;
- in a quarter of the cases with a second note on a POLY part, all five LFOs key-unsynced on a rate below 128, the rates moved onto note values, the second note following, then the bender: it starts with the first tone's clock step, and that update gives it its own.

The sweep gains the Patch Tempo and each OSC's Tempo Sync, Time Trip Sw and Beat Keep, each moved alone, on a wave with the pad held and let go after the move. The firmware's `0x894f4da0` runs on XtSynth's clock, with its count of blocks from the harness. Its pad goes through `0x894d8360`, and a touch or release is followed by its update. New, at each update: the player's position and +0x50…+0x64, the factor, the integral, both bytes, +0x9b0. The pad's state after each message. An LFO's clock fields start from the port's, as nothing else sets them, and with OSC 1 off so does +0x494.

The clock's rate is the one thing that comparison cannot see, so a last section runs the firmware's own `0x89471030` for two minutes at each of five tempos and counts its ticks against XtSynth's clocks over the same 165375 blocks, within one clock.

**The result.** fcheck: 2000 cases and the sweep, 3381 notes, 1412 taken by a sounding tone, 412 truncated, 272,827 blocks, 10.6 million checks, 0 different; a wave on OSC 2, 1411 notes, 106,834 blocks, 3.8 million checks, 0 different. The clock's rate within one clock at all five tempos. `xt_lfo` gains 7 cases on note values, the clock's fields compared too, all identical. `fcheck.sh --update` adds those and the rate's lines, and changes only `xt_tone`'s count lines. A/B 4/4 identical to v114. PluginTest 4/4 bit-identical at the defaults; with a tempo, Tempo Sync, an LFO and an attack on note values; with Tempo Sync at another Original Tempo and the Time Trip knob moved and let go; and with the knob moved over an LFO on a note value. 3000 cases and the sweep on each of seeds 31337, 9, 2007, 90210 and 55555: 0 different. On 777, 4242 and 1234, below.

**Seeds that showed something else.** Each gives one block past `kSoloTol`, on a SOLO wave.
- 777, case 4698, block 101: a stereo SOLO wave on OSC 2 under Tempo Sync falls silent. 35,610 of that block's peak of 292,083, which is −95 dB of full scale. The block before, with a peak of 19 million, had 48,288.
- 4242, case 1673, block 40: a SOLO wave on OSC 1 under a held pad, into a TVF whose output is small just then. 15,926 of a peak of 94,348, with OSC 1's buffer 0.01% off.
- 1234, case 4082, block 56: a stereo SOLO wave on OSC 2, with neither Tempo Sync nor the pad. 0.01% off at OSC 2's buffer, 2.08% at the mix.

For the last two, given the firmware's OSC buffer, the port's mix and COSM output are exact in every block of the case, as v114 found on its seed 777. The first is at the OSC's own buffer, so there is nothing further to give it. Unmodified v114 shows the same class on seeds 2222, 6666 and 7777. So these are `XtSoloVoice`'s known inexactness, not this version's, and `kSoloFloor` stays where it was. Another session is tracing them.

A tolerance could hide a fault in the new seek, so I measured SOLO's error directly: on blocks with a peak above 2^20 at Structure Type 1, the largest error over the peak, 3000 cases and the sweep.

| Seeds 2024, 31337 | Blocks | Median | 99% | Worst |
|---|---|---|---|---|
| Neither Tempo Sync nor Time Trip | 6833, 6416 | 7.4e-6, 6.4e-6 | 1.1e-4, 1.0e-4 | 2.4e-3, 2.8e-3 |
| Under either | 4735, 4658 | 7.3e-6, 5.9e-6 | 1.0e-4, 1.1e-4 | 2.2e-3, 4.2e-3 |
| Within 200 blocks of a seek (103, 137 seeks) | 330, 541 | 3.6e-6, 6.3e-6 | 9.5e-5, 1.8e-4 | 2.1e-4, 1.2e-3 |

So neither the seek nor the steering adds to it.

**Planted errors.** `tone_muts.txt` gains 173 and loses 8 whose lists are gone (`diff-lfo`, `diff-clfo`, `diff-tlfo-…`, `tlfo-live`): 376 in all, all caught at 2000 cases. New ports: `PORT=tonelfo`, `toneenv`, `tonesolo`; `toneband` covers `XtBeat`. The first run missed 17 of 382.
- Six are exact, and not in the list. `pad-chunks` and `pad-chunks-up`: half a turn is 24 steps, so the firmware's loop goes round once, and the port no longer has it. `beat-clk-sign` and `mb-beat-edge-sync`: a clock is taken in the block right after it, so the byte is 0 when the bit comes on, and it could go negative only after 128 clocks untaken. The firmware's lines stay. `beat-legato`: a legato moves no knob, and the pad has already given its bits. `lfo-upd-legato`: it shows only on a note following another tone, which takes a third note.
- Eleven were the check's weakness, and led to what it now draws. The pad was never held still (`beat-trip-sides`, `beat-trip-again`, `beat-trip-stop`), never let go by one controller or touched by one (`pad-x-touch`, `pad-y-touch`, `pad-y-lift`), never jumped (`pad-fwd-limit`, `pad-first`), and never touched in the fade (`beat-reset-trip`). The firmware's clock read the port's count of blocks (`clock-blocks`) and ran at the port's rate (`clock-rate`). That last one is now 1/4800 fast, not 1/24000, which two minutes at 250 BPM can tell from one clock.
- Tightening the draws then lost `lfo-upd-blocks` and `lfo-limit-hi`, which had been caught by chance at cases 3155 and 2837. Hence the cases following a tone with no note value, and the tempo's two ends.
- `chase-start` on some draws crashes the run before it reports: a stale flag on an analog voice follows a null wave. `mutate_tone.py` now counts a run that dies as caught, and says so.

**What it does.** A wave of a burst every half second, played on all three types: at tempo 240 against an Original Tempo of 120 the bursts come every 0.25 s, at tempo 60 every 1 s, at 240 with Tempo Sync off every 0.5 s. A pad touched at 1.1 s and let go at 3.6 s: nothing from 1.1 s, then bursts at 4.0, 4.5, 5.0, 5.5, on the grid they had. That is the port's render, not the XT's. It shows that the behaviour the check holds to the firmware is the one the manual describes.

**The plugin.** Parameters `patchTempo` (20…250 BPM), `timeTrip` (the pad as one knob, below), and for each OSC `tempoSync`, `origTempo`, `timeTripSw`, `beatKeep`, on the XT's defaults. Every envelope's Attack, Decay and Release and every LFO's Rate go on to 144, shown as 1/32…2/1 with the triplets and dotted ones. That widens those parameters' ranges, so a host's automation, which is normalized, maps differently; a saved session keeps its values. Rows: Sync, Trip, Keep and Original Tempo in the PCM OSC's place of the analog's fourth row, and Patch Tempo and Time Trip in a row of their own (`kXtStripH` 1030, window 1330). CLI `XT_TEMPO`, `XT_SYNC`, `XT_RATE`, `XT_TRIP`; PluginTest `TEST_TEMPO`, `TEST_SYNC`, `TEST_RATE`, `TEST_ATTACK`, `TEST_TRIP`.

Time Trip is a plain knob for now (`XtSynth::timeTripKnob`): 0 lifted, 1…127 a finger going clockwise round the pad's rim, one turn over the range. So its whole travel is 48 steps, two quarter notes, with OFF at the bottom letting go. No MIDI CC is mapped to it, though `XtSynth::timeTrip(x, y)` takes the two controllers' values.

**Not covered.**
- A real XT.
- The clock's phase. On the XT the clock is a timer's and the blocks the audio's, so a clock falls anywhere against a block and against the tone update's task. Here it falls on a block's start, ahead of the update, on both sides of the check. The rate is checked, the phase is not.
- An external or MIDI clock (the other branch of `0x894710e0`), the host's tempo, and MFX tempo, with the effects.
- Playback Mode other than the default, and Vari off.
- The pad by anything but its two controllers: the D Beam's rotation (`0x894d8160`), and the TRIP-R matrix source (`0x894e6c60`). Hold and Pad Mode: the object's +8 is all this reads.
- A third note: the check plays two, so a legato on a note following another tone's LFO is not reached. That is why `lfo-upd-legato` is not in the list.
- With OSC 1 off, an LFO's tempo at note-on is whatever the tone block last held. Both sides start from the port's. The port's is the last it had in that voice, 0 before any.

### The patch TVA's level and pan (2026-11-03, v114)

Level, Pan, their Keyfollows and LFO Depths, LFO 5, the MFX Send Level, and CC 7, 11 and 10 are ported, with their parameters and rows in the editor. The TVA has been at level 1, centred, with no LFO since v46. At the defaults every new factor is exactly 1, so the renders are unchanged: `ab.sh` 4/4 identical to v113.

This answers v101's open question. With the TVA Switch off CC 7, CC 11 and the MFX Send Level still count; Level, Pan, their Keyfollows and LFO Depths and CC 10 do not.

**The inputs.** `0x894eb170` reads the IDs into the input table, 0x16 bytes per tone from `0x89d2a6d0` (0x160 per part); `0x894eb260` reads the velocity pair ahead of them.

| Input | ID | |
|---|---|---|
| +0, +2 | 0x3b7, 0x3b8 | Env 13's Velocity Curve and Sens, through the converters (v48) |
| +4 | 0x299 | TVA Switch |
| +6, +8, +0xa | 0x29a, 0x29b, 0x29c | Level, Level Keyfollow ×10, Level LFO Depth |
| +0xc, +0xe, +0x10 | 0x29d, 0x29e, 0x29f | Pan, Pan Keyfollow ×10, Pan LFO Depth |
| +0x12, +0x13, +0x14 | 0x2a0, 0x2a1, 0x2a2 | MFX, Chorus and Reverb Send Levels |
| +0x15 | 0x2a3 | Output Assign: 0 MULTI, 1 MAIN, 2 DIR (manual p. 97) |

`0x894ea990` at the note's start copies these to its object from +6 on, with the key at +2 and velocity at +3 (the key with the Octave Shift), and latches the velocity gain at +0x1c. A legato (`0x894eaa90`) takes a new key, velocity and gain from the inputs it has. An update (`0x894eaae0`) copies the inputs again and leaves the gain as it was.

**The converter.** `0x894ea380` at a note's start and a legato, `0x894eab40` in the conversion pass, the same arithmetic. It writes the block at tone block + 0x67c. T is the level curve (128 u16 at `0x897cfcb8`) and Pn the pan law (128 u16 at `0x897cfdb8`, 0 at 0, 1 at 63 and 64, 1.427 at 125…127), each /32768. So a hard pan is 3.1 dB up on its own side, and silent on the other.

| Block | Switch on | Switch off |
|---|---|---|
| +0, +4 | 13, 5: the envelope and LFO the note-on binds | the same |
| +0x10, +0x14 | 0, 5: +0x14 is the pan's LFO | the same |
| +8 | Level LFO Depth, (d/63)·\|d/63\| | 0 |
| +0xc | T[CC 7]·T[L]·T[CC 11]·velocity, in that order | T[CC 7]·T[CC 11] |
| +0x18 | 0 | 0 |
| +0x1c | Pan LFO Depth, (d/63)·\|d/63\| | 0 |

- L = Level + trunc(127·KF·(key − 60)/6000), within 0…127, by the reciprocal multiply of `0x894e0c90` (v53). The Pan the same with its own KF.
- CC 10 then moves the pan toward the end it points at: P + (CC − 64)·(127 − P)/63 from 64 up, P − (P·(64 − CC))>>6 below. So CC 10 at 0 or 127 is a hard pan whatever the knob.
- The pair is s·Pn[127 − P] and s·Pn[P], with s = T[MFX Send], which the Switch does not touch. Off it is s·Pn[63] and s·Pn[64], so s on both.
- Output Assign says which pair of the block takes it, the other two left 0: 0 +0x28, +0x2c; 1 +0x20, +0x24; 2 +0x38, +0x3c when the system's Output Mode (`0x89d2a6d0 + 0x1604`) is Parallel, else as 1.
- +0x30 = T[CC 91]·T[Reverb Send], +0x34 = T[CC 93]·T[Chorus Send].
- All eight × 2^(g/6), g the system's Output Gain (`0x89d2a6d0 + 0x1600`), 0 here. The routine also sets the truncate step (v46's).
- Each input also adds a matrix term (`0x894d96d0` on `0x89cf7634`, destinations 0x39, 0x3a, 0x40…0x44) and a controller term from `0x894e7860`'s table. Both are 0 here.

**The controllers.** `0x894e9fe0`, in each conversion pass, copies the channel's values to the note's object: +0x178 C + 0x84 + 7, +0x179 + 11, +0x17a + 10, +0x17b + 93, +0x17c + 91. So a moved CC reaches a sounding TVA as a knob does, at the next tone update, and that update hands out all the blocks. `XtSynth::controller` does what `pitchBend` does. The check writes C and runs the update. The path from a MIDI message to C is not traced, like the bender's in v99.

**The object** (tone +0x1650). Note-on `0x894f50e0`, after the LFOs: copies the block; binds Env 13 (+0x94), LFO 5 (+0x98, +0x9c). With a0 = env·level, a8 = LFO·(+8), ac = LFO·(+0x1c):
- targets (+0x60, +0x64) = (1 + a8)·a0·(1 ∓ ac), within ±1. The gains (+0x68, +0x6c) the same with the envelope's onset level.
- the bus levels (+0x70…+0x8c) start at the block's, so a note's first sample is panned.

`0x894f5de0` → `0x894f5310`, after the LFOs in `0x894dba00`, takes +8, +0xc, +0x18, +0x1c and +0x20…+0x34 from a new block. Not +0x38 and +0x3c, so Output Assign DIR's pair stays as at note-on. Update `0x894f5360` forms the targets as above, then the ramp as in v46, a step per channel. Done (`0x894f54b0`): both gains at or below 2^−16.

**The render** (`0x894f5720`). The gains as before, one per channel. Each bus level moves by (block − level)·0.0009375 per sample, formed once per block: 3% of the gap each block, a time constant of 33 blocks, 24 ms. So a Pan or Send moved glides, while the LFO's pan moves with the gains. The frame's 32 bytes:

| Frame | |
|---|---|
| +0, +4 | L·gain·(+0x70), R·gain·(+0x74): Output Assign MAIN |
| +8, +0xc | ½(L·gain + R·gain)·(+0x80), ·(+0x84): the Reverb and Chorus sends |
| +0x10, +0x14 | L·gain·(+0x78), R·gain·(+0x7c): MULTI, to the MFX |
| +0x18, +0x1c | L·gain·(+0x88), R·gain·(+0x8c): DIR |

The loop is hard to read in Ghidra's listing, as it runs under `fschg`: with FPSCR.SZ = 1, `fmov fr0,fr1` is `fmov dr0,xd0`, a pair to the other bank, and `fmov fr1,fr0` the way back. That is how the gained samples reach the bank holding the last two buses.

A truncated tone (`0x894f5570`) holds gains and bus levels: L·gain·fade·(bus level), in that order.

**LFO 5.** IDs 0x3de…0x3e5, the XT's defaults as LFO 3's (TRI, Rate 98, Offset 0, no delay, Key Sync off). The note-on sets its +0x80 whatever the TVA Switch, so it always ticks. Its seed is the sixth drawn (LFO 0…7). Key-unsynced it follows a sounding tone's or takes its free object, like the others (v94); `XtSynth::freeTlfo`. A live Fade Mode has no effect, as the mode is latched at the trigger (v94), so a planted error leaving it out of the change list is exact and not in the list. A legato's update of it moves nothing either, as a legato moves no knob.

**Left out, with reasons.**
- The Chorus and Reverb sends, and +8, +0xc of the frame: there are no effects yet, so nothing to send to. The check does not compare them.
- Output Assign: the plugin has one output. With no MFX all three buses would be the same sound. It stays at MULTI on both sides, and the check reads the MULTI pair. Other values would need two more things: COSM 1 on the mix reads the bus's first pair past its block (v104), and DIR's pair does not update.
- The matrix, and the controller terms, at rest. CC 121 and other resets are not modelled: `controller` takes only 7, 10 and 11.

**The check.** `xt_tone` draws Level, Pan, their KFs and depths, the MFX Send, LFO 5's seven fields and the three CCs (at rest in two cases of three, so the sweep's moves can be seen), and moves them live. New, every block: +8, +0x1c, +0x28, +0x2c, +0x78, +0x7c. The right channel's gain, target and step were always compared, against the left's values, as nothing could part them; now they have their own. At every update: LFO 5's output. At each case's start: its free object's phase. The sweep gains LFO 5's fields, the TVA's, and each CC alone, on a note with both depths on and LFO 5 fading in or in a short delay.

**The result.** fcheck: 2000 cases and the sweep, 3353 notes, 1373 taken by a sounding tone, 416 truncated, 271,325 blocks, 8.0 million checks, 0 different; a wave on OSC 2, 1412 notes, 107,310 blocks, 2.8 million checks, 0 different. 3000 cases and the sweep on each of four other seeds (777, 31337, 9, 4242), 0 different. `fcheck.sh --update` changes only `xt_tone`'s five count lines. PluginTest 4/4 bit-identical at the defaults; with the TVA's knobs and LFO 5 set, the Switch on and off; and with CC 7, 10 and 11 moved during a note. A/B 4/4 identical to v113.

**A seed that showed something else.** With the new draws seed 777 gave one difference, at COSM output in a case with a SOLO wave on OSC 2, a Compressor, then an Amp Simulator: 2.19% of the block's peak, past `kSoloTol`'s 2%, with OSC 2's buffer only 0.002% off. Three runs of that case settle it. Given the firmware's OSC 2 buffer, the port's COSM output is exact in every block. With COSM 2 off the jump is still there (0.26%). With COSM 1 off it is gone. So the Compressor's level decision flips on the SOLO player's −93 dB difference, as Lo-Fi's quantizer does, and the check now replaces both on a SOLO wave. Nothing of the TVA in it: that tap is before the TVA.

**Planted errors.** `tone_muts.txt` gains 66, 19 of them in the converter (PORT=toneosc, XtOsc.cpp; `mutate_tone.py` takes a fourth field for a port). All 211 are caught at 2000 cases. The first run missed five, and two of those were the check's weakness rather than exactness: moving LFO 5's Offset or Fade alone was invisible when the drawn Fade Mode left it at amplitude 0. So the sweep now fixes the mode and a Fade in progress. `cosm-past-always`, an old one, had also slipped through, by chance of the new draws: it reads OSC 2 past the block on Type1, which only a stereo Side Band shows. A quarter of the cases made for those words now stand on Type1.

**The plugin.** Parameters `tvaLevel`, `tvaLevelKF`, `tvaLevelLfo`, `tvaPan` (shown L64…0…63R), `tvaPanKF`, `tvaPanLfo`, `mfxSend`, `tvaLfo…`. Rows TVA, TVA Pan and LFO 5 (two) at the end of the XT strip, after the COSMs as the signal goes: `kXtStripH` 1000, window 1300. MIDI CC 7, 10, 11 go to `XtSynth::controller`. The TVA Switch is off by default, so Level, Pan and LFO 5 do nothing until it is ticked, as on the XT. A session saved before v114 comes up at the defaults, and so sounds as it did. CLI `XT_TVA`, `XT_LFO5`, `XT_CC`; PluginTest `TEST_TVA`, `TEST_LFO5`, `TEST_CC`.

**Not covered.** A real XT. The matrix and controller terms. How a CC message reaches C. CC 91 and 93.

### The SOLO encoder's stereo mode (2026-11-03, v113)

`xtSoloEncode(x, n, 2)` now gives the records of `0x8951e120(3)`, the mode the XT's driver gives a stereo wave (`0x89515ecc`: channels 2 gives 3, otherwise 2). Before this, the port and `xt_enc`'s `:2` mixed to `(L+R)>>1` and ran mono mode, so the check compared two encoders on the wrong path. On all 87 stereo waves in the repo the port and `0x8951e120(3)` now agree on every start, length and info word, and so do 100 generated ones, bar one the firmware has no result for. The old path differed from mode 3 on 44 of the 87: `drum` 188 of 609 records from record 420, wave 403 with 47 records against 24. (v112 counted 45 of 89. My list leaves out a 32-bit test file, and I did not find what else it held.)

**The modes.** `0x8951dd70` takes the mode through a jump table at `0x8951e044` and sets three function pointers and a stride.

| mode | reader `0x8a015478` | samples `0x8a015470` | filter `0x8a015474` | stride `0x8a01547c` |
|---|---|---|---|---|
| 2, mono | `0x8951f5b0`: x | `0x8951f6e0`: x | `0x8951f700`: FIR(x) | 1 |
| 3, stereo | `0x8951f5e0`: 0.5·(L+R) in float | `0x8951f6f0`: (L+R)>>1 | `0x8951f740`: 0.5·(FIR(L)+FIR(R)) | 2 |
| 0, left | `0x8951f650`: L | `0x8951f6e0` | `0x8951f740` | 2 |
| 1, right | `0x8951f620`: R | `0x8951f6e0`, the pointer 2 bytes on | `0x8951f740` | 2 |

The driver only passes 2 or 3, so 0 and 1 are not ported. Their filter still averages both channels, which nothing reaches.

**Who reads what.** Only these use the wave. Everything else works on the buffers.

| | routine | samples | decimated or scratch values |
|---|---|---|---|
| first fill | `0x8951f790` from `0x8951e204` | samples | reader, through `0x8951f680` |
| shift, `off + 0x3000 ≤ n − maxLag` | `0x8951f880` from `0x89522138` | samples | filter |
| shift, otherwise, with its zero tail | `0x8951f790` | samples | reader |
| scratch, away from the ends | `0x8951fab0` | | filter, every 4th sample |
| scratch, within `0x3f` of either end | `0x8951fab0` | | reader, every sample |
| drift | `0x89520d30` | | reader |
| crossing scan | `0x895214b0` | | reader through FTRC, no filter |

In mono the reader's path and the filter come to the same floats, which is why the port had one `decAtF`. In stereo they don't. The reader's sum is exact, as 16-bit sums fit a float, so the FIR runs once on 0.5·(L+R). The filter runs two FIRs, each with its own round-to-zero steps, and averages them. They differ by about one unit in the last place. FTRC then truncates 0.5·(L+R) where the samples floor: (−1, 0) reads as −1 from the samples and 0 in the crossing scan.

By the arithmetic the filter stays inside the wave. Its callers test `pos ≥ u + 0x3f` and `pos + u + 0x3f < n`, or `off + 0x3000 ≤ n − maxLag`, with maxLag 2004 whenever a shift is reached. So its missing bounds check never matters, and the port's `firChannel` keeps one anyway.

**The port.** `fetch` is the reader, `sample` the samples, `decAtF` the FIR through the reader, `decFast` the filter. `fill` takes a flag for `0x8951f880`. Mono is unchanged: `decFast` is `decAtF` there, and `fcheck`'s mono lines did not move. XtSynth, `XtToneCheck`, `xt_fw solocmp` and `xt_analog`'s PCM waves all give the encoder the interleaved wave now.

**Why the check now compares memory.** Three planted errors changed no record on any wave here: the scratch through the reader's path, near-the-ends scratch through the filter, and drift through the filter. A scan of 400 random stereo waves showed none of them, and 46 waves built for it, a loud carrier cancelling between the channels, showed only the drift, on one wave. One unit in the last place rarely moves an FTRC, and a moved FTRC rarely moves a mark. So records alone cannot show these paths are right.

`xt_enc cmp` now also hashes the encoder's memory after each step, on both sides:
- after each fill and scratch fill, the whole 0xc000 bytes at `0x8a0090f4`: samples, decimated signal, scratch;
- after each drift, the 0x18000 bytes at `0x89ff10c8`.

It says so only when they differ, so the baseline's lines stay as they were. On all 275 waves, mono and stereo, every step of every wave with a firmware result is identical to the bit. `xtSoloEncode` and `xtFwSoloEncode` take an optional `trace`, and `xtEncHash` is the hash.

**The waves.** `make_inputs.py` writes `enc2_*`, which `fcheck` runs in one section as 170 waves with `:2`, through `xargs -P 8`: 275 separate processes at once would be a lot.
- the project's 70 stereo waves, by number (`enc2_prj_*`);
- `drum`, `w358`, vocal against chord, and vocal against itself a sample later, each gated, quiet, clipped, reversed, padded and swapped;
- the sum odd everywhere (`odd_`, `oddp_`, `anti_`), and gaps holding only −1, 0 and 1 (`dith_`), where floor and truncation differ;
- both channels the same (`same_`), which by the arithmetic gives mono's records;
- detuned tones, one channel silent, low notes, fast falls, fades on a constant, lengths from 2 to 14000, constants at the ends of the range, clicks in one channel, noise, and 12 mixes from a seed.

`enc2_mix02` has no firmware result: the firmware reads outside memory after a NaN drift (`read32 at 8201xxxx`, pc `0x89521a1c`), as in v112. The port's records tile it. A random corpus outside `fcheck`, 1200 more stereo waves (tones, gated, quiet, short and noisy, with the channels a phase apart, one off, cancelling, the same, one silent or dithered): 1179 identical in records and memory, 344,011 records, and 21 with no firmware result, all gated. No wave differed, and the port's records tile those 21.

**Planted errors.** `mutate_enc.py`: 34 of 34 caught, now about 8 minutes. The 11 new ones, in waves: `st-reader` 160, `st-first-fast` 157, `st-sample` 156, `st-fir-left` 138, `st-fir-mix` 134, `st-scratch-slow` 113, `st-shift-slow` 83, `st-shift-fast` 77, `st-scratch-end` 54, `st-drift-fast` 50, `st-cross` 4. `st-cross` is the one that only records can catch, as the crossing scan leaves nothing in that memory; its 4 waves include project wave 392, where it makes a record too many. Memory also raised the older ones, `fir-order` from 5 waves to 260 and `drift-stale` from 2 to 41. One is exact and left out of the list: the zero tail through the fast filter, which reads only zeros.

`xt_tone` gives both sides the port's records, and `w358`, its stereo SOLO wave, has the same records in either mode. So the old path in XtSynth would pass it. It now also compares XtSynth's own records with the firmware's on `vocal.bin` and `drum.bin` ("the SOLO records of"), which does not change what it draws. `solo-old-mix` plants the old path in XtSynth: 187 different. `mutate_tone.py`: 145 of 145 caught.

**Against records from an instrument.** The project's waves store their own records, and a stereo one holds the list twice: `xtrecs.py`, in `fcheck`, compares them with the port.

| | waves | records | equal to stereo mode | equal to the old path |
|---|---|---|---|---|
| stereo, ISFT "V-Synth Ver.1.50" | 25 | 3190 | 24 | 18 |
| mono, "V-Synth Ver.2.01" (003, 004) and "1.50" (345) | 3 | 2241 | 3 | not applicable |

This is the strongest evidence there is that stereo mode is right, with a limit: those stereo records were written by the original V-Synth at 1.50, per the file, not by the XT's 2.02 that the harness runs. No project wave has XT-made stereo records.

The one that differs is wave 373, 42 of 146 records, first at record 29 (1367 against 1366), the same again from record 100. The old path has 4 different on it. One step at one mark explains all 42:
- At that mark two lag sums tie exactly: D(lag) = D(lag+1). So the fit's numerator is exactly half its denominator, and the exact quotient is 32768.
- The divide (`0x89522800`) rounds its divisor up and gives 32767, on the rom as in the port.
- The period comes out 5898236, one step (4 units of 1/65536 sample) under 5898240, where `per >> 12` goes up.
- Raising that one period to 5898240 makes all 146 records equal the stored ones. Trying every tracker step of the wave, up and down, nothing else does; the next best leaves 2. The next mark falls 1/16 sample later, so the segment's drift changes, and that moves those 42 records by 1/16 sample.

I don't know why the stored run has that step. What I tried:
- An exact quotient wherever an exact tie meets this rounding is not it. That leaves 373 at 40 different and breaks 344, 352, 365 and 369, which match today. Many of these waves have such ties, up to 6 a wave, and elsewhere the stored records agree with rom.2's rounding.
- Float rounding is not it: the filter everywhere through the reader, or everywhere through the fast path, round-to-nearest, or the older coefficient order (`(X·u)·u`) all leave those 42 records as they are.

The firmware that wrote this file may differ here from 2.02, but I have no 1.50 image to test that on. On the XT's own rom the port is right, as it equals `0x8951e120(3)` on this wave too.

**Playback.** `solocmp` gives both sides the firmware's records, now in mode 3. New cases, `drum` at 1.0 and at 0.8 with formant and walk: −101.1 and −98.2 dB, in line with the rest. Waves 405, 403 and 411 by hand: −104.7, −86.7 and −87.8 dB.

**A/B.** `verify.sh` is 4/4 identical against v112, PluginTest 4/4, because `w358` has the same records in both modes and those renders' `drum` is BACKING. Stereo SOLO renders change where records do, as intended. XtSynth CLI, two notes as in `ab.sh`, against v112:

| wave | frames | against v112 | from frame |
|---|---|---|---|
| `w358` | 13298 | identical | |
| `drum` as SOLO | 64962 | identical: its records differ only from record 420, sample 42,567, which these notes don't reach | |
| 343 | 22396 | −98.9 dB | 12,783 |
| 386 | 27804 | −31.7 dB | 3,000 |
| 397 | 28660 | −32.9 dB | 2,389 |
| 403 | 7496 | −54.0 dB | 183 |
| 405 | 45419 | −25.7 dB | 1,223 |
| 411 | 41729 | −65.4 dB | 2,240 |
| 376 | 35730 | −50.7 dB | 6,090 |

Of the project's 70 stereo waves, 30 get different records.

**What this leaves.**
- Wave 373, above.
- The events: the port still takes only {n·16}, as `xtFwSoloEncode` does. The 25 waves above have just that, so this did not need them.
- `VariphraseEngine` still mixes and calls mono. The plugin only uses it for `analyzeContent` and the old offline renderer, not for what it plays.

### The SOLO encoder against the firmware's (2026-11-03, v112)

`xtSoloEncode` now gives the records of rom.2's own encoder (`0x8951dc40`, `0x8951e120` in the interpreter), every start, length and info word, on all 2138 of the 2157 waves tried that have a firmware result: the project's 115 real waves (stereo mixed), sines, saws, gated, clipped and short waves, and random mixes. Before this, 73 of 413 generated waves came out the same.

Nothing had compared them: `solocmp` gives both sides the firmware's records and `xt_tone` both sides the port's. The port had been checked on waves 003 and 004 and eight factory waves, and those never reach most of what follows. It came from `vpenc.dll`, and the XT differs from `vpenc` in more places than the table in "Native port" lists.

**What separated the sines.** Level. `sine.bin` peaks at 16423. The silence test (`0x8951ea80`) sums x²/256 over 2·maxLag samples, which passes 2³¹ on a sine of amplitude about 16564. So 0.5 of full scale was the same and 0.55 was not, and 0.8 was the same again because its wrapped sum happened to end above the gate.

**The causes.**

| | firmware | the port had | shows as |
|---|---|---|---|
| 1 | `0x8951ea80` adds in int32 and tests the sign: a sum that wraps counts as loud | the same in C, where signed overflow is undefined: clang dropped the test (deleting it from the fixed source gives the old records), so the wrapped sum could read as silent | 0.7 sine: port 128 records, firmware 661; the first 32080 0x80 against 106 0x87 |
| 2 | fills (`0x89521750`, `0x895215a0`) step by `0x895214b0`: from the mark, one whole cycle of the wave's raw samples, up to 882; the period if that is shorter or the limit is hit | `vpenc`'s: always the period | gated vocal, record 247: 2361 against 2369; `drum`, record 420: 710 against 717 |
| 3 | those fills change from the first routine to the second under span 0x44e0 (17632) | 16000 | same start and length, 0x5f against 0x87 |
| 4 | `0x895215a0` clamps a to b and b to a after a step, and works out the next periods from the stepped ones | no clamp, the periods before the step | gated waves |
| 5 | the lag search (`0x8951f8f0`) takes a flag: the tracker and the octave-down run look at 12/16 to 21/16 of the lag, the octave votes 15/16 to 17/16 | 15/16 to 17/16 for all | fast falls; same start and length with another info (0x9f against 0xa7: a voiced mark's quality comes from the lag found) |
| 6 | `0x8951fab0` fills the scratch from every 4th sample; within the filter's reach of either end of the wave (`pos < u + 0x3f` or `pos + u + 0x3f ≥ n`) the slower path takes every sample, over the same count | every 4th, always | short waves, first and last notes |
| 7 | after the last fill (`0x8951e880`), a mark at 2·tail − previous if that is before the end, with the first node's low bits | none | a wave that ends voiced with room for one more period: one record split in two |
| 8 | setup (`0x8951dd70`): under maxLag samples, pmin = 44100 / (n / 2), maxLag = 44100 / pmin, and the filter is designed from that pmin | pmin 22 always | every wave under 2004 samples |
| 9 | past the end at the first hop, with `0x8987caa8` still unset, the encoder returns −2 with no list (`0x8951e288`): n = 2, 2004, 2005 | records | those lengths. n = 1 divides by 0 and never returns; the port gives no records for it either |
| 10 | filter design (`0x8951f380`): X·(u·u) | (X·u)·u | 5 of the 64 coefficients 1 ulp off; 3 of 240 fast falls differ, by 1/16 sample |
| 11 | drift (`0x89520d30`) keeps its two buffers between calls, and the list head, a pointer, lies just under the first: as a float tiny and negative. Under that come small integers, which the FPU takes as 0 | a fresh buffer each call, 0 outside the window | a tone fading on a constant: a scan that finds no crossing within the period reads those, and the drift moves by a sample |

Read from the disassembly and ported, but no wave here depends on them. Planting each error changes no record on 968 waves:
- `0x89521a80` returns 0x8000 for an all-zero window, where the port returned 0.
- `0x89522940` shifts with SHAD: a quotient over 2²⁴ shifts left. The port shifted right by the wrong amount and let the upward correction walk there.
- The zero-crossing count for the octave hint is on the floats (`0x8951e320`), not the truncated values.
- The work buffers are one block in the port now, because they are in rom.2: samples 0x8a0090f4, decimated 0x8a00f0f4, scratch 0x8a0120f4. A search near the top of the decimated signal reads on into the scratch, by the arithmetic up to about 1030 values on a long lag. 76 of 1055 waves read past the top, and it made no difference. The port used to read its own object layout there.
- FTRC saturates. The port's `(int)` happened to on ARM.

The rest of the encoder (onset, candidates, refine, tracker, octave, gap fill, back extend, shift, first mark, the fit, the node list) reads the same as the port. Signed products that wrap on the SH-4 now wrap in C too.

**Where the firmware has no result.** A mark in a run of exact zeros makes drift 0/0. FTRC makes the NaN 2³¹, every mark after the segment moves by that, and the tracker reads outside memory (`read32 at 8201xxxx`, pc `0x89521a1c`). 16 of the 2157 waves do this. That is a fault in the harness. On the XT the read lands in unmapped P1 space and I don't know what it returns. Three more never return: n = 1, twice, and one 3504-sample random wave where `0x89522800` rounds its divisor down to 0 and `0x89522940` then loops for ever (the port's divide returns 0). So there is nothing to match: the port leaves the marks alone (`spreadDrift`), and the check requires its records to tile the wave. With the firmware's arithmetic they did not: 68275 records from 66150 samples, starting at −2³¹.

**The harness.** `xtFwSoloEncode` now walks the list from the firmware's head (`0x89ff10c4`) rather than taking the longest chain in the node pool. No wave changed. The pool holds n/160 nodes, so under 160 samples the firmware calls `malloc(0)`. The bump allocator gave back the same block twice, which faulted, so `malloc(0)` now gives a block. The XT's own `malloc` is behind an OS pointer (`0x8987cab4`) and if it returns null there, the encode fails instead: not checked.

**The check.** `xt_enc <rom> cmp <file[:2]>` (`XT_ENCODER_MAIN`), one line a wave, one `fcheck` section a wave, 105 waves in about 4 s of wall time: the five inputs and `gated.bin`, and `make_inputs.py`'s `enc_*`, in plain Python: loud sines and saws, the waves that first showed this, each input gated at 3, 7, 17 and 47 Hz, quiet, clipped, reversed and padded, low notes, fast falls, tones fading on a constant, lengths from 2 to 14000, silence, a constant, clicks, noise, and 12 mixes from a seed. The baseline gained those lines and changed none. `enc_vocal_g47` is the firmware-fault wave.

`mutate_enc.py` runs the planted errors in `enc_muts.txt` through `PORT=enc mutate.sh`, about 2.5 minutes. 23 of 23 caught, on 1 to 25 waves each. Each cause above has at least one: `silent-wrap` (the original bug) 22 waves, `cross-a` 19, `fill-limit` 4, `lag-above` 9, `scratch-step` 3, `end-mark` 3, `short-lag` 14, `no-list` 6, `fir-order` 3, `drift-stale` 2, `drift-head` 2, `drift-nan` 1. One that is exact and left out of the list: `a` not clamped to `b` (`b` then only meets it in a comparison that gives the same answer).

**A/B.** `verify.sh` is 4/4 identical against the merge below this, and PluginTest 4/4: of those renders only `vocal` and `w358` are SOLO, and their records did not change. Where the records changed, SOLO renders do, as intended (XtSynth CLI, two notes, against v111):

| wave | against v111 | why |
|---|---|---|
| 0.7 sine at 440 Hz | −1.3 dB, RMS 0.64 to 0.50 | no longer unvoiced from the start |
| 0.7 saw at 440 Hz | −0.3 dB, RMS 0.47 to 0.44 | the same |
| vocal gated at 3 Hz | −30.0 dB, from frame 28,818 | the fills |
| `drum` at 48 kHz, `chord` | identical | |

**The anchor.** With these records a backward SOLO note reaches v111's overflow without Robot Voice. Through the XtSynth CLI, this encoder with v109's `XtSoloVoice`, Time Offset BWD, 200,000 frames: `gated` hangs at note 96 (×8) and note 120 (×32), and was killed after 25 s. With `wrapAnchor` both finish in a second. `enc_vocal_g3` and `enc_drum_g17` finish on either build. So v111 is needed here, and it is in this version. v111's table describes the records before this version.

**`xt_tone`.** Seeds 90210 and 2007 at 6000 cases: no line with a difference. `mutate_tone.py`: 144 of 144 caught, as the list needs re-running when a port changes.

**What this leaves.**
- **The firmware's stereo mode** (done in v113, "The SOLO encoder's stereo mode"). This check, like the numbers it set out to explain, gives the firmware `(L+R)>>1` in mono mode, as XtSynth gives the port. The XT encodes a stereo wave in mode 3 (`0x8951e120(3)`), which is not the same: the full-rate samples are that floor, but the filter reads `0.5·(L+R)` in float (`0x8951f5e0`, and `0x8951f740` on its fast path, which filters each channel and then averages), and the crossing scan truncates that float where the mix floors. On the project's 89 stereo waves, mode 3 and the port on the mix differ on 45; `drum` 188 of 609 from record 420 (713 against 717). It needs a stereo path through `xtSoloEncode` and XtSynth.
- The events: the port takes only {n·16}, as `xtFwSoloEncode` does.
- Below the buffer (`0x89ff10c4` and down) only what a scan reached in these waves is modelled. Above it in `0x8a0150f4`, a read past the scratch, reads 0 here and rom.2's variables there; none was seen.

### The anchor's overflow: a SOLO note that never came back (2026-11-03, v111)

`XtSoloVoice::render` could fail to return. Its backward repeat ran in `double` where the firmware's runs in int32 and ends by overflowing, so the plugin's audio call would hang on such a note. Checking the fix over a long render then showed a second int32 the port did not have, the grain timer. Both now behave as the firmware's.

**The loop** (`0x8950d01a`, `anchorBackward`). Under the record's start, with the position still at or above the start and budget left, it does `r1 = anchor − start; r3 = end; do { r1 += r3; more = start ≥ r1; r3 −= start; } while (more)`.
- r3 falls by the start each round. With a start above the length, r1 reaches at most anchor + 2·length and then falls.
- So an anchor two lengths or more under the start never gets above it. The firmware's r1 runs down until it overflows to near +2^31, which ends the loop. In `double` it falls for ever.
- Example, `gated.bin` at ×32, walk −1. At frame 10,440 the record is 19,558…19,561 and the anchor 19,514.875. It takes 119 rounds and lands at 133,446,453.875 samples.
- The record then holds until the anchor comes back at one step per sample, at frame 4,180,031 here, about 94 seconds later.

The forward loop (`0x8950cf4a`) has its r3 negative from the second round, so it always ends, and it does not overflow. It stays as it was.

**What reaches it.** The step is the record's rate: at most 32 samples without Robot Voice, since the pitch stops at ×32. With Robot Voice it follows the period, up to the next record's length × pitch ÷ 100, so a voiced record next to a long one can step far past its own length.

| records | settings | before v111 |
|---|---|---|
| the plugin's (XtSynth CLI), 11 waves with such a pair, from 454 | Robot Voice, Time Offset BWD, ×32 | 7 hung |
| | ×28.5 | 6 hung |
| | ×8 or lower | none (×8 to ×28.5 not tried) |
| the plugin's, 466 waves | no Robot Voice | not reached: shortest record 18.6 samples, so two lengths is over 32 |
| the firmware's encoder (`xt_fw solocmp`) | no Robot Voice, `gated.bin` ×8 walk −0.5, ×32 walk −1; another gated tone at ×4 | hung |

The stack in a hung note was `XtSynth::render`, `mixBlock`, `XtSoloVoice::render`, `anchorBackward`. The firmware finishes the `solocmp` cases and the `xt_tone` notes below (the CLI runs have no firmware side). None of the checks' five inputs can reach it: their shortest record is 43.8 samples, and with Robot Voice at ×32 the worst step is 0.34 of two lengths.

**The two encoders differ** (until v112, "The SOLO encoder against the firmware's"). That is why the table has two kinds of records. The firmware's encoder ends a voiced run with records as short as 1/16 sample; `xtSoloEncode` has not been seen to. They were never compared: `solocmp` gives both sides the firmware's records and `xt_tone` both sides the port's. On 413 generated waves 73 came out the same; of the five inputs (stereo as (L+R)>>1) `drum` differs, 188 of 609 records. So "its records match" in the section on the interpreter holds for what it was checked on, not in general. If the port's records are made equal, this path opens to ordinary pitches without Robot Voice, and the fix here already covers that.

**The timer** (`0x8950d080`, grain +0xc). It is an int32 in 1/4096 samples: +0x1000 each sample, minus the period at a trigger, and one trigger a sample at most.
- With a period under a sample (a record under 32 samples at ×32) it gains on every trigger and overflows, about 17 seconds in.
- It is then negative, so no trigger comes for 524,288 samples, about 12 seconds.
- The rate here is 0x1000 − 0x500 a sample, so the first return should be at 10,440 + 762,601 + 524,288 = 1,297,329. The firmware's first burst is at 1,297,328, and the next is two cycles on.
- The port's `double` timer kept triggering. That was −25.9 dB against the firmware on the long case, the only difference found after the anchor's fix. I did not look into why only every second return makes a sound.

The grains' ages (+0x10, +0x14) are int32 too and stay `double` in the port. They wrap only after 524,288 samples without a trigger, and a grain's window has closed long before that (at most 32,768 samples), so it can't be heard. That is reasoning; no case plants it.

**The fix.** `wrapAnchor` on every move of the anchor except the forward loop, and the timer's wrap. Values in range are returned untouched, so nothing changes until an overflow: A/B against the merge below it is 4/4 identical, PluginTest 4/4 bit-identical.

**The checks.**
- `make_inputs.py` generates `gated.bin`, a gated tone with vibrato, and `fcheck` gains five `solocmp` cases on it: backward at ×8, ×32 turned forward after the overflow, Robot Voice at ×32 backward, and Robot Voice forward then backward.
- The fifth is 4.3 million frames: ×32, walk −1, then −0.001 from frame 10,464. The slow walk keeps grains firing until the anchor comes back, so its landing is heard. Without that, the anchor returns unseen or not at all, and a wrong landing passes.
- All five are at the usual SOLO level, −105.9 to −117.9 dB. A `solocmp` that hangs now dies by `alarm(120)` and fails the check.
- `mutate.sh` gains `PORT=solo`: XtSoloVoice.cpp against the baseline's `solocmp` cases, one verdict a case.

| planted | result |
|---|---|
| nothing (a control) | 12 same |
| the loop in `double` | the 5 new cases hang, and fail by the alarm in 123 s |
| the landing 32 samples off, only when far | long case 0.0934, −38.4 dB |
| the landing 256 samples off | 0.105, −32.2 dB |
| the round 2^27, not 2^28 | 0.46, −23.2 dB |
| the timer not wrapped | 0.317, −25.9 dB |
| the timer wrapping at half its range | 0.317, −25.9 dB |
| the timer's round 1 sample off | 0.511, −24.0 dB |
| r3 not wrapped; `>` for `≥` in the loop; the first r1, either step or either skip not wrapped | 12 same: not reached by these cases |

For that last row, one run outside the repo compared the wrapped arithmetic with the firmware's int32 instructions directly, on about 10 million random records and anchors, 7 million of them past 3 rounds: 0 different in the backward loop, the forward loop, and steps and skips at the edge. It left out 33,730 cases that run over 2 million rounds in int32 too (a very small start with a very far anchor). The real settings searched took at most 153 rounds.

**Through the whole tone.** `xt_tone` at 1500 cases with the waves that hung in place of its two SOLO waves, 7 waves, 2 seeds:
- The old build did not finish in 280 s on 3 runs (`f1000` on both seeds, `f2000` on seed 7). The new one finishes them in about 30 s, 0 different.
- The other runs are the same on both builds. Some have differences, on SOLO waves about 3% off in a buffer: `f5000`, a 5 kHz gated tone, 15 and 8; a click train 2. Not traced.

Seeds 90210 and 2007 at 6000 cases: 0 different, and seed 90210's counts are as in v108. `verify.sh`: fcheck as the new baseline (five lines added), A/B and PluginTest identical, engine tests, auval.

**What this leaves.**
- `xtSoloEncode` against the firmware's encoder, above. That is its own piece of work (v112).
- The band player's `stepBack` still relies on `int` wrapping (v108).
- The SOLO cases past the tolerance stay open, these and seed 3 case 9455, seed 4 case 529, seed 2007 case 9186.

### `xt_analog` past the far band reads, and the Alternate loop (2026-11-03, v110)

`xt_analog`'s wave pass stopped comparing a note at 3× or above going backward, or in an Alternate loop. It no longer does. It gives far band reads zeros, as `xt_tone` has since v108, and every note now runs to its end: **0 different**, on eight seeds. This changes the check and no audio.

**The change.**
- `xtFwHookBandReads` (XtFirmware) is v108's hook, moved out of `XtToneCheck.cpp` and now shared by both checks. `XtFwBandReads` holds the bands and the count, and `kXtFwWavePad` is the 256 KB of zeros. `xt_tone`'s results are the same line for line.
- The stop is gone from `XT_ANALOG_MAIN`. A read that leaves memory by another path would still end its note and be counted, and none does.
- The pass's wave layout rounded each gap down, so a gap could be up to 4 KB under the pad. The hook lands exactly one pad under a band, so a short gap could have put it on the piece before. The gaps now round up.
- `XT_ANALOG_SEED` sets another seed. `mutate.sh` gains `PORT=analogband`, `PORT=analogfw` and `PORT=tonefw`, because `PORT=analog` plants only in `XtAnalog.cpp`, and neither the band player nor the hook is there.

**Results.** `XT_ANALOG_ONLY=pcm` unless it says fcheck.

| | Note-ons and updates | Blocks | Left off | Far reads | Different |
|---|---|---|---|---|---|
| fcheck, 3,000 cases, before | 57,154 | 133,531 | 580 | — | 0 |
| fcheck, 3,000 cases, now | 68,547 | 162,930 | 0 | 253,346 | 0 |
| Wide, 21,000 cases, before | 409,838 | 959,388 | 4,186 | — | 0 |
| Wide, 21,000 cases, now | 480,297 | 1,141,680 | 0 | 2,347,470 | 0 |

- Seven more seeds at 21,000 cases (1, 2, 3, 7, 2024, 31337, 90210): each 480,297 and 1,141,680, 0 left off, about 2.1 to 2.4 million far reads, 0 different. Those two counts are the same on every seed because no note is cut any more.
- A SOLO player's largest difference over the eight seeds is 6.93e−5 of full scale, against the limit of 2e−4. In fcheck it goes from 8.31e−6 to 2.5e−5, still with 0 samples past the limit. SOLO has no far-read hook, and its limit is unchanged.
- This pass has a mono BACKING wave, which `xt_tone` does not. So the mono kernel's two read groups, which v108 could not exercise, are reached and tested here (below).

**The Alternate loop is the same thing.** The old comment, "an Alternate loop, which comes back", was about the playing direction and not about the reads. A traced build on seed 12345 at 21,000 cases, hooking the repeat at `0x8950f50a` and all eight read groups:
- Crossing the loop's end in Alternate sets `flags3 |= 8` (`crossForward`), and the block's start then negates the walk step. So the stepper runs through `stepBack` with the walk knob positive, and reaches the same repeat.
- In an Alternate loop, 70 notes ran away with the walk never negative, and 34 more with it positive at the time after going negative earlier. Outside Alternate, all 1,202 notes with a run-away had the walk negative at the time. So the old rule's two conditions, walk negative or Alternate, cover every run-away in this run.
- An example, case 523, block 10: stereo BACKING on OSC 2, ratio 7.611, walk +4.952, loop 271…1426, `flags3` 0x28, walk step −166,168,032. The position is 22,835 under the record 23,707…23,971, which is 264 long and starts far above its length, as in v108. It goes to 2,138,927,766 in 428 rounds.
- All 2,347,470 far reads follow a run-away in the same band. None comes from a repeat that ended normally, or from a band with no repeat yet.
- **A far position does come back, but only through a loop jump, which Alternate never makes.** It came back 1,283 times, all after a jump (`0x8950f710`) and all on forward loops. Alternate only sets the flag (`if (lmode1 == 2) { flags3 |= 8; return; }`), so there its far position stays far until the note ends.
- No run-away came before the old stop would have fired on its note, so the stop did cover them in this pass. The lowest ratio at any run-away was 2.905, on a note whose stop had already fired while the pitch was higher.
- ENSEMBLE's upper bands are not in this. They read frames by the walk position (`ensembleBand`), not by the read position, so only its band 0 goes far.

**Planted errors.** Each ran at 3,000 cases, with the check as it stood and as it is now.

| Error | Before | Now |
|---|---|---|
| `band-repeat-meant`: the repeat as the firmware meant it, adding the record's length | **0** | 10,390 different |
| `band-repeat-clamp`: a far result put back at the record's start | **0** | 10,357 |
| `band-far-1`: the port reads 1 instead of 0 only 300,000 frames or more outside a band | **0** | 8,743 |
| `band-outside-1`: the port reads 1 anywhere outside a band | 16,251 | 26,620 |
| `band-back-tiny`: no tiny gain at the wave's start | 8,252 | 11,795 |
| `band-back-prev`: the wrong sign loading the previous record | 41,187 | 46,794 |
| `band-turn-forward`: the turn lands on the record's start | 22,737 | 35,038 |
| `band-alt-flag`: Alternate does not set the bounce | 7,302 | 6,206 |
| `band-bounce-huge`: the bounce's start test always repeats | 5,102 | 4,210 |

The first three are the point: errors in what follows a run-away, which this check could not see at all. They used to be visible only through `xt_tone` (v108).

Two more stayed at 0 in both, and neither is a gap in the far reads:
- `band-bounce-start`, the bounce's start test taking the wave's start in place of 0, is exact in this pass: the wave's start was 0 at all 3.3 million steps looked at. `band-bounce-huge` shows the branch is reached.
- `mod-level-tie`, `>=` for `>` in ENV RING's choice of time, needs two equal levels with the follower not yet at 0. At 21,000 cases that is 309 blocks, all with a SOLO OSC, where the MOD's state isn't compared. In the exact notes the follower is 0 at every tie.

The check's own parts, now only:

| Error | Result |
|---|---|
| `no-hook` (`PORT=analog`) and `far-reads-kept` (`PORT=analogfw`): nothing moved | 16 blocks different, 187 notes left off. Those 16 come only from reads the hook moves |
| `pad-short` (`PORT=analog`): 4 KB between pieces | 2,972 different, 172 left off |
| `bands-unlisted` (`PORT=analog`): bands 1…5 not listed | 61,361 different |
| `far-reads-all`: every band read gets zeros | 108,196 different. The compare is alive on real samples |
| `far-reads-wide`: the hook's bound doubled | Passes with the same counts, and is exact, as in v108 |
| Stereo band 0's two out of the list | 33 different, 8 left off |
| Mono band 0's two | 44 left off, 0 different |
| The stereo kernel's two | 89 left off, 0 different |
| **The mono kernel's two** | **80 left off, 0 different. v108 could not reach these; they are needed** |

`far-reads-kept` through `PORT=tonefw`, seed 90210 at 6000 cases, gives v108's original failure through the moved hook: 226 + 385 notes left off, 3 different.

`verify.sh`: fcheck as the new baseline, whose only changes are `xt_analog`'s two `pcm tone` lines, A/B 4/4 identical, PluginTest 4/4 bit-identical, engine tests, auval. `mutate_tone.py` at 2000 cases, the whole list of 144: all caught.

**What this leaves.**
- The limit is v108's: on the XT these reads get whatever is in its RAM, and that can't be compared. It also means an Alternate loop on a BACKING or ENSEMBLE wave, at about 3× and above with the walk positive, should reach the same reads on the hardware. Nobody has listened for that on the unit.
- `xt_fw bandcmp` has no hook and still faults on these notes. fcheck's cases don't reach them. Not changed.

### A band read far from its wave: seed 90210, case 9398 (2026-11-03, v108)

`xt_tone` on seed 90210 at 6000 cases gave 3 different, all in case 9398 and all in block 62: `osc 2 buffer`, `cosm output` and `output`, from word 24, fw −22236.084, port −18823.516. v106 left this open as possibly real. **It is not a fault in XtSynth or a port.** The firmware's band 0 read past the zeros `xt_tone` lays around a wave and landed on the wave's own vpdt, where the port reads 0. So this version changes the check and no audio. COSM 2's Side Band Filter, the words past OSC 2's buffer and the forced case only set the case up and carry the difference: `mixPast` is equal on both sides up to that block.

**The chain, each step run.**
1. Stereo BACKING on OSC 2 (`drum.bin`), key 96. During block 61 the walk turns backward, and band 0's position falls 947 (1/16 sample) per list entry.
2. The stepper's repeat (`0x8950f50a`, `stepBack` in the port) starts from position 47,390, under the record 49,551…49,944, which is 393 long. It does `r1 -= start; do { r1 += r3; more = start >= r1; r3 -= start; } while (more)`, r3 starting at the record's end. The first round adds the end, the second the length. Each round after that adds the start less than the one before. So with a start larger than the length, a position two lengths or more under the start never comes back: 785 under this record's start is fine, 786 is not. r1 falls until it overflows and wraps, here after 296 rounds, to 2,146,351,899. Run by hand from those three numbers, the loop gives exactly that.
3. The port's loop is the same. All 184 read positions of blocks 58…63, logged on both sides, are equal, the far ones too. So the positions were never the difference.
4. That position is 2^31 − 1,131,749. Stereo band 0 reads 4 bytes per frame, so the read is at `0xac67cec4`, in P2, the same memory as `0x8c67cec4`. That is 282,937 bytes below the band. The zeros reach 262,144. What lies there is this wave's vpdt, whose words (`0x00990400`…) the firmware takes as samples. The port reads 0 (`smp`, past the band's end).
5. The last two reads of block 61 come out of the tree at word 24 of block 62. In block 63 band 1's own position has overflowed too. Near 2^31 that is +128 MB for it, `0x9471a224`, which is not mapped, so the note is left off. That is why only one block differed.

**Why it is so rare.** Before this change, on this seed at 6000 cases, 626 cases made 4,203 far reads, and only two of those cases landed on anything but zeros. The other is 2499, where the read lands in block 41 and the note leaves memory in block 42, before the read reaches the output.

**Why not more zeros.** Where r1 lands depends on the record's start, in no simple way. Run by hand over starts from 2,000 to 4,000,000, it lands from 160 KB to 23 MB under a stereo band 0. The upper bands go elsewhere: position >> shift >> 6 words puts band 1 at +128 MB and, by the same arithmetic, this wave's band 5 at +8 MB, among the COSMs' buffers. No layout of `xt_tone`'s memory keeps all that clear. Leaving a note off at its first far read also clears case 9398. I ran that as well: 652 notes left off, against 611 before and none now.

**The change, in `xt_tone`.**
- `Firmware::hookBandReads` (since v110 `xtFwHookBandReads`, in XtFirmware) hooks the band players' eight groups of reads (`0x8950e980` and `0x8950eb60`, mono and stereo, twice each). If `r7` is further than `kWavePad` from the band's data, measured in 32 bits so that an alias does not count as near, it moves `r7` into the zeros. `r7` is loaded again before each group, so nothing else sees this. Only the firmware's memory changes: its position, stepping and gains run on as they were.
- Each part's summary now counts these reads, so the baseline shows the path is used.
- `Sh4::call`: a hook that leaves `pc` alone is now followed by the instruction. Before, it was called again forever. No hook relied on that.
- `mutate.sh`: `PORT=toneband`, `PORT=tonecheck`, `SEED=`.

**Results.** So a band read outside a wave now means the same on both sides, however far. The notes that used to be cut at their first unmapped read are compared to their end.

| Seed 90210, 6000 cases | Notes | Blocks | Checks | Left off | Far reads | Different |
|---|---|---|---|---|---|---|
| First part, on v106 | 8,864 | 675,278 | 14,797,712 | 226 | — | 0 |
| First part, now | 8,918 | 689,015 | 15,327,654 | 0 | 1,125,256 | 0 |
| A wave on OSC 2, on v106 | 4,120 | 289,565 | 5,560,791 | 385 | — | 3 |
| A wave on OSC 2, now | 4,210 | 330,952 | 6,371,019 | 0 | 1,503,864 | 0 |

This was found and first run on v106, then moved onto master with v105 and v107, and everything here was run again there. Notes, blocks, far reads and differences came out the same; the checks also count v107's new compares. v107's section makes no far reads, and is still 0 different.

- Nineteen seeds, each part: no note left off. 2024, 2007, 3, 4 and 90210 at 6000 cases; 777, 31337, 9, 4242, 1007, 1, 2, 5, 11, 2025, 65535, 123456, 8086 and 40000 at 3000. On 17 of them, 0 different. So every note plays out the same on both sides to its end, over millions of far reads.
- The other two are SOLO, past its tolerance, and new only because the draws moved (a note no longer cut plays out its later draws). Seed 1007 case 4888: stereo SOLO on OSC 2, COSMs off, `osc 2 buffer` in one block, 2.9e−2 of the peak. Seed 40000 case 4953: stereo SOLO on OSC 1 into a 1st Side Band, `cosm output` in 6 blocks, 2.4e−1 of the peak. Both are the family v104 and v106 describe. I did not trace them.
- fcheck's baseline against master: only `xt_tone`'s count and SOLO lines change, in all three sections. Still 0 different.

**Seed 3 case 9455, seed 4 case 529 and seed 2007 case 9186 are not this.** Counting on the old behaviour, none of them makes a far read that lands on anything but zeros. All three play SOLO waves (9186's is mono, on OSC 2, which `XT_TONE_DUMP` does not print). Those seeds now print 0 different, but only because the draws moved: those cases are no longer played. They stay open. SOLO's own reads are not among the eight, and I did not watch them.

**Planted errors.**

`mutate.sh` has new `PORT=toneband` and `PORT=tonecheck`, and `SEED=`. Fcheck's seed at 2000 cases unless it says otherwise. "a + b" is the two parts.

| Error | Result |
|---|---|
| `far-reads-kept`: the hook moves nothing (seed 90210, 6000) | The original failure to the digit: case 9398, block 62, fw −22236.084, port −18823.5156, 3 different, 226 + 385 notes left off |
| `far-reads-all`: every band read gets zeros (300 cases) | 13,179 + 17,880 different. The compare is alive on real samples |
| One read group out of the list: stereo band 0's two | 512 + 183 and 512 + 210 different, 20 + 42 notes left off |
| Mono band 0's two (ENSEMBLE) | 38 + 43 notes left off, against 0 in the baseline. 0 different, as these reads are unmapped |
| The stereo kernel's two | 33 + 83 notes left off, likewise |
| The mono kernel's two | Nothing changes. `xt_tone` has no mono BACKING wave, so these two hooks are not reached (v110: `xt_analog` reaches them, and they are needed) |
| `far-reads-wide`: the hook's bound doubled (seed 90210, 6000) | Passes, same counts, and it's exact. A far read is about 2^29 off by its arithmetic, and none lies between one pad and two |
| `band-repeat-meant`: the port's loop as the firmware meant it, adding the record's length | 14,911 + 24,573 different; on v106, 71 + 135 |
| `band-far-1`: the port reads 1 instead of 0 only 300,000 frames or more outside a band | 14,907 + 22,328 different; on v106, 71 + 70 |
| `band-outside-1`: the port reads 1 outside a band | 33,902 + 40,125 different |

The two with v106's numbers are the point: an error in what follows a far read used to show only until the note was cut.

`mutate_tone.py` at 2000 cases, the whole list of 144: all caught.

`verify.sh` against master: fcheck as the new baseline, A/B 4/4 identical, PluginTest 4/4 bit-identical, engine tests, auval.

**What this leaves.**
- On the XT these reads get whatever is in its RAM, and XtSynth's get silence. That can't be compared, and it is the limit of this check: the firmware's note is compared with that one question taken out. I don't know what the XT does at the addresses outside its RAM, such as `0x9471a224`.
- The port's loop relies on `int` wrapping, which C++ does not promise. Clang wraps here, at `-O2` and with the player at `-O3` (fcheck's seed: same counts, 0 different). Not changed.
- `XtSoloVoice::anchorBackward` has the same loop in `double`, where nothing overflows, so two record lengths under the start would never end. No run here hung, but it can be reached: v111, "The anchor's overflow".
- `xt_analog` still stops comparing a note at 3× or above backward. The same hook could lift that. Not done. (Done in v110.)

### The COSMs' held levels (2026-11-03, v107)

This fixes the difference v105 found: a second update inside one tick. A COSM's new block took the envelopes' live output, where the firmware takes the levels from its last tick.

**The firmware.** Two routines write a COSM's +0x104…+0x110, from the bound envelopes' +0x4c and LFOs' +0x38: the note-on (`0x894edf70`, just before it calls the block copy `0x894ee4a0`) and the 10 ms update (`0x894ee6a0`). A new block reads those words (`0x894ed8f0` for the TVF). So it gets the levels as of the last tick, however many updates come first.

**XtSynth.** `CosmVoice` holds `heldA`, `heldB` and `heldLfo`. `hold()` takes them at the note-on and just before the update, and `cosmSetBlock` reads nothing else. An LFO's output only changes in its tick, so holding it changes nothing; it is held because the firmware does, and so that the block reads nothing live. The block still comes before the envelopes in `setControls` and `legatoNote`, the firmware's order, but that order now has no effect.

**The check, two additions.**
- The held words themselves, at every update and for every type, by what the binding put in each slot (`cosm N held level`). They are equal, so no other writer of those words shows up, which v105 could not say.
- A last section of 48 cases, `tone, a second update in a tick`: each of the sixteen types on COSM 1, with COSM 2 seven types on, times three second updates. A Sustain moves in the sustain stage at block 60, followed straight away by a legato, the same knob again, or the bender. v105 ran only the legato; the other two were by reading.
- The section's knobs are pinned to the middle. With them drawn at random, 14 of the 48 cases showed nothing in the audio. I did not work out why for each; a target at its limit and a silent output are the likely causes. That matters because an error in one type's `setBlock` line leaves the held words right, so only the audio can catch it.
- The section comes last, so no earlier draw moves. The baseline's diff is two counts of checks and three new lines; notes and blocks are unchanged.

**Results.**
- fcheck's seed: 0 different in all three sections.
- Seeds 1, 2, 3, 4, 5007 and 90210 at 6000 cases: the same differences as v105's build, apart from the new section, 0 different on all six. The four cases v105 listed on seeds 3, 4 and 90210 are still there on this version alone. They have another cause, and one of them is v106's; see below.
- The old behaviour planted (`held-live`): 0 and 0 different in the two earlier sections, 3541 in the new one. So only this section sees it.

**Planted errors.** `live-order` is exact now, as v105 predicted, 0 different, so it is gone. `cosm-tvf-env` follows the new names. Fourteen are new, all caught at 2000 cases:

| Planted | Different |
|---|---|
| `held-live`, the block on the live levels | 3541 |
| `held-note-on`, not taken at the note-on | 112,233 |
| `held-tick`, not taken at the update | 147,621 |
| `held-tvf`, `-dual`, `-tb`, `-comb`, `-sbf`, `-res`, `-fs`, `-lofi`, `-ws`, `-od`, `-amp`: one type's line on the live levels | 240 to 962 |

`mutate_tone.py`, the whole list of 142: all caught.

**What it changes in sound.** Only a second update inside one tick after a Sustain moved in the sustain stage, and then a block's target by up to 10 ms. In the plugin that takes a host buffer under 441 samples, since `setControls` runs once per buffer. `verify.sh` green: fcheck as the new baseline, A/B 4/4 identical against v105, PluginTest 4/4 bit-identical, engine tests, auval.

**Together with v106.** v105 and v107 were written beside v106 and merged with it after.
- `xt_tone` merged cleanly. The baseline was regenerated: notes and blocks unchanged, and the counts of checks are v107's plus v106's new compare (+1,994, +293, and +48 in the new section), so nothing else moved.
- fcheck's seed: 0 different in all three sections. `mutate_tone.py`, the whole list of 144: all caught. `verify.sh` green, A/B 4/4 identical against v107.
- At 6000 cases: seeds 1, 2 and 5007 are 0 different. Seed 90210's case 2055 is gone, as v106 reported, and its case 9398 remains. Seed 3 case 9455 and seed 4 case 529 remain. The new section is 0 different on all of these and on seed 2007.
- Seed 2007's case 3066 is gone. At 6000 cases that seed has one more, past the 3000 v106 ran: case 9186, 2 different, block 58, `cosm output` fw −18281.9141, port −33263.25, then `output`. The values are the same on v106 alone, so neither this version nor the merge causes it. Not traced. (v108: it plays a mono SOLO wave on OSC 2, past the tolerance. `XT_TONE_DUMP` prints only OSC 1's wave.)

### FM's lines between cases: seed 2007 (2026-11-03, v106)

`xt_tone` on seed 2007 gave 140 different, all in one case, 3066: `cosm output` from block 2, fw −648220.562, port −648202.812. **It is not a fault in XtSynth or a port.** A SOLO wave's known small differences stayed in FM's line pool after one case, and another case 99 later, which has no tolerance, read them. So this version changes the check and no audio. The Comb, the Side Band Filter and the second note are not involved, except that COSM 2 carries the difference on and the mix's tap is skipped with a COSM on, which is why it first shows at `cosm output`.

**The chain, each step run.**
1. Case 2967: a SOLO wave on OSC 1 under FM, MONO without Legato. Its second note lands on voice 1, so OSC 1 goes into tone 1's line. `XtSoloVoice` is not bit-exact, so the two sides' words differ: at most 38.5 on a peak of 1.24 million, 3.1e−5, far inside `kSoloTol`, and allowed in that case. The line then stays. Only an FM note-on of tone 1 clears it.
2. Case 3066: analog, Structure 1, FM at Level 77, on voice 0. At block 2, sample 24, OSC 2 is past negative full scale, so d is negative (f = −0.0639), the wrap never fires and the tap runs past the line's end, as v89 says it can. `0x89527bfc` and `0x89527c06` read words 506 and 505, which are words 10 and 9 of tone 1's line. `XtMod::lineAt` reads the same place.
3. Those words are fw −465261.281 and −454445.844, port −465248.656 and −454434.469. With that f they predict a difference of −12.705 in the MOD's output, and the output differs by −12.719 (the outputs themselves are rounded). I took the firmware's samples from inside the routine (a scratch hook at `0x89527c1e`). This is the only far read of the case, one sample in one block. COSM 2's state carries it on, which is the rest of the 140.

**Why it is so rare.** It takes all of these: a SOLO note under FM on one voice, nothing clearing that tone's line since, and later an exact case under FM with OSC 2 past full scale, on the voice below. In all 4,980 cases of seed 2007, 3066 is the only one that reads outside its own line at all. On fcheck's seed 2024 only 1439 does, 518 reads, and its OSC 1 is off, so its lines hold nothing.

**The change, in `xt_tone`, at each case's end.**
- FM's whole pool (`kFmLines`, 48 lines) is compared bit for bit: `fm lines`, a new check. Until now a line was only ever seen through a far read.
- A case is exempt if it played a SOLO wave, for the reason above, or if a note left memory: the firmware's render stops inside that block and XtSynth's finishes it, up to 64 words.
- Then the firmware's pool takes the port's, so each case starts equal. That is `carryFrom`'s reasoning, and only this one thing has to wait for the case's end.

**Those two are the whole list, as far as this goes.** Counted before the carry, cases ending with unequal lines:

| Seed | Cases | SOLO | Left memory | Any other |
|---|---|---|---|---|
| 2007 | 4,980 | 51 | 4 | 0 |
| 2024 (fcheck) | 3,480 | 39 | 3 | 0 |

**Results.**
- Seed 2007: 0 different in both parts. Notes and blocks are as before (4,671 and 363,276 in the first part), so no later draw moved.
- Taking the carry and the new compare out again brings back exactly the original: 140 different, case 3066, the same first values.
- fcheck's baseline: only the two lines of `xt_tone` counts change (+1,994 and +293 checks), for the new compare.
- Sixteen seeds at 3000 cases (2024, 777, 31337, 9, 4242, 1007, 1, 2, 3, 5, 11, 2025, 65535, 123456, 8086, 40000): 0 different in both parts.
- Seed 90210 at 6000 cases: case 2055 had the same cause, 30 different at `cosm output` from block 2, fw 155.534515, port 155.534698. It is gone.

**Planted errors.** `mutate.sh` has a new `PORT=toneanalog`, XtAnalog.cpp under `xt_tone`.

| Error | Result |
|---|---|
| `fm-own-line`: reads kept to the tone's own line | Seed 2007: 176 different, first at case 3066, block 2. So the read that started this is still compared. Seed 2024: 0 different, as it has no such read; `xt_analog`, also in fcheck, catches it with 19,061 in `mod fm` and 125 in `structure tone` |
| `fm-next-line`: a note-on also clears the next tone's line | Seed 2024: `fm lines` alone, 17 + 5 |
| `fm-tone-0`: every voice on tone 0's lines | Seed 2024: 2,227 + 1,034 |
| `fm-lines-cleared`: `allNotesOff` clears the pool | Seed 2024: `fm lines` alone, 99 + 15 |

The last two are in `tone_muts.txt`. `mutate_tone.py` at 2000 cases, the whole list of 131: 130 caught. `legato-order` passes as it did before this work, and is v105's subject.

**Still open, seen while testing, not traced.** At 6000 cases, with this fix and without it, unchanged:
- Seed 90210, case 9398: 3 different, `osc 2 buffer` at block 62, fw −22236.084, port −18823.516. **No SOLO wave**: stereo BACKING on OSC 2, OSC 1 off, Type3, a 2nd Side Band on COSM 2, one of the forced cases. This one may be a real difference. (Traced in v108, it is not: "A band read far from its wave".)
- Seed 3, case 9455 (SOLO on both OSCs, `osc 1 buffer`) and seed 4, case 529 (stereo SOLO on OSC 1 into Wave Shape on COSM 2, `cosm output`): both past the SOLO tolerance. Their parts' worst are 5.5e−2 and 2.6e−2 of the peak. They look like v104's seed 5007, but I did not run its test of them.

**Not covered.**
- What a SOLO note's leftovers do in a later case. The check now starts from the port's words, so such a read compares equal by construction. It is the SOLO tolerance by another route.
- Reads below the pool, as v89 and v100 say. Seed 2024's case 1439 reads 156 words below tone 0's line, which the interpreter holds at 0 (`rd`, after C startup, and 0 different here). On a real XT it is other memory.

### A legato's order, and the COSMs' held levels (2026-11-03, v105)

The planted error `legato-order` moved `cosmSetBlock` after the two envelopes' `update` in `legatoNote`, and `xt_tone` passed it from v99 on. v99 called it exact without showing it. It is exact, and looking into why turned up a real difference next to it, which is not fixed here (v107 fixes it).

**Why the order makes no difference.** On a legato the two touch nothing in common.
- `cosmSetBlock` reads the envelopes' `level()`, the LFO's `out()`, the voice's note and velocity and the knobs, and writes only the section's block and targets.
- `XtEnvelope::update` writes the times, the levels, the scale and the rate, none of which the block reads. It writes the output only in the sustain stage, as `level[3]`.
- `level[3]` comes from the Sustain knob alone, with no key or velocity in it, and a moved knob has already been through `setControls`' update. In the sustain stage the output is already `level[3]`, so that write changes nothing.
- The other two branches of `update` need a negative scale or stage 6. The scale is always 1 here.

**What was measured.**
- Counters round the legato's updates, at fcheck's 2000 cases: 3456 envelope updates (stage 0: 1822, 1: 1559, 2: 72, 3: 3), the level moved by none. Three in the sustain stage is thin, so a forced run (below) put more there: 58 of 2180, again none moved.
- The planted error against the unplanted build, whole output compared: identical on seeds 1, 2, 3, 4, 5007 and 90210 at 6000 cases each, and on 300 forced cases each of a Comb, a TVF and a Resonator.
- The block after the LFO's `update` as well: 0 different at 2000 cases. That `update` does not write the output either.

**In the firmware the order cannot be seen at all.** A COSM's new block (`0x894f5d10` → `0x894ee4a0`, then `0x894ed8f0` for the TVF) takes the levels from the object's own +0x104…+0x110. The 10 ms update `0x894ee6a0` fills those from the envelopes' +0x4c and the LFOs' +0x38. The envelope's update (`0x894f5cc0` → `0x894f0080`) writes the envelope's +0x4c, not those. I read these routines; I did not search for every other writer of +0x104, and the note-on's path to them is unread. The scratch build below is the evidence that the picture is right.

**A correction to v99.** Its list of `0x894dba00` had two labels wrong. `0x894f5be0`, twice, goes to `0x894f2600` on the OSC voices (+0x20, +0x9d4). `0x894f5d10`, twice, goes to `0x894ee4a0` on the COSMs (+0x1388, +0x14ec). v99 called them the COSMs and the TVA. The order it drew from them, COSM blocks before the envelopes, stands. Corrected there and in XtSynth's comment.

**What does matter on a legato.** Only that the block comes after the new key and velocity. `legato-order` now plants that: the block ahead of `v.note = key`. It is caught, 115,606 + 8,289 different at 2000 cases. Dropping the block altogether gives 116,409 + 8,032. Nothing in the list covered a legato's COSM block before.

**Still different from the firmware, not fixed here (see v107): a second update inside one tick.** XtSynth's block reads the envelopes' live `level()`. v99's "block first" stands in for the firmware's held levels, but only for the first update after a tick. Move a Sustain in the sustain stage, and a legato before the next tick gives:
- XtSynth: the block takes the moved level, because the knob's update has already moved the output.
- Firmware: the block takes the held level, until the next tick.

Forced in a scratch copy of the check (Sustain moved at block 40, the legato straight after, envelope depths on, 150 cases), compared with the same scratch build holding the levels:

| COSM | as XtSynth is | levels held at note-on and at the update |
|---|---|---|
| Comb | 1460 different | 0 |
| TVF | 1682 different | 0 |
| Resonator | 1078 different | 0 |

The differences are in the COSM output and the output, and in OSC 1's buffer where the COSM sits on it. They start at the legato's block. The scratch build is also 0 different on fcheck's 2000 cases. The planted error is exact here too: both orders read the same moved level.

`xt_tone` has one knob move per case and no forced case for this, so fcheck's seed never gets there. By reading, two knob moves in a tick or a knob and then the bender take the same path; only the legato was run. In the plugin `setControls` runs once per host block, so buffers under 441 samples can do it. The block's target moves up to one tick (10 ms) early.

Fixing it changes XtSynth, and it also makes `live-order` exact, since nothing the block reads would then depend on the order. That entry would become "the block reads the live level". So the fix and its forced case are left as their own version. The case cannot go into `xt_tone` before the fix, because fcheck would no longer be 0 different.

**Other seeds.** At 6000 cases, four cases differ from the firmware on planted and unplanted builds alike: seed 3 case 9455 (OSC 1's buffer), seed 4 case 529 (COSM output), seed 90210 cases 2055 and 9398. The scratch build with held levels leaves them as they are, so they have another cause. Not traced here. (Case 2055 was FM's lines: v106.)

**Checks.** No check or result changes: `XtSynth.cpp` changes by one address in a comment and `XtToneCheck.cpp` not at all, so `fcheck_baseline.txt` stands. `mutate_tone.py`, the whole list of 129 at 2000 cases: all caught, no standing miss. `verify.sh` green: fcheck as baseline, A/B 4/4 identical against v104, PluginTest 4/4 bit-identical, engine tests, auval.

### COSM: the stereo routines (2026-11-02, v104)

With a stereo wave on the OSC a section follows, the firmware runs a second set of COSM routines. Until now `XtCosm` had only the mono ones, and `xt_tone` turned those COSMs off. Now all sixteen types have them.

**What picks them.** The chooser `0x894eef70` takes the voice's flag (+0x960, see "The whole tone against XtSynth"). Set, it installs from the table at `0x8987c840` (the image at `0x898582cc`), otherwise from `0x8987c7ec`. `XtCosmSection::stereo` is that flag at note-on: the section's OSC (`xtCosmOsc`) is on, is PCM and holds a stereo wave. So COSM 2 in Type3 goes by OSC 2's wave, and the others by OSC 1's. The chooser also writes a value per code at object +0x14c (`0x897d01b4`, mono `0x897d0144`). The routines don't read it; I did not trace who does.

| Type | Code | Mono | Stereo |
|---|---|---|---|
| OD | 1 | `0x894fef70` | `0x894ff340` |
| DS | 2 | `0x894fc570` | `0x894fc7e0` |
| Wave Shape | 3 | `0x89503ef0` | `0x89504890` → `0x89503f4c`, `0x89504400` |
| Amp Simulator | 4 | `0x894f8a30` | `0x894fa890` → `0x894f8a70` |
| Speaker Simulator | 5 | `0x89501300` | `0x895014c0` |
| Resonator | 6 | `0x894ffa10` | `0x894ffe4c` |
| 1st Side Band | 7 | `0x89500640` | `0x89500940` |
| 2nd Side Band | 8 | `0x89500d40` | `0x89500fc0` |
| Comb | 9 | `0x894fa8d0` | `0x894faa00` |
| Dual Filter | 12 | `0x894fd090` | `0x894fd9f0` → `0x894fd100`, `0x894fd3f0`, `0x894fd6d0` |
| TVF | 13 | `0x89502890` | `0x89503810` → `0x89502a00`…`0x895037a0` |
| Dynamic TVF | 14 | `0x89502920` | `0x895038a0`, the same kernels |
| Compressor | 15 | `0x894fab80` | `0x894fadcc` |
| Limiter | 16 | `0x894fe450` | `0x894fe67e` |
| Frequency Shifter | 18 | `0x894fda60` | `0x894fde84` |
| Lo-Fi | 19 | `0x894fe9c0` | `0x894febe0` |
| TB Filter | 20 | `0x89501950` | `0x89501aa0` |

Through (code 0, `0x894fb720`) is an `rts`. Codes 10, 11 and 17 have stereo routines too, but no converter writes those codes.

**The shape.** Each routine runs L and R apart: the mono routine's arithmetic on each channel, its own states and delay buffers, reading and writing its own channel. Everything per block (slews, targets, a new delay, Compressor and Limiter gains' slews) happens once. The ramps, the coefficients stepped before every fourth sample, the phase, the crossfade and the write index are the same for both. Some routines interleave L and R in a frame, some run 32 frames of L and go back. It makes no difference to the numbers, so the port makes one pass per channel and sets the ramps back for the second (`Chan`, `passes()`).

**Where R lives.** D = object + 0x58.
- States: L's D+0x1c…, R's 0x38 on, D+0x54…. All of them are in what the note-on clears (D+0x1c…0x88), except as below.
- TB Filter: LPF L D+0x2c…0x38, R D+0x4c…0x58; HPF L D+0x68…0x74, R D+0x84…0x90.
- Comb and Side Bands: the DC follower L D+0x34, R D+0x6c; D+0x38 the coefficient for both.
- Buffers: D+8 L, D+0x10 R; the Side Bands' second line D+0xc L, D+0x14 R. The allocator clears all four, as it did.
- Compressor: gain target, gain, envelope L D+0x98, 0x9c, 0xa0; R D+0xcc, 0xd0, 0xd4. The note-on (`0x894ee390`) starts both alike.
- Limiter: gain, smoothed L D+0x90, 0x94; R D+0x98, 0x9c. The note-on (`0x894ee410`) starts all four at 1.

**Where stereo is not two monos.** All kept:
- **Amp Simulator.** `0x894fa890` reads the Type (`mov.b @(4,r5),r0`) and clears it on the next line (`mov #0,r0`), where the mono routine has `shll2 r0`. So a stereo wave always gets Type 0's amp. Its table has all three (`0x894f8a70`, `0x894f9400`, `0x894f9db0`), and the other two are never reached.
- **TVF kernels.** LP24, BP24, HP24, NOTCH12, BP12, HP12, LP6 and HP6 are the mono ones per channel, on the mono's coefficients. The others are not:
  - LP12 negates its input like the rest of the 12-dB kernels. The mono LP12 is the one that keeps the sign, so the default filter comes out inverted against mono.
  - NOTCH24's second stage takes t + b (mono t + a).
  - PEAK24's second stage takes −t + b (mono t − a) and it outputs −(u − e) (mono u − e).
  - PEAK12 outputs −(t − b) (mono t − b).
- **Dynamic TVF.** The stereo routine still calls the mono follower `0x895017e0`: Σ|L| only.
- **Side Band followers** (`0x89501850`; v90 noted this routine and no caller, and these two are its callers). Each of its four rounds reads sixteen words and advances seventeen: the fourth group's `add #4,r4` and the branch's delay slot both step. So it sums words 0…15, 17…32, 34…49 and 51…66, of `lr` as L, R pairs. Words 16, 33 and 50 are never read. **Words 64, 65 and 66 are past the block.** No frame is counted twice, which the mono follower does.
- **2nd Side Band, R's gain.** R's output multiplies by bank 1's fr9, copied once before the loop (`fschg; fmov DR8,XD8`). L's fr9 steps toward the gain's target every fourth sample, and is what's stored. So R uses the gain as the block began, for the whole block. The 1st Side Band reads its gain from memory and has no such difference.
- **TB Filter.** R's second HPF stage is at D+0x8c and D+0x90, past the note-on's clear and past `0x894fbd20`'s, which only clears L's. So it starts from whatever the object's last note left there.

**What's past the block.** Three of the Side Band follower's words depend on which buffer it was given (the render, `0x894f6d40`, `0x894f6e20`, `0x894f6f40`):
- On the mix (`0x89d2ca10`): `0x89d2cb10`, which the voice loop hands to `0x894f4e50` (at `0x894dac9c`). I read it as the output bus, without following that call down to the tones. If so, the three words are its first pairs, zero while every tone's Output Assign is MFX, which is all XtSynth has. In `xt_tone` the bus is elsewhere and those words are zero too.
- On OSC 1's buffer (`0x89d2c810`; COSM 1 in Types 2 and 3): OSC 2's buffer, `0x89d2c910`, its first L, R, L. XtSynth passes `tmp2`.
- On OSC 2's (`0x89d2c910`; COSM 2 in Type3): the mix's, `0x89d2ca10`, as the last tone left it, since this tone's mix comes later. XtSynth keeps those three words between voices and blocks (`mixPast`).

**One object, all the types.** The firmware has one COSM object per section, and every type's fields overlay. So the TB Filter's two uncleared words hold whatever the last type put there. What each type leaves, run on all sixteen with a sentinel in both words:

| Type | D+0x8c | D+0x90 |
|---|---|---|
| Dynamic TVF | — | Q Own, slewed |
| Dual Filter | — | Filter 1's Q at note-on |
| TB Filter | R's HPF | R's HPF |
| Side Bands | gain | HPF or Q, slewed |
| Compressor | attack factor | Sustain |
| Limiter | slope | L's gain |
| Wave Shape, OD/Dist | gain | — |
| Amp Simulator | Level | Treble |
| TVF, Comb, Resonator, Frequency Shifter, Lo-Fi, Speaker | — | — |

`XtCosmSection::wordsNow` is that table, and `holdWords()` takes them at each note-on before the type changes. XtSynth's voices are not the firmware's tones (the tones come from a pool), so `xt_tone`'s `carryFrom` starts the firmware's two words from the port's, as it does with the follower and the MOD. That makes the table invisible to `xt_tone`; `xt_cosm` checks it, against each type's own run. `xt_tone` does the same with `mixPast` at each case's start, since XtSynth's tails go on rendering between cases and the firmware's stop. Within a case those words are compared by their effect.

**Checks.**
- `xt_cosm`, `XT_COSM_MAIN`'s last section. Seventeen routines (OD and DS, both Side Bands, TVF and Dynamic TVF apart), each 1000 notes of 40 blocks through the firmware's own note-on (`0x894edf70`), updates, new blocks (`0x894ee4a0`) and routine: 680,000 blocks, L and R, all states, the buffers every eighth block and the two words at each note's end. All 0 different. The mono lines are unchanged.
- The port starts each note from one that has played, so anything a note-on should clear shows.
- `xt_tone`: both exclusions (a stereo OSC 1 with a COSM on, a stereo OSC 2 with COSM 2 in Type3) and their counterpart in the live update are gone. 2000 cases and the sweep: 3278 notes, 250,114 blocks, 5,581,967 checks, 0 different. A wave on OSC 2, 1000 cases: 1361 notes, 95,031 blocks, 1,805,868 checks, 0 different. 3000 cases on each of seeds 777, 31337, 9, 4242 and 1007, both parts: 0 different.
- The baseline gains the new lines, and three existing lines change, all counts:
  - The first part's checks, from 5,597,162: with a COSM on, the mix's own tap isn't compared, and the stereo cases now have COSMs on.
  - The OSC 2 part's counts (from 1367 notes, 96,381 blocks, 1,870,252 checks) and its SOLO line (from 5.0e-03 of the peak to 1.2e-02, still inside 2%; from 138 notes left off to 144). Lifting the exclusions changes the draws of later cases. I did not trace which draw. The cases left off are all the band reads above, at the same three addresses, none in a COSM.
- Some stereo BACKING cases with OSC 1 on (`g % 10 == 3`) are forced to a Side Band Filter on COSM 1 in Types 2 and 3, with OSC 2 on and Own depths, so that OSC 2's words past the block matter. At random, 2000 cases missed that wiring; 12,000 caught it on all seven seeds tried. Stereo BACKING cases on OSC 2 in Type3 likewise, for COSM 2 and the mix's words. Forcing OSC 1 on as well moved the rest of the run: a wave newly played can leave memory (the band reads, `0x8950e99a`, `0x8950ea00`, `0x8950ec7e`, none in a COSM), which ends a case early and changes the draws after it.
- Some analog cases leave a stereo wave loaded in XtSynth (`g % 4 == 2`), with none in the firmware. Until now every analog case had no wave, so `!v->analog` in the flag was never tested. Nothing else moved: 0 different.
- PluginTest with COSMs on the two stereo inputs: bit-identical to XtSynth. `ab.sh` with a COSM on: the two mono inputs identical to v100, the stereo ones different, as intended.
- Planted errors, 72, all caught. 46 in `xt_cosm`: each difference above, R's states and buffers shared with L's, ramps not set back, R's start values at note-on, each row of the words table. 15 of those again under `xt_tone` (`PORT=tonecosm`, new), one per type, which shows the stereo paths really run there. 11 in XtSynth's wiring, added to `tone_muts.txt`.
- Three of those passed at first, and changed the check. The Limiter's R start: each case began from a fresh port, whose values are what a reset gives, so each now begins from a port that has played. The wiring of OSC 2's words, and `!v->analog`: hence the two kinds of case above.
- `mutate_tone.py`, the whole list of 129: 128 caught. `legato-order` passes, and passes on v103 without this work too (not looked into here; v105 did: it is exact, and the entry is reworked).

**A case left as it is.** 12,000 cases on each of seven more seeds, on v100 and before the forced cases: six 0 different, and seed 5007 has 4 differences in one case (2955). That's a stereo SOLO wave through a Compressor and a 1st Side Band Filter, past the SOLO tolerance of 2% of the block's peak (fw −545112, port −521479). With the firmware's own OSC 1 samples put into XtSynth's SOLO buffer for Structure 0's SOLO cases, and those compared bit for bit with no tolerance, 3100 cases on that seed are 0 different. So the COSMs are exact. `XtSoloVoice`'s small differences (not bit-exact, `xt_fw solocmp`) moved the follower, and with it a delay and a crossfade, as Lo-Fi's quantizer does (which the check replaces on SOLO waves). I did not exclude the Side Band Filters on SOLO, since the seed in fcheck passes and the case costs nothing else.

**A SOLO wave beside a Side Band Filter.** The follower's words past the block carry a SOLO wave's small differences into the buffer beside it: a stereo BACKING OSC 1 with a Side Band COSM 1 next to a SOLO OSC 2, or a stereo BACKING OSC 2 with a Side Band COSM 2 in Type3 after a SOLO OSC 1's mix. Those buffers were compared bit for bit. fcheck's seed has no such case; on the six other seeds tried, 9 cases differed by a few units, and all had this shape. So with a SOLO wave and a Side Band Filter in a case, both OSC buffers take the SOLO tolerance (`soloBeside`).

**Beyond this work.** Seed 2007 has 140 differences in one case (3066), in v103 alone as well: a sweep case, analog and mono, a Comb and a 1st Side Band with FM. Not looked into here. Traced in v106 ("FM's lines between cases").

**Not checked.**
- The bus's first pairs as past-the-block words with an Output Assign other than MFX: XtSynth has none.
### PCM on OSC 2, with its own wave (2026-11-02, v103)

Each OSC now has its own loaded sample and encode, as the XT binds a wave per OSC (IDs
`0x2be`, `0x2f2`). OSC 2 plays SOLO, BACKING and ENSEMBLE through the same players, with
its own Time, Formant, Energy, Robot, Loop, Offset, Original Key and Encode settings.

**A VariPhrase voice is kind 2.** The block's +0 comes from the note-time converter
(`0x894e2d20`): Encode Type 0 gives kind 1, types 1, 3 and 5 kind 2. So the note-on of
everything the port plays is `0x894f3110`, not `0x8950b5b0` as pass 7 and v49 took it;
that one is kind 1's. `0x894f3110` computes the five values, takes one voice update
(`0x8950c1b0`, which moves nothing, the values being at their targets) and calls `0x8950bf70`,
which sets the gain target (+0x154) and the gain (+0x158). This work and `xt_tone` found the
same gain error from different sides; v95 has the fix.

**The wave in the block.** The converter copies the input's +0xac…+0xd8 to the block's
+0x84…+0xb0, which the voice keeps, and the player reaches them through its +0x44:

| Block | What |
|---|---|
| +0x84, +0x88 | the vpdt; the samples (band 0) |
| +0x8c…+0x9c | bands 1…5 |
| +0xa4 | the wave's header: +2 stereo, +8 the rate (44000 gives +0x990 = 1), +0xa the key, +0x18 tune, +0x1c tempo |
| +0xac | the loop record (v55): +4…+0x10 the points, +0x18, +0x19 the modes, +0, +0x1b tuning, +0x14, +0x16 rate offsets |
| +0xb0 | the cached loop events; its +0x2d0 is a rate factor |
| +0xb4 | flags: 0x80 positions in samples, 0x40 stereo, 0x20 no VariPhrase (input +0x60 = 0), 0x10 Robot, 1 loop |
| +0xb8, +0xbc | start and end: samples, or event numbers with 0x80 clear |

- The block's +0x60 is the tempo (`0x894e2d20`); ×10000 it goes to the player's +0x4c,
  against the header's +0x1c at +0x48. The check sets them equal. The port has no tempo. (Since v118 it has, and `xt_tone` draws the header's: "Tempo sync and Time Trip".)
- The Wave Number reaches the wave store (`0x8949e5c0`…) as 2999 + the number, in r4.
- A PCM OSC 2's mask is 0 in every block, so under OSC SYNC it syncs nothing.

**The stereo flag**, a voice's +0x960. The setup (`0x894f21c0`) clears it and sets it from the
wave's header for a PCM voice. Corrects v86: an analog voice never has it, HQ-SQUARE included (`0x89505170`
clears it again, and the check confirms it on every waveform).

| Reads it | Takes | Does |
|---|---|---|
| COSM 1's pick (`0x894eef70`) | OSC 1's | 1: the stereo table; 0: the mono routines |
| COSM 2's pick | OSC 1's; OSC 2's in Type3 (v92) | the same |
| TVA (`0x894f50e0`) | OSC 1's | stores it at +0x4c; nothing reads it |
| MOD note-on (`0x894f1b00`) | both | +0x38: 0 neither, 1 both, 2 OSC 1, 3 OSC 2 |

- So a stereo OSC 2 under a mono OSC 1 (or OSC 1 off) goes through the mono COSM routines,
  which read L and write it to both. With a COSM on its R is lost; with none, L and R reach
  the TVA apart.
- The TVA's render (`0x894f5720`) reads L and R apart, flag or no flag. Its pair at +8, a send
  the port does not have, takes (L + R)/2.
- All the port's COSMs are the mono routines, which already behave so, so this case needed no code.
  The gap is the other one: **a stereo wave on OSC 1 with a COSM on, and a stereo OSC 2 with COSM 2 on
  in Type3,** take the stereo table on the XT and the mono routines here. `xt_tone` leaves those out. (Closed in v104.)
- `XtSynth` now gives the MOD OSC 2's flag too. FM's and ENV RING's routines depend on the two together.

**Where the firmware's reads leave the wave.** Backward, at high pitch, a band player's read
position runs far outside its wave, and on the XT it would read whatever is in memory. Here the
interpreter faults on unmapped memory, in `xt_fw bandcmp` as well:
- BACKING stereo: at 3× the pitch with walk −1, 3.5× at −0.5, 4× at −2, 5× at −4.8. Forward at
  7.5×: 0 different.
- BACKING mono: seen from about 6× (5.8× at walk −3.4). ENSEMBLE mono: seen at 14× and above.
- ENSEMBLE stereo: seen from about 7×, without a fault while the read lands in mapped memory.

The port reads 0 outside a wave. The check in `xt_analog` stops comparing a note at 3× or above
backward, or in an Alternate loop, which comes back. A fifth of its notes end so (4,186 of 21,000), because it draws wide pitches; none faulted first. `xt_tone` lays its waves among zeros and
counts the notes that leave memory altogether. (Since v108 a far read gets zeros too, and no note leaves memory. Since v110 so does `xt_analog`'s, and it no longer stops: the Alternate loop is the same repeat, and "comes back" meant its direction. See "`xt_analog` past the far band reads".)

**Checks.**
- **`xt_tone`, the real XtSynth against the firmware's whole tone**, with new cases after its others: a wave on
  OSC 2, over OSC 1 off, analog or on another wave, with every COSM type, the MOD Types, the three Structures,
  loops, Robot, second notes, legato, truncation and the panel moved during a note. Compared every block as in
  v100: both OSC buffers, the mix, COSM's output, the TVA and the output. In fcheck, 1,000 such cases: 1,367 notes, 96,381 blocks, 1,870,252 checks,
  0 different. Four other seeds, 1,500 cases each: 8,245 notes, 578,634 blocks, 11,176,012 checks, 0 different; 738 of
  those notes were left off when a read left memory.
- Its exceptions: a SOLO wave's buffers and what follows them keep v100's tolerance, as `XtSoloVoice` is not exact;
  the stereo COSM cases above (until v104); Lo-Fi over SOLO; X-MOD over a SOLO OSC 2. A stereo OSC 2 under a mono OSC 1 stays
  in with its COSMs on, so the mono routines' dropped R is compared.
- `xt_analog`, a pass with six encoded waves, the players on their own, with the five values, both buffers, the
  mix and the MOD's state every block, and RING, FM, ENV RING and OSC SYNC over a PCM OSC 2. In fcheck, 3,000 cases: 57,154 note-ons and updates, 133,531 blocks, 0
  different. Wide, 21,000 cases: 409,838 and 959,388, 0 different. (v110, no note cut: 68,547 and 162,930; wide 480,297 and 1,141,680; 0 different.)
- **SOLO is not 0 different and cannot be yet:** its largest difference there is 4.14e−5 of full scale, against
  a limit of 2e−4. Its five values are exact, and so is the firmware's sum of its own buffers.
- The stereo flag at the firmware's own tone note-on (`0x894f5e60`): the six combinations of OSC 1 off, mono or
  stereo with OSC 2 mono or stereo, 0 different. The TVA's flag: 0 words different with it 0 and 1; changing
  R moved no word of the main pair's L and all 32 of its R.
- Deliberate errors. In `xt_analog`, 37: 33 caught. The 4 unnoticed change nothing compared: one moves the loop
  modes on both sides alike, so it is no error, and three change the wave's loop record, which the players do not
  read on this path. In XtSynth through `xt_tone` (`mutate_tone.py`), 14 added on the two waves' paths: all caught.
  The list now stands at 118, and 116 are caught at fcheck's 2,000 cases. The two others: `legato-order`, recorded as
  exact in v100, and `loop-frames`, which moves a loop point by one sample only when the rounding flips, and which 6,000 cases
  catch (560 differences).
- Earlier rounds found real gaps, now closed. Two SOLO gain errors only moved the largest difference, hence the
  limit. Energy's step changes nothing while its target stands still, hence Energy moving. And the analog OSC 1
  skipped HQ-SQUARE: with it in, the check refused a correction I had made to the MOD's flags on the strength
  of v86's note, which was wrong.

**The wiring, through XtSynth's CLI and PluginTest:**
- OSC 1 alone against OSC 2 alone, the same wave and settings (Time, Formant, Energy, Loop, level, amp envelope,
  pitch, LFO, pitch envelope), all six wave types, and Robot on SOLO: bit-identical.
- OSC 2 alone on wave B while OSC 1 holds wave A, against OSC 1 alone on wave B, four pairs: bit-identical.
  No other comparison would notice OSC 2 playing OSC 1's wave.
- Both OSCs on two different waves against each alone, summed: at most 1.64e−7 apart, peak up to 1.9.
- No wave on OSC 1 is the same as OSC 1 off, bit for bit.
- The Structures, by settings that must agree, on three pairs of waves: OSC 2 alone through COSM 2 after the mix
  (Type1) and before it (Type3), and OSC 1 with a silent OSC 2 through COSM 1 after the MOD (Type1) and before it
  (Type2), for ten COSM settings: 60 comparisons, all bit-identical.
- PluginTest at notes 30, 60 and 100, with OSC 2 on BACKING, ENSEMBLE, SOLO, stereo SOLO with knobs and a loop, Robot,
  and alone: 4/4 bit-identical each. Under Type2 and Type3 with a TVF on each COSM, 18 runs: the same.
  All of this with the TVA Switch off, the default since v102, and again on (`TEST_TVA_ON=1`, `XT_TVA_ON=1`): the same results.

**Not checked.**
- Against real hardware. The check is the firmware in the interpreter.
- `xtMbLoopEvents` and `xtFwSoloImage` build the cached loop events: the firmware's own builder is still
  not found (v55), so a loop is compared from those events. `0x894f3110` also copies the wave's loop
  record into the voice, but changing the record's points or modes changed nothing compared: on this path
  the players act on the cached events.
- An OSC Type changed between notes: the firmware's analog note then starts from what the player left
  in the same words of the voice. The port keeps them apart.
- A wave whose header has another rate, tune or tempo (the checks set them to the values that leave the
  pitch alone, as the port assumes). The tempo since v118.
- Two waves that share one record of cached loop events: `xt_tone` gives each OSC its own wave, as the port
  computes a loop from each OSC's knobs.

**Plugin.**
- `XtSynth::setSample(e, osc)`, `cur`, `pending` and `retired` per OSC; a new wave on either stops the
  voices. A voice latches its wave at note-on (`OscVoice::wave`). `waveLoop(s, o)` reads the OSC's own knobs.
- Parameters at the end of the layout: `osc2EncodeType`, `osc2EncodeDepth`, `osc2RootKey`, `osc2Energy`,
  `osc2Robot`, `osc2OscTime`, `osc2OscTimeOffset`, `osc2OscTimeKF`, `osc2OscFormant`, `osc2OscFormantKF`,
  `osc2Loop`, `osc2LoopMode`, `osc2LoopStart`, `osc2LoopEnd`.
- `VSynthEmuProcessor::loadFile(file, osc)`, one encode queue for both, the path saved as
  `loadedFilePath2`. Status lines for OSC 2 start "OSC 2: ".
- Editor: with OSC 2 selected, Load loads into it and the file name shows its wave; the PCM rows, the
  TIME and FORMANT knobs and Robot re-attach (`attachOsc`), so `kXtStripH` does not change.
- `XT_WAVE2`, `XT_WAVE1`, `XT_OSC2`, `XT_LOOP2`, `XT_ROBOT2`, `XT_KEY2`; `TEST_OSC2_WAVE`, `TEST_OSC2_OSC`,
  `TEST_OSC2_LOOP`, `TEST_OSC2_KEY`, `TEST_OSC2_ROBOT`, `TEST_SNAPSHOT_OSC2=pcm`. `XT_ANALOG_ONLY=pcm` runs
  `xt_analog`'s new passes alone.

### The TVA Switch off by default (2026-11-02, v102)

The user chose the XT's default over v101's on. So a new patch has the TVA off: full level from the note's start, no velocity on the level, the amp envelope's knobs doing nothing until the box is ticked. The sound now ends with the note: a note-off ends the voice within one update, about 10 ms, whatever the Release, a COSM's tail or the OSC's own amp envelope would have done. Measured on a held note with the TVA Release at 64: off, the last non-zero frame is 431 after the note-off; on, it rings past 30,000.

`Controls::tvaOn` and the parameter default to false. A session saved before v101 has no such parameter, so it comes up off too, and sounds different from when it was saved. `XT_TVA_ON=1` and `TEST_TVA_ON=1` turn it on. `ab.sh` turns it on unless `XT_TVA_ON=0`, so that its renders still go through the TVA, and it stays 4/4 identical against v101 that way. `xt_tone` sets the switch itself, so its results do not change.

### The TVA Switch (2026-11-02, v101)

ID 0x299 is the first of the TVA's inputs (`0x894eb170` stores it at +4). `xt_tone` showed what it does, with the switch drawn in its cases and moved during notes:

- **Off:** the TVA's level (+0xc) is exactly 1, so no velocity, and envelope 13's times are all 0 and its levels [0, 1, 1, 1, 0]. So the note sounds at full level from its start and the amp envelope's knobs do nothing. A note-off still ends it. The check keeps CC7, CC11 and TVA Level at rest, so whether those still count with the switch off is not known. (v114: CC 7 and 11 count, Level does not.)
- **Moved during a note:** it takes effect at once, on the level and the envelope.
- **The velocity factor is latched at note-on and at a legato** (`0x894ea990`, TVA +0x1c), with the Sens and Curve of that moment. Switching back on returns to that factor and not to one from the knobs as they now are. `Voice::velGain` holds it, and `Voice::gain` is `tvaOn ? velGain : 1`.

**Default.** The XT's descriptor default is off. v101 made the plugin's on (`Controls::tvaOn`, parameter `tvaSwitch`, the TVA box at the left of the amp envelope row), to keep every render as it was. v102 follows the XT.

**Checks.** `p[0x299]` is now `Controls::tvaOn` in `patchOf`, off in a quarter of the cases, and the switch is in the sweep. fcheck's `xt_tone` counts change with the new draw, and it stays 0 different; four other seeds at 3000 cases and the sweep, 0 different. `tone_muts.txt` gains seven errors: 103 of 104 caught, the one missed still the exact `legato-order`. PluginTest with the switch off: 4/4 bit-identical. With the switch on, the A/B renders are identical to v100.

### The whole tone against XtSynth (2026-11-02, v100)

Each port has its own check, but XtSynth, which wires them together from the knobs, had never been compared with a firmware tone. `xt_tone` (`XtToneCheck.cpp`) does that: the firmware from the parameter IDs on, XtSynth on the same `Controls` and notes, compared inside XtSynth's block. v93…v100 are what it found.

**What runs on the firmware's side.** Nothing of its path is set by hand.
- The parameter getter (`0x8947d310`, `0x8947d330`) is hooked and answers from a table made from `Controls`; any other ID gets its default (descriptor default − centre).
- The updaters `0x894dfe10` make the part's tables. The note's start `0x894dc130(note, K, msg, key, [stack] A1, layer)` makes the block (`0x894db740`, `0x894db770`), the input tables (`0x894eb4d0`, which runs each converter) and the tone.
- **The tone's constructor is `0x894f5860(this or 0, id)`.** `0x894f1f50` is only the voice's. `0x894f5860` gives the LFOs their vtables, so no stubs are left. It runs for every note: 8 LFO constructors, each drawing a seed, and the ramps.
- `0x894db830(block, prev, key)` runs the tone's note-on `0x894f5e60`, then `0x894f4f70` starts it (+0xc = 2, by the vtable slot `0x894f6be0`).
- Every 441 frames `0x894f1540` then the update `0x894f75f0(tone, 1)`; every block the render at tone +0x177c. A note-off is `0x894dc270(note)`, with the release velocity at note +0x2f. A sounding tone taking a note is `0x894dc1e0`. Truncation is `0x894d5ba0`; the part's `0x894d4a80` truncates all its notes, released ones too. A knob moved is the updaters and `0x894dc0b0(note)`.
- Hooked: only the OS. Its locks (`0x89301920`, `30`, `50`, `60`, `860`) and clock (`0x89301940`), the voice allocator's lists (`0x894d9230`, `0x894d91a0`), the tone-end callbacks (`0x894db720`, `730`), `0x894796d0`, the COSM section marker `0x894798a0`, the COSM buffer pool (`0x894ec2b0`), and the wave store (`0x8949e5c0`, `0x8949e850` chunks, `0x8949ed70` bands, `0x8949e790`).
- Run once before the first note: `0x894e5010` (the random pitch's seed), `0x894f1820` (the LFOs' seed), `0x894f1400(0x89fa90b0)` (the free LFOs), `0x894e5f80` (system calibration), `0x894fc4c0` (the TB filter's constants), the Truncate Time at `0x89d2a6d0 + 0x1602`, the OS object at `0x898585dc`.

**Things the patch needs for it to sound as XtSynth's does.**
- ID 0x113 Mono/Poly: 0 is MONO, 1 POLY.
- ID 0x299 TVA Switch. Its default is off, which sets envelope 13's times to 0 and its levels to [0, 1, 1, 1, 0]. The harness set it on here; v101 gave XtSynth the control.
- CC7 and CC11 at 127 and CC10 at 64 (controllers at C + 0x84 + cc, C = *N, N = *K), or the TVA's level is 0. The bender is the u16 at C + 0.
- A wave's chunks: 2 header (+1 type, +2 stereo, +8 44000 u16, +0xa original key), 3 steps, 4 vplp (+0x1a = 1 so the Loop switch decides), 5 vpdt, 10 loop events (no loop: W2+0xb8, W2+0x16c = 0x7fffffff). Bands in slots 0, 2…6.

**Where it is compared.** XtSynth has taps behind `XT_TONE_CHECK`: OSC 1, OSC 2, the mix, COSM's output. Every block: the four buffers, the TVA (+0xc level, +0x68, +0x6c gain, +0x60, +0x64 target, +0x58, +0x5c step) and the output, which is the frame's third pair on the bus, Output Assign MFX (+0x10, +0x14 of each 32-byte frame). At every update: the envelopes' outputs and stages, LFOs, pitch terms and ramps, the voices' fields, the free LFOs' phases.

**Tolerance: none, but for SOLO.** Everything is bit for bit in the XT's units, the last stage included, because I changed XtSynth's TVA and output sum to the firmware's arithmetic rather than widen the check (v96). The only tolerance is on a SOLO wave's buffers and what follows them: 2% of the block's peak plus −108 dB of full scale (`kSoloTol`, `kSoloFloor`), since `XtSoloVoice` is not bit-exact (`xt_fw solocmp`). The worst seen is printed; the SOLO wave's envelopes, LFOs, pitch, walk and gains are still bit for bit.

**The result.** fcheck: 2000 cases and the sweep, 3259 notes, 1301 taken by a sounding tone, 400 truncated, 277,762 blocks, 6.2 million checks, 0 different. 3000 cases and the sweep on each of four other seeds (777, 31337, 9, 4242), 0 different.

**The cases.** Random settings over the OSCs, envelopes, LFOs, portamento, velocity and key follow, all sixteen COSM types, all five MOD Types, all three Structure Types, loops; analog OSC 1 and 2, and four waves on OSC 1 (mono and stereo SOLO, stereo BACKING, mono ENSEMBLE). A fifth of the cases each: a second note on a POLY part, on a MONO part with Legato, and without. A third move knobs during a note: half the whole panel with the bender, half one word of `Controls`. Some run past the wave's end. Idle time between cases, so the free LFOs must keep up. After these, **the sweep**: each field in one of XtSynth's lists of what changed, moved alone in a held note, once per envelope stage, with the bender moved alone as one more field. Only there can a field left out of a list not be hidden by another.

**What it found.**
- v93. A note's first pitch included the previous note's glide. The tone is constructed for each note, so its ramps start off (`0x894f4860`). XtSynth kept them on the voice.
- v94. The LFOs at note-on, several things:
  - The seed map starts at 0.7, not 0.3141 (`0x894f1820`, at `0x89f91c48`), in SH-4 arithmetic, and a tone draws eight seeds in order whichever LFOs are used.
  - All of a tone's LFOs start whatever the OSC or COSM Switch.
  - **The scheduler, which earlier notes had not found:** around `0x894f4d10`–`0x894f4d9c`. It computes n from the frames passed (× 0.000726 / 0.01), ticks `0x894f1540(0x89fa90b0, n)`, 384 free-LFO objects of 0x38 bytes at pool + 0x3040 (tick `0x894f09b0`, constructor `0x894f0950`, 1 Hz, phase 0), then updates each tone. The free LFOs run with nothing sounding, and their rate is set only at a key-unsynced note-on (`0x894f0a30`), not by the knob. XtSynth's took the phase after a first output, followed the knob and stopped without a sample. `XtLfoFree` is now just rate and phase.
  - A key-unsynced LFO with a tone of the same part sounding copies the newest one (`0x894f0db0`: phase, last value, hold, chaos state). Only with none does it take the free object (`0x894f1590`, `0x894f0e60`).
  - The fade decision at note-off reads the mode latched at the trigger (+0x2c), so a live edit does not change it.
- v95. A PCM OSC's gain target at note-on was its start level, so an attack did not begin moving until the first update. The target (+0x154) is the envelope's output (`0x894f3dc0`), the follower (+0x158) starts at the start level (`0x894f3e20`).
- v96. The TVA's gain is level × envelope, not envelope with level applied afterward, and a voice is over at the envelope's end *and* gain ≤ 2^−16 (`0x894f54b0`). The whole block, all OSCs and COSMs, now runs in the XT's units, the TVA and the sum in SH-4 arithmetic, ±1 only at the very end. Band players take the firmware's own gain (`setGainXt`), so a value 2^16 below flush is not lost: that was the reason the check first needed a tolerance on them.
- v97. XtSynth stopped a voice 8192 frames after a wave's end. The firmware keeps it until its envelope ends. A player past the end is silent, but a COSM's tail rings on through the release, and XtSynth cut it.
- v98. **The band players' start.** An OSC's note-on sets byte 0 bit 7 of the player, and `0x8950fa60` then takes the other path: the event before the position (`0x8950f9c0`), that event's records, each band's segments and records forward to the position (`0x89510bf0`). `xt_fw bandcmp` had never set that bit, so the port was checked on the other path, and followed it. It only shows when a walk starts backward (at the wave's end) and then turns forward: the port read its next segment past the list and restarted from the wave's start, the firmware stayed at the end. Now `xtFwBandPlay` sets the bit and the port takes this path. Two bandcmp lines added.
- v99. **A tone update.** `0x894dba00`'s order is OSCs (`0x894f5e20`, then `0x894f5be0` twice), MOD (`0x894f5da0`), COSMs (`0x894f5d10` twice), envelopes (`0x894f5cc0`), LFOs (`0x894f5c70`), and a legato (`0x894db8a0`) runs the same. (The two labels corrected in v105.)
  - COSM blocks go first, on the levels the COSM cached at its last update (object +0x104, +0x108, +0x10c). XtSynth set them after the envelopes, so a Sustain moved reached the block at once; on the XT it waits for the next tick.
  - The pass runs on any change, whatever moved. A COSM block's targets take the levels without the rates, so it can be heard with no COSM knob moved. XtSynth compared a long list of COSM fields; `Controls` now has `operator==`, and the list is gone.
  - The same pass is how the MOD's new block (`0x894f1c60`) arrives, and it clears an old OSC SYNC note's flag on Types 0…3. So a legato or any knob clears it, not only a MOD knob.
  - A bender move is the same pass. The conversion `0x894ebbd0` and `0x894dba00` have only `0x894dc0b0` as caller, so a new value can reach a sounding tone no other way. `pitchBend` now sends the blocks. The message's path to `0x894dc0b0` is still not traced (the slot is in the vtable at `0x89833e30`, 12 bytes per entry); this is inference from the callers.

**What the check needs from outside the tone, and does.** All of it is in `carryFrom`, but for FM's lines, at each case's end (v106).
- A firmware tone comes from a pool, XtSynth's voice keeps its objects. So before each note the port's analog fields (phases and whatever a waveform leaves), the DTVF's follower (D+0x98) and the MOD's (+0x18…+0x20, +0x28, +0x2c, +0x30, +0x40) are copied into the tone, and the MOD's tone number (+0x3c) is the voice's. Without this a note's start is not defined on either side. **So whether XtSynth's voice allocation hands the same leftovers as the XT's pool is not checked.**
- Past the wave's data a player reads what lies there. The waves lie among 256 KB of zeros, what the port reads. On a real XT it is other memory. Until v108 a note whose read left memory altogether was left off and counted (about 2.5% of notes). Now a band read past those zeros reads them anyway ("A band read far from its wave"), and none is left off.
- FM's far tap reads below its line pool into the lists of sounding tones. Those words read 0 during a render here, as in the port.

**Planted errors.** `research/tools/mutate_tone.py [cases]` plants each error in `tone_muts.txt` in XtSynth.cpp through `PORT=tone mutate.sh` and runs the check. 97 planted, at fcheck's 2000 cases and the sweep: the pitch, key, velocity, envelope, LFO, portamento, legato, TVA, COSM, MOD and Structure wiring, the order of the stages, and the change lists. 96 are caught. The one that is not, `legato-order`, the COSM block after the envelopes in a legato, is exact: a legato moves no knob and a COSM envelope's Sustain does not depend on velocity, so its update leaves the level where it was. The order stays the firmware's. `cosm-lim`, the Limiter's Attack and Release swapped, needs about 2000 cases to catch, which is why fcheck runs 2000: the level must be over the threshold.

**What is not covered.**
- A real XT. Everything here is rom.2 in the interpreter.
- The voice allocator and stealing: two notes at most, the part's note-on logic chosen in the harness as `0x894dbcb0` and `0x894dbea0` do.
- The tick's timing against the audio: the firmware's update runs on XtSynth's clock. The scheduler's n is not checked.
- The start state, as above, and everything before a note on the real panel: patch load, the editor.
- The matrix, controllers other than the bender (CC7, CC10, CC11 at rest), Hold, the TVA's Level, Pan, LFO and sends (at defaults; CC 7, 10, 11 and all but the Chorus and Reverb sends since v114), velocity on the pitch, time and formant envelopes' depth paths beyond what the OSC converter takes.
- PCM on OSC 2. EXT IN. Tempo sync, Time Trip, playback modes, start offset, the wave's tuning fields. (Tempo sync and Time Trip since v118.)
- The loop's cached events: the firmware's builder is not found, and the check's are built apart from the port's by the same rule.
- Mid-note OSC Switch, Type, Waveform, Sub Octave, Time Offset, Key Sync, Robot, Structure, COSM Switch, COSM Type and loop edits: held fixed in the live cases.
- `Controls::pitchSemis`, the plugin's own shift, has no patch parameter here.
- **Both OSCs off**: excluded, below. **Stereo OSC 1 into a COSM** was too, until v104.

**Decisions left, not changed.**
- TVA Switch (ID 0x299): done in v101.
- With a stereo OSC 1 the firmware's COSMs take their stereo routines, and `XtCosm` had only the mono ones. The check turned COSMs off for stereo waves. Closed in v104 ("COSM: the stereo routines").
- With both OSCs off XtSynth starts no voice, so the key state, random pitch and LFO seeds do not advance. The firmware starts a tone. The check keeps one OSC on. The user's decision (2026-11-03): leave it for now, so such a note does nothing and makes no sound.
- Lo-Fi on a SOLO wave is replaced by TVF in the check: its quantizer turns the SOLO player's small differences into a step of its own. The Compressor too since v114.

### The Structure Types (2026-11-02, v92)

Structure Types 1 and 2, the panel's Type2 and Type3, are ported. They set the order of the tone's sections. Type1 is what the port always played.

**The parameter.** ID 0x293, Structure Type, 0…2, default 0, named Type1…Type3 in rom.2. The tone block's +0, kept at tone +0x1770, the render's pointer-to-member at +0x1774.

**The renders** (read from the listings, buffers as in v86):

| Structure | Routine | Order | COSM 1 on | COSM 2 on |
|---|---|---|---|---|
| 0, Type1 | `0x894f6d40` | OSC 2, OSC 1, MOD, COSM 1, COSM 2, TVA | the MOD's output | the MOD's output |
| 1, Type2 | `0x894f6e20` | OSC 2, OSC 1, COSM 1, MOD, COSM 2, TVA | OSC 1's buffer | the MOD's output |
| 2, Type3 | `0x894f6f40` | OSC 2, COSM 2, OSC 1, COSM 1, MOD, TVA | OSC 1's buffer | OSC 2's buffer |

- All three clear both voices' sync inputs (+0x97c, +0x1330) first. The MOD always writes the third buffer, and the TVA reads that.
- **OSC SYNC's mask.** The first two test the MOD's flag right after OSC 2. The third runs COSM 2 first, keeps the mask in r12 across it and tests after. The mask is OSC 2's own return, so COSM 2 cannot change it: the rule is the same in all three.
- **X-MOD.** OSC 1's X-MOD reads and clears OSC 2's buffer, so in Type3 it reads OSC 2 after COSM 2, and COSM 2's output is what modulates. The MOD then gets zeros, as in the other two.
- A value above 2 selects `0x894f7020`, a bare `rts`: nothing renders, not even the buffer clears. The parameter's range never gives it, so the port clamps.

**The note-on** (`0x894f5e60`) switches on the Structure twice, Types 1 and 2 sharing a case:

1. `0x894f6320`, the COSMs' first part (`0x894ee480`, then `0x894edf70`). Each COSM object takes four words:
   +0 the pitch ratio (voice +0x144), +4 the root (voice +0x9a4), +8 the stereo flag (voice +0x960), +0xc a byte (voice +4).
   Types 1 and 2 give both sections OSC 1's. **Type3 gives COSM 2 OSC 2's** (tone +0xb18, +0x1378, +0x1334, +0x9d8) and COSM 1 OSC 1's. That is every place the third Structure differs here.
2. `0x894f6540`, the installs and choosers:
   - Types 1, 2: install OSC 1, read its flag, install OSC 2, the MOD's chooser, COSM 1's chooser with that flag, COSM 2's chooser **with what COSM 1's returned**.
   - Type3: install OSC 1, COSM 1's chooser with OSC 1's flag, install OSC 2, COSM 2's chooser with OSC 2's flag, then the MOD's chooser.

So each section's chooser reads its flag after that OSC's install, and no chooser reads the other's work except by the return.

**What the chooser returns** (`0x894eef70`). A stereo pick returns 1. A mono pick returns 1 only for block code 10, otherwise 0, and no converter writes code 10 (v65). So in Types 1 and 2 COSM 2's flag is OSC 1's, which is what v65 and v86 said without the condition. A decompile would show a plain flag; the `cmp/eq #0xa,r0; movt r2` at `0x894ef4d2` is in the listing.

**Which flag each takes, run:** the voices' installs hooked to leave a chosen flag, on all four pairs and three Structures: 24 routines, 0 different against `xtCosmOsc`. The analog install (`0x89505170`) clears +0x960 itself, so an analog voice takes the mono routines in every Structure and every COSM port applies as it is. That was already true of HQ-SQUARE (see v88). The TVA's flag stays OSC 1's in all three, and is stored and never read.

**The update** (`0x894f75f0`) switches a third time, at `0x894f76b0`, on one thing: the pitch given to each COSM's update in fr4. Types 1 and 2: OSC 1's ratio to both. Type3: OSC 1's to COSM 1, **OSC 2's to COSM 2**. Only the Comb and Side Band Filters read it. An OSC that is off holds 1.0 and root 440, as in v86, so a Comb on an OSC that is off follows neither OSC.

**A sounding note keeps its Structure.** Only the note-on writes +0x1770 and +0x1774. That comes from a search of the tone's code (`0x894f0000`…) for those offsets built up in a register: one write, and reads in the note-on, the update and `0x894f6bf0`. The search can miss an offset built another way, and it did miss the note-on's first switch, found by reading it. The check's change of the patch's value under each note is the other evidence. A new value in the patch does nothing to a held note, or to a legato note, which takes no note-on. The next note gets it.

**One more reader, not traced to a caller.** `0x894f6bf0` repeats the installs and choosers with the same order for each Structure. No pool load, raw word or `bsr` in the region reaches it. It changes nothing in the port.

**+0xc looks unused.** The byte is voice +4. The only reader I found in the COSM code is `0x894ed750`, the block copy of a code no converter writes.

**A COSM can make sound from a silent OSC.** Wave Shape (type 2, Level 127, Drive 90) on OSC 2 off in Type3 moves the output by up to 0.0077 against no COSM. So the mix can't skip OSC 2's buffer whenever OSC 2 is off. A COSM put out something on an OSC that was off in 3,490 blocks of the wide run, on either OSC, all equal to the firmware. I did not trace which types do it, or why.

**In XtSynth.** `xtToneOrder` is a table of the steps by Structure, in XtAnalog, and `mixBlock` walks it, so the check and the synth run the same table. `xtCosmOsc` says which OSC a section follows; the note-on and the update use it. The COSM dispatch moved out of `mixBlock` into `XtCosmSection` (XtCosm), so its `update` and `process` are what the check calls too; `Voice::CosmVoice` derives from it and the fields keep their names. COSM 2 on OSC 2 runs in the XT's units (scale 1), as OSC 2's buffer is; everything on OSC 1's buffer and the MOD's output is in the player's units as before. `osc2.curA` is 1 at a note-on with OSC 2 off, as OSC 1's already was.

**Sound** (A4 on OSC 1, E5 on OSC 2 at Coarse +7, both SAW; the TVF on LP24 at Cutoff 40; dB from renders):

| Setting | 440 Hz | 659 Hz | 1320 Hz | 1978 Hz |
|---|---|---|---|---|
| Each OSC alone, no COSM | −16.1 | −15.0 | −25.4 | −24.9 |
| Type1, LP on COSM 2 | −63.6 | −76.8 | −107.8 | −124.9 |
| Type2, LP on COSM 2 | −63.6 | −76.8 | −107.8 | −124.9 |
| Type3, LP on COSM 2 | −16.1 | −76.7 | −25.4 | −114.7 |
| Type1, LP on COSM 1 | −63.6 | −76.8 | −107.8 | −124.9 |
| Type2, LP on COSM 1 | −63.6 | −15.0 | −21.2 (OSC 2's 1319) | −24.9 |
| Type3, LP on COSM 1 | −63.6 | −15.0 | −21.2 (OSC 2's 1319) | −24.9 |

So each filter reaches what the table says, and the other OSC's partials stay as they were alone.

- RING at Level 0, SAW × SINE at E5, LP on COSM 1. Type1 filters the sidebands: 219 Hz at −52.5, 1099 at −108.8. Type2 filters OSC 1 first, so each low partial keeps both its sidebands: 219 at −77.3, 1099 at −77.1. That equal pair is the signature of a filter before a ring modulator.
- Comb on COSM 2, Tone 127. Type1 on the mix, tuned to OSC 1: 440 at −10.1, 659 at −51.8. Type3 on OSC 2, tuned to OSC 2: 440 at −16.1 as alone, 659 at −9.0. With OSC 1 off, Type1 still shows 659 at −51.8, as its Comb has ratio 1 and root 440, and Type3 shows −9.0.
- X-MOD on OSC 1 at PW +63 under an E5 SAW. Type3 with COSM 2 off: 1099 at −25.7, 219 at −28.3. With the LP on COSM 2 the modulator is almost gone: 440 at −16.1, 880 at −20.9, as a plain SAW.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline only added 4 lines):
- A tone run by the firmware's own note-on, update and render (through its pointer at +0x1774), with the MOD and both COSM sections live, on the sixteen types by their own converters, buffers from a hooked pool slot finder. Only the envelopes' and LFOs' note-ons and ticks are hooked, so that the check sets their outputs, and the TVA stubbed. 3,000 notes in fcheck, a third on each Structure, each note a Structure other than the last, all five MOD Types, any waveform on either OSC, an OSC or a COSM sometimes off: 15,000 note-ons and updates, 72,000 blocks, 0 different. Compared every block: OSC 1's and OSC 2's buffers and the MOD's output, after whatever COSM ran on each; both masks; the voices' fields; the MOD's fields and lines. At each note-on: each COSM's ratio, root and flag, its code, and that its routine is the mono table's.
- What the run reached, on the wide run: OSC 2 sounding in 325,128 blocks; its mask at OSC 1 in 154,444; X-MOD reading OSC 2 after COSM 2 in 11,089; COSM 2 on an OSC 2 of another pitch than OSC 1's on 1,544 notes of the three pitch-reading types. A band of cases keeps those types on COSM 2.
- The patch's Structure changed under each note at its tenth block: the firmware's render is unmoved. Through the CLI, `XT_STRUCT_AT`: a change during a note changes no sample of it, and the next note plays the new one, equal to a render with that Structure from the start.
- Wide run, 21,000 notes: 504,000 blocks, 0 different.
- Deliberate errors: 38 planted at 600 notes, 37 caught, most by thousands of blocks.
  - Nine in the order table, among them COSM 2 moved after OSC 1 in Type3, which only X-MOD can show (3,403 blocks).
  - Five in `xtCosmOsc`, each also caught by the flags pass.
  - In the check's wiring: the note-on's pitch and the update's taken from OSC 1, each alone; the root not 440; X-MOD given no buffer; the mask ignored; a stale ratio on an OSC that is off; the MOD given OSC 1 as its own modulator.
  - In `XtCosmSection` (`PORT=analogcosm`): each type's envelopes swapped, the Comb's and Side Band Filters' pitch fixed at 1, three types not processing.
  - The one missed is exact: Lo-Fi's scale dropped, as this check runs at scale 1. XtSynth's scale on OSC 1's buffer did not change (`ab.sh`, below).

**Two things the first runs turned up, in the check.**
- The firmware's one COSM object holds every type's fields and the note-on clears only some; the port has one object per type. So after a note of another type, a Dynamic TVF's follower starts from whatever that type left at D+0x98, and the port's from its own last note. The first run showed this as soon as types changed. Each note here starts from zeros on both sides, as `XT_COSM_MAIN` does case by case. It is an existing gap, it is one follower value at the start of a note, and since the port's voice allocation is not the XT's a real unit would differ here anyway.
- FM past full scale reads below tone 0's line (v89). In the earlier passes that memory was zero. Here it holds real data, as this pass runs the TB Filter's start-up, so 23 blocks in 504,000 differed, all under FM with OSC 2 above 2^31. This pass puts its tone at 12 in the pool, where a modulator 23 times full scale still reads inside. That is the same gap v89 listed, seen for the first time; the port still reads 0 past the pool's ends.

**Wiring.** With no check against the firmware at this level (the whole-tone check runs in its own session and does not yet cover the Structures):
- CLI identities: no COSM, all three the same bits. COSM 2 alone, Type2 equals Type1. OSC 2 off with both COSMs, Type2 equals Type1. OSC 2 off with COSM 1, Type3 equals Type1. OSC 1 off with COSM 2, Type3 equals Type1. Type3 whole equals OSC 1 with COSM 1 plus OSC 2 with COSM 2, at most 3e−8 apart with a peak of 0.39.
- PluginTest bit-identical at notes 30, 60 and 100: Type2 and Type3 with both COSMs over a PCM OSC 1 and an analog OSC 2 on all four inputs, a Comb on OSC 2 under RING, Type2 under FM, and Type3 under OSC SYNC with OSC 2 off. Set on the processor's side only, it fails 0/4, so the parameter is what is being tested.
- With the Structure at Type1, `ab.sh` against v91 is 4/4 identical on five settings: the defaults, and OSC 2 on with both COSMs, RING, FM, and OSC 1 off, across TVF, Comb, Side Band, Wave Shape, Lo-Fi, OD/Dist and TB, also with both COSMs and OSC 2 on.

**Not checked.**
- Against real hardware. The check is the firmware in the interpreter.
- A PCM voice in these passes, so a stereo flag is run only through the routines' choice. The COSM ports are the mono routines: a stereo PCM OSC 1 with a COSM on is the gap it was, now in three orders. In Type3 a stereo PCM OSC 2 would pick COSM 2's stereo routine, which matters once PCM on OSC 2 lands.
- XtSynth's own COSM note-on chain: the check has its own copy of the argument order, equal to the firmware, as the blocks come from XtSynth's Controls. Its `update` and `process` are shared.
- A Structure above 2.
- The caller of `0x894f6bf0`.

**Plugin.** Parameter `structure`, "Structure Type", Type1…Type3, default Type1, at the end of the layout. A row above the MOD with the selector and the order it gives, `kXtStripH` 880, `setSize` 1180. `XT_STRUCT`, `XT_STRUCT_AT`, `TEST_STRUCT`.

**For PCM on OSC 2** (branch `osc2-pcm`, not merged here). Its `mixBlock` keeps both buffers in the XT's units with OSC 2 on, and changes to OSC 1's at the MOD. In Type2 and Type3 COSM 1 runs before that, so with OSC 2 on it takes scale 1, and `fs` only with OSC 2 off. COSM 2 on OSC 2 is scale 1 either way. With OSC 2 off and a COSM on it in Type3 the mix still has to run. `xtCosmOsc` already points COSM 2 at OSC 2's `curA` and root in Type3, for a PCM OSC 2 too.

### The MOD section: OSC SYNC (2026-11-02, v91)

Type 4, the last. All five Types are now ported. OSC 2 restarts OSC 1, and OSC 2 itself is not heard at the block's level.

**The mask's path** (the render, `0x894f6d40`; `0x894f6e20` has the same lines, and `0x894f6f40` tests the same flag, though neither is ported). It clears both voices' sync inputs (+0x97c, +0x1330 of the tone) each
block, runs OSC 2, then, if the MOD's +0x40 is set, writes OSC 2's returned mask to OSC 1's input and to the MOD's +0x44. Then OSC 1, then the MOD.
So OSC 1 restarts at OSC 2's mask bits, within the same block. With OSC 2 off, or on NOISE, the mask is 0 and OSC 1 runs free. OSC 2's own input stays
0, and OSC 1's mask goes nowhere.

**The routines**, chosen at note-on by OSC 1's kind (`0x894f1d90`, r5 = kind is 3), not by the stereo flags:
- OSC 1 analog (`0x894f1aa0`): out = L1·v + L2·(1 − v), both channels, v = +0x28. As product, product, then sum.
- Otherwise (`0x894f1a40`), a PCM OSC 1 or none: at a mask bit, the held values (+0x18, +0x1c) become L1·v and R1·v. out = (L1 − held)·(1 − v/2), with the
  held values changing before that sample's output. So a PCM wave gets no restart, only a step taken out at each mask bit, and half the level.
- v comes from the block's a: 1 for OSC SYNC, so OSC 2 is multiplied by 0 and OSC 1 passes as it is. A held note given an ENV RING block takes v = Attack/127,
  and then OSC 2 is heard. That is the only way to hear it.
- OSC 1 off: the second routine on zeros, silence, whatever OSC 2 does.

**The flag outlasts the note.** Note-on sets +0x40 for Type 4 and clears it only for a type above 4, which no converter writes. The others leave it, so a voice's next
note, of any Type, still restarts OSC 1 at OSC 2's mask, until a new block arrives on it (`0x894f1c60` clears it for a held Type 0…3, and sets it for a held Type 4). It
looks like an oversight, and the port keeps it: the field stays on the `XtMod`, and a voice keeps its `XtMod` across notes. On a patch whose Type never changes it does not
show. Which voice a note gets is the port's allocation, not the XT's, so which note it would hit will differ.

**The tone's note-on rule** (`0x894f6148`). If the block's MOD type is 4, OSC 2's block kind is 3 and OSC 2's code is 10 (LA-SAW), it writes 2 (SAW) into the block's code. Only the code
changes: the level, gain and tones stay LA-SAW's, so it is a SAW at LA-SAW's level. Only OSC 2, and it looks at the block's +0 and not the Switch directly: with the Switch off the block
says 0. The port does this in `oscStart`, before the voice's note-on. LA-SQUARE is left alone. I did not find why the firmware does this for LA-SAW.

**Why the note-on's +0x18 is 0 and the update does not touch it.** Case 4 of the update writes +0x28 only, so a held note's holds carry on.

**Sound** (A4 SAW on OSC 1, Impact 0; dB from renders):
- OSC 1 alone: 440 Hz at −14.9, 880 at −20.9, 1320 at −24.4.
- OSC 2 SAW at E4 (Coarse −5): 330 Hz at −15.2, 659 at −24.8, 989 at −24.0, 1319 at −25.8, 1648 at −28.6, all harmonics of the master and no 440 Hz, with OSC 1's level kept: peak 0.280 as OSC 1 alone.
- OSC 2 SINE at E4: exactly those levels. The master's shape does not matter, only its period.
- OSC 2 at A3 (2:1): as OSC 1 alone, as the restart lands where the phase already is.
- OSC 2 NOISE, or off: as OSC 1 alone, every printed partial, as there is no mask.
- OSC 2 LA-SAW at E4: as OSC 2 SAW. Alone with no MOD the same setting gives LA-SAW's own 330 Hz at −11.6.
- OSC 1 off: silence.
- PCM OSC 1 (vocal) under E4: partials near 648–663 Hz at about −37, and energy at a few Hz, from the held values' steps; RMS 0.040 against the MIX's 0.175.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline only added 4 lines):
- The object alone, 3,000 cases in fcheck, Types 4, 4, 0, 1, 3 in turn on one object with nothing reset, so the flag outlasts notes; any mask while it is set; both routines; one note in about 80 of a
  type above 4: 15,000 note-ons and updates, 72,000 blocks, 0 different. Compared every block: the flag, the mask, the holds, v, the output.
- Both voices through the firmware's render, a quarter of the notes of another Type after an OSC SYNC note: 38,250 note-ons and updates, 126,000 oscillator blocks, 0 different, with both buffers, the mix, both
  masks and the fields; OSC 2's mask at OSC 1 in 16,361 blocks and 45,336 blocks with OSC 2 sounding.
- The firmware's own tone note-on, a third of the cases LA-SAW on both OSCs, an OSC sometimes off: 1,500, 0 different. So the rule's conditions are the firmware's: 519 notes took it over the two tone passes.
- Wide run, 21,000 cases: 504,000 blocks alone, 882,000 in the tone, the note-on on 10,500, 0 different.
- Deliberate errors: 25 planted, 24 caught. The one left is exact: setting the flag again on a held Type 4 note, where the note-on has already set it. At first one more was missed: v taken as 1 at note-on, since a Type 4 block's a is always 1. One such block in four now has another value. The rule's errors (applied with the Switch off, to OSC 1, to LA-SQUARE, or not at all) are caught by the firmware's own note-on.

**A slip in the check, fixed here.** Each tone pass started with the firmware's block holding what the last pass left and the port's empty, so if the first note was of a Type that leaves words, the two
differed for that note. It only showed once the random stream moved. Both now start from zeros.

**Wiring.** With no check against the firmware at this level:
- The Sound section above, through `XT_MOD`: the mask reaches OSC 1, the master is not heard, the LA-SAW rule.
- PluginTest bit-identical at notes 30, 60 and 100: analog over analog, under a PCM OSC 1, and an LA-SAW master.

**Not checked.**
- Against real hardware.
- A PCM voice through the firmware's render: the routine for it is checked on filled buffers and masks only.
- The rule's other effect: the block keeps code 2 after the note-on, so when the OSC's block is formed again and copied into a held voice (`0x894f2600`), the code returns to 10 under a SAW routine.
  The port latches the code at note-on and I did not trace that path.
- The other two Structure Types' renders, which are not ported.

**Plugin.** OSC SYNC is enabled in the Type box, so all five are; `xtModPlays` is gone. `XT_MOD` and `TEST_MOD` take Type 4.

### The MOD section: ENV RING (2026-11-02, v90)

Type 3. OSC 1 under OSC 2's level. OSC 2 itself is not heard. Attack and Release are the follower's rates.

**The level** (`0x895017e0`), once a block, of L only in both routines, from OSC 2's buffer:

    s = 0; for each of 32 samples: s += |x|, and again if it is the last of its eight
    level = s·0x2e7fffff

- The doubling is in the listing: `fadd fr2,fr4` twice at `0x89501826` and `0x8950182a`. It looks like a slip that stayed,
  as the weights sum to 36 and not 32. A decompile would hide nothing here, but a summary of "sum over 32" would be wrong.
- 0x2e7fffff is about 2^−34, so the level is 4.5 times mean|L2|/2^31: a full-scale square is 4.5, a SINE at 0.396 about 1.13.
- `0x89501850` beside it looks like a stereo version. Neither routine calls it. (The stereo Side Band Filters do: v104.)

**The routines** (`0x895279d0`, or `0x89527aa0` when either voice's stereo flag was set), block by block:

    gain step = (+0xc − gain)·0.009375                  gain is +0x18, at note-on +0xc
    t = last > level ? Release (+8) : Attack (+4)       last is +0x2c
    last = level
    rate = (1 − t)·0.005 + 0.0001                       product then sum; 0x3ba3d70a, 0x38d1b717
    4 samples, 8 times:
        gain += step
        out = L1·(env·gain)                             env is +0x28, 0 at note-on
      then env += (level − env)·rate

- env moves once per 4 samples, and gain every sample.
- +0x2c is not cleared at note-on. The note's first block compares with whatever the tone last held. It is also not read by
  any other Type, so it can be very old.
- Nothing limits env. It follows the level, which can pass 1 easily: out is L1 × 4.5·mean|L2| at most for a full-scale L2,
  and more past full scale. The peak in the object pass, about 200 of 2^31, comes from buffers at ten times full scale.
- The first routine reads L1's L and writes it to both channels. The second reads both.
- gain's target is 1 in every real block, so gain stays 1. The check moves it by the words the converter leaves.

**Attack and Release share a block-by-block test.** The test is on one block's level against the last, with no smoothing, so
the level's own ripple picks the rate. Under a SINE, |L2| has a ripple that 32 samples do not span, and about half the blocks
count as falling. So the note's env moves at Attack's rate half the time and Release's the other half, even on a steady
modulator. With Release at 0, a long Attack does little:

| Attack | Release | Time to 63%, rendered | From the rates |
|---|---|---|---|
| 0 | 0 | 16.9 ms | 17.8 ms (4 samples/0.0051) |
| 64 | 0 | 21.5 ms | 23.6 ms (the two rates averaged) |
| 127 | 0 | 35.1 ms | 34.9 ms (the same) |
| 64 | 64 | 32.8 ms | 35.2 ms |
| 127 | 64 | 59.0 ms | 67 ms |
| 127 | 127 | 822.7 ms | 907 ms |

The renders measure RMS in 2 ms windows on a tone that is also under its modulator's ripple, so they run a little short. Falling
away after OSC 2's own decay, to 37%: 16.3 ms at 0, 0; 31.2 ms at 64, 64; 801.7 ms at 127, 127. So the real range only shows
with both knobs up, and that is not a port artefact: it is what the code says, and the times above are what a player should get.
It would be worth comparing with a recording.

**Sound** (A4 SINE on OSC 1, E5 SINE on OSC 2, Impact 0; dB from renders):
- Steady, Attack and Release 0: 440 Hz at −6.3, 1.5 dB over OSC 1 alone (−7.8), so a gain of about 1.19. No sign of 659 Hz or any
  sideband above −57 dB: OSC 2 sets a slow gain and is not multiplied in.
- OSC 2 Level 64: 440 Hz at −18.2, 11.9 dB down as curve(64) = 0.254 says.
- OSC 2 SAW: −11.9.
- OSC 2 off: silence, as the level is 0.
- Under a PCM OSC 1 (vocal, stereo inputs too): runs, bit-identical to XtSynth in PluginTest.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline added 3 lines, and see the correction below):
- The object alone, 3,000 cases in fcheck, note after note, every stereo flag: 15,000 note-ons and updates, 72,000 blocks, 0
  different. Compared every block: the targets, gain, env, last, the output. One block in eight aims one sample so the level lands on
  the last block's, or a unit to either side of it, as a rising or falling test turns on that.
- Both voices through the firmware's render, the firmware's OSC 2 as the modulator: 38,250 note-ons and updates, 126,000 oscillator
  blocks, 46,248 of them with OSC 2 sounding, 0 different.
- The firmware's own tone note-on: 1,500, 0 different.
- Wide run, 21,000 cases: 504,000 blocks alone, 882,000 in the tone, the note-on on 10,500, 0 different.
- Deliberate errors: 25 planted, 25 caught, none missed: among them the doubled sample dropped or moved, `>` for `>=`, env's move on
  the wrong sample of its four, and a note-on clearing +0x2c.

**A correction to v88 and v89.** Their tone passes were weaker than those sections said. `modTonePass` left X-MOD's pass on, so OSC 1
was X-MOD in nearly every case, and X-MOD clears OSC 2's buffer before the MOD reads it. So L2 was zero outside the cases with OSC 1 off.
The object passes, on filled buffers, were never affected. Now both OSCs take any of the fourteen waveforms, and each tone line prints how
many blocks the MOD was given a sounding OSC 2, and its peak: RING 46,128 blocks, peak 5.6 of 2^31; FM 44,544, peak 4.7; ENV RING 46,248,
peak 4.0. All still 0 different, so the port was right and the check was not showing it. The peaks are also the first direct sign that real
voices go well past full scale: FM's reads outside its line are a normal case at high Impact and Wave Gain, not a corner. The change moves
fcheck's lines for those passes, as the random stream moved: `mod ring tone` and `mod fm` and `mod fm tone` changed, with 0 different throughout.

**Not checked.**
- Against real hardware. The Attack and Release behaviour above is the one a recording would settle.
- ENV RING with a real stereo PCM voice through the firmware's render, as with FM.

**Plugin.** ENV RING is enabled in the Type box. Attack and Release are the knobs; Original Level does nothing here. `XT_MOD` and
`TEST_MOD` take Type 3. PluginTest is bit-identical at notes 30, 60 and 100, on analog voices and under PCM OSC 1s.

### The MOD section: FM (2026-11-02, v89)

Type 2. OSC 2 moves a read point in a delay line that OSC 1 writes, so it is phase modulation of OSC 1 by
OSC 2, not FM of an oscillator's pitch.

**The routines.** `0x89527b80`, or `0x89527c50` when either voice's stereo flag was set (+0x38 not 0). They
differ in channels, not in sum. Each sample, for a line of 496 floats:

    w = w + 1, 0 at 496                                   +0x30, kept from note to note
    line[w] = L1
    d = ((L2 + K1)·a)·K2                                  K1 = 0x4effffff = 2^31 − 128, K2 = 0x33f80000
    n = d truncated, f = d − n
    i = w − n, j = i − 1, each +496 if negative, once
    out = ((line[j]·f + line[i]·(1 − f)) + L1·b)·c

- K1·K2 is 248, so at a = 1 the delay runs 0…496 samples across L2's ±2^31, and no modulator means 248.
- The input goes in before the read, so a delay of 0 reads this very sample.
- The first routine reads L only, writes one value to both channels and leaves the R line alone. The second
  writes both lines before either tap, and uses one w for both.
- b is the Original Level: the dry signal, added to the delayed one.
- a and b move by 0.009375 of the difference per sample like the others. c moves by (target − c)·(2^31 − 128):
  the compiler reloaded fr11 for K1 before that product. c is 1 in every block FM is given, so it never moves.
  If it did it would run away, by about 2^36 a block.

**The lines are one pool.**
- +0x10 = 0x89f91c5c + tone·1984 and +0x14 = 0x89f9d65c + tone·1984, set at every note-on. +0x3c is the tone's
  number (tone +0x10, copied at construction `0x894f5a48`). With 24 tones the L lines end exactly where the R
  lines start, and the R lines end at 0x89fa905c.
- The note-on clears both of the tone's lines. It does not reset w.
- Nothing checks that i and j land in the line. When L2 passes full scale, by more than w/248 of it, the read is
  in the tones next door: lower tones when d is too large, higher ones when d is negative (truncation toward
  zero, f negative, and the wrap never fires).
- So `XtModShared` is the whole pool as one array, and `lineAt` reads anywhere in it. Past its ends the XT reads
  whatever is there. Just below are the lists of sounding tones (`0x89f91c50`, `0x89f91c54`); above, `0x89fa9094`.
  The port reads 0. In the interpreter that memory is 0 too, so the check says nothing about it.
- A voice can get there. OSC 2 with an Impact, or at Wave Gain +12, passes full scale. In a 200-case tone pass, truncating
  toward −∞ instead changed 2 blocks in 8,400, so it is rare but real.
- XtSynth's voices are tones 0…15 of the pool. Which voice a note lands on is not the XT's tone allocation, so out
  of range the neighbour's contents will differ from a real unit's anyway.

**The stereo flag.** A PCM OSC 1 with a stereo wave takes the second routine. An analog OSC 1 never does, not even
HQ-SQUARE (v88), and OSC 2 plays only ANALOG so far.

**Sound** (A4 SINE on OSC 1, SINE on OSC 2, Impact 0; dB from renders):
- OSC 2 off, Level 0: exactly the delayed OSC 1, 440 Hz at −7.8.
- OSC 2 off, Level 127: dry + a 248-sample delay. 440 Hz at −23.6: 248 samples is 2.47 cycles, so nearly a null,
  and 2·|cos(π·2.474)| says −15.7 dB against one alone. That agrees.
- OSC 2 at E5 and Level 40, Original Level 0: 440 Hz at −8.6, 219 and 1099 Hz at −18.6, 661 and 1979 at −41. A small
  index: one strong pair of sidebands, and the second pair much weaker.
- OSC 2 at E5, Level 127, Original Level 0: an index near 6. Partials at 219, 879, 1099, 1758, 2197… all near −19, to well
  past 3700 Hz. RMS unchanged at 0.289: it is a delay, so it moves energy around without adding any.
- OSC 2 at A4: harmonics of 440 Hz, 880 at −19.4, 1320 at −12.8, 2640 at −13.4.
- SAW carrier under E5: 219, 878, 1099, 1759 Hz at about −26.
- OSC 2 at Wave Gain +12: reads leave the line, and the energy lands up at 12–18 kHz, near −23. This is the out of
  range case; that it matches is a statement about the interpreter's memory.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline only added 3 lines):
- The object alone, 3,000 cases in fcheck, on a tone number that varies, with 0 and 23 always among them, every stereo
  flag, buffers up to ten times full scale, any Type's block on the held note: 15,741 note-ons and updates,
  72,000 blocks, 0 different. Compared every block: the targets, current values, w, the output, both of the tone's
  lines. At each note's end all 48 lines.
- Both voices through the firmware's render (with OSC 2 mostly cleared by X-MOD until v90, see there): 38,250 note-ons and
  updates, 126,000 oscillator blocks, 0 different, with the lines.
- The firmware's own tone note-on: 1,500, 0 different, lines cleared and w kept.
- Wide run, 21,000 cases: 504,000 blocks alone, 882,000 in the tone, the note-on on 10,500, 0 different.
- Deliberate errors: 25 planted, 25 caught. Before the last check, 1 was missed: R's line written after L's tap. It takes
  L's read landing exactly on R's newest sample, which is a modulator 48 times full scale. No voice gets there; one
  sample a block now aims at it. Worth noting among the 25: reads kept to the tone's own line (shows the object pass
  does read other lines), truncation toward −∞ (shows negative d is run), and c's step with the wrong constant (shows
  c is moved at all, by a block written only for that, at a note's end so it cannot overflow).

**Forced c.** Under FM the conversions no longer put other values in +0xc as they do for the rest, since c would run to
infinity and the two sides' NaNs have no reason to agree. Instead a block near each note's end has c off target, for
two blocks. So the peak the FM line prints, about 1e23 of 2^31, comes from that block.

**Not checked.**
- Against real hardware. The check is the firmware in the interpreter.
- Reads past the pool's ends, on the XT.
- FM with a real stereo PCM OSC 1 through the firmware's render. The routine is checked on buffers with L ≠ R, and
  PluginTest runs it under stereo inputs, but no firmware PCM voice runs in `xt_analog`.

**Plugin.** FM is enabled in the Type box. Original Level is the dry signal. `XT_MOD` and `TEST_MOD` take Type 2.
PluginTest is bit-identical at notes 30, 60 and 100 on FM over analog voices and under PCM OSC 1s, stereo included.

### The MOD section, and RING (2026-11-02, v88)

The MOD section gets its own object in the port (`XtMod`, in XtAnalog): the converter, the note-on,
a new block on a held note, MIX with gains that can move, and Type 1, RING. FM, ENV RING and OSC SYNC
follow. The map is in the v86 section.

**The patch's values** (descriptors at `0x897a167c`): Switch 0…1, default 0; Type 0…4, default 0;
Original Level 0…127, default 127; Attack and Release Times 0…127, default 0.

**The converter** (`0x894e0920`) writes into the block and leaves what it does not write:

| Switch, Type | +0 | +4 | +8 | +0xc |
|---|---|---|---|---|
| off, or Type 0, or above 4 | 0 | 1 | 1 | left |
| RING, FM | 1, 2 | 1 | level curve(Level)/32768 | 1 |
| ENV RING | 3 | Attack/127 | Release/127 | 1 |
| OSC SYNC | 4 | 1 | left | left |

So the block can hold another Type's words. `xtModBlock` writes into a block for that reason.

**The note-on** (`0x894f1b00`) copies the four words and stores:
- +0x34, the type's low byte.
- +0x38 from the voices' stereo flags: 0 neither, 1 both, 2 OSC 1's only, 3 OSC 2's only.
- By the type:
  - MIX: +0x18 = +4, +0x1c = +8.
  - RING: +4 /= 2^31, then +0x18 = +4, +0x1c = +8, +0x20 = +0xc.
  - FM: the same with no division, and it clears both delay lines.
  - ENV RING: +0x18 = +0xc, +0x28 = 0.
  - OSC SYNC: +0x40 = 1, +0x18 = +0x1c = 0, +0x28 = +4.
- It clears +0x40 only for a type above 4, which the converter never gives. So after an OSC SYNC note the
  flag stays set through the next note of another Type. See v91.

**The order** (corrects v86, which had the MOD after the installs). The tone's note-on runs the voices'
setups, their ramps, the MOD's note-on, the COSMs' first part, then the installs, then the chooser
(`0x894f1d90`) with r5 = OSC 1's kind is 3. The setup clears +0x960, and no analog voice has it set when
the MOD reads it, HQ-SQUARE included: +0x38 is 0 on every pair of the fourteen waveforms. So the check's
v86 pass, with the installs first, was out of order. It made no difference at gains of 1.

**The update** (`0x894f1c60`) is not the 10-ms one; that is `0x894f1d40`, a bare `rts`. It runs from
`0x894f5da0` when the patch's block changes, and it switches on the *object's* type, not the block's:
- MIX: +4, +8 from the block, +0x40 = 0.
- RING: +4 = (block +4)/2^31, +8, +0xc, +0x40 = 0.
- FM, ENV RING: +4, +8, +0xc, +0x40 = 0.
- OSC SYNC: +0x40 = 1, +0x28 = block +4.

So a held note keeps its routine and takes gains meant for another Type. Turn the Switch off under a held
RING and it stays RING at Level 1. Set a held MIX to RING at Level 0 and OSC 2 fades out with no sidebands.

**The gains' move.** Each routine computes step = (target − current)·0.009375 (`0x3c199999`, a unit below
the nearest float) at the block's start, and adds it each sample, so about 30% of the way per block. It
rounds each add, so 32 of them do not land where one product would.

**MIX** (`0x89527d80`): L1·a + L2·b as product, product, then sum; a and b move after each sample.

**RING** (`0x89527e00`), both channels, with SZ = 1 pair moves:

    out = ((L2·a + b)·c)·L1        a = 1/2^31, b = the Original Level, c = 1

in that order, with a rounding at each step. L1 is not moved to ±1 first. c is 1 in every block a RING
note can be given, except a block no converter has written to yet; the check forces other values anyway.

**In XtSynth.** `plain()` means MIX with all four gains at 1. That path stays `xtAnalogMix` in OSC 1's
units, so with the MOD off or on MIX nothing changed (`ab.sh` against v87: 4/4 identical). Anything else
moves OSC 1's buffer to the XT's units (×2^31 for ±1, ×2^16 for the band players), runs the routine in
place and moves it back. RING with OSC 2 off is therefore b·L1, not L1. RING with OSC 1 off is silence.

**Sound** (A4 SINE on OSC 1, E5 SINE on OSC 2, Impact 0; dB from renders):
- MOD off: 440 Hz at −7.8, 659 Hz at −8.2.
- RING Level 127: 440 Hz at −7.8, 219 and 1099 Hz at −22.0. OSC 2's own partial is gone.
- Level 64: 440 Hz falls to −19.7, 11.9 dB down as curve(64) = 0.254 says; the sidebands stay at −22.0.
- Level 0: only the sidebands.
- Level 0 with OSC 2's Wave Gain at +12: sidebands at −9.9.
- OSC 2 off: Level 127 equals OSC 1 alone, Level 0 is silence.
- SAW × SINE at Level 0: 220 Hz at −28.6, 661 at −38.6, 1099 at −29.3, 1539 at −35.5, sidebands of each
  partial.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline only added 4 lines):
- The converter on 200,000 cases, a sixteenth with a Type above 4, block compared word by word: 0 different.
- The object alone, 3,000 cases in fcheck, note after note with nothing reset, every stereo flag: firmware's
  note-on, chooser, update and dispatch on buffers the check fills (none, small, full, ten times that, one
  channel, the same on both). 15,000 note-ons and updates, 72,000 blocks, 0 different. Compared every block:
  targets, current values, +0x34, +0x38, the routine and the output. Each update carries any Type's block.
- A block in which `plain()` is true must also equal `xtAnalogMix`: 14,709 of them, 0 different. That is the
  shortcut XtSynth takes.
- Both voices through the firmware's render (with OSC 2 mostly cleared by X-MOD until v90, see there), RING or MOD off, every waveform, OSCs sometimes off, new
  blocks on held notes: 38,250 note-ons and updates, 126,000 oscillator blocks, 0 different.
- The firmware's own tone note-on, all fourteen waveforms on both OSCs, an OSC sometimes off: 1,500, 0 different.
- Wide run, 21,000 cases: 504,000 blocks alone, 882,000 in the tone, the note-on on 10,500, 0 different.
- Deliberate errors: 24 planted, 24 caught. At first 4 were missed, all one gap: c is never other than 1 in
  real blocks, so c's move, its store and the regrouping of (x·c)·L1 changed nothing, and neither did the
  converter writing +0xc for OSC SYNC. One conversion in eight now finds other values in the words the
  converter leaves.

**Wiring.** With no check against the firmware at this level:
- PluginTest bit-identical at notes 30, 60 and 100: RING over analog voices, RING under a PCM OSC 1 (all
  four inputs, band and SOLO), and MIX with the Switch on.
- New knobs on held notes through `XT_MOD_AT`: as the Sound and update sections say.

**Not checked.**
- Against real hardware. The check is the firmware in the interpreter.
- Who else calls `0x894f5da0`. The port runs the update on sounding voices when a MOD knob moves. On the XT
  it goes through the block's +0x6bc, the tone bound to that block, and I did not trace when that pointer
  goes away after a note ends. It matters only for what an idle voice's fields hold at its next note.
- XtSynth as a whole against a firmware tone.

**Plugin.** Parameters `modSwitch`, `modType`, `modLevel`, `modAttack`, `modRelease`, at the end of the layout.
A row between the OSC rows and the COSM selector (`kXtStripH` 850, `setSize` 1150). The Type box enables
what `xtModPlays` says, MIX and RING so far; a Type the port does not play yet is treated as the Switch off.
`XT_MOD`, `XT_MOD_AT`, `TEST_MOD`.

### The analog waveform X-MOD (2026-11-02, v87)

Code 13, patch waveform 13: a SAW whose phase is moved by the other OSC's output. With it
all fourteen analog waveforms are ported.

**The converter's case** (`0x894e208a`, from the switch at `0x894e206e`):
- +0x98 is the modulation's depth (`0x894e53d0`). i = PW + 64, 0 if not above 1, within
  0…127; then i/127 + (key − 60)·KF/12000, within 0…1. So PW −63 is none, 0 is 0.504 and +63
  is 1.
- +0x9c and +0xa0 are its depths, (d/63)·|d/63| (`0x894e5470`), the envelope's × its velocity
  factor, the LFO's d within ±63.
- **On OSC 2 all three are 0**, so its X-MOD has no modulation.
- +0x84, +0x88, +0x90 are Fat's, as SAW's. +0xb4 = 0, so no sub oscillator. The Detune knob,
  Sub Octave and Sub Level do nothing.
- The level and Slope are the Saw's (v79, v83).

**The targets** (`0x89505f10`, the note-on's first step and each update):
- +0x154 = the gain target.
- +0x24c = the depth's sum, within 0…1.
- +0x1c0 = ½ − ½·Fat at once, without the plain update's 70%; 0.002 if not above 0.0039.
- It does not write +0x1c8, so the Pulse Width below is fixed for the note.

**The note-on** (`0x89505ec0`, after the common part): +0x1c8 = +0x1cc = ½ + ratio·Slope·0.01;
+0x250 = +0x24c; +0x1c4 = +0x1c0.

**The routine** (`0x89507b60`), installed at +0x96c. The renderer (`0x894f2834`) tests the code
and calls that one instead of +0x968, with the other OSC's buffer in r7 as for the others. Against
SAW (v79):
- The step's constant is 0x3ba377ed, a unit below SAW's. b = a + (1 − a)·d rather than a + d −
  a·d, which rounds differently.
- y and the half-cycle flags are SAW's, and a sync bit sets the phase to 0.
- m = (L/2^31 + R/2^31)/2 from the other buffer, whose two words it then clears.
- phase += m·0.2·(+0x250), then += the step, in that order.
- At or above 1 the phase takes 1 off. At or below 0 it gets 1 added, so landing exactly on 0
  gives 1.
- It does this once, so a large modulation leaves the phase out of range, and y carries on along
  whichever line it is on. Nothing limits the output.
- out = (y·sign + Impact)·gain·2^31, the exact 2^31 and not SAW's 2^31 − 128.
- The gain moves 1/32 per sample. Each block Fat, PW and depth move **0.2** of the way, not
  1/32.

**Three copies of the loop.** The listing has three, which look like a state machine but are
not one. At the first wrap the routine compares a·d with a. If they differ it continues in a copy
that never sets a mask bit for a wrap; if equal, in one that always does. Both are constant within a
block, so one test does it. They are equal only at d = 1, a Pulse Width of −½, which no note
reaches: the Pulse Width is ½ or above. So a wrap never sets a bit, and the mask comes from the
flags alone. The check forces −½ to run that copy anyway.

**Who it reads.**
- OSC 1's X-MOD reads OSC 2's output and clears it, so OSC 2 is consumed and not heard: MIX then
  adds zeros. OSC 2 keeps running, envelopes and all.
- With OSC 2 off its buffer is zeros, so OSC 1 plays unmodulated.
- OSC 2's X-MOD reads and clears OSC 1's buffer too, at depth 0. What is there is last block's,
  about to be overwritten, so it changes nothing. It sounds like a SAW and is summed as any other.
  The port passes no buffer; the check confirms that a stale one makes no difference.
- In Structure 2 the render runs COSM 2 on OSC 2 first, so X-MOD would read OSC 2 after the filter.
  That comes with the Structure Types.

**Sound** (A4, Impact 0, against OSC 2 SINE at A4 unless noted; dB from renders):
- OSC 2 off, or PW −63: partials exactly SAW's, −14.9, −21.0, −24.7 dB at 440, 880, 1320 Hz. With
  PW −63 and OSC 2 on, the output equals the one with OSC 2 off in every sample.
- PW 0: 440 Hz gone from the top partials; 880 Hz at −19.7, 1760 at −22.3, 3519 at −22.9 dB.
- PW +63: 440 at −22.1, 1320 at −22.6, and energy out at 6160 (−23.1) and 7040 Hz (−23.5). The
  RMS stays at 0.145, as SAW's, because the wave's range is unchanged.
- OSC 2 up an octave at PW +63: 5720 Hz at −20.0 dB.
- Fat 64, PW −63: 219 Hz at −18.0 dB, as SAW's Fat.
- OSC 2's X-MOD alone equals OSC 1's at no depth, and SAW + OSC 2 X-MOD doubles (−8.9 dB).
- On real notes the phase stays in range: PW +63 under SINE, SAW or a wide SUPER-SAW peaks at
  0.28. With Impact 4.0 on both and OSC 2's Wave Gain at +12, OSC 2's start pushes it out and the
  peak is 25.5. That is what the routine does, and it is unclamped here too; I did not check what
  the rest of the XT's output path does with it.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline only added 4 lines):
- The converter's case: 200,000 cases on OSC 1, 100,000 on OSC 2, 0 different.
- The voice alone, 3,000 cases in fcheck, a quarter on OSC 2: the setup, install, updates and the
  routine with a buffer the check fills. 21,000 note-ons and updates, 90,000 blocks, 0 different,
  with the fields, the mask, and the buffer cleared on both sides.
- The buffer is none, a small tone, full level, ten times that, or one channel only. Phases go on
  each turn, a step from 1, on 0, and on −step with nothing to add so the step lands exactly on 0.
- One block in seven forces the Pulse Width to −½: a·d == a, and the move back.
- Both voices through the firmware's render, X-MOD on OSC 1 and sometimes on OSC 2, every other
  waveform under it: 126,000 oscillator blocks, 56,712 of X-MOD, 0 different. So X-MOD's input is
  what the firmware's own OSC 2 wrote.
- Wide run, 21,000 cases: 630,000 blocks alone and 882,000 in the tone, 0 different.
- Deliberate errors: 50 planted, 46 caught. At first 7 were missed. Three showed real gaps, now
  closed: no case reached the third copy; the move of the Pulse Width was never seen, as nothing else
  changes it; and landing exactly on 0 was undone by the phase being set afterwards. The other four
  are exact:
  - The quotient by 2^31 − 128 for 2^31: under round-to-zero it truncates to the same float for
    every input.
  - Regrouping the division by 2^31 or the product by 2^31, which are powers of two.
  - OSC 2's X-MOD given a stale buffer: depth 0, which is the claim above.

**Not checked.**
- Against real hardware. X-MOD's level and its out-of-range behaviour rest on the firmware in the
  interpreter.
- The output path after the voice with such a peak.
- Structure 2's order, and the MOD's other types with X-MOD.

**Plugin.** X-MOD is enabled in the Waveform box on both OSCs. No new parameters: PW is its depth,
with PW KF, LFO and Env; Fat and its depths as SAW's. `XT_ANALOG`, `TEST_ANALOG`, `XT_ANALOG2` and
`TEST_OSC2` take waveform 13. The PluginTest is bit-identical at notes 30, 60 and 100 with X-MOD over a
SINE, with KFs and OSC 2 off, on both OSCs, and on OSC 2 over a PCM OSC 1.

### OSC 2, and how the two OSCs are mixed (2026-11-02, v86)

A tone has two oscillator voices: OSC 1 at tone +0x20 and OSC 2 at tone +0x9d4, the
same object 0x9b4 bytes apart. This version plays OSC 2 as ANALOG, with every analog
waveform but X-MOD, and sums it with OSC 1 as the MOD section does when it is off or set
to MIX. Left: X-MOD (next version), PCM on OSC 2, the MOD's other types and the
Structure Types other than the first.

**The tone's render** is a pointer-to-member at +0x1774, chosen by the Structure Type
(ID 0x293, the tone block's +0, kept at +0x1770; table at `0x89858244`):

| Structure | Routine | Order |
|---|---|---|
| 0 | `0x894f6d40` | OSC 2, OSC 1, MOD, COSM 1, COSM 2, TVA |
| 1 | `0x894f6e20` | OSC 2, OSC 1, COSM 1 on OSC 1, MOD, COSM 2, TVA |
| 2 | `0x894f6f40` | OSC 2, COSM 2 on OSC 2, OSC 1, COSM 1 on OSC 1, MOD, TVA |

The buffers are `0x89d2c810` (OSC 1), `0x89d2c910` (OSC 2) and `0x89d2ca10` (the MOD's
output). The port plays Structure 0, which is what it has always played.

**What differs between the OSCs.** Nothing in the routines. A voice reads its own fields
and the envelopes and LFOs it is bound to:
- The analog converter (`0x894e1920`) tests the input's byte +2 only for the matrix
  destinations, the controller getters (`0x894e70b0`, `0x894e7520` for OSC 2) and the
  envelope and LFO numbers it stores: +0x20, +0xa4, +0x8c, +0x4c are Env 1, 2, 3, 4 for
  OSC 1 and 5, 6, 7, 8 for OSC 2; +0x38, +0x94, +0xa8, +0x50 are LFO 1 or LFO 2.
- With no matrix or controllers the block is the same, so `xtAnalogBlock` needs no OSC
  argument yet. X-MOD's case is the exception (v87).
- The binding (`0x894f7070`) turns those numbers into the pointers at voice +0x970…+0x98c.

**The order.**
- Per-note setup (`0x894e4940`): OSC 1 then OSC 2. The shared random pitch generator
  steps for each OSC with a depth, whatever its Switch.
- Tone note-on (`0x894f5e60`): the envelopes and LFOs; the voices' setup and note-on
  (`0x894f21c0`) OSC 1 then OSC 2, so SUPER-SAW's shared counter and FEEDBACK-OSC's lines
  go in that order; the pitch ramps; the installs, the MOD and the COSMs.
- Tone update (`0x894f75f0`): envelopes, LFOs, OSC 1, OSC 2, the MOD, the COSMs, the TVA.
- Render: OSC 2 first, because OSC 1 can read its output or its mask.

**The OSC Switch** (IDs 0x2a4, 0x2d8, the input's +8): `0x894e4830` writes kind 0 into the
block when it is off, and skips the converter. For kind 0:
- the renderer (`0x894f2800`) clears the OSC's buffer and returns mask 0;
- the update does nothing;
- the note-on (`0x894f2f80`) sets +0x144 = 1.0, and the setup gives it root 440. So with
  OSC 1 off the Comb and Side Band Filters, which read OSC 1's ratio and root, see 1 and
  440, not what the voice's last note left. The port does this (`curA = 1`).

**The MOD section.** One object at tone +0x1708. Its block is at the tone block's +0x5dc,
from the converter `0x894e0920` (input +6 the Switch, ID 0x294; +7 the Type, 0x295):

| Switch, Type | Block's type | +4 | +8 | +0xc | Routine |
|---|---|---|---|---|---|
| off, or Type 0 | 0 | 1 | 1 | | `0x89527d80` |
| Type 1 | 1 | 1 | level curve(+8)/32768 | 1 | `0x89527e00` |
| Type 2 | 2 | 1 | level curve(+8)/32768 | 1 | `0x89527b80`, `0x89527c50` |
| Type 3 | 3 | (+9)/127 | (+0xa)/127 | 1 | `0x895279d0`, `0x89527aa0` |
| Type 4 | 4 | 1 | | | `0x894f1a40`, `0x894f1aa0` with OSC 1 analog |

The types are named at rom.2 file offset 0x49d6f0: MIX, RING, FM, ENV RING, OSC SYNC. Note-on
`0x894f1b00`, routine chooser `0x894f1d90`, update `0x894f1c60`, dispatch `0x894f1d50`.
The input's bytes +8, +9 and +0xa are the MOD Original Level, Attack and Release Times (IDs
0x296…0x298). With Type 4 and an analog OSC 2 the tone's note-on turns OSC 2's code 10
(LA-SAW) into 2 (SAW). Mapped, not ported.

**MIX** (`0x89527d80`) is out = a·L1 + b·L2 with a and b moving toward +4 and +8. With the
converter's gains of 1 they start at their targets and stay, so the block is the plain
sum: L1 + L2, rounded once toward zero (`xtAnalogMix`). This is the path with the MOD off
too, so nothing about the mix is left out by not having a MOD Switch yet.

**OSC SYNC runs one way.** v79 called the mask "the other oscillator's OSC SYNC input".
It is not symmetric. The render clears both voices' mask inputs (+0x95c) each block, and
writes OSC 2's mask into OSC 1's only if the MOD's +0x40 is set, which only Type 4 does.
OSC 2's input stays 0 and OSC 1's mask goes nowhere. So with the MOD off neither
oscillator is synced. The port passes no mask.

**The per-routine constants.** The install's +0x9ac (v79, v85: "unexplained") has a
sibling: the MOD's chooser stores one per routine at +0x54, from the table at `0x897d0124`.
Both look like a cost per routine. I did not find what reads them, and the port holds
neither.

**Sound** (A4, Impact 0, from renders):
- OSC 1 SAW alone: 440 Hz at −14.9 dB, 880 Hz at −21.0 dB. OSC 2 SAW at Coarse +7: 659 Hz
  at −15.1 dB, 1319 Hz at −21.5 dB. Together: all of them, at the same levels, except
  where partials coincide.
- Unison: exactly twice OSC 1 alone, every sample, as both phases start at 0.
- OSC 2 SQUARE at Coarse −12 and Level 90: 219 Hz at −18.1 dB, beside OSC 1's unchanged.
- Both on against each alone, summed: at most 6e−8 apart, with a peak of 0.56.
- Against a PCM OSC 1 (SOLO, BACKING, ENSEMBLE): at most 5e−8 apart. So the two scales of
  OSC 1's buffer, ±1 for SOLO and int16 for the band players, both line up.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline only added 4
lines):
- The converter with the input's +2 set, on every waveform so far, the sub oscillator,
  SUPER-SAW and FEEDBACK-OSC: 500,000 cases, 0 different. Every converter pass now also
  compares the envelope and LFO numbers.
- Both voices in a tone object, 3,000 cases in fcheck, each with its own random waveform,
  knobs, pitch and level depths: the binding, the voice setup, the install, the MOD with its
  Switch off, the updates and the firmware's own render, with the COSM sections and TVA
  stubbed. 26,250 note-ons and updates, 126,000 oscillator blocks, 0 different. Compared
  every block: 42 fields of each voice, both buffers, the mix, both masks and the shared
  delay lines.
- One case in eight has OSC 1 off, one OSC 2 off. Phases go on breakpoints. Both on
  FEEDBACK-OSC, both on SUPER-SAW, and all eighteen lines taken so both voices share line 0.
- The order, from the firmware's own tone note-on (`0x894f5e60`) on a whole block, with the
  COSM sections, TVA and LFO note-ons stubbed: 1,500 cases, 0 different.
- Wide run, 21,000 cases: 183,750 note-ons and updates, 882,000 blocks, and the note-on on
  10,500, 0 different.
- Deliberate errors: 19 planted in the mix and in the check's wiring, 19 caught. At first
  four were missed, all real gaps:
  - The pitch envelope, pitch LFO and level LFO numbers: no case had those depths, so
    nothing read them. The tone pass now sets them.
  - The note-on order: the check called the firmware's setup itself, so reversing it
    reversed both sides. That is why the tone note-on pass exists.
  - Rendering OSC 1 before OSC 2 is caught too, though no routine reads the other's
    buffer yet: both voices sharing line 0 hear each other's writes.

**The wiring.** The passes above run the voices as the firmware does, but they do not
run XtSynth. Against that:
- The same settings through OSC 1 alone and through OSC 2 alone render the same bits:
  SAW, Fat and PW with their envelopes and LFO, the pitch envelope, the level with its KF,
  LFO and Wave Gain, the OSC amp envelope, the sub oscillator, SUPER-SAW, FEEDBACK-OSC,
  NOISE, HQ-SQUARE and LA-SAW.
- OSC 2's envelopes and LFO are separate objects, so that check also shows they are not
  OSC 1's.
- With OSC 2 off the renders are identical to v85's (`ab.sh`).

**Not checked.**
- Against real hardware. The check is the firmware in the interpreter.
- XtSynth as a whole against a firmware tone. The firmware's envelopes, LFOs and pitch
  ramps are checked apart (`xt_env`, `xt_lfo`, `xt_osc`), and here their outputs are set by
  the check. A run of the tone's note-on, update and render from a full tone block, compared
  with XtSynth's mix, would close this.
- An OSC's Switch or Type changed during a note. The port latches them at note-on. The
  firmware's update copies a new block into the voice (`0x894f2600`), and I did not trace a
  kind that changes.
- The firmware's pointer for an off OSC: the binding leaves it on Env 0 and LFO 0, which
  the port does not have. Nothing of an off OSC reads them.

**What PCM on OSC 2 needs.** It is the same voice code, so the players, converter and
pitch path are done and checked. What is left:
- A wave for it. The plugin has one sample and one `XtEncoded`. The XT binds a wave per OSC
  (`0x2be`, `0x2f2`), so this means a second load and encode, or OSC 2 sharing OSC 1's wave
  as a first step.
- The Time, Formant, Energy, Robot, Loop and Offset rows, re-attached by `attachOsc`, and
  the PITCH, TIME and FORMANT knobs. The fields are already in `OscControls`.
- The stereo flag. A voice's +0x960 is 1 for a stereo wave (not for HQ-SQUARE, v103). In
  Structures 0 and 1 both COSMs and the TVA take OSC 1's flag only (`0x894eef70`,
  `0x894f50e0`), so a stereo OSC 2 under a mono OSC 1 goes through their mono routines. The
  port's COSM and TVA treat both channels alike, which is right while both are the same on
  L and R, as every analog waveform is, and has to be looked at again for this.
- `xtAnalogMix` for its units, and the end of a wave: `tail` stops a voice only with OSC 2
  off, and would have to wait for both players.
- A pass with a PCM voice in the tone check.

**Plugin.**
- `XtSynth::Controls` now derives from `OscControls`, what an OSC has of its own, and holds
  a second one as `osc2`, off by default. `Voice` likewise from `OscVoice`, with `osc2`.
- OSC 1 keeps its fields' names, so nothing using them changed.
- Parameters: `oscSwitch` (OSC 1, on) and for OSC 2 each of OSC 1's own with `osc2` before it
  (`osc2OscSwitch`, off; `osc2OscType`; `osc2AnaWave`…), with OSC 1's ranges and defaults, at
  the end of the layout.
- Editor: an OSC 1 / OSC 2 selector and an On switch on the top row, like the COSM
  selector. It re-attaches the OSC's own rows, so `kXtStripH` does not change. With OSC 2
  selected PCM is disabled, and Encode, TIME and FORMANT stay OSC 1's.
- `XT_ANALOG2`, `XT_PENV2`, `XT_TENV2`, `XT_FENV2`, `XT_LFO2`, `XT_OLEV2`, `XT_OENV2`,
  `XT_VEL2`, `XT_PITCH2`, `XT_TUNE2`, `XT_OSC1OFF`; `TEST_OSC2`, `TEST_OSC2_PITCH`,
  `TEST_OSC2_ENV`, `TEST_OSC2_LEV`, `TEST_OSC2_LFO`, `TEST_OSC1_OFF`, `TEST_SNAPSHOT_OSC2`.

### The analog sub oscillator (2026-11-02, v85)

With Sub Octave above 0 an analog voice runs a second set of code, and most
waveforms play two waves in one routine, each with its own phase and shape. All of
it is ported. Left in the analog oscillator: X-MOD, which needs OSC 2. This
version also corrects the SH-4 interpreter's pair-move order (below).

**The map.** The install (`0x895069d0`, from the ordinary one at `0x89505170`):

| Code | Waveform | Routine with the sub oscillator |
|---|---|---|
| 0, 1 | HQ-SAW, HQ-SQUARE | their plain routines: no second wave |
| 2 | SAW | `0x89508f30` |
| 3 | SQUARE | `0x89509360` |
| 4 | TRIANGLE | `0x89509680` |
| 5 | RAMP | `0x89508960` |
| 6 | SINE | `0x89509230` |
| 7 | JUNO | `0x89508060` |
| 8 | NOISE | its plain routine: no second wave |
| 9 | LA-SQUARE | `0x89508670` |
| 10 | LA-SAW | `0x89508350` |
| 11, 12, 13 | SUPER-SAW, FEEDBACK-OSC, X-MOD | stubs `0x895066d0/e0/f0` jumping to SAW's; never reached, as their converter cases set +0xb4 = 0 |

The install also stores a different constant at +0x9ac per code, from the table at
`0x897d00a8`; it is still unexplained and the port does not hold it.

**The converter** (`0x894e288c`, patch waveforms 0…10 with Sub Octave above 0):
- +0xb4 = 1.0.
- +0x84, in octaves, is the Detune curve plus −2, −1 or 0 for Sub Octave 1, 2, 3.
- The curve (`0x894e1090`), from Detune d (+0x3a) and Fat's KF:
  - x = d·12000/63 within ±12000, then x/12000·63;
  - cents(x)/1200 + (key − 60)·KF·10/120/1200, within ±1. The 1/120 is stored as
    0x3c088889.
- cents(v), with v's sign: |v| to 10: v; to 15: 2v − 10; to 31: 5v − 55; to 51:
  25v − 675; above: 50v − 1950. So Detune ±63 is an octave, and +20 is 45 cents.
- +0x88 = cents(Fat Env depth)/1200 × the velocity factor; +0x90 = cents(Fat LFO
  depth, within ±63)/1200. The second wave's pitch therefore follows the OSC's third
  envelope and its LFO, the slots Fat otherwise uses. The Fat knob is unused.
- +0xb8 = level curve(Sub Level, +0x3c)/32768, for every waveform.

**The targets** (`0x89506bf0`, the note-on's first step and each update):
- +0x1c0 = powf(2, +0x84 + env·+0x88 + LFO·+0x90): sr, the second wave's pitch against
  the first's.
- w = +0x98 + env·+0x9c + LFO·+0xa0 within 0…1, without the plain code's span.
- HQ-SAW, HQ-SQUARE, NOISE: +0x154 = the gain target, +0x1c8 = w. NOISE's hold is as
  plain, set to 1 if 0 at every update, not only at note-on (w never gives 0).
- The others, with r = 2·ratio: +0x154 = gain·2^26, +0x200 = level·0.002, and +0x1f4 =
  r·k, +0x1f8 = (r·sr)·k, k = 0x3ba377ed. Each wave's Pulse Width is kept at least m
  from its ends (`0x89507480`), m by its own frequency, then ×0.002 (+0x1c8, +0x1ec):
  - SAW: |w − ½| with m = f/44100·0.35 (0x3eb33332), f = r·880 and sr·f.
  - TRIANGLE, SINE: w with m = f/44100·¼, same f.
  - SQUARE, JUNO: w with m = f/44100·¼, f = r·880 and r·440·sr·2. Also +0x234 and
    +0x23c = 44100/f·0.008 (0x3c03126f), the steepness.
  - RAMP: w with m = e/8, e = f/44100 as for SQUARE. Also +0x208 and +0x210 = e/16,
    the last segment's width.
- LA-SQUARE (a = r) and LA-SAW (a = ratio): the steps use a. The corner is 156·
  2^(7·Tone/127)·(1 + KF factor·(p − 1)), at most 20 kHz, with hz = a·440:
  - p is a·2^(9/12) for LA-SAW's first wave, but a·2^(−3/12) (0x3f5744fc) for
    LA-SQUARE's. The second wave's is a·sr·2^(9/12) for both.
  - If hz is above the corner: corner = hz and m = ½. Else m = a·220/corner, and for
    the second wave sr·hz/corner.
  - +0x224 = (corner/hz)·2·0.002; +0x22c = (corner/(sr·hz))·2·0.002.

The Tone KF table these index, `0x897edd0c`, holds the same bytes as v82's
`0x897d4c6c`, so the port reads one.

**The note-on** (`0x89506700`, after the ordinary note-on's common part at
`0x89504b98`):
- +0x158 = start gain·2^31; +0x1c4 = +0x1c0; +0x1cc and +0x1f0 = target/0.002; +0x204 =
  +0x200·32. So the level starts at 0.064 of its value and rises with a time constant
  of about 11 ms.
- p = the phase, scaled by Impact below 1: a second time, after the common part.
- HQ-SAW, HQ-SQUARE (the integrator cleared) and NOISE: +0x158 = start gain, +0x1cc =
  +0x1c8, p or 0 outside 0…1.
- SAW, TRIANGLE, SINE: p + ½·(+0x1cc limited by +0x208). SQUARE: the same + ½, and the
  steepness from its targets. RAMP, JUNO: p alone. Then +0x1d4 = p and +0x1fc = sr·p,
  both 0 if p is outside 0…1.
- LA-SQUARE, LA-SAW: x = p + ½·(+0x1cc limited by +0x20c) + ½, 0 outside 0…1; +0x1d4 =
  x, +0x1fc = sr·x; +0x218 and +0x21c = 2·(¼ − phase) + 2, less 2 if above 1. It reads
  +0x20c and +0x214 as limits.

**+0x20c stays 0, and v82's port stands.** The sub oscillator's update, note-on and
routines store to neither +0x20c nor +0x214. The check confirms it: every word of
+0x144…+0x27c that no port field holds is unchanged across the sub passes. What does
carry over is **+0x208**, which only RAMP's update writes and which later SAW,
SQUARE, TRIANGLE and SINE note-ons read. The port keeps it across notes (`rampK`).

**The routines** use x·0.998 + target every sample for the Pulse Widths, steepness and
level, x·0.96875 + target for the gain. The mask follows the first wave only.
- SAW, TRIANGLE: y = 2v − 1, v = p/w below w, else (1 − p)/(1 − w). Out = (y + second·
  level + Impact)·gain. A sync bit sets the phases to w (SAW) or w/2 (TRIANGLE).
- SINE: as TRIANGLE, y·(2 − |y|). The second wave is shaped after its level is applied.
- SQUARE: g = (w − |2p − 1|)·q within ±1, no shaping. Out = −(g + second·level −
  Impact)·gain. A sync bit sets the phases to (1 − w)/2.
- RAMP: 0…1 over 0…w, 1…−1 over w…1 − k, −1…0 over the last k. A sync bit sets 0.
- JUNO: x = 2(p + w/4) wrapped to 0…1; (|2x − 1| − 1 + w)·q, at most 2p − 1, at least
  −1. A sync bit sets 0.
- LA-SQUARE: g as SQUARE's, shaped g·(2 − |g|). Out = (−(g + second·level) + Impact)·
  gain. A sync bit sets 0.
- LA-SAW: as LA-SQUARE, each wave × its own second wave z (+0x218, +0x21c): z moves
  down by the wave's step twice a sample, adding 2 when it reaches −1, and multiplies by
  (2|z| − 1) shaped alike. The second wave's level comes before its z. A sync bit sets
  the phases to 0 and both z to ½.

**Correction: the interpreter's pair moves.** Under FPSCR.SZ = 1, `fmov @Rm,DRn` moves
two words. `Sh4Interp` put FR(n) at the higher address, an order it took from Ghidra's
little-endian model and never checked. On this chip it is the lower address: the
SH7750 Series Hardware Manual (Rev. 6.0: the note closing section 2.5, p. 54, and the
programming note on FPSCR.SZ, p. 52) says 64-bit data gets no endian conversion, which
is why in little-endian mode a double must be moved as two 32-bit words. The firmware
agrees.
RAMP's routine loads its constants this way, from 0.96875, 0.998 and 1.0, 0.998 at
`0x89508a90`:
- With FR(n) lower, the gain moves by 0.96875, the level and second width by 0.998,
  the wrap is at 1.0 and the width moves by 0.998, which is what each register is
  used for.
- With the old order the width never decays, the phase wraps at 0.998 and the gain
  settles 24 dB high. The port disagreed with the interpreter only there.

Flipping the order changes no earlier result: all of fcheck's baseline lines are
unchanged, and the A/B renders against v84 are identical. Logged across the harnesses,
only three instructions move pairs through memory: RAMP's two loads, and
SQUARE's store of L and R, which are equal. A simple scan of rom.2 finds this form in
39 stretches of code between `fschg`s (for example `0x89500a90`, `0x89509ff0`,
`0x8950a09c`, `0x8950b1b0`, `0x89527e2c`, and the stores at `0x895099ae…0x89509f3e`).
Whoever ports those should read the pairs this way; Ghidra's listing shows them as
plain `fmov.s`.

**Sound** (A4, Impact 0, Sub Level 127, against the 440 Hz partial, from renders):
- Sub Octave −2: 110 Hz at −2.4 dB. −1: 220 Hz at −4.8 dB. 0: in unison, twice the peak.
- Sub Level 80: 220 Hz at −10.9 dB. Sub Level 0: the same partials as OFF.
- Detune +20: the second wave at 226 Hz (45 cents).
- SQUARE, TRIANGLE, SINE, LA-SQUARE: 220 Hz at −1.2 dB. LA-SAW's 220 Hz is 2.6 dB above
  its 440 Hz; RAMP's is 2.3 dB and JUNO's 9.7 dB below. HQ-SAW and NOISE: no second wave.
- A2 and C7 with Sub Octave −1: 55 Hz at −3.2 dB, 1047 Hz at −4.5 dB.

**Checks** (`xt_analog`, further passes after the others; fcheck's baseline only
added 48 lines):
- The converter with Sub Octave, Sub Level and Detune, on waveforms 0…10: 200,000 cases,
  0 different. +0xb4 and +0xb8 are now compared in every converter pass.
- 3,000 cases in fcheck: 84,843 note-ons and updates, 34,280 blocks of each of eleven
  waveforms, 0 different. Each compares every field above whatever the waveform, so
  what a note leaves on the voice has to match too.
- LA-SQUARE and LA-SAW with random Tones and Tone KFs through `0x894e5f80`: 34,280 blocks
  each, 0 different.
- Plain and sub cases in random order on one voice: 31,124 note-ons and updates, 0
  different.
- Wide run, 21,000 cases: 594,000 note-ons and updates, 240,000 blocks each, 0 different.
- Blocks set each phase on 0, on w, on w/2 or RAMP's 1 − k, a step from 1, or where
  JUNO's x turns; LA-SAW's z where its subtractions land on −1. One case in eight
  plays Pulse Width ±63 on high notes, and JUNO's w is forced below 0.
- Deliberate errors: 132 planted, 126 caught. At first 13 were missed. Seven showed real
  gaps, now closed: the Tone KF's two constants and the 20 kHz limits never mattered at
  the default Tones (hence the pass above), z never started exactly on 1 for the second
  wave, and no note drives JUNO's x below 0. The other six are exact or unreachable: the
  hold of 0; HQ's phase test after the common part's; a ×2 regrouped; and a compare at
  equality in the rising wave, RAMP's first segment and JUNO's wrap, where both sides
  give the same value.

**Not checked.**
- Against real hardware: the check is the firmware in the interpreter, so RAMP rests
  on the manual and its constants, not on a listening test.
- A held note whose Sub Octave changes. The firmware's update reads +0xb4 live; I did
  not trace when it installs the routine again. The port latches +0xb4 at note-on, so
  a change takes effect on the next note.

**Plugin.** New parameters `anaSubOct` (OFF, −2, −1, 0; default OFF) and `anaSubLevel`
(0…127; default 127), and a fourth analog row holding Detune, Sub Oct and Sub Lev, so
`kXtStripH`, `setSize` and the limits rise by 30. PCM leaves that row empty. `XT_ANALOG`
and `TEST_ANALOG` take them as optional twelfth and thirteenth values. The PluginTest
is bit-identical with thirteen settings across the waveforms at notes 30, 60 and 100.

### The analog waveform FEEDBACK-OSC (2026-11-01, v84)

Code 12, patch waveform 12. With it every analog waveform is ported except
X-MOD, which needs OSC 2 (see the v83 section). Left: X-MOD and the sub
oscillator.

**The converter's case** (`0x894e22c0`), +0xb4 = 0, the Saw Slope at +0xb0:
- +0x98, the second tap's place, from the Pulse Width (`0x894e5210`): i = PW +
  64, 0 if not above 1, within 0…127; i/254 + (key − 60)·KF·½/12000, within
  0…½. +0x9c and +0xa0 are its depths, (d/63)·|d/63|·½ (`0x894e52c0`).
- +0x84, the second tap's level, from the **Detune knob** (`0x894e5320`), with
  Fat's KF: i likewise; 1 − ((127 − i)/127)² + (key − 60)·KF/12000, within 0…1.
  +0x88 and +0x90 are Fat's depths, (d/63)·|d/63| (`0x894e53b0`). The Fat knob
  is unused, as for SUPER-SAW.

**The update** (`0x895054c0`): +0x154 the gain target; +0x24c = the place's sum
within 0…½; +0x254 = the level's sum within 0…1.

**The note-on** (`0x89504d30`):
- +0x1c8 = +0x1cc = ratio·Slope·0.01; +0x250 = +0x24c; +0x258 = +0x254.
- +0x274 = 0 (the DC block's state), +0x278 = 0 (the write place).
- +0x27c = a delay line (`0x895049f0`): the first of eighteen flags at
  `0x89ff0c4c` that is 0, set to 1. If all are set it returns line 0 and sets
  nothing, so that voice shares line 0.
- The line is cleared (`0x895049c0`).

**The voice's stop** (`0x894f1fd0` → `0x895053c0`, for each oscillator): for code
12 with no sub oscillator it clears the flag (`0x89504a20`). The plugin has 16
voices and one oscillator, so it never runs out of the eighteen. The port frees
a stopped voice's line just before the next note-on allocates one, which only a
note-on could tell from the firmware's order.

**The delay lines.** Eighteen lines of 2,700 **signed bytes** at `0x89fe4e74`,
which end exactly at the flags. A sample is stored as ftrc(x·127), so the
feedback path is 8 bits.
- Write (`0x89504910`): w + 1, 0 at 2700; then the byte at the new w.
- Tap (`0x89504950`) t samples back: n = ftrc(t), f = t − n; i0 = w − n and i1 =
  i0 − 1, each + 2700 if negative; (line[i0]·(1 − f) + line[i1]·f)/127.

**The routine** (`0x89507900`). The block's values:
- step = ratio·440/44100 (0x3c2377ed);
- t = 1/step, halved until at most 2699 (`0x895048f0`);
- t2 = t·(½ + +0x250), so the second tap is ½ to 1 of t back;
- lev = +0x258·1.6.

Each sample:
- A sync bit sets the phase to 0, nothing else.
- v = p/s below s, else (1 − p)/(1 − s), then y = 2v − 1. The phase steps and
  wraps by adding −1, which sets the mask's bit.
- x = y·0.55 (0x3f0ccccc) + tap(t)·0.32 (0x3ea3d709) + tap(t2)·lev, within ±1.
- z = x·0.99804 (0x3f7f7f8d) + state; state = z·(−0.003919) (0xbb806af3) + state:
  a DC block.
- Write z·0.66 (0x3f28f5c2) to the line.
- out = (z + Impact)·gain·2^31. Impact by its decay, the gain 1/32 toward its
  target.

Once a block +0x250 and +0x258 move 0.2 of the way. The flags and Fat's and PW's
moves are not used.

**Sound** (A4; PW and Detune set the second tap, at PW 63 one period back):
- PW −63, Detune −63 (the second tap's level 0): a plain saw's harmonics, −6.0,
  −9.0, −12.1, −14.1 dB, because the remaining tap is one period back.
- PW 0, Detune 0 (0.75 of a period back, 1.2): the 4th harmonic equals the
  fundamental, with −9.2 and −10.1 dB between; −28 dB off the series.
- PW +63, Detune +63 (one period back, 1.6, so past 1 and limited): still
  harmonic, −14.6, −10.7, −15.9 dB, and about 8 dB louder than the other two.
- PW −30, Detune 40: −0.3 dB off the series, the largest partial at 1355 Hz: the
  loop rings at its own pitch rather than the note's.

**Checks** (`xt_analog`, further passes after the others):
- The converter's case: 200,000 cases, 0 different.
- 3,000 cases in fcheck: 9,425 note-ons and updates and 42,840 blocks, with
  +0x24c…+0x258, +0x274, +0x278, +0x27c; every eighth block and at both ends of
  a note, the eighteen flags and the voice's whole line: 0 different. The voice's
  stop runs the firmware's own reset.
- One case in eight runs 120 blocks, so the line wraps. At block 90 of those, the
  ratio is set for a period exactly at the 2699 limit, or past twice it, with the
  line full. One in eight plays the lowest notes. One in eight marks other lines
  in use, half of those all of them.
- Deliberate errors caught, all 37: the index's "not above 1" rule; 254; the
  KF's ½; each bound of each curve and target; the level's square, its clamp and
  its input; the depths' ½ and clamp; the note-on's state and write place; line 0
  when none is free; the flag not set; the line not cleared; the flag not freed;
  a tap's interpolation, wrap and /127; the halving as a single step and at ≥; the
  second tap on the target; 1.6, 0.55, 0.32 and both of the DC block's constants a
  unit off; each side of x's limit; the line's wrap; the written value; z for x in
  the output; the phase's wrap at 1.
  - 0.66 a unit off, and (0.66·127) grouped the other way, change a byte so rarely
    that they show only on the wide run (21,000 cases), not in fcheck's 3,000.

**Plugin.** FEEDBACK-OSC is enabled in the Waveform box. The PluginTest is
bit-identical with it at notes 30, 60 and 100, with modulation and at the
extremes of PW and Detune. XtAnalogShared now holds the lines and flags (one per
XtSynth, about 49 kB).

### The analog waveform SUPER-SAW (2026-11-01, v83)

Code 11, patch waveform 11. Left: FEEDBACK-OSC (code 12), X-MOD (code 13,
which needs OSC 2: see below) and the sub oscillator.

**The converter has its own case** (`0x894e2500`, from the switch on the
patch waveform − 11 at `0x894e206e`, table at `0x894e2084`: 11 `0x894e2500`, 12
`0x894e22c0`, 13 `0x894e208a`). +0xb4 = 0: never the sub oscillator's routines.
- **+0x98 is the saws' spread**, from the Pulse Width (`0x894e5060`):
  - i = PW + 64, 0 if not above 1, within 0…127;
  - (table[i]/2 + 2)/32767 + (key − 60)·KF·0.019532/12000, within 6.1e−5…
    0.019593. The table is 128 shorts at `0x897cef58`.
  - +0x9c and +0xa0 are its depths, (d/63)·|d/63|·0.019532 (`0x894e5120`), the
    envelope's × the velocity factor.
  - Outermost saws at PW −63, −32, 0, 32, 63: ±0.6, 10, 19, 47, 180 cents.
- **+0x84 is the side saws' level m**, from the **Detune knob** (+0x3a, ID
  0x30d), not Fat (`0x894e5140`):
  - i as above from Detune (stored ±63, centre 64);
  - (64·i + 400)/32767 + (key − 60)·KF·0.248055/12000, within 0.012207…
    0.260262. The KF is Fat's (+0x32).
  - +0x88 and +0x90 are Fat's depths, (d/63)·|d/63|·0.248055 (`0x894e51f0`).
  - The Fat knob itself is unused.
- **+0xb0 is the Saw Slope/100.** Correction to v79: the switch at `0x894e14a0`
  (table at `0x894e14f0`) sends patch waveforms 11, 12 and 13 to SAW's branch.

**The update** (`0x89505c90`):
- +0x154 = the gain target;
- +0x24c = the spread's sum, within 6.1e−5…0.019593;
- +0x254 = m's sum, within 0.012207…0.260262.

The bounds are the converter's, applied again.

**The note-on** (`0x89505bc0`):
- +0x1c8 = +0x1cc = ratio·Slope·0.01; this is the saws' s, fixed for the note.
- +0x250 = +0x24c, +0x258 = +0x254.
- The side saws' phases +0x25c…+0x270 start at a global counter, 0.1 apart
  (each sum wrapped from 1); the counter (`0x8987c894`) ends 0.6 on. Only this
  code uses it, so all voices share one sequence. It is 0 at startup. The port
  keeps it in `XtAnalogShared`, one per XtSynth.
- The centre's phase, +0x1d4, is kept from the last note as usual.

**The routine** (`0x89507520`):
- Seven saws, one phase of 0…1 a cycle. The step is ratio·440/44100
  (0x3c2377ed) for the centre, and step ± d·{1, 3.2, 5.6} for the six others
  (d = spread·step), in the order +, −, +3.2, −3.2, +5.6, −5.6.
- Each saw: p/s below s, else (1 − p)/(1 − s), then ·2 − 1. It rises −1…+1 over
  0…s and falls back over s…1.
- out = ((centre·0.2 + sum·m)·4(1 − 2.2·m) + Impact)·gain·2^31; the sum is
  added in order, the centre's product first in the FMAC.
- At Detune −63, 0, +63 the centre weighs 0.78, 0.56, 0.34 and each side saw
  0.05, 0.38, 0.45.
- The mask follows the centre only. Each sum wraps by adding −1.
- A sync bit sets the phases to 0, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6 (stored
  0x3e999999 and 0x3f199999, units off the nearest float).
- Once a block +0x250 and +0x258 move 0.2 of the way (0x3e4ccccd). It uses
  neither the flags nor Fat's or PW's moves.

**X-MOD needs OSC 2.** The voice's render (`0x894f6d40`) runs the oscillator at
voice +0x9d4 (OSC 2) first, into `0x89d2c910`, then the one at +0x20 (OSC 1)
into `0x89d2c810`, passing the other's buffer. The renderer (`0x894f2800`) hands
it on in r7. X-MOD (`0x89507b60`) reads that buffer, adds (L/2^31 + R/2^31)/2 ×
0.2·(+0x250, its Pulse Width's value) to its phase, and clears it, so OSC 2 is
consumed and not heard. For OSC 2 itself the converter sets X-MOD's +0x98,
+0x9c and +0xa0 to 0. It stays unported until OSC 2 exists.

**Sound** (A4):
- PW 0, Detune 0: the harmonic series as SAW's at −8.9, −11.9, −14.8, −16.8 dB;
  the energy off the series is −5.9 dB (the detuned saws).
- PW −63, Detune −63: nearly one saw, −28 dB off the series, as SAW's −28.
- PW +63, Detune +63: −0.4 dB off the series, with the largest partial at
  488 Hz, the +5.6 saw.

**Checks** (`xt_analog`, further passes after the others):
- The converter's new case: 200,000 cases with knob + matrix sums and velocity
  curves, 0 different.
- 3,000 cases in fcheck: 7,713 note-ons and updates, and 34,000 blocks, with
  every phase, +0x24c…+0x258 and the shared counter: 0 different.
- Every sixth block sets the centre on s or where a step lands on 1. Likewise
  each side saw, by its own step. One case in sixteen sets the counter where
  its step lands exactly on 1.
- Deliberate errors caught (32 of 33): the Slope off; the table's rounding; the
  index's "not above 1" rule; each KF constant; each bound in the converter and
  in the update; Fat for Detune; a depth's velocity factor; the LFO depth's
  clamp; the counter not stored, its step, its wrap at 1 exactly; the note-on's
  s; a current value not set; the step, 3.2, 5.6 and 2.2 constants; the order
  of the increments; the sync's phases; both wraps at 1 exactly; the sum's
  order; the centre's weight; 2^31 − 128 for 2^31; 1/32 for the move.
- Missed: the saw's turn at s as ≥, which is exact (both branches give 1 there).

**Plugin.** SUPER-SAW is enabled in the Waveform box, and a Detune knob joins
the analog rows (`anaDetune`, ±63, default 0, used only by this waveform so
far). `XT_ANALOG` and `TEST_ANALOG` take it as an optional eleventh value. The
PluginTest is bit-identical with it at notes 30, 60 and 100, with modulation
and at extreme PW and Detune.

### The analog waveforms: LA-SQUARE and LA-SAW (2026-11-01, v82)

Codes 9 and 10, the first two with note-on and update cases of their own.
Left: SUPER-SAW, FEEDBACK-OSC, X-MOD (codes 11…13) and the sub oscillator.

**The cases by code.** The first switch in the note-on (`braf` at
`0x89504bc8`, table at `0x89504bec`) and in the update (`braf` at
`0x89505448`, table at `0x89505494`), r2 = code − 9:

| Code | Waveform | Note-on | Update |
|---|---|---|---|
| 9, 10 | LA-SQUARE, LA-SAW | `0x89504bf6` | `0x8950549e` |
| 11 | SUPER-SAW | `0x89504d20` | `0x895054b0` |
| 12 | FEEDBACK-OSC | `0x89504d30` | `0x895054c0` |
| 13 | X-MOD | `0x89504e40` | `0x89505580` |

**The converter** sets the LA Tone settings for every waveform: +0xbc =
float of LA Squ Tone (+0xc206, ID 0x529) for patch waveform 10 and LA Saw
Tone (+0xc205, 0x528) for the others; +0xc0 likewise from the Tone Keyfollows
(+0xc208/+0xc207, 0x52b/0x52a). The defaults are Saw Tone 120, Square Tone
117, both Keyfollows 3.

**The targets** (`0x89505950`, from both the note-on and the update):
- Fat: x as usual, F = ½ − ½x, at or below 0.0039 it is 0.002. Then
  +0x1c0 = F·0.002.
- Pulse Width: p = 0.0039683 + 0.992063·x, not folded.
- The Tone is a corner frequency:
  - c0 = powf(2, Tone/127·7)·156, about 156 Hz to 20 kHz;
  - k = table[Tone KF], from the 15 floats at `0x897d4c6c` (−0.125, −0.0625,
    −0.03125, 0, 0.125 … 1, 1.25, 1.5, 2; KF 3 is 0, so the corner stays put
    by default);
  - c = c0·(1 + k·(ratio·2^(9/12) − 1)).
- With f = ratio·440 for LA-SQUARE and ratio·220 for LA-SAW:
  - above 20 kHz: c = 20000, m = f/20000;
  - else if 2f > c: c = 2f, m = ½;
  - else m = f/c.
- +0x1f4, the phase's step, is ratio·220/44100 for LA-SQUARE (0x3ba377ed)
  and half that for LA-SAW (0x3b2377ed).
- +0x224 = (c/f)·0.002, the steepness' target.
- +0x1c8 = w·0.002, with w = p within m…1 − m (`0x89507480`).
- +0x154 = −gain·2^26. The routine's move is per sample and its sign is
  inverted: gain = gain·0.96875 + target, which settles at −gain·2^31.

**The note-on** (`0x89504bf6`):
- +0x158 = −(start gain)·2^31.
- +0x1cc, +0x1c4 and +0x228 = their targets/0.002.
- +0x1d4 (the phase): 0.25; with Impact below 1, 0.25·Impact + (½ + ½·PW)·a,
  a being +0x1c4; 0 outside 0…1.
- +0x218 (LA-SAW's second wave): 1 + 4·((½ + ½·½)·a − phase), less 2 if
  above 1. So it sits in the same place in the cycle as the phase.
- It reads +0x20c twice, as the lower limit of the same clamp. Nothing in
  the voice's code writes +0x20c (`poolrefs.py` and the disassembly find no
  store), and the check finds it 0 throughout, so the port takes 0: the
  limits do nothing. (v85 read the sub oscillator's code too: it does not write
  +0x20c either.)

**The routines** (LA-SAW `0x895081f0`, LA-SQUARE `0x89508540`). They keep
their values in the FPU's two banks (`frchg`), and LA-SAW loads its targets
with `fmov XD,DR` pair moves under `fschg`, so Ghidra's `fmov fr1,fr0` there
means xf0, xf1 → fr0, fr1.
- A phase of 0…1 holds two cycles, the first over 0…a; x = 0…1 within its
  cycle. Flags: 2 in the first cycle, 1 in the second (the reverse of SAW's).
- g = (w − |2x − 1|)·q within ±1, then g·(2 − |g|). This is a pulse with
  parabolic edges: w sets its width, and q (+0x228, the corner's ratio to the
  wave) its edges' steepness.
- Per sample: a, w and q move by x·0.998 + target (0x3f7f7ced); Impact by
  its decay; out = (y + Impact)·gain.
- **LA-SQUARE**: y = g, at 2 × the phase's rate, so ratio 1 is 440 Hz. At the
  turn it compares ph > a. A sync bit sets the phase to 0 but also forces the
  second cycle's branch, so x starts at −a/(1 − a).
- **LA-SAW**: the phase runs at half LA-SQUARE's rate, and y = g·w(2 − |w|)
  with w = 2|z| − 1. z is the second wave (+0x218): it moves down by the step
  four times a sample (four subtractions, not 4·step) and adds 2 when it
  reaches −1. Each of g's half cycles flips the sign of one half of z's
  triangle, and the two rise together: a saw at twice g's rate, so ratio 1 is
  again 440 Hz. At the turn it compares a > ph. A sync bit sets the phase to
  0 and z to ½.

**Sound** (Fat 0, PW 0, default Tone):
- LA-SQUARE at A2/A4/C7: odd harmonics only, the even ones below −84 dB;
  −9.0, −14 and −17 dB for the 3rd, 5th and 7th at A4 (a square's are −9.5,
  −14, −17). At C7 the corner starts to show: −10.3, −17.8, −26.5.
- LA-SAW at A2/A4/C7: every harmonic: −8.9, −11.5, −15.2, −16.8 dB at A4.
  SAW's are −6.0, −9.0, −12.1, −13.9, so it is about 3 dB duller from the
  second harmonic. At C7: −9.1, −12.5, −17.4, −19.6.
- The energy off the harmonic series is −31…−34 dB (SAW −28).

**Checks** (`xt_analog`, a third pass after the others, so their lines are
unchanged):
- 3,000 cases in fcheck: 7,713 note-ons and updates, and 17,000 blocks of
  each, with +0x1f4, +0x218, +0x224, +0x228 and +0x20c: 0 different. The
  converter's check now also compares +0xbc and +0xc0 (0 different).
- A fourth pass sets random Tones (0…127) and Tone KFs (0…14) in the
  calibration through the firmware's `0x894e5f80`: 34,000 blocks of each, 0
  different. The port's defaults are only one point of those.
- Every sixth block sets the phase on a, or where a step lands exactly on 1.
  For LA-SAW, z is set where its four subtractions land on −1, or on 0 or 1.
- One LA case in eight plays Impact 0, PW 0: the second wave starting exactly
  on 1.
- Deliberate errors caught: the turn's compare in each routine; a flag; z's
  four subtractions as one; z's wrap at −1 exactly; the phase's wrap at 1
  exactly (both routines); z unshaped; the sync's z; LA-SQUARE's sync branch;
  0.998 a unit off; the mask's bit; logf(2) and 2^(9/12) a unit off; the Tone
  KF's index; the 20 kHz limit; the 2f limit; m's division; w's clamp; each
  step constant; f; Impact's two terms; z's start, at exactly 1 and not
  wrapped; the gain's sign and its move; the Tone's ID.
- Missed: `2f > c` as `2f ≥ c`, which is exact (c and m come out the same).

**Plugin.** LA-SAW and LA-SQUARE are enabled in the Waveform box. The
PluginTest is bit-identical with each, at notes 30, 60 and 100, with and
without key-follows, LFO and envelope depths.

### The analog waveforms: HQ-SAW and HQ-SQUARE (2026-11-01, v81)

The two band-limited waveforms, codes 0 and 1. With them the note-on's
whole default case is ported. Left: codes 9…13 (LA-SQUARE, LA-SAW,
SUPER-SAW, FEEDBACK-OSC, X-MOD) and the sub oscillator.

**How they work.** Both are a leaky integrator (+0x1e4) of a band-limited
impulse.
- The impulse is A (interpolated) × B (a window) × k3, fired as a scaled
  phase x crosses 0.
- Integrating impulses gives the edges. HQ-SAW adds a DC step every sample
  for its ramp.
- Lengths are in samples × 32768:
  - P = 2·722534400/(440·ratio), the period;
  - HQ-SQUARE's halves are L1 = H + d and L2 = H − d, with H = P/2 and d =
    (1 − 2p)·H. So L1 = P(1 − p) and L2 = P·p, p being +0x1cc.
- The impulse's width Pc is the shorter length (HQ-SAW: P), within 65536…
  4718590 (2 to 144 samples).
  - k1 = 8192/Pc; x = phase·k1, so the window is |x| < 8192.
  - x steps by k1·65536 a sample.
  - g = max(¼, ½ − 1.42845/(Pc·2^−15 − 1)), with 2^−15 stored a unit low.
  - k2 = g·2^−9/k1 (2^−9 a unit low), the step through A; k3 =
    g·3.41186e−5, the impulse's gain.
- The leak is 1 − 14614.5/P a sample.
- out = (y·2^47 + Impact·2^31 − 2^30)·gain. The −2^30 gives a DC of about
  −0.1 of full scale.
- The phase is kept as x/k1 (+0x1d4), in these units. A later note-on
  mostly finds it outside 0…1 and starts at 0.

**The tables.** The pointers at `0x8987c898` and `0x8987c89c` are set at
startup to A's centre `0x897d94dc` and B's centre `0x897e5ce0`.
- A runs over ±4608 floats: symmetric, 2.0 at its centre and 0 at its ends.
- B runs over ±8192: symmetric, 1 at its centre and 0.00095 at its ends.
- They lie end to end, A's last float just before B's first. The port
  reads the 25,602 floats from rom.2 at run time and checks the centres.
- The tap (`0x89506080`, inlined in HQ-SAW): for −8192 < x < 8192, t =
  k2·x, i = ftrc(t), and v = (A[i+1] − A[i])·(t − i) + A[i], × B[ftrc(x)].
  - The truncation is toward zero, so for negative t it extrapolates from
    A[i] toward A[i+1]. A is symmetric.
  - Outside the window there is no impulse.
- The tap for a sample is formed at the end of the sample before. So a sync
  bit clears x, y and the direction, but the sample still takes the old
  tap.

**HQ-SAW** (`0x895063f0`).
- x runs over ±P·k1. It wraps by −2·P·k1 as it passes P·k1, which sets the
  sample's mask bit.
- The impulse is added as x crosses 0, and −1.118/P every sample.
- Within 2…144-sample periods the window covers the whole period; below
  306 Hz the impulse is 144 samples wide.
- No Fat or PW; it moves neither.

**HQ-SQUARE** (`0x895060e0`).
- x rises to L1·k1, then reflects (x = 2·L1·k1 − x). That turn sets the
  direction (+0x1e8) and the mask bit.
- It falls to −L2·k1 and reflects again, setting the direction back to
  rising.
- Rising, the impulse is added; falling, subtracted. So the wave is high
  for the (1 − p) share.
- PW moves 0.2 of the way to its target once a block (`0x3e4ccccd`), not
  1/32. Fat is not moved.

**The note-on** clears +0x1e4 and +0x1e8 (`0x895060d0`) after setting the
Pulse Width target to p. The update only sets the target. Neither routine
sets the FPSCR: they run in the renderer's 0x40001, as the interpreter's
call sets it.

**Sound** (A4, Fat 0, PW 0, against SAW and SQUARE):
- the same harmonic series as SAW and SQUARE;
- the energy off the harmonic series falls from −29 to −62 dB (HQ-SAW) and
  from −24 to −63 dB (HQ-SQUARE);
- at key 100 (2.6 kHz), from −21 to −57 dB and from −22 to −51 dB.

**Checks** (`xt_analog`, a second pass after the other waveforms, so their
lines are unchanged):
- 21,000 cases: 54,000 note-ons and updates, and 120,000 blocks of each,
  with the state (+0x1e4, +0x1e8) and the masks: all 0 different.
- Every sixth block sets the phase on ±Pc and on HQ-SAW's P or HQ-SQUARE's
  L1 and −L2.
- Deliberate errors caught:
  - the leak's, g's, the DC step's, the −2^30's and k2's constants a unit
    off;
  - the 144- and 2-sample clamps;
  - the interpolation from A[i−1];
  - the window's lower edge;
  - the tap formed after a sync;
  - the phase as x·(1/k1);
  - the mask's bit;
  - L2 as P − L1;
  - PW moved by 1/32;
  - a mask bit at the turn at −L2;
  - the note-on not clearing the direction.
- Missed:
  - x ≥ L1 for x > L1, since x never lands on L1 exactly after a step;
  - an exact equivalent (2·L2·k1 − x as an addition).

**Plugin.** HQ-SAW and HQ-SQUARE are enabled in the Waveform box. The
PluginTest is bit-identical with each, at notes 30, 60 and 100.
TEST_ANALOG now rejects KFs that are not multiples of 10%: the plugin's
parameters step by 10, so another value snapped and made the test differ
from its reference. That is what a key-follow of "5" did in v80's test,
unseen at note 60.

### The analog waveforms: SQUARE, TRIANGLE, SINE, RAMP, JUNO, NOISE (2026-11-01, v80)

This version ports six more analog waveforms, the rest of the note-on's
default case except its HQ pair. Left: HQ-SAW and HQ-SQUARE (codes 0, 1),
and the five with note-on cases of their own (LA-SQUARE, LA-SAW, SUPER-SAW,
FEEDBACK-OSC, X-MOD, codes 9…13). The map is in the v79 section below.

**The note-on and the update by code.**
- The note-on (`0x89504af0`) sends codes 9…13 to their own cases (table at
  `0x89504bec`). Codes 0…8 share the default case (`0x89504e80`): SAW's
  Fat, then p = 0.0039683 + 0.992063·x for the Pulse Width, then a switch
  on the code (table at `0x89504fb0`).
- The Pulse Width's target (+0x1c8) by code; the note-on also sets +0x1cc
  to it:

| Code | Waveform | +0x1c8 | Also |
|---|---|---|---|
| 0, 1 | HQ-SAW, HQ-SQUARE | p | `0x895060d0` sets up its state |
| 2 | SAW | folded, + the Slope (v79) | |
| 3 | SQUARE | p | +0x1e0, the edge |
| 4, 6 | TRIANGLE, SINE | p/2 | |
| 5, 7 | RAMP, JUNO | p | |
| 8 | NOISE | p | +0x248, the hold |

- The update (`0x89505400`, table at `0x89505698`) does the same, without
  the HQ setup and without the note-on's 0 → 1 for NOISE's hold.
- SQUARE's edge, +0x1e0 = Slope × (ratio × f) × 0.00498866 (0x3ba377ed, a
  unit below the phase step's 0x3ba377ee).
  - f steps with the pitch: 0 below 2^(−2/12) (G4), 0.05 below 1 (A4), 0.1
    below 2^(1/12), 0.15 below 2^(4/12), 0.2 below 2^(9/12) (F#5), 0.25
    above.
  - The Slope is the Square Slope/100 (2 by default) with Alias Cancel on,
    else 0: hard edges.
- NOISE's hold, +0x248 (a byte), is ftrc of 1 + 30·(1 − 2p) below p = ½,
  else of 1 − 30·(1 − 2p). So PW 0 holds 1 sample (white noise), and the
  extremes 31.

**The install** (`0x89505170`, from `0x894f2010` for a kind-3 voice):
- +0x960 = 0; the code's routine at +0x968, and a constant at +0x9ac from
  the table at `0x897d0070` (2.50969e−5 HQ-SAW, 5.69826e−6 SAW, 6.37251e−6
  SQUARE, …).
- With the sub oscillator on it jumps to `0x895069d0` instead.
- The check runs it after the note-on and finds the map's routine for every
  code.

**The routines.** They share SAW's frame (v79):
- a = +0x1c4 (Fat), p = +0x1cc, and a phase of 0…1 holding two cycles, the
  first over 0…a;
- the same per-sample tail: ((y + Impact)·gain)·(2^31 − 128), and the 1/32
  moves;
- each returns its wrap mask: bit i where the half-cycle flags (1 in the
  first cycle, 2 in the second) have both been seen.

Each waveform's breakpoints and segments:
- **SQUARE** (`0x89508aa0`). pa = p·a, b = a + p − pa.
  - −1 to pa, +1 to a, −1 to b, +1 to 1.
  - Each edge is a ramp over ±s around it (s = +0x1e0): y = (ph −
    edge)/s, or (edge − ph)/s on a falling edge. The edge at 0/1 is split:
    −ph/s and (1 − ph)/s.
  - Flags: 1 below a − s, 2 from there.
  - With s = 0 the ramps are never reached, so 1/s = inf is never used.
  - The Pulse Width widens the low parts: PW +40 is high for 18% of a
    cycle.
- **TRIANGLE** (`0x895094c0`), p halved.
  - Each cycle rises 0…1, falls 1…−1 and rises −1…0.
  - The first over 0…a·p, …a − a·p, …a; the second over a…p + a − a·p,
    …1 − p + a·p, …1.
  - Odd harmonics at 1/n².
- **SINE** (`0x89509030`, FPSCR 0x40801, the invalid-operation trap on), p
  halved.
  - TRIANGLE's frame with eight parabolic quarters: x·(2 − x) toward each
    peak and x·(x + 2) toward each trough, x the quarter's −1…1.
  - The peaks at a·p and p + a − a·p; the zero crossings at a/2 and ½ +
    a/2.
  - Two quarters' slopes are 1/d for the same d formed by two different
    sums; the port keeps both.
  - The third harmonic is at −29 dB at Fat 0, PW 0.
- **RAMP** (`0x89508800`). SAW's breakpoints with d = p, without the fold
  or the inversion.
  - Each cycle rises 0…1 (over 0…a·p and a…b) and falls 1…−1 (over a·p…a
    and b…1).
  - At PW 0 it is near a triangle that steps from −1 to 0 at each cycle's
    start, with a DC of +¼ of its peak.
- **JUNO** (`0x89508db0`). A rising saw each cycle (−1…1 over 0…a and
  a…1).
  - It is replaced by −1 − Impact while a pulse at twice its rate is low:
    over the first (1 − p) share of each half cycle.
  - At PW 0 its second harmonic is 4.9 dB above the fundamental.
- **NOISE** (`0x89504a40`). Two 32-bit states:
  - s1 at +0x1dc, the flags word, cleared at every note-on;
  - s2 at +0x244, which the note-on never sets, so it runs on from note to
    note (the port starts it at 0);
  - s1 = −250·s1 + 127·s2 + 30, then s2 = 252·s2 + s1.
  - out = (Impact + float(s1))·gain; s1 is already ±2^31.
  - Each value is held for the hold's samples, and the hold starts again
    every block.
  - It ignores the phase, the sync mask and Fat; its PW and Fat are not
    moved; it returns 0.

**Correction to v79.** SAW returns its wrap mask like the others (see
there). The check now compares every routine's returned mask.

**Port and checks** (`xt_analog`):
- Each voice case takes one of the seven waveforms in turn.
  - The install's routine is checked against the map.
  - Every block compares the outputs, the state (with +0x1e0, +0x244 and
    +0x248) and the returned mask.
  - Every sixth block sets the phase exactly on one of the routine's own
    breakpoints.
  - One case in eight plays G4, A4, A#4, C#5 or F#5 untuned: SQUARE's
    pitch steps.
- 21,000 cases: 189,000 note-ons and updates and 120,000 blocks of each
  waveform, all 0 different. fcheck runs 3,000 cases.
- Deliberate errors caught (`PORT=analog mutate.sh`):
  - SQUARE: the edge constant a unit off, the A4 step's > as ≥, the edge
    ramp's sign, a flag;
  - TRIANGLE: the last breakpoint by another sum, a segment's > as ≥;
  - SINE: the shorter sum for its second 1/d, a quarter's parabola, PW not
    halved;
  - RAMP: the rising slope doubled, Impact dropped;
  - JUNO: the gate's > as ≥, the low value's Impact sign;
  - NOISE: the increment, the hold, Impact;
  - the mask's bit; PW's 1/32 move.
- Missed:
  - exact equivalents: independent statements reordered, a sign moved
    between numerator and denominator, (1 + a)/2 as ½ + a/2;
  - SINE's p + a − a·p by the other sum, which gave the same value in
    every case tried.

**Plugin.** The Waveform box now enables SQUARE, TRIANGLE, SINE, RAMP, JUNO
and NOISE. The PluginTest is bit-identical with each.

### The analog oscillator, and SAW (2026-11-01, v79)

OSC Type ANALOG plays one of fourteen DSP routines, chosen by the waveform.
This version maps the analog oscillator and ports SAW, the default
waveform, without the sub oscillator.

**Where it runs.**
- The OSC converter dispatch (`0x894e48a0`) picks by OSC Type (input +9):
  0 ANALOG → `0x894e1920`, 1 PCM → `0x894e2f20` (VariPhrase), 2 EXT IN →
  `0x894e3830`.
- A voice of kind 3 (analog) has its own routines: install `0x89505170`
  (from `0x894f2010`), note-on `0x89504af0`, the 10-ms update `0x89505400`,
  and a FEEDBACK-OSC reset `0x895053c0` (from `0x894f1fd0`).
- The per-block renderer (`0x894f2800`) calls the routine at voice +0x968
  (X-MOD: +0x96c) as fn(out, voice + 0xd4, the sync mask at +0x95c), 32
  stereo frames. It returns the mask of samples where the phase wrapped:
  OSC SYNC's input. (Corrected in v86: only OSC 2's reaches OSC 1, and only with
  MOD Type OSC SYNC.)
- With the sub oscillator on (voice +0xb4 = 1.0) a second set runs: install
  `0x895069d0`, note-on `0x89506700`, update `0x89506bf0`.
- The voice setup (`0x894f21c0`) gives a kind-3 voice rate ratio 1, no
  offsets, and root frequency 440 (+0x9a4), as for Robot Voice.

**The waveforms.** The patch numbers them in the display's order. The
converter turns them into codes (+0x80), and the install routine switches on
the code:

| Patch | Waveform | Code | Routine |
|---|---|---|---|
| 0 | SAW | 2 | `0x89508c20` |
| 1 | SQUARE | 3 | `0x89508aa0` |
| 2 | TRIANGLE | 4 | `0x895094c0` |
| 3 | SINE | 6 | `0x89509030` |
| 4 | RAMP | 5 | `0x89508800` |
| 5 | JUNO | 7 | `0x89508db0` |
| 6 | HQ-SAW | 0 | `0x895063f0` |
| 7 | HQ-SQUARE | 1 | `0x895060e0` |
| 8 | NOISE | 8 | `0x89504a40` |
| 9 | LA-SAW | 10 | `0x895081f0` |
| 10 | LA-SQUARE | 9 | `0x89508540` |
| 11 | SUPER-SAW | 11 | `0x89507520` |
| 12 | FEEDBACK-OSC | 12 | `0x89507900` |
| 13 | X-MOD | 13 | `0x89507b60` (at +0x96c) |

The install also stores one constant per code at +0x9ac (about 1e−5;
5.698e−6 for SAW), not yet explained. HQ-SAW is a leaky integrator of a
band-limited impulse built from two rom.2 tables (0x897d94dc, 0x897e5ce0;
pointers at 0x8987c898/c89c), minus a DC term.

**The converter's input** (the OSC table updater `0x894e55d0`). I found the
layout by running the updater in the interpreter with the parameter getter
hooked to return a tag per ID:
- +0x24 Waveform (ID 0x2b1), +0x25 Impact;
- +0x26 Pulse Width, +0x28 PW KF ×10, +0x2a PW LFO depth, +0x2b PW Env
  depth, +0x2c/+0x2e the PW envelope's Velocity Curve and Sens;
- +0x30 Fat, +0x32 Fat KF ×10, +0x34 Fat LFO, +0x35 Fat Env, +0x36/+0x38
  the Fat envelope's pair;
- +0x3a Detune (0x30d), +0x3b Sub Octave, +0x3c Sub Level.

**The converter** (`0x894e1920`). Its first part is the VariPhrase
converter's: the pitch, the Pitch knob, the pitch envelope's and LFO's
depths, the level and its LFO depth. The cents are against A4: +8 =
(pitch10 − 69000)/10, within ±6000, as for Robot Voice. Then:
- +0x80 the code; +0xac Impact·4/40; +0xb0 the Saw or Square Slope
  (`0x894e14a0`, below).
- +0x5c = 2^(Wave Gain/6) × the waveform's level at the note's pitch
  (`0x894e1350`, below), in place of the VariPhrase's level curves.
- +0x98 the Pulse Width (`0x894e0ea0`):
  - x = PW (within ±63) + (key − 60)·KF·126/1200 + 63;
  - its integer part (toward zero) is wrapped into 0…126, the fraction
    kept, then /126.
  - So at PW KF 100% the width sweeps its whole range once an octave.
- +0x9c its envelope depth, (d/63)·|d/63| × the velocity factor; +0xa0 its
  LFO depth, (d/63)·|d/63|, d within ±63.
- +0xa4/+0xa8 its envelope and LFO: 2 and 1 (OSC 2: 6 and 2).
- Sub Octave OFF: +0xb4 = 0.
  - +0x84 Fat (`0x894e1010`): F/127 within 0…1, then + (key − 60)·KF/1200,
    within 0…1.
  - +0x88/+0x90 its depths, as the Pulse Width's.
  - +0x8c/+0x94 its envelope and LFO: 3 and 1 (OSC 2: 7 and 2).
- Sub Octave on: +0xb4 = 1.0, and +0x84 holds the Detune curve
  (`0x894e1090`) plus −2, −1 or 0 octaves. Its envelope depth is piecewise
  in cents (to 10: d; to 15: 2d − 10; to 31: 5d − 55; to 51: 25d − 675;
  above: 50d − 1950), /1200. Ported in v85.
- +0xb8 Sub Level through the level curve; +0xbc/+0xc0 the LA Tone settings.

The voice binds +0x8c's envelope at +0x974 and +0xa4's at +0x978. So Fat
follows the OSC's third envelope (the formant slot) and the Pulse Width its
second (the time slot).

**The analog calibration.** Hidden system parameters 0x528…0x55b go to the
OSC table at +0xc204… (`0x894e5f80`):
- LA Saw and LA Square Tone, and their Tone Keyfollows (+0xc205…+0xc208);
- Alias Cancel (+0xc209, default on), Saw Slope (+0xc20a, 100), Square
  Slope (+0xc20c, 200);
- for each waveform SAW…LA-SQUARE, four bytes from +0xc20e: KF Bias Point,
  Upper Level KF, Lower Level KF, Level. The KFs are stored as value − 128
  (the getter's value). Saw: 60, −15, +20, 68.

`0x894e1350` uses them for the waveform's level, by the patch's waveform
(SUPER-SAW, FEEDBACK-OSC and X-MOD take the Saw's):
- level curve(Level)/32768 × max(0, 1 + d·KF/6e6);
- d = pitch10 − bias·1000, with the Upper KF above the bias point and the
  Lower KF (and −d) below it.
- The Saw loses 3% an octave above C4 and gains 4% an octave below.

`0x894e14a0` gives SAW the Saw Slope/100 and SQUARE the Square Slope/100
when Alias Cancel is on, else 0. (Corrected in v83: SUPER-SAW, FEEDBACK-OSC and
X-MOD take the Saw's too.) The port uses the descriptors' defaults. A
unit could hold other values, if they were set at the factory.

**Note-on** (`0x89504af0`, every waveform):
- +0x1bc = the code. +0x144 = the pitch ratio: `0x894f3960`, which is
  xtPitchTarget without the detune term.
- +0x154/+0x158: the gain target (`0x894f3dc0`) and its start
  (`0x894f3e20`).
- +0x1dc, the routine's flags, = 0.
- Impact: +0x1d8 = +0xac. Its decay per sample, +0x1d0, is powf(3.05185e−5,
  ratio/100.227), so Impact falls 90 dB in one period.
  - The firmware's powf is its expf core (`0x89531a28`) of y × its logf core
    (`0x89531b78`). With the constant base it is xtCosmExpf(K(0xc1265ad5)·y),
    equal on 400,000 cases.
- The phase, +0x1d4, is kept from the voice's last note. It is scaled by
  Impact when Impact is below 1, then set to 0 if outside 0…1.
- Fat: x = clamp(+0x84 + env·+0x88 + LFO·+0x90, 0…1). +0x1c0 = +0x1c4 =
  ½ − ½x; at or below 0.0039 it is 0.002.
- Pulse Width: x = clamp(+0x98 + …, 0…1), p = 0.0039683 + 0.992063·x.
  - SAW folds it to ½ or above (1 − p below ½).
  - It adds s = ratio·Slope·0.01 (0.01 is stored a unit low), at most 1 − s.
  - +0x1c8 = +0x1cc = the result.

**The update** (`0x89505400`):
- +0x144 = the pitch target: no 70% step, so the analog pitch has no glide;
- +0x154 = the gain target;
- +0x1c0 moves 70% of the way toward ½ − ½·Fat;
- +0x1c8 = SAW's folded Pulse Width.

**SAW** (`0x89508c20`, FPSCR 0x40001):
- The phase steps by ratio·220/44100 (0x3ba377ee). A phase of 0…1 holds two
  cycles, so ratio 1 is 440 Hz.
- a = +0x1c4 (Fat); d = ½ − +0x1cc (PW), plus 1 with the wave inverted when
  negative. The breakpoints are a·d, a and b = a + d − a·d.
- The wave rises over 0…a·d and a…b and falls over a·d…a and b…1, each
  segment linear from −1 to +1.
  - At Fat 0 the two cycles are equal saws.
  - Fat moves a, and the unequal cycles add the sub-octave: Fat 64 on A2
    puts 55 Hz at +2.3 dB to the 110 Hz fundamental.
  - At Fat 0 and PW 0, the harmonics fall as 1/n (−6.0, −9.6, −12.1 dB…).
- out = ((y + Impact)·gain)·(2^31 − 128), on L and R. The gain moves 1/32
  toward its target every sample, and Impact by its decay.
- Once a block, Fat's and PW's current values move 1/32 toward their
  targets.
- A set bit in the sync mask resets the phase at that sample.
- It returns the wrap mask: r2 starts at 0, collects the bits, and moves to
  r0 in rts's delay slot. (Corrected in v80: this section first said it
  returned an address.)
- With the Slope on, d is never 0, so the inverted branch always runs.

**Port and checks** (`xt_analog`, new; the others unchanged):
- the converter: 200,000 cases over SAW…LA-SQUARE, with knob + matrix sums
  and velocity curves: the analog block, +8 and +0x5c;
- note-on and update: 27,000 cases, the voice's fields;
- SAW: 120,000 blocks, the outputs and the state, with sync masks and with
  the phase set exactly on the breakpoints;
- all 0 different. The check sets the calibration by running `0x894e5f80`
  with the getter returning the descriptors' defaults.
- Deliberate errors are caught (`PORT=analog mutate.sh`):
  - the Slope's 0.01 a unit off;
  - a segment's > as ≥ (two of three; the third gives the same value at the
    point);
  - Impact added after the gain;
  - 2^31 for 2^31 − 128;
  - the flags' reset;
  - the PW wrap one off;
  - the level KF's side;
  - the phase not scaled;
  - the decay's grouping.

**Plugin.**
- OSC Type (ANALOG, PCM; PCM the default) sits on the ROM row.
- ANALOG shows the Waveform (only SAW enabled), Impact, Pulse Width and
  Fat, with their KFs and their LFO and Env depths, where the Encode,
  VariPhrase and Loop rows are.
- An analog patch plays without a sample. Its level and pitch run through
  the same OSC TVA, COSM sections and patch TVA as a sample's.
- PW follows the time envelope and Fat the formant envelope.

### COSM: the Speaker Simulator (2026-11-01, v78)

COSM Type 4, block code 5: a fixed linear filter for each of twelve
Speaker Types. It has no envelopes, LFO, buffers, start values, note-on case
or update. This is the last COSM type: all sixteen are now ported.

**Parameter**: 0x8ea, Speaker Type 0…11 (shown 1–12), default 0.

**The converter** (`0x894dcef0`): +0 code 5, +4 the type as a byte. The
block case (`0x894ee520`) only stores the type (OBJ+0x14). A new type takes
effect at the next block, with no crossfade.

**The filter** (`0x89501300`; the stereo table's entry is `0x895014c0`):
- It uses the type's 27 floats C at 0x897d4670 + 0x6c × type, one sample at
  a time, from L.
- A state-variable stage (C8 its frequency, C9 its damping, about −0.99 or
  −0.5):
  - lo += C8·band;
  - hp = x + C9·band − lo;
  - band += C8·hp.
- Two all-pole sections:
  - y = C0·hp + C1·y' + C2·y'';
  - w = C3·y + C10·w' + C11·w''.
- Three biquads in direct form I:
  - o1 = C12·w + C13·w' + C14·w'' + C15·o1' + C16·o1'';
  - o2 (C17…C21) on o1;
  - o3 (C22…C26) on o2.
  Several types leave some as a plain gain (0.9999, 0 …).
- A last section, o4 = C4·o3 + C5·o3'' + C6·o4' + C7·s. Two register quirks:
  - it takes o3 two samples back where o3' would be expected;
  - s (D+0x58) is never written, so after note-on that term is 0.
- out = o3 + 0.5·o4, on L and R.
- The states D+0x1c…0x58 (fr2…fr15) clear at note-on.
- The filter is linear, so it runs at any scale.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 2,000 cases;
- note-on (the firmware's own) and type changes through the block
  dispatcher: 7,667 cases;
- the filter: 120,000 blocks, all twelve types, at levels from 1 to 2.1e9;
- all 0 different.
- Deliberate errors are caught:
  - o4 from o3' instead of o3'';
  - 0.25 for 0.5;
  - the state-variable sum regrouped;
  - o1's feedback states swapped;
  - the table one type off;
  - o4's state given o3.

**Plugin.**
- COSM Type gains Speaker Simulator (index 15), with its Speaker Type
  (cosmSpkType, cosm2SpkType) on one row.
- The COSM Type box is 30 px wider, so "Speaker Simulator" fits.
- Responses, from the impulse (dB):

| Type | 50 Hz | 100 | 200 | 500 | 1k | 2k | 4k | 6k | 8k | 12k |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | −23.8 | −11.1 | 0.1 | 0.9 | 0.8 | 1.8 | 3.0 | 4.9 | −7.1 | −23.5 |
| 2 | −23.4 | −9.7 | 6.6 | 3.0 | 3.7 | 4.0 | 3.2 | −19.8 | −31.5 | −45.2 |
| 3 | −17.0 | −4.1 | 2.7 | 1.1 | −1.6 | −7.1 | 2.9 | −3.5 | −9.3 | −22.8 |
| 4 | −11.0 | 0.3 | 1.7 | 1.8 | 1.4 | 2.3 | 3.4 | 5.0 | −7.5 | −25.2 |
| 5 | −18.5 | −5.6 | 1.3 | 0.5 | −0.4 | −0.5 | −9.5 | −6.9 | −16.8 | −30.8 |
| 6 | −18.5 | −5.6 | 1.7 | 4.7 | 2.7 | 3.6 | 3.6 | −5.3 | −19.2 | −33.9 |
| 7 | −19.2 | −4.2 | 4.0 | 6.7 | 3.1 | 2.1 | 0.1 | −9.4 | −20.6 | −34.8 |
| 8 | −15.5 | −1.1 | 6.6 | 3.4 | 3.1 | 2.8 | −6.4 | −9.7 | −21.5 | −35.3 |
| 9 | −15.4 | −0.5 | 7.6 | 3.4 | 2.0 | −7.1 | −7.4 | −15.9 | −26.8 | −40.0 |
| 10 | −14.1 | 0.5 | 7.4 | 0.2 | −2.4 | 1.9 | 4.4 | 0.4 | −14.8 | −30.3 |
| 11 | −18.2 | −3.5 | 7.4 | 5.3 | 2.5 | 1.0 | 2.3 | −16.3 | −28.9 | −42.9 |
| 12 | −8.2 | 8.0 | 4.8 | 9.3 | 3.3 | 1.9 | 4.1 | −11.5 | −18.4 | −40.3 |

All roll off below 100 Hz and above 6–8 kHz, like guitar cabinets.

### COSM: the Amp Simulator (2026-11-01, v77)

COSM Type 3, block code 4: three guitar amps, each a chain of one-pole
filters, double-rate clippers and a cubic shaper with a Bass/Middle/Treble
stack. It has no buffers. Only slot 1 (Gain's Env A) and LFO 3 (4) are
bound.

**Parameters** (0x8e2…0x8e9, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x8e2 | Amp Type (shown 1–3) | 0…2 | 0 |
| 0x8e3 | Bass | 0…127 | 64 |
| 0x8e4 | Middle | 0…127 | 64 |
| 0x8e5 | Treble | 0…127 | 64 |
| 0x8e6 | Output Level | 0…127 | 127 |
| 0x8e7 | Gain | 0…127 | 0 |
| 0x8e8 | Gain LFO Depth | ±63 | 0 |
| 0x8e9 | Gain Env Depth | ±63 | 0 |

**The converter** (`0x894dcd80`) writes the block:
- +4 Amp Type, a byte;
- +0x1c Bass·4/127, +0x20 Middle/127, +0x24 Treble·2/127, each clamped to
  0…127 first (Treble with a controller term);
- +0x28 Level and +8 Gain, through the 128 shorts at 0x897cfcb8 / 32768;
- +0xc Gain's Env depth (A), +0x10 its envelope (9 or 11), +0x14 its LFO
  depth, +0x18 its LFO.

**The object** (start values `0x894ec540`; block copy `0x894ecfe0`; update
`0x894ee920`):
- Gain' = 1 + 7 × clamp(Gain + Env·depth + LFO·depth, 0…1): ×1 to ×8
  (D+0xbc; target D+0xa4).
- Note-on (`0x894ee170`) sets Level, Treble, Middle and Bass in use
  (D+0x8c…0x98) to the block's.

**The filter** (`0x894f8a30`; the stereo table's entry is `0x894fa890`):
- It calls `0x894f79d0`, `0x894f7f00` or `0x894f8450` by Type.
- Per block:
  - Level, Treble, Middle and Bass slew 0.1 toward the block's;
  - Gain' steps before every fourth sample.
- Each amp works on u = x · 2^−31 (one unit low, 0x2fffffff) · Gain', and
  ends × 2^31 × Level.
- The shared pieces:
  - one-pole stages s += k·(in − s): their output s is a lowpass, and in − s
    (before the step) a highpass;
  - the clip stage: c = clip(a·v, ±0.5) and m = clip(a/2·(v + v'), ±0.5),
    out = 0.25·c + 0.25·c' + 0.5·m (a = 2 or 16), so it clips at twice the
    rate;
  - the cubic (y³ − 0.5·y² − 4·y)/7 (0x3e12491a, 0xbd92495d, 0xbf12492b);
  - the stack: Treble × the highpass + Middle × the band (a lowpass's step)
    + Bass × the lowest lowpass.

The three chains (k from rom.2; a for each clip stage):

| Type | Chain |
|---|---|
| 0 | highpass 0.007256 → lowpass 0.61676 → clip (a 2) → cubic → lowpass 0.130608 → clip (a 16) → cubic → stack (0.230741, 0.0384568) |
| 1 | highpass 0.0029024 → lowpass 0.58048 → clip (a 2) → cubic → stack (0.239448, 0.062918) → clip (a 16) → cubic → highpass 0.0058048 |
| 2 | highpass 0.002837 → stack (0.231466, 0.0246704) → clip (a 2) → cubic → highpass 0.0166888 → clip (16, see below) → cubic → lowpass 0.2721 → highpass 0.010884 |

Quirk: Type 2's second clip stage loads the constant 16 where Types 0 and 1
multiply the signal by 16 (`fmov fr0,fr3` for `fmul fr0,fr3`), in all four
unrolled copies.
- That term always clips to 0.5.
- The stage outputs 0.25 + 0.5·clip(8·(v + v')), so the cubic sees a bias of
  0.25 and only the midpoint's signal.
- The last highpass removes the bias from the output.

The states are D+0x1c…0x40 (Type 0 leaves D+0x28 unused), cleared at
note-on.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases;
- note-on (the firmware's own), updates and new blocks: 22,871 cases;
- the filter: 120,000 blocks, all three Types, at levels from 1 to 2.1e9;
- all 0 different.
- Deliberate errors are caught:
  - Type 2's quirk replaced by 16·v;
  - Type 0's midpoint from the new state alone;
  - a one-sided clip;
  - the cubic's y² sign flipped;
  - 8 for 7;
  - 2^−31 as C rounds it;
  - Treble and Middle swapped;
  - Bass not slewed;
  - Gain' stepped every sample;
  - Type 1's last highpass a unit off.
- XtSynth runs it in the XT's units, as Lo-Fi.

**Plugin.**
- COSM Type gains Amp Simulator (index 14), with its own parameters
  (cosmAmp… and cosm2…) at the XT's defaults. Its rows are Amp (Type, Gain
  and its depths, Level) and Amp EQ (Bass, Middle, Treble).
- Measured on a 441 Hz sine (EQ at 64, Level 127; levels as peaks in the
  SOLO buffer, ±1):
  - Types as shown (1–3, the firmware's 0–2). At Gain 0, quiet sines gain
    +19.7, +23.1 and +11.6 dB for Types 1–3.
    A −20 dB sine already clips to odd harmonics, 3rd about −10 to −14 dB.
  - Gain 127 adds 18 dB more on quiet sines, and clips from −40 dB.
  - The cubic's y² term adds a 2nd harmonic. It is strongest on Type 2
    (−15 dB at −6 dB in).

### COSM: OD/Dist (2026-11-01, v76)

COSM Type 1: a filtered hard clipper. The converter writes block code 1 (OD)
or 2 (DS) by the Type. The two codes share their object code (start values,
block copy, update, note-on) and differ only in the DSP routine. It has no
buffers.

**Parameters** (0x8d0…0x8d8, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x8d0 | Type (OD, DS) | 0…1 | 0 |
| 0x8d1 | Output Level | 0…127 | 127 |
| 0x8d2 | Drive | 0…127 | 127 |
| 0x8d3 | Drive LFO Depth | ±63 | 0 |
| 0x8d4 | Drive Env Depth | ±63 | 0 |
| 0x8d5 | Tone | 0…127 | 127 |
| 0x8d6 | Tone Keyfollow | ±20 | 0 |
| 0x8d7 | Tone LFO Depth | ±63 | 0 |
| 0x8d8 | Tone Env Depth | ±63 | 0 |

**The converter** (`0x894dc8b0`) writes the block:
- +0 code 1 when Type is 0, else 2;
- +0x2c Level and +4 Drive, through the 128 shorts at 0x897cfcb8 / 32768,
  as Wave Shape's;
- +8 Drive's Env depth (A), +0xc its envelope (9 or 11), +0x10 its LFO
  depth, +0x14 its LFO;
- +0x18 Tone through `0x894dc420`, a third copy of Lo-Fi's Filter curve;
- +0x1c Tone's Env depth (B), +0x20 its envelope (10 or 12), +0x24 its LFO
  depth, +0x28 its LFO.

The binding puts Drive's envelope in slot 1 and Tone's in slot 2.

**The object** (codes 1 and 2: start values `0x894ec3a8` and `0x894ec430`;
block copy `0x894ecce0` and `0x894ecdd0`; update `0x894ee78c` and
`0x894ee810`):
- Drive' = 1 + 12 × clamp(Drive + Env·depth + LFO·depth, 0…1): a gain of
  1…13 (D+0xbc; target D+0xa4).
- Tone' = B[trunc(127 × clamp(Tone + depths, lo…1))], B being Lo-Fi's Balance
  curve (D+0xc0; target D+0xa8).
  - lo is 0.1 in the start values and the update, but 0 in the block copy.
    That is a firmware quirk: a new block with Tone + depths below 0.1 sets
    a lower target than the next update does.
- The block copy stores the Level. Note-on (`0x894ee142`) sets the gain in
  use (D+0x8c) to it.
- A new block of the other code (the Type changed while a note sounds) is
  not copied: the block dispatcher (`0x894ee4a0`) returns when the code
  differs. A Type change takes effect at the next note.

**The filters** (OD `0x894fef70`, DS `0x894fc570`; the stereo table's
entries are `0x894ff340` and `0x894fc7e0`), on the XT's scale (int16 ×
65536):
- Per block, like Wave Shape:
  - the gain slews 0.1 toward the Level;
  - Drive' and Tone' step before every fourth sample.
- The clip stage, used once by DS and twice by OD, with gain g and limit L:
  - c = clip(g·v, ±L);
  - m = clip(g·(v + v')/2, ±L), v' being the last sample's v;
  - out = 0.25·c + 0.5·m + 0.25·c', c' being the last sample's c.
  So the clipper runs at twice the rate and is averaged back down.
- DS, from L:
  - hp = x − s0, then s0 += 0.03205·hp (0x3d0346db);
  - s1 += 0.32769·(hp − s1) (0x3ea7c6fb);
  - the clip stage on s1, with g = Drive' and L = 2^28.
- OD, from L:
  - hp = x − s0, then s0 += 0.0484·hp (0x3d463f14);
  - the clip stage on hp, with g = Drive' and L = 2^29;
  - s3 += 0.32769·(y − s3);
  - h = s3 − s4, then s4 += 0.03205·h;
  - the clip stage on h, with g = 1 and L = 2^28.
- Then a two-pole lowpass at Tone' (b += Tone'·a; a += Tone'·(y − a − b);
  OD subtracts b first, then a), and out = b × the gain, on L and R.
- The states are D+0x1c…0x2c (DS) or …0x3c (OD), cleared at note-on.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases, both Types;
- note-on (the firmware's own), updates and new blocks through the
  dispatcher: 26,359 cases, about a quarter of the new blocks of the other
  code;
- the filters: 120,000 blocks, at levels from 1 to 2.1e9;
- all 0 different.
- The first port read OD's first limit as 2^30. Its constant 0x4e000000 is
  2^29, and the check caught it once signals reached the clip.
- Deliberate errors are caught: each limit doubled or halved, the block
  copy's Tone clamp at 0.1, a block of the other code copied, OD's Tone
  subtractions swapped, OD's midpoint without Drive', 11 for 12, 0.32769 a
  unit off, the gain not slewed, and DS's highpass at OD's 0.0484.
- XtSynth runs it in the XT's units, as Lo-Fi.

**Plugin.**
- COSM Type gains OD/Dist (index 13), with its own parameters (cosmOd… and
  cosm2…) at the XT's defaults. Its OD/Dist and OD Tone rows share Wave
  Shape's layout.
- Measured on a 441 Hz sine (Level 127, Tone 127; levels as peaks in the
  SOLO buffer, ±1):
  - The limit sets the output ceiling: 2^28 is 18 dB below the buffer's
    full scale.
  - At Drive 127 (×13), sines up to −40 dB pass at +19.4 dB (OD) or
    +21.3 dB (DS).
  - From −20 dB both clip to odd harmonics: 3rd −10, 5th −15, 7th −18 dB.
  - At Drive 0 a −6 dB sine already clips.

### COSM: Wave Shape (2026-11-01, v75)

COSM Type 2, block code 3: a waveshaper through one of three curves, with a
two-pole filter and a DC blocker. It has no buffers.

**Parameters** (0x8d9…0x8e1, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x8d9 | Type (shown 1–6) | 0…5 | 0 |
| 0x8da | Output Level | 0…127 | 127 |
| 0x8db | Drive | 0…127 | 127 |
| 0x8dc | Drive Env Depth | ±63 | 0 |
| 0x8dd | Drive LFO Depth | ±63 | 0 |
| 0x8de | Tone | 0…127 | 127 |
| 0x8df | Tone Keyfollow | ±20 | 0 |
| 0x8e0 | Tone LFO Depth | ±63 | 0 |
| 0x8e1 | Tone Env Depth | ±63 | 0 |

**The converter** (`0x894dcbf0`) writes the block:
- +4 Type, a byte;
- +0x30 Level and +8 Drive, each through the 128 shorts at 0x897cfcb8 /
  32768 (about (i/127)²). Drive adds a controller term and is clamped to
  0…127 first. Level is used as stored.
- +0xc Drive's Env depth (A), +0x10 its envelope (9 or 11), +0x14 its LFO
  depth, +0x18 its LFO;
- +0x1c Tone through `0x894dc4b0`, which is the same code as Lo-Fi's Filter
  curve (`0x894dc810`): (T·0.9/127 + 0.1) × powf(2, KF·10·(key − 60)/1200),
  within 0.1…1;
- +0x20 Tone's Env depth (B), +0x24 its envelope (10 or 12), +0x28 its LFO
  depth, +0x2c its LFO.

The Env depths are not clamped; the LFO depths are, with their controller
terms. The binding puts Drive's envelope in slot 1 and Tone's in slot 2.

**The object** (start values `0x894ec4c0`; block copy `0x894ecee0`; update
`0x894ee8a0`):
- Drive' = clamp(Drive + Env·depth + LFO·depth, 0…1) (D+0xbc; target
  D+0xa4).
- Tone' = B[trunc(127 × clamp(Tone + depths, 0.1…1))] (D+0xc0; target
  D+0xa8). B is Lo-Fi's Balance curve (128 floats at 0x897d1668).
- The block copy also stores the gain, Level/8. Note-on (`0x894ee160`) sets
  the gain in use (D+0x8c) to it.
- The rates are Lo-Fi's: 4 × the envelopes' follower coefficients.

**The filter** (`0x89503ef0`; the stereo table's entry is `0x89504890`):
- It calls `0x89503980` for Types 0–2 and `0x89503c40` for Types 3–5.
  (The stereo routine calls `0x89503f4c` and `0x89504400`.)
- Per block:
  - the gain slews 0.1 toward Level/8;
  - the steps rate·(target − value) for Drive' and Tone' are taken once,
    and both step before every fourth sample.
- The shaper, on the XT's scale (int16 × 65536):
  - y = 64 + 64·(v · 10/2^31), with 10/2^31 one unit low (0x319fffff);
  - the curve C at y: rom.2 has three curves of 128 floats at 0x897d3ff0,
    and Type n and n + 3 share curve n. It is linear between entries, holds
    C[127] from 127 up and C[0] below 0, and extrapolates for −1 < y < 0
    (FTRC truncates toward zero);
  - s = (Drive'·C + (1 − Drive')·(y/64 − 1)) × 2^31.
  So Drive 0 is the input × 10, and the curves span ±1/10 of full scale.
- Types 0–2, from L, with k = 0.95·(1 − Drive')² + 0.05 (computed after
  each step):
  - b += Tone'·a; a += k·(x − b − a);
  - v = s(b).
- Types 3–5:
  - w = s(x);
  - b += Tone'·a; a += Tone'·(w − a − b);
  - v = b.
- Then the DC blocker: d = v − c; c += 0.001·d (0x3a83126e, a unit low);
  out = d × the gain, on L and R.
- The states: Types 0–2 use D+0x1c…0x24 (a, b, c), Types 3–5 D+0x28…0x30.
  Note-on clears them all.

The curves:
- 0 is sin(θ) for θ from −126° to +126°: −0.81 at the low end, through −1,
  0 at the centre, +1, and 0.81 at the top.
- 1 is a triangle over about one cycle: from 0 down to −0.97, up to +0.9,
  and back to 0.03. It is not odd, so it adds even harmonics.
- 2 is about 2.5 cycles of a sine, 0.12 at the centre.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases;
- note-on (the firmware's own), updates and new blocks: 23,996 cases;
- the filter: 120,000 blocks, all six Types, at levels from 1 to 1e9;
- all 0 different.
- Deliberate errors are caught: k's 0.95 a unit off, the dry term weighted
  by Drive', each end of the curve held at the wrong entry, 0.001 as C
  rounds it, Drive' stepped once per block, the gain not slewed, Tone
  clamped at 0, 10/2^31 as C rounds it, Types 3–5's filter using the old b,
  and Type 3 sent to the other routine.
- Lo-Fi's Filter curve is now shared with Tone (`keyTone`). Lo-Fi's checks
  are unchanged.
- XtSynth runs it in the XT's units, as Lo-Fi.

**Plugin.**
- COSM Type gains Wave Shape (index 12), with its own parameters (cosmWs…
  and cosm2…) at the XT's defaults. Its W-Shape and WS Tone rows are in the
  type area.
- Measured on a 441 Hz sine (Level 127, Tone 127, Drive 127 unless noted;
  levels as peaks in the SOLO buffer, ±1):
  - Drive 0 is clean at +1.9 dB for every Type: ×10 × 1/8.
  - Types 1 and 4 (curve 0) are odd-order. At −20 dB the 3rd harmonic is
    10 dB under the fundamental.
  - Types 2 and 5 (curve 1) give a strong 3rd and 5th at −20 dB. At −6 dB
    the fold leaves the fundamental under its harmonics (−23 dB overall).
  - Types 3 and 6 (curve 2) are steep at the centre: +19.6 dB at −40 dB,
    with 2nd and 3rd harmonics about 30 dB down.

### COSM: the Limiter (2026-11-01, v74)

COSM Type 13, block code 16: a gain follower with a 10-sample lookahead. As
with the Compressor, the binding gives it no envelopes or LFOs, and it uses
the first of the allocator's four buffers as a 3266-sample ring.

**Parameters** (0x935…0x939, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x935 | Threshold Level | 0…127 | 64 |
| 0x936 | Ratio (1.5:1, 2:1, 4:1, 100:1) | 0…3 | 1 |
| 0x937 | Attack | 0…127 | 64 |
| 0x938 | Release | 0…127 | 64 |
| 0x939 | Output Level | 0…127 | 115 |

**The converter** (`0x894ddd20`) writes the block:
- +4 Ratio, a byte;
- +8 Threshold, +0xc Attack and +0x10 Release, each through the 128 shorts
  at 0x897cfcb8 / 32768. That is the TB Filter's gain curve: about
  32768·(i/127)², and 32768 at 127.
  - Attack and Release each add a controller term (0 without one) and are
    clamped to 0…127 first.
  - Threshold is used as stored.
- +0x14 the Output Level's gain, as the Compressor's: (L/100)², or 1 + 3·(L
  − 100)/27 above 100.

**The object**:
- The block copy (`0x894edb20`) stores:
  - the slope above the threshold, by Ratio from the 4 floats at 0x897d1884:
    0.5, 0.25, 0.0625, 0.01 (D+0x8c);
  - the threshold t = Threshold·2^28 + 2^20, on the XT's scale (int16 ×
    65536);
  - Attack's rate, (1 − Attack)·5e-11 + 5e-12;
  - Release's rate, (1 − Release)·5e-11 + 1e-13;
  - the gain.
- It writes the last four twice, with interrupts masked:
  - to the targets (+0x18…0x24);
  - to the values the filter slews (D+0xcc…0xd4, D+0xa0).
  So the filter's per-block slew of 0.1 toward them never moves anything.
- There are no start values or update.
- Note-on (`0x894ee410`) sets D+0x90…0x9c to 1: the gain g, its smoothed s,
  and two more, presumably the stereo routine's. The allocator clears the
  ring. The write index starts at 0xcc2.

**The filter** (`0x894fe450`; the stereo table's entry is `0x894fe67e`):
- Per block, the threshold, both rates and the gain slew 0.1 toward the
  block copy's (0x3dcccccd).
- Per sample, from L:
  - the write index steps, wrapping at 3266, and x goes into the ring;
  - the wanted level w = |x|, or (1 − slope)·t + slope·|x| when |x| > t;
  - g += rate·(w − |x|·g): Release's rate when w > |x|·g, Attack's
    otherwise;
  - s += 0.01·(g − s);
  - out = (the ring 10 samples back × s) × the gain, on L and R.
- The state written back: the write index, g and s, and the slope
  (unchanged).

Quirks:
- The Ratio's names do not match its slopes. "1.5:1" is a 2:1 slope, "2:1"
  4:1, "4:1" 16:1, and "100:1" 100:1.
- The rate multiplies a level difference, so the gain moves faster on loud
  signals. At Attack 64 (4.2e-11) and a full-scale peak (2^31) it closes 9%
  of the gap per sample. So g follows the waveform within a cycle, and s
  averages it.
- Threshold 127 is t = 2^28 + 2^20: 18 dB below the buffer's full scale.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases;
- note-on (the firmware's own, with the pool hooked) and new blocks: 7,582
  cases;
- the filter: 120,000 blocks, the ring compared after every block. Some
  notes fade to silence, and some samples sit exactly at ±t;
- all 0 different.
- Deliberate errors are caught: a 9-sample delay, the two rates swapped,
  0.01 as C rounds it (0x3c23d70a, one unit from the firmware's), g starting
  at 0, and Release's floor at 5e-12.
- ≥ for > at the threshold is caught only because of the samples at ±t:
  random samples never land on it.
- XtSynth runs it in the XT's units, as the Compressor.

**Plugin.**
- COSM Type gains Limiter (index 11), with its own parameters (cosmLim… and
  cosm2…) at the XT's defaults. They sit on one Limiter row in the type
  area: Ratio, Thr, Atk, Rel, Lvl.
- Measured on steady 1 kHz sines, levels as peaks in the SOLO buffer (±1):
  - At Threshold 64 (−29.8 dB) with Level 100, sines at −30 dB and below
    pass at unity.
  - Between −20 and −6 dB the output rises 11.6, 8.8, 4.1 and 1.4 dB for
    the four Ratios. That is about 1.2, 1.6, 3.4 and 10:1: softer than the
    slopes, because the gain follows each cycle.
  - At the defaults (Level 115, ×2.67): +8.5 dB on quiet sines, +3.9 dB at
    −20 dB and −1.3 dB at −6 dB.

### COSM: the Compressor (2026-11-01, v73)

COSM Type 12, block code 15: a sustainer with a 60-sample lookahead. It has
no envelopes or LFOs: the binding gives code 15 none.

**Parameters** (0x932…0x934, second descriptor bank): Attack 0…127 (64),
Sustain 0…127 (64), Output Level 0…127 (115).

**The converter** (`0x894ddc50`) writes the block:
- +4 Attack/127 + 0 and +8 Sustain/127 + 0, each plus a controller term
  (0 without one), clamped to 0…127 first;
- +0xc the gain: (L/100)² for L ≤ 100; 1 + 3·(L − 100)/27 above, which is
  2.67 at 115 and 4 at 127.

**The object**:
- The block copy (`0x894ee5c0`) stores the attack factor 1.03 + 0.07·(1 −
  Attack), Sustain and the gain.
- There are no start values or update.
- Note-on (`0x894ee390`):
  - the allocator gives code 15 the Comb's four cleared buffers, and it uses
    the first as a 3266-sample ring;
  - the slewed factor, Sustain and gain (D+0x8c…0x94) start at the block's;
  - the gain and its target (D+0x9c, +0x98) start at Sustain·2^27/(factor +
    2^20) + 1 − Sustain, without the gain;
  - the envelope (D+0xa0) starts at the attack factor.
  The last two are quirks: the factor stands where the envelope belongs.
  D+0xcc…0xd4 get copies, presumably for the stereo routine.

**The filter** (`0x894fab80`; the stereo table's entry is `0x894fadcc`):
- Per block, the factor, Sustain and gain each slew 0.1 toward the block's
  (0.1 here is 0x3dcccccd).
- Per sample, from L:
  - the write index steps, wrapping at 3266, and x goes into the ring;
  - g += 0.05·(target − g);
  - out = g × the ring 60 samples back, on L and R;
  - the envelope e, on the XT's scale: e·factor + 132154 while |x| > e,
    otherwise e·0.9995;
  - the new target: (2^27/(e + 2^20)·Sustain − Sustain + 1) × gain.
- So silence gets up to (1 + 127·Sustain) × gain, and a full-scale signal
  (1 − Sustain) × gain.
- The state written back: the write index, the target, e and g.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases;
- note-on (the firmware's own, with the pool hooked) and new blocks: 7,154
  cases;
- the filter: 120,000 blocks, the ring compared after every block, with some
  notes fading to silence so the envelope releases;
- all 0 different.
- Deliberate errors are caught: the envelope starting at 0, the note-on gain
  with the Level, a 59-sample delay, and 0.9995 as C rounds it
  (0x3f7fdf3b, one unit from the firmware's).
- XtSynth runs it in the XT's units, as Lo-Fi.

**Plugin.**
- COSM Type gains Compressor (index 10), with its own parameters (cosmComp…
  and cosm2…) at the XT's defaults, on one Comp row in the type area.
- On steady 1 kHz sines 30 dB apart, at the defaults:
  - +3.7 dB on the loud one and +18.0 dB on the quiet one, so the 30 dB
    becomes 14.3;
  - Sustain 0 is unity;
  - Sustain 127 at Level 100 leaves 0.2 dB of the 30.

### COSM: Lo-Fi (2026-11-01, v72)

COSM Type 15, block code 19: an anti-alias lowpass, a sample hold, a bit
quantizer and a lowpass after, mixed with the dry input. It has no delay
buffers and no note-on case of its own.

**Parameters** (0x941…0x949, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x941 | Sample Bit Down | 0…15 | 10 |
| 0x942 | Sample Rate Down | 0…5 | 4 |
| 0x943 | Filter | 0…127 | 64 |
| 0x944 | Filter Keyfollow | ±20 | 0 |
| 0x945 | Filter LFO Depth | ±63 | 0 |
| 0x946 | Filter Env Depth | ±63 | 0 |
| 0x947 | Dry/Effect Balance | 0…100 | 50 |
| 0x948 | Balance LFO Depth | ±63 | 0 |
| 0x949 | Balance Env Depth | ±63 | 0 |

**The converter** (`0x894ddf80`) writes the block:
- +4 Bit Down and +5 Rate Down (bytes);
- +8 Filter (`0x894dc810`): (F·0.9/127 + 0.1) × powf(2, KF·10·(key −
  60)/1200), within 0.1…1;
- +0xc its Env depth (A), +0x10 its envelope (9 or 11), +0x14 its LFO depth,
  +0x18 its LFO;
- +0x1c Balance/100 + 0, +0x20 its Env depth (B), +0x24 its envelope (10 or
  12), +0x28 its LFO depth, +0x2c its LFO.

The binding puts the Filter's envelope in slot 1 and Balance's in slot 2.

**The object** (start values `0x894eca70`; block copy `0x894edd60`; update
`0x894eeee0`):
- Filter' = clamp(Filter + Env·depth + LFO·depth, 0.1…1) (D+0xbc; target
  D+0xa4).
- Balance' = B[trunc(127 × clamp(Balance + depths, 0…1))] (D+0xc0; target
  D+0xa8). B is 128 floats at 0x897d1668, from 0 through 0.5004 at 64 to
  0.9966.
- The block copy also stores, from tables:
  - by Rate Down (0…5): the hold mask (bytes at 0x897d187c: 0, 1, 3, 7, 15,
    31; D+1) and the lowpass coefficient (0x897d1894: 1/2^r; D+0x94);
  - by Bit Down (0…15): 2^b (0x897d18ec; D+0x98) and 2^−b (0x897d18ac;
    D+0x9c).

**The filter** (`0x894fe9c0`; the stereo table's entry is `0x894febe0`):
- The steps rate·(target − value) for Filter' and Balance' are taken once per
  block, and both step before every fourth sample.
- Per sample, from L, with c = 1/2^Rate:
  - two two-pole lowpasses: b1 += c·a1; a1 += c·(x − b1 − a1); b2 += c·a2;
    a2 += c·(b1 − b2 − a2);
  - the hold takes b2 in the four-sample groups whose count (0, 4, … 28,
    restarting each block) has no bit in common with the mask;
  - q = trunc(hold·2^−b·2^−15), on the XT's scale (int16 × 65536);
  - a two-pole lowpass at Filter' over q·2^b·32768: b3 += f·a3; a3 += f·(q'
    − b3 − a3);
  - out = Balance'·b3 + (1 − Balance')·x, on L and R.
- The six states (D+0x1c…0x30) clear at note-on.

Quirks:
- The count steps by 4 per group, so masks 1 and 3 (Rate Down 1 and 2) never
  hold. Those settings only change the anti-alias lowpass.
- Rate Down 3–5 hold whole groups of four samples.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases;
- note-on (the firmware's own), updates and new blocks: 23,999 cases;
- the filter: 120,000 blocks;
- all 0 different.
- Deliberate errors are caught: the hold per sample, the Balance index
  rounded, and the second lowpass fed from x. Folding 2^−b·2^−15 into one
  factor is exact either way.
- XtSynth runs Lo-Fi in the XT's units: its buffer × 2^31 (SOLO) or × 2^16
  (BACKING/ENSEMBLE) in, and back out. These scalings are exact.

**Plugin.**
- COSM Type gains Lo-Fi (index 9, latched at note-on), with its own
  parameters (cosmLofi… and cosm2…) at the XT's defaults.
- Its Lo-Fi and Lo-Fi F rows are in the editor's type area.
- On a 1 kHz sine at −8.7 dBFS, fully wet:
  - Bit Down 10 puts harmonics 33 dB down;
  - Bit Down 15 silences it: it is below one step;
  - Rate Down 2 only filters;
  - Rate Down 3 adds images 14 dB down;
  - Rate Down 5's 1/32 lowpass removes most of it.

### COSM: the Frequency Shifter (2026-11-01, v71)

COSM Type 14, block code 18. It is a single-sideband shifter: the input's
Hilbert pair times a quadrature oscillator, mixed with the dry input.

**Parameters** (0x93a…0x940, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x93a | Effect Depth | 0…127 | 80 |
| 0x93b | Effect Depth Keyfollow | ±20 | 0 |
| 0x93c | Effect LFO Depth | ±63 | 0 |
| 0x93d | Effect Env Depth | ±63 | 0 |
| 0x93e | Dry/Effect Balance | 0…100 | 50 |
| 0x93f | Balance LFO Depth | ±63 | 0 |
| 0x940 | Balance Env Depth | ±63 | 0 |

**The converter** (`0x894ddde0`) writes the block:
- +4 Depth through `0x894dc7a0`, which is the same code as the Side Band
  Width's `0x894dc5f0`: (D/127 + 0) × powf(2, KF·10·(key − 60)/1200),
  within 0…1;
- +8 its Env depth (A), +0xc its envelope (9 or 11), +0x10 its LFO depth,
  +0x14 its LFO;
- +0x18 Balance/100 + 0, +0x1c its Env depth (B), +0x20 its envelope (10 or
  12), +0x24 its LFO depth, +0x28 its LFO.

The binding puts Depth's envelope in slot 1 and Balance's in slot 2.

**The object** (start values `0x894ec9f0`; block copy `0x894edc80`, a plain
copy; update `0x894eee60`; note-on case `0x894ee430`):
- Depth' = 0.0126 × clamp(Depth + Env·depth + LFO·depth, 0…1) + 0.001
  (D+0xbc; target D+0xa4). This is the oscillator's phase step per sample:
  44 Hz at 0 to 600 Hz at full depth.
- Balance' = clamp(Balance + Env·depth + LFO·depth, 0…1) (D+0xc0; target
  D+0xa8).
- Note-on clears the phase (D+0x94). The allocator gives code 18 the Comb's
  four cleared buffers, and the shifter uses the first as a 3266-sample
  ring; the write index starts at 3266, so the first sample goes to 0.

**The filter** (`0x894fda60`; the stereo table's entry is `0x894fde84`):
- The steps rate·(target − value) for Depth' and Balance' are taken once per
  block. Before every fourth sample both step; then the increment is |Depth'|
  and the sign s is −1 (Depth' ≥ 0) or +1.
- Per sample: the write index steps, wrapping at 3266, and x (L) goes into
  the ring.
- H, the Hilbert output, is a 32-tap FIR on every other sample: H(n) =
  Σ c_j·x(n − 2j), with c_j from 0x897d45f0 and its centre at n − 31. The
  firmware forms four outputs at once in the back register bank: at each
  group's first sample, every term whose sample is already in the ring, in
  ascending j; at each later sample, that sample's terms in the outputs
  still to come. A plain ascending sum differs in 86% of blocks.
- Phase += increment, less 1 when above 1. At y = 127·phase, i = trunc(y),
  f = y − i: sin = (1 − f)·T[i] + f·T[i+1], and cos the same at i + 32
  (mod 128). T is 129 floats at 0x897d3dec, 128 a cycle. The 127 is a
  firmware quirk: each phase cycle reads 127 of the table's 128 steps, then
  jumps back, which adds small spurs at multiples of the shift.
- out = Balance' × (s·x(n − 31)·cos + H·2·sin) + (1 − Balance')·x, on L and
  R.
- The state written back: the write index (D+4), the phase (D+0x94), Depth'
  and Balance'.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- The FIR's order of additions was taken from the code mechanically: a
  symbolic run of its unrolled loop listed each accumulator's (tap, sample)
  terms.
- the converter: 200,000 cases;
- note-on (the firmware's own, with the pool hooked as for the Comb),
  updates and new blocks: 22,844 cases;
- the filter: 120,000 blocks, with the ring compared after every block;
- all 0 different.
- Deliberate errors are caught: the plain FIR order, a 128 for the 127, and
  the oscillator's sign. A `>=` for the phase's `>` is not: the phase never
  lands on exactly 1 in these cases.

**Plugin.**
- COSM Type gains Frequency Shifter (index 8, latched at note-on), with its
  own parameters (cosmFs… and cosm2…) at the XT's defaults.
- Its Shift and FS Bal rows are in the editor's type area (v70).
- On a 1 kHz sine, fully wet:
  - Depth 80 moves it to 1,394 Hz, Depth 0 to 1,044 Hz, and Depth 127 to
    1,600 Hz;
  - no other line is above −34 dB;
  - Balance 50 gives both lines at the same level.

### COSM: the Resonator (2026-10-31, v69)

COSM Type 5, block code 6: six two-pole resonators in parallel, their
frequencies scaled by Size, mixed with the dry input by Balance. It has no
delay buffers (the allocator skips code 6) and no note-on case of its own.

**Parameters** (0x8eb…0x8f2, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x8eb | Filter Type | 0…2 | 0 |
| 0x8ec | Size | 0…127 | 64 |
| 0x8ed | Size Keyfollow | ±20 | 0 |
| 0x8ee | Size LFO Depth | ±63 | 0 |
| 0x8ef | Size ENV Depth | ±63 | 0 |
| 0x8f0 | Balance | 0…100 | 50 |
| 0x8f1 | Balance LFO Depth | ±63 | 0 |
| 0x8f2 | Balance ENV Depth | ±63 | 0 |

The plugin labels the types 1–3; the XT's names for them are not traced.

**The converter** (`0x894dcf00`) writes the block:
- +4 the Filter Type (a byte);
- +8 Size (`0x894dc700`): x = (key − 60)·KF·10/100/12; x·logf(2)/1.6 +
  2·Size/127 − 1, within ±1. The 1.6 here is 0x3fcccccc, and logf(2) is the
  firmware's (0x3f317219, as in v68);
- +0xc its Env depth, A × (d/63)·|d/63|; +0x10 its envelope (9 or 11);
  +0x14 its LFO depth; +0x18 its LFO;
- +0x1c Balance/100 + 0 (plus a controller term, 0 without one); +0x20 its
  Env depth (B); +0x24 its envelope (10 or 12); +0x28 its LFO depth; +0x2c
  its LFO.

The binding (`0x894f7330`, by code: the block offsets of slot 1's and 2's
envelopes and LFOs) puts Size's envelope in slot 1 and Balance's in slot 2.

**The object** (start values `0x894ec580`; block copy `0x894ed0a0`, a
plain copy; update `0x894ee960`):
- Size' = expf(1.6 × clamp(Size + Env·depth + LFO·depth, ±1)), with this
  1.6 at 0x3fcccccd, one unit above the converter's;
- Balance' = clamp(Balance + Env·depth + LFO·depth, 0…1);
- note-on takes the start values from the envelopes' start levels (D+0xbc,
  D+0xc0), and the block and update put the targets at D+0xa4 and D+0xa8;
- rates are 4 × the envelopes' coefficients, as for the TVF.

**The firmware's expf** (`0x8952f6f4`, errno around `0x89531a28`):
- NaN gives 0x7fc00001; 0 gives 1; |x| ≥ 2^15 gives 0 or ∞.
- y = x·log2 e (0x3fb8aa3b); n = trunc(y) (with y's sign, so −0 for small
  negatives); f = y − n.
- If |f| ≥ ½, n moves one step toward f and f one unit the other way.
- t = f·ln 2 (0x3f317218); P(t)/P(−t), where P has degree 6 and its two
  coefficient tables (0x897eeebc, 0x897eeed8) are read from rom.2; then
  ldexpf (`0x89532ea8`, the exponent field moved: 0 below, ∞ above).
- It is within a few units of the true e^x (e.g. expf(1) = 2.71828151).

**The filter** (`0x894ffa10`; the stereo table's entry is `0x894ffe4c`):
- The steps rate·(target − value) for Size' and Balance' are taken once per
  block. Before every fourth sample both step, and the six coefficients
  become c_i = f_i·Size'.
- The table at 0x897d4b80 holds 18 floats per Filter Type: (f_i, d_i, g_i)
  for each resonator. Type 1's f_i are 0.0224…0.0943, about 157…662 Hz at
  Size' = 1.
- Per sample, from L, each resonator: b += c·a; t = d·a + x − b; a += c·t.
  Out = Balance' × Σ g_i·a_i + (1 − Balance')·x, written to L and R.
- The states (a, b per resonator, D+0x1c…0x48) clear at note-on.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- expf: 400,000 cases, half on its range here (±1.6) and half random bit
  patterns;
- the converter: 200,000 cases, the whole block compared;
- note-on (the firmware's own), updates and new blocks: 22,500 cases;
- the filter: 120,000 blocks;
- all 0 different. Four deliberate errors are caught: the 1.6 one unit low,
  expf without its |f| ≥ ½ step, the first step skipped, and a resonator's b
  updated after its t.

**Plugin.**
- COSM Type gains Resonator (index 7, latched at note-on), with its own
  parameters per section (cosmResType… and cosm2…) at the XT's defaults.
- The editor has a Res row (Type, Size and its KF, LFO and Env depths) and a
  Res Bal row (Balance and its depths). The window is 60 px taller.
- On noise at key 60, fully wet (Balance 100):
  - Type 1 at Size 64 peaks near 169, 270 and 673 Hz;
  - Size 127 moves its peaks up by e^1.6 = 4.95 (top peak 3,319 Hz);
  - Size 0 moves them down by the same factor;
  - Balance 0 is the dry input exactly.

### COSM: the Side Band Filters (2026-10-31, v68)

COSM Types 6 and 7, block codes 7 and 8. Each converter writes a fixed
code; the type map's note that they choose it at run time was wrong. Both
remove the Comb's DC follower from the input and feed it through delay lines
tuned to the note, fed back through the Side Band Width.

**Parameters** (second descriptor bank; the patch values sit at +0x12, +0x14,
… in ID order):

| 1st (ID) | 2nd (ID) | Parameter | Range | Default (1st, 2nd) |
|---|---|---|---|---|
| 0x8f3 | | HPF Cutoff Freq | 0…127 | 48 |
| | 0x901 | Q | 0…127 | 64 |
| 0x8f4 | 0x902 | LPF Cutoff Freq | 0…127 | 64 |
| 0x8f5 | 0x903 | LPF Cutoff Freq Keyfollow | ±20 | 10 |
| 0x8f6 | | Sub Delay Line Detune | ±100 | 0 |
| 0x8f7 | 0x904 | Octave Select | 0…2 (−2…0) | 1 |
| 0x8f8 | 0x905 | Detune | ±100 | 0 |
| 0x8f9 | 0x906 | Detune LFO Depth | ±63 | 0 |
| 0x8fa | 0x907 | Detune Own Env Depth | ±63 | 0 |
| 0x8fb | 0x908 | Detune Env Depth | ±63 | 0 |
| 0x8fc | 0x909 | Side Band Width | 0…127 | 16, 32 |
| 0x8fd | 0x90a | Side Band Width Keyfollow | ±20 | 0 |
| 0x8fe | 0x90b | Side Band Width LFO Depth | ±63 | 0 |
| 0x8ff | 0x90c | Side Band Width Own Env Depth | ±63 | 0 |
| 0x900 | 0x90d | Side Band Width Env Depth | ±63 | 0 |

The Octave Select labels are inferred: index i takes the factor at
0x897d1868 + 4i (4, 2, 1), and the Comb's index 2 (factor 1) is "0".

**The converters** (`0x894dd0a0`, `0x894dd2c0`). The 1st's block, by offset;
the 2nd's is the same from Detune on, 4 bytes later, after its Q at +8, with
no HPF or Sub Detune:
- +4 the Octave (a byte); +8 Detune as a float (±100, plus a controller term
  that is 0 without one); +0xc its Env depth, B × (d/63)·|d/63|; +0x10 its
  envelope (10 or 12); +0x14 its LFO depth (d/63)·|d/63|; +0x18 its LFO (3
  or 4); +0x1c its Own depth (d/63)·|d/63|;
- +0x20 Width (`0x894dc5f0`): (W/127 + 0) × the firmware's powf(2,
  KF·10·(key − 60)/1200), the VariPhrase Time KF's power (v58), within 0…1;
- +0x24 its Env depth (A), +0x28 its envelope (9 or 11), +0x2c its LFO
  depth, +0x30 its LFO, +0x34 its Own depth;
- +0x38 −HPF/127 + 0; +0x3c the LPF (`0x894dc540`); +0x40 Sub Detune as a
  float; +0x44 1.0. The 2nd's Q is Q·0.96875/127 − 1 and its 1.0 is at +0x40.
- The LPF (`0x894dc540`): x = (key − 60)·KF·10/100/12; Cutoff/2/127 + ½, plus
  or minus (by x's sign) powf(x, 2) × logf(2) × ¼, within ½…1. The
  firmware's powf(x, 2) is not x·x: they differ in 4,330 of the 5,248 cases.
  logf(2) is 0x3f317219 (the true ln 2 is 0x3f317218). The plugin builds
  the 41 × 128 squares with the firmware's powf at load
  (`xtFwCosmKeySquares`).
- `0x8952f784` is the firmware's logf.

**The object** (block copy `0x894ed1a0`/`0x894ed2a0`; start values
`0x894ec5f0`/`0x894ec6b0`; note-on cases `0x894ee1a0`/`0x894ee260`; update
cases `0x894ee9d0`/`0x894eea20`; D = obj + 0x58):
- The block copy stores 1 − Detune/2400, 1 − Sub/2400 and HPF/2, the
  Octave's factor (D+0x9c), and the modulated targets: Detune' = (1 −
  Detune/2400) + Env·depth + LFO·depth (D+0xa4), and Width' = Width +
  Env·depth + LFO·depth (D+0xa8). The update recomputes these two.
  Unlike the Comb, the update reads the ratio correctly.
- Start values: delay = 44100 · factor · (1 − Detune/2400 + Env start ·
  depth + LFO · depth)/(pitch × root), within 0…3267 (D+0xbc); w = Width +
  Env start · depth + LFO · depth, within 0…1, or 0…0.95 for the 2nd
  (D+0xc0).
- Note-on clears D+0x1c…0x88 (the states and the DC follower) and the
  follower's level. The allocator takes the Comb's path: two of the slot's
  four buffers, 3,267 floats each, cleared, with the write index at 3266.
  The note-on case sets:
  - the slews to their blocks' values (HPF/2 or Q at D+0x90, LPF at
    D+0x94, the Sub factor at D+0x98 for the 1st, Own depths at D+0xcc and
    D+0xd0);
  - the gain to 2 − 2|Width'| + Width'², the unclamped target, D+0x8c;
  - the new delay to the start delay, the crossfade to 1, and the DC
    coefficient to 0.001551;
  - the allpass fractions: frac(delay) (D+0xdc) and, for the 1st,
    frac(delay × Sub factor) (D+0xe0).
- Envelopes and LFOs: Detune takes slot 1 (Env B, Env 10 or 12), Width
  slot 2 (Env A, Env 9 or 11); both use LFO 3 or 4.

**The filters** (`0x89500640` 1st, `0x89500d40` 2nd). Per 32-sample block:
- The level follower (`0x895017e0`, the Dynamic TVF's sum) moves the level
  0.001 toward the block's. The Own depths, the LPF, the HPF or Q, and the
  Sub factor each slew 0.1 toward their block values.
- The Width target is Width' + Own × level, within 0…1 (0…0.95 for the 2nd).
  Its step is (target − w) × rate 2 (4 × Env A's coefficient). The gain
  target is 2(1 − |t|) + t² × the block's 1.0, or 0 when t = 0, so a Width
  of exactly 0 is silent.
- Once the last crossfade has ended (p ≥ 1), the new delay is 44100 ·
  (Detune' + Own × level) · factor/(root × pitch), folded by `0x89501910`:
  below 1 it is 1, otherwise halved until under 3267. If it differs from the
  current one it becomes the new target and p = 0. The 1st's taps are
  trunc(delay), trunc(delay × Sub), trunc(new), trunc(new × Sub); the 2nd's
  trunc(delay) and trunc(new).
- Every fourth sample from the first: w += step, gain += 0.0125·(target −
  gain).
- Per sample: the write index steps (wrapping at 3267); e = x − dc, dc +=
  e·0.001551; each tap reads B[i − d] mixed by the crossfade, tap(new)·p +
  tap(old)·(1 − p); p += 1/132. Above 1: p = 1, delay = new delay, and the
  taps take the new ones.
- **1st**, two lines (line 2 on buffer 2 at the Sub taps). For each line:
  l = l·(1 − lpf) + tap·lpf; t = l − (1 − f)·a; o = (1 − f)·t + a; a = t;
  v = (e − o)·w; y = h·hs + (1 + hs)·v + o; h = v; B[i] = y. Here f is the
  line's fraction and hs the slewed HPF/2, −½…0. Out = (y₁ + y₂) × gain on
  L and R.
- **2nd**, one delay and both buffers: A = LPF(allpass(tap of buffer 2)),
  B = LPF(allpass(tap of buffer 1)), with one fraction; buffer 2 ← A + w·B;
  buffer 1 ← (Q·B + e − (A + w·B))·w + B; out = (A + w·B) × gain.
- The states at D+0x1c…: the 1st's allpass 1, HPF 1, allpass 2, HPF 2,
  LPF 1, LPF 2; the 2nd's LPF B, allpass B, LPF A, allpass A.
- A second routine table follows the first (.data 0x8987c840, 21 more
  entries). `0x894eef70` picks it when its flag is set: for COSM 1 the
  tone's +0x980, and for COSM 2 what COSM 1's pick returns. These look like
  the stereo versions (`0x89500940` for code 7 reads its follower from L and
  R, `0x89501850`). All the ports so far are the mono routines; whether
  XtSynth's stereo waves should take the second table is not traced.

Quirks:
- The allpass fractions are set only at note-on, from the start delay. They
  are never updated, through crossfades or pitch changes.
- The note-on gain uses the unclamped Width' and skips the t = 0 test.
- Line 2's taps, at delay × (1 + 100/2400) and with the delay up to 3267,
  reach up to 136 samples before buffer 2. The pool (`0x894ec1d0`) lays out
  each kind of buffer as one array of 24 slots, 0x330c bytes apart: buffer 2
  pointers from 0x89dc6150, after buffer 1's from 0x89d79830. So these reads
  take the end of the previous slot's buffer 2, another voice's line (or,
  for slot 0, the last buffer-1 slot's). The port reads zeros there, as the
  check's memory holds; the XT's value depends on the other voices.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converters: 100,000 cases each, the whole block compared byte for
  byte, with the keyfollow tables built by the interpreter's own powf;
- note-on (the firmware's own, as for the Comb, with the state left from
  the last note filled with junk), updates with a moving pitch and new
  blocks: 11,272 (1st) and 11,230 (2nd) cases;
- the filters: 60,000 blocks each, both buffers compared after every
  block;
- all 0 different. Three deliberate errors in the port are caught: the 1st
  line's fraction on line 2 (1,483 blocks differ), the zero-Width test
  removed (1st and 2nd), and a non-zero guard before buffer 2 (120 blocks).
  The last shows the reads before buffer 2 do occur in the cases.
- AddressSanitizer and UBSan: clean.

**Plugin.**
- COSM Type gains 1st Side Band and 2nd Side Band (indices 5 and 6,
  latched at note-on). Each type has its own parameters (cosmSb1…,
  cosmSb2…) at the XT's defaults.
- The editor's three Side Band rows (Octave with HPF or Q, LPF, KF, Sub;
  Detune and its depths; Width and its depths) show the 2nd's set when the
  type is the 2nd, and the 1st's otherwise.
- As for the Comb, the note-on runs after the voice's pitch is set. The
  follower reads the buffer at the XT's scale, as the Dynamic TVF's does.
- On noise at key 60 (root 261.6 Hz):
  - the 1st at its defaults has peaks every 130.7 Hz (Octave −1), and every
    261.4 Hz at Octave 0;
  - Width 127 with HPF 0 is flat at +6 dB (each line passes e, and the two
    are summed);
  - Sub Delay −100 detunes line 2 (spacing 127.9 Hz);
  - the 2nd at its defaults has the same spacing, with −31 dB notches.

### COSM: the Comb Filter (2026-10-31, v67)

COSM Type 8, block code 9. A feedforward comb tuned to the note: the input,
with its DC removed, plus Tone × itself delayed by the note's period × the
Octave's factor.

**Parameters** (0x90e…0x914, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x90e | Octave Select | 0…4 (−2…+2) | 3 (+1) |
| 0x90f | Detune | ±100 | 0 |
| 0x910 | Detune LFO Depth | ±63 | 0 |
| 0x911 | Detune Env Depth | ±63 | 0 |
| 0x912 | Tone | 0…127 | 0 |
| 0x913 | Tone LFO Depth | ±63 | 0 |
| 0x914 | Tone Env Depth | ±63 | 0 |

**The converter** (`0x894dd4f0`) writes the block:
- [1]: the Octave, as a byte;
- [2] Tone·2/127 − 1, [3] its Env depth (A × (d/63)·|d/63|), [4] its
  envelope (9 or 11), [5] its LFO depth, [6] its LFO;
- [7] Detune as a float (±100), [8] its Env depth (B), [9] its envelope (10
  or 12), [10] its LFO depth, [11] its LFO.

The binding puts Detune's envelope in slot 1 and Tone's in slot 2.

**The pitch.** Before each COSM note-on, the tone note-on (`0x894f5e60`) runs
the voice's own note-on (the voice struct is at tone +0x20). It then copies
two values into the COSM object (`0x894ee480`):
- obj+0 = the voice's pitch ratio (+0x144, the slot the 10-ms voice update
  moves 70%);
- obj+4 = the voice's root frequency (+0x9a4). The voice setup
  (`0x894f21c0`) forms it as exp2((Original Pitch·100 − offset +
  detune)/1200 − 5.75)·44000·0.01, i.e. 440·2^((P − 69)/12) for a wave
  without fine tuning, or 440 for Robot Voice.

The 10-ms update (`0x894ee6a0`) stores its fr4 argument at obj+0. The tone
update (`0x894f75f0`) passes the voice's +0x144: OSC 1's, or OSC 2's
(+0xb18/+0x1378) in one structure branch.

**The delay**, in samples, is
44100 · F · (1 − Detune/2400 + Env·depth + LFO·depth) / (ratio × root),
within 0…3267 (`0x89501900`). F is the octave factor (4, 2, 1, ½, ¼, table
0x897d1868).
- Start values (`0x894ec740`, the start table indexed by code − 1) use the
  envelopes' start levels. Tone's start is clamp(Tone + depths, ±1).
- The block (`0x894ed390`) computes the delay from the outputs. It takes
  it as a new target only if the last crossfade has ended (obj+0x13c ≥ 1) and
  the delay differs from the current target. A new target sets
  obj+0x130 = the delay, +0x138 = its fraction, +0x13c = 0.
- The update (`0x894eea90`, the update table also indexed by code − 1)
  works the same way, after the Tone target.
- **Quirk:** the block stores 1 − Detune/2400 at obj+0x2c, where the update
  reads Detune. After the first update the factor is
  1 − (1 − Detune/2400)/2400: about −0.7 cents, whatever the Detune.

**Note-on** (`0x894edf70`, code 9):
- The allocator (`0x894ecb00`) takes buffers from a pool (0x89f91884, or
  0x89f91a64 for COSM 2) and clears them: 3,267 floats each.
- The write index starts at 3266.
- The start values and the block run.
- Then `0x894ee300` sets the new target to the start delay, both fractions
  to its fraction, the crossfade to 1, and the DC coefficient to 0.001551.

**The filter** (`0x894fa8d0`), per sample:
- the write index steps, wrapping at 3267;
- e = x − dc; dc += e·0.001551;
- the read is linearly interpolated, a·(1 − f) + b·f, at index − delay and
  index − new delay;
- the two reads are mixed by the crossfade p: A·(1 − p) + B·p;
- the buffer takes e;
- out = mix·Tone + e, written to L and R;
- p += 0.002 (500 samples). Above 1: p = 1, delay = new delay, and
  fraction = new fraction.

Quirks:
- Tone steps toward its target only on the first sample of each block. The
  every-fourth-sample test sits outside the loop.
- Both reads use the current fraction.
- The delays' integer parts are taken once per block.
- The fraction set at the crossfade's end is not stored back. The stored
  one (obj+0x134) is the note-on's for the rest of the note.
- At the longest delay (3267) with the write index at 0, the second tap
  wraps once, to −1: the firmware reads the word before its buffer, weighted
  by the fraction. That word is 0 in the check. On the XT it is whatever
  precedes the pooled buffer (not traced). The port keeps a guard word of 0
  there. Without it, the port's read ran off the vector, and the check
  crashed now and then.

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases;
- note-on, run as the firmware's own `0x894edf70`, with the pool's slot
  finder `0x894ec2b0` hooked to hand out buffers full of junk, which the
  allocator clears;
- updates with a moving pitch (crossfades starting, ending mid-block and
  being refused), and new blocks while the note sounds: 22,517 cases;
- the filter: 120,000 blocks, the whole delay buffer compared after every
  block;
- all 0 different. 12 runs in a row, and a run with AddressSanitizer and
  UBSan, are clean.

**Plugin.**
- COSM Type gains Comb Filter (index 4, latched at note-on).
- Its own parameters per section use the XT's defaults (Octave +1).
- The editor has a Comb row (Octave, Detune and its LFO and Env depths) and
  a Comb T row (Tone and its depths).
- The comb's note-on runs after the voice's pitch is set, as on the XT. It
  takes the voice's pitch ratio (`curA`, the +0x144 the update moves) and the
  root frequency at the patch's Original Key. Each 10-ms update passes the
  ratio again.
- Checks:
  - renders are bit-identical to v66;
  - PluginTest is bit-identical with the Comb Filter at its defaults; with
    Detune, Tone, both envelopes, the LFO and a second note; and on COSM 2
    behind a TB Filter in MONO legato.
- On noise at key 60 (root 261.6 Hz):
  - Tone +1 at Octave 0 gives peaks every 261.4 Hz (+6 dB, notches below
    −28 dB);
  - Octave +1 gives peaks every 523.2 Hz;
  - Tone −1 moves the peaks to between the harmonics.

### COSM: the TB Filter (2026-10-31, v66)

COSM Type 16, block code 20: a three-pole lowpass with resonance feedback,
then a two-pole highpass and a gain. Unlike the TVF, it computes its
coefficients from frequencies in Hz.

**Parameters** (0x94a…0x954, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x94a | LPF Cutoff | 0…127 | 90 |
| 0x94b | LPF Cutoff Freq Keyfollow | ±20 | 0 |
| 0x94c | LPF Cutoff LFO Depth | ±63 | 0 |
| 0x94d | LPF Cutoff Env Depth | ±63 | 0 |
| 0x94e | LPF Q | 0…127 | 100 |
| 0x94f | LPF Q Follow | ±63 | 25 |
| 0x950 | HPF Cutoff Freq | 0…127 | 45 |
| 0x951 | HPF Cutoff Freq Keyfollow | ±20 | 0 |
| 0x952 | HPF Cutoff LFO Depth | ±63 | 0 |
| 0x953 | HPF Cutoff Env Depth | ±63 | 0 |
| 0x954 | Gain | 0…127 | 127 |

**The converter** (`0x894de110`) writes 17 words:
- [1] the gain: rom.2's table of 128 shorts at 0x897cfcb8, /32768 × 2;
- [2] LPF Cutoff, [4] Q and [6] HPF Cutoff: clamp(0…127)/127;
- [3], [7] the keyfollows: (clamp(KF·10, ±200) + 200)·4/400 − 2, i.e.
  −2…+2 octaves per octave;
- [5] Q Follow: (clamp(F, ±63) + 63)·2/126 − 1;
- [8] the LPF Env depth (A × (d/63)·|d/63|) and [9] its envelope, 9 or 11;
- [10] the LPF LFO depth and [11] its LFO, 3 or 4;
- [12] the HPF Env depth (B) and [13] its envelope, 10 or 12;
- [14] the HPF LFO depth (not clamped) and [15] its LFO;
- [16] the key, a byte.

The matrix terms add to the LPF Cutoff, its LFO depth and Q.

**Constants.** A start-up routine (`0x894fc4c0`) sets five globals at
0x89fe4e60…70: 0.009375, 3.2677, −0.062, 3.57981 and 1.34468. Call the last
four G1…G4. The interpreter does not run it, so the harness calls it.

**exp2** (`0x894ec090`), through rom.2's tables from 0x897d0228. For x ≥ 0,
i = trunc(x) and f = trunc(frac·65536):
2^x = T[f>>8] (2^(k/256)) × T[i] (2^i, 0x897d1228) × T[f&255] (2^(k/65536)),
multiplied in that order. For x < 0 it uses the inverse tables (0x897d0a28,
0x897d1244, 0x897d0e28) on −x. An i past the seven entries of the 2^i table
reads on into the next table.

**The targets**, from the envelopes' and LFOs' outputs: at note-on and on a
new block (`0x894fc110`, which also copies the block), and every 10 ms
(`0x894fbdc0`). Slot 1 is the LPF's (Env A), slot 2 the HPF's (Env B).
- P(c) = ((−1.161c + 2.1096)c − 0.1344)c − 0.1, with c the LPF Cutoff.
- m = (LFO·G4)(depth·G3) + (Env depth·G3)(Env·G4) + 0.64717.
- fc = 278 · exp2(½(G1·P + G2 + m))² · exp2(KF·(key − 60)/24)². That is
  278·2^(G1·P + G2 + m) Hz, keyfollowed in octaves.
- fc is clamped to 1…20,000 Hz. The cutoff alone reaches about 2.1 kHz at
  127; the Env and LFO terms carry it further.
- Q = (Q + Q Follow·m·0.2) · (P/3.5 + 0.7) · 7.9, clamped to 0…7.9. Above
  3,700 Hz it is also × (1.09077 − 2.48205e-5·fc).
- w = 1.2π·fc/44100 and r = (w − 2)/(w + 2). The LPF's targets are
  c0 = (r + 1)/1.3, c1 = 0.3·c0, c2 = −r, and Q.
- HPF: y = 13·(Cutoff + depths × Env and LFO). Below 0 the frequency is
  1 Hz; above 14 it is 10,000 Hz; otherwise it is exp2(y/2)². It is then
  keyfollowed and capped at 10,000 Hz.
- Above 1 Hz, with w = 2π·f/44100: c0 = 2/(w + 2) and c1 = (2 − w)/(w + 2).
  At 1 Hz or below, both are 1.
- The gain is the block's gain.
- All seven targets are stored × 0.01.

**Note-on** (`0x894edf70`, code 20):
- the block copy sets the targets;
- `0x894fbd20` sets the coefficients to target/0.01 and clears the states.

The start-value routine is empty for this type, so the envelopes' start
levels and step rates go unused.

**The filter** (`0x89501950`) is hand-written. It holds the LPF in one FPU
bank and the HPF in the other, and forms each stage with FIPR. Per sample:
- u = x − Q·s3;
- three one-poles: y = c0·in + c1·in′ + c2·y′. That is
  y1 = (Q, c1, c2, c0)·(0, s0, s1, u) by FIPR, then y2 on (0, s1, s2, y1),
  then y3 on (0, s2, s3, y2);
- the states become s0 = x − Q·y3, s1 = y1, s2 = y2, s3 = y3;
- the LPF's output is Q·y3 + y3;
- two one-pole highpasses, y = c0·(in − in′) + c1·y′, each with FIPR;
- the output is the second highpass × the gain, written to L and R.

After use in each sample, every coefficient moves 1% toward its target:
c = c·0.99 + target·0.01. The states are at D+0x2c…0x38 (LPF) and
D+0x68…0x74 (HPF); the coefficients at D+0x1c…0x28 and D+0x5c…0x64; the
targets at D+0x3c… and D+0x78….

**Port and checks** (`xt_cosm`, new lines; the others unchanged):
- the converter: 200,000 cases;
- note-on, blocks while the note sounds, and updates: 12,982 cases;
- the filter: 120,000 blocks;
- all 0 different.

FIPR is ported as the interpreter forms it: the products summed in double,
each sum rounded toward zero, then the float rounded toward zero. Real
hardware FIPR is not modelled beyond this, as elsewhere in the project.

**Plugin.**
- COSM Type gains TB Filter (index 3, latched at note-on).
- Its 11 parameters per section use the XT's defaults.
- The editor has a TB LPF row (Cut, KF, Q, QF, LFO, Env) and a TB HPF row
  (Cut, KF, LFO, Env, Gain).
- Checks:
  - renders are bit-identical to v65;
  - PluginTest is bit-identical with the TB Filter at its defaults; with
    both envelopes, the LFO, keyfollow and a second note; and on COSM 2
    behind a Dual Filter in MONO legato.
- On noise (BACKING; TB on against COSM off):
  - the LPF falls about 18 dB an octave;
  - Q 110 at Cutoff 70 gives a +12 dB peak near 500 Hz;
  - the defaults give +20 dB near 1 kHz;
  - HPF 90 is −32 dB at 63 Hz.

### COSM: the Dual Filter, and the map of the types (2026-10-31, v65)

**The types in rom.2.** The COSM converter (`0x894de3b0`) switches on the
type to a converter per type, which writes a block code. The voice's COSM
routine table (.data 0x8987c7ec) is indexed by that code. The block copy
(`0x894ee4a0`), binding (`0x894f7330`), start values (`0x894ec340`), note-on
(`0x894edf70`) and update (`0x894ee6a0`) all switch on the code too.

| Type | Name | IDs | Converter | Code | DSP |
|---|---|---|---|---|---|
| 1 | OD/Dist | 0x8d0–0x8d8 | `0x894dc8b0` | 1 or 2 | `0x894fef70`, `0x894fc570` (v76) |
| 2 | Wave Shape | 0x8d9–0x8e1 | `0x894dcbf0` | 3 | `0x89503ef0` (v75) |
| 3 | Amp Simulator | 0x8e2–0x8e9 | `0x894dcd80` | 4 | `0x894f8a30` (v77) |
| 4 | Speaker Simulator | 0x8ea | `0x894dcef0` | 5 | `0x89501300` (v78) |
| 5 | Resonator | 0x8eb–0x8f2 | `0x894dcf00` | 6 | `0x894ffa10` (v69) |
| 6 | 1st Side Band Filter | 0x8f3–0x900 | `0x894dd0a0` | 7 | `0x89500640` (v68) |
| 7 | 2nd Side Band Filter | 0x901–0x90d | `0x894dd2c0` | 8 | `0x89500d40` (v68) |
| 8 | Comb Filter | 0x90e–0x914 | `0x894dd4f0` | 9 | `0x894fa8d0` (v67) |
| 9 | Dual Filter | 0x915–0x91f | `0x894dd690` | 12 | `0x894fd090` |
| 10 | TVF | 0x920–0x927 | `0x894dd870` | 13 | `0x89502890` |
| 11 | Dynamic TVF | 0x928–0x931 | `0x894dda40` | 14 | `0x89502920` |
| 12 | Compressor | 0x932–0x934 | `0x894ddc50` | 15 | `0x894fab80` (v73) |
| 13 | Limiter | 0x935–0x939 | `0x894ddd20` | 16 | `0x894fe450` (v74) |
| 14 | Frequency Shifter | 0x93a–0x940 | `0x894ddde0` | 18 | `0x894fda60` (v71) |
| 15 | Lo-Fi | 0x941–0x949 | `0x894ddf80` | 19 | `0x894fe9c0` (v72) |
| 16 | TB Filter | 0x94a–0x954 | `0x894de110` | 20 | `0x89501950` (v66) |

The routine table by code (0…20): `0x894fb280`, `0x894fef70`,
`0x894fc570`, `0x89503ef0`, `0x894f8a30`, `0x89501300`, `0x894ffa10`,
`0x89500640`, `0x89500d40`, `0x894fa8d0`, `0x894fb2b0`, `0x89501bf0`,
`0x894fd090`, `0x89502890`, `0x89502920`, `0x894fab80`, `0x894fe450`,
`0x894fb530`, `0x894fda60`, `0x894fe9c0`, `0x89501950`. Blank cells were not
traced. The Side Band converters turned out to write fixed codes (v68), so
"chosen at run time" was wrong for them. The codes left over are 0 (Through),
10, 11 and 17 (the Resonator's is 6, v69; the Frequency Shifter's 18, v71;
Lo-Fi's 19, v72). The ID ranges are from the table at 0x897cebd8.

All sixteen types are ported and equal the firmware (v61–v78). The Speaker
Simulator's converter is a small function of its own (`0x894dcef0`), not
inline as first noted. Their stereo routines, by another table: v104, "COSM:
the stereo routines".

**The Dual Filter** (COSM Type 9, block code 12).

Parameters (0x915…0x91f, second descriptor bank):

| ID | Parameter | Range | Default |
|---|---|---|---|
| 0x915 | Filter Type | 0…2 | 0 |
| 0x916 | Filter 1 Frequency | 0…127 | 90 |
| 0x917 | Filter 1 Frequency Keyfollow | ±20 | 10 |
| 0x918 | Filter 1 Q | 0…127 | 108 |
| 0x919 | Filter 1 LFO Depth | ±63 | 0 |
| 0x91a | Filter 1 Env Depth | ±63 | 0 |
| 0x91b | Filter 2 Frequency | 0…127 | 80 |
| 0x91c | Filter 2 Frequency Keyfollow | ±20 | 10 |
| 0x91d | Filter 2 Q (named "Filter 1 Q" in rom.2) | 0…127 | 120 |
| 0x91e | Filter 2 LFO Depth | ±63 | 0 |
| 0x91f | Filter 2 Env Depth | ±63 | 0 |

The Filter Types, from the UI strings: LPF/HPF, LPF->HPF, BPF/BPF.

**The converter** (`0x894dd690`) per filter forms the TVF's sums:
- Frequency: the keyfollowed cutoff (`0x894dc660`: (key − 60)·KF·10/100/127
  + clamp(F, 0…127)/127, within 0…1; the same as the TVF's inline sum);
- Q: clamp(Q)·0.984375/127 − 1;
- LFO depth: (d/63)·|d/63| of the clamped depth;
- Env depth: A (Filter 1) or B (Filter 2) × (d/63)·|d/63|.

The block: [1] type, [2…7] Filter 1 (Frequency, Env, Q, envelope 9/11,
LFO, LFO 3/4), [8…13] Filter 2 (envelope 10/12). So Filter 1 runs on Env A
(Env 9, COSM 2 Env 11) and Filter 2 on Env B (Env 10, 12), both on LFO 3
(LFO 4). The binding puts Filter 1's envelope in slot 1 (+0x150). That is
the slot the TVF gives its Q.

**The object.**
- Both coefficients (D+0xbc, +0xc0) are cutoffs, one per filter. Each is
  the TVF's table step (`0x894ec140`) at Frequency + Env depth × envelope +
  LFO depth × LFO, capped by that filter's own Q:
  - the start values use the envelopes' start levels (`0x894ec340`);
  - the targets use their outputs: at note-on and on a new block
    (`0x894ed5d0`), and every 10 ms (`0x894ee6a0`).
- The step rates are the TVF's: 4 × the envelopes' follower coefficients.
- At note-on (`0x894edf70`, code 12) D+0x90/+0x94 take the two Qs. The
  filter does not read them there.
- The filter (`0x894fd090`):
  - computes each step as rate·(target − c);
  - passes both Qs from the block itself (+0x10, +0x28). So a new block
    changes Q at once, unsmoothed;
  - jumps to the kernel for the type (table at 0x894fd0f0, no bounds
    check).

**The kernels** (`0x894fcbe0` LPF/HPF, `0x894fcd70` LPF->HPF, `0x894fcef0`
BPF/BPF). Each runs 32 frames; the coefficients step before every fourth
frame. Each filter is two 24-dB SVF stages. For a filter with cutoff c,
Q q, input x and states s0…s3:
- s1 += c·s0; t = x·q − s1 + q·s0; s0 += c·t;
- s3 += c·s2; u = in₂ − s3 + q·s2; s2 += c·u;
- in₂ is s1 (LP) or, for BPF/BPF, s0 (BP).

The kernels differ in how the two filters connect:
- **LPF/HPF:** Filter 1 is LP24 (output s3). Filter 2 is HP24 on the input,
  with in₂ = t and output u. The output is the sum of the two.
- **LPF->HPF:** Filter 2's input is Filter 1's LP24 output; the output is
  Filter 2's HP24.
- **BPF/BPF:** both filters are BP24 (output s2), and the output is their
  sum.

The kernel reads L and writes both channels. The states are D+0x1c…0x38.
Unlike the TVF's, the input is multiplied by q itself, not −q.

**Port and checks** (`xt_cosm`, new lines; TVF results unchanged):
- the converter: 200,000 cases;
- the start values, block targets (at note-on and on new blocks while the
  note sounds) and updates: 14,993 cases;
- the filter: 120,000 blocks, all three types, the coefficients stepping;
- all 0 different.

**Plugin.**
- COSM Type gains Dual Filter (index 2, latched at note-on).
- Per section the Dual Filter has its own parameters, with the XT's
  defaults (as on the XT, each type keeps its own):
  - Dual Filter Type;
  - Filter 1 and 2 Frequency, Keyfollow (±200%), Q, LFO and Env depths.
- Its Env A/B and LFO are the section's.
- The editor: the Dual Filter Type on the Type row, and a row per filter.
- Checks:
  - the existing renders are bit-identical to v64;
  - PluginTest is bit-identical with the Dual Filter at its defaults; as
    LPF->HPF with both envelopes, the LFO and a second note; and as BPF/BPF
    on COSM 2 behind a TVF on COSM 1, in MONO legato.
- On noise (BACKING; Dual Filter on against COSM off, third-octave bands):
  - LPF/HPF (F1 60, F2 110, Q 100): resonant peaks at about 250 Hz and
    8 kHz, −47 dB at 1–2 kHz;
  - LPF->HPF (F1 100, F2 60): a band from about 500 Hz to 4 kHz;
  - BPF/BPF (60, 100, Q 110): peaks at about 250 Hz and 4 kHz.

### COSM: the Dynamic TVF (2026-10-31, v64)

COSM Type 11, block code 14. It is the TVF with a follower of the section's
input level, which moves Q and the Cutoff.

**Parameters** (0x928…0x931, second descriptor bank):
- Filter Type 0…11 (default 5) and Q;
- Q LFO Depth, Q Own Env Depth, Q Env Depth;
- Cutoff (default 127) and Cutoff Keyfollow (±20);
- Cutoff LFO Depth, Cutoff Own Env Depth, Cutoff Env Depth.

All depths are ±63.

**The converter** (`0x894dda40`) forms the TVF's block at the same places.
It adds the Own Env depths as (d/63)·|d/63|, with no clamp: Q's in [6] and
the Cutoff's in [12].

**The object.**
- The binding and the start values (`0x894ec340`) are the TVF's.
- The block copy (`0x894eda10`) and the 10-ms update (`0x894eeda0`) set the
  targets to the raw sums: Q (or Cutoff) + its depths × the envelope and
  LFO, with no clamp and no table.
- At note-on (`0x894edf70`, type 14) the two slewed Own depths (D+0x90,
  +0x94) start at the block's depths.
- The follower (D+0x98) is not reset; it carries on from the voice's
  previous note.

**Per 32-sample block** (`0x89502920`):
1. The level is Σ|L| over the block × 0x2e7fffff (a unit below 2^-34)
   (`0x895017e0`). It counts the eighth frame of every eight twice. The
   XT's buffer is int16 × 65536.
2. e += 0.1·(level − e).
3. The Q depth slews 0.001 a block toward −(Q Own depth): it starts at
   +depth, so Q's response is inverted at first and turns around over
   about a second. The Cutoff depth slews toward +(Cutoff Own depth). The
   0.001 is 0x3a83126e, a unit below the nearest float.
4. Q = clamp(Q target − slewed Q depth · e, −1…−1/64).
5. The cutoff coefficient = the table step (`0x894ec140`, the TVF's) at
   Cutoff target + e · slewed Cutoff depth.
6. Both coefficients step toward these targets as in the TVF, through the
   same twelve kernels (0x895028f0).

**Port and checks** (`xt_cosm`):
- the converter: 200,000 cases;
- start values, block targets and updates: 120,000 cases, half TVF and half
  Dynamic TVF;
- the filter: 1,600,000 blocks, with the follower starting from random
  levels and buffers at the XT's int32 scale.
- All 0 different.

**Plugin.**
- New per section: Type (TVF, Dynamic TVF; latched at note-on), and the
  Cutoff and Resonance Own Env depths, on a new editor row.
- The follower reads XtSynth's buffer scaled to the XT's: SOLO (±1) ×
  2^31, BACKING/ENSEMBLE (int16) × 2^16. These scalings are exact.
- XtSynth also keeps each voice's follower across notes.
- Checks:
  - with the type at TVF the output is bit-identical to v62 (COSM off, and
    COSM 1 on);
  - PluginTest is bit-identical with the Dynamic TVF on COSM 1, on COSM 2
    behind a TVF on COSM 1 with a second note, and in MONO legato;
  - on the drum (LP24, Cutoff 30, Cutoff Own +63), the spectral centroid
    rises from 54 Hz to 332 Hz with the hit and falls back with the level.

### COSM 2 (2026-10-31, v63)

COSM 2 is a second COSM section after COSM 1, on the same buffer: the tone
render (`0x894f6d40`) runs COSM 1's routine, then COSM 2's, and the tone
update (`0x894f75f0`) updates them in the same order. As a TVF it is the
same object and code, with its own sources:
- Env 11 (Cutoff) and Env 12 (Q), whose Velocity Curve/Sens give its A/B
  factors;
- LFO 4.

The converter's COSM index (+2) changes only those indices.

**Plugin.**
- New: COSM 2 as a TVF with its own Env A/B and LFO 4. Its parameters are
  COSM 1's with the prefix "cosm2"; COSM 1 keeps its v61 IDs.
- The editor's COSM rows show one section at a time, chosen by a
  COSM 1/COSM 2 selector that re-attaches them.
- XtSynth keeps both sections per voice and a free-running LFO for each.
  The LFO seeds are drawn COSM 1 first.
- Checks:
  - with COSM 2 off, the output is bit-identical to v62, with COSM 1 off
    or on;
  - COSM 2 alone renders bit-identically to COSM 1 alone with the same
    settings;
  - the two in series (LP24 85 then HP24 70) give exactly the product of
    their responses;
  - PluginTest is bit-identical with COSM 1, COSM 2, and both with MONO
    legato and a second note;
  - the editor renders with the selector on COSM 2.

### Exact sums in XtFloat (2026-10-31, v62)

XtFloat's `add` and `sub` (the SH-4's single precision, round to zero) used
to round the double sum toward zero. That is exact only while the operands'
exponents are within about 29 bits. Further apart, the double sum is already
rounded, and a tiny addend is lost; the SH-4 instead moves the large value
one unit toward zero. The COSM TVF met this as its cutoff decayed (v61,
which fixed it locally).

- `add`/`sub` now use TwoSum: the double sum plus its exact error. That
  decides the truncation, or for XT_ROUND=nearest a tie.
- Against exact rational arithmetic, on 300,000 random sums (exponent gaps 0
  to 63 bits, with sparse mantissas for ties):
  - round to zero: 0 different (the old sum: 23,755 different);
  - round to nearest: 0 different (the old: 0).
- Every firmware check gives the same results as v61:
  - xt_osc, xt_cosm, xt_lfo and xt_env exact;
  - BACKING and ENSEMBLE bit-exact against the firmware players;
  - SOLO −83…−100 dB on the same cases. Its residual does not come from
    this.
- Renders are bit-identical to v61 at the defaults, with the TVF, and with
  the pitch envelope, LFO, OSC knobs and portamento set. The speed is
  unchanged.
- XtCosm now uses the shared sum.

### COSM: the TVF (2026-10-31, v61)

**The COSM sections.**
- Each tone has two COSM sections in series. The tone render (`0x894f6d40`)
  runs OSC2, OSC1, the mixer, COSM 1 (object at voice +0x1388), COSM 2
  (+0x14ec), then the TVA.
- COSM Type (ID 0x3e8), 0…16: Through, OD/Dist, Wave Shape, Amp Simulator,
  Speaker Simulator, Resonator, 1st and 2nd Side Band Filter, Comb Filter,
  Dual Filter, TVF, Dynamic TVF, Compressor, Limiter, Frequency Shifter,
  Lo-Fi, TB Filter. COSM Switch is 0x3e7.
- **Patch storage** (`0x894de820`): 0x2a bytes per part, tone and COSM:
  - +0: the switch; +1: the type;
  - from +10: the type's parameters (shorts). Each type has its own ID
    range, from the table at 0x897cebd8: the TVF's is 0x920…0x927.
- IDs from 0x55d up read their ranges from a second descriptor bank (8
  bytes per ID from 0x897a56d8: offset, default, minimum, maximum). The TVF:

  | ID | Parameter | Range | Default |
  |---|---|---|---|
  | 0x920 | Filter Type | 0…11 | 5 |
  | 0x921 | Q | 0…127 | 0 |
  | 0x922 | Q LFO Depth | ±63 | 0 |
  | 0x923 | Q Env Depth | ±63 | 0 |
  | 0x924 | Cutoff | 0…127 | 127 |
  | 0x925 | Cutoff Keyfollow | ±20 | 0 |
  | 0x926 | Cutoff LFO Depth | ±63 | 0 |
  | 0x927 | Cutoff Env Depth | ±63 | 0 |

  The filter types, from the UI strings: LP24, BP24, HP24, NOTCH24, PEAK24,
  LP12, BP12, HP12, NOTCH12, PEAK12, LP6, HP6.
- **The velocity factors A and B** (`0x894de8f0`) take the Velocity Curve
  and Sens of Env 9 and 10 (COSM 1) or Env 11 and 12 (COSM 2). They go
  through the curve and sens converters; the converter (`0x894de3b0`)
  forms factor = the velocity curve (`0x894ebd60`) / 32768, not squared.

**The TVF's converter** (`0x894dd870`; block code 13), with the matrix and
controller terms 0:
- Q: clamp(Q, 0…127)·0.984375/127 − 1. Its LFO depth is (d/63)·|d/63|
  (LFO 3, or 4 for COSM 2). Its Env depth is B × (d/63)·|d/63| (Env 10, or
  12).
- Cutoff: (key − 60)·(KF·10)/100/127 + clamp(Cutoff)/127, within 0…1. Its
  LFO depth uses the same LFO; its Env depth is A × (d/63)·|d/63| (Env 9,
  or 11).
- The filter type, as is.
- The products are formed as velocity × ((d/63)·|d/63|).

**The voice's COSM object** (0x160 bytes; the DSP context D = object +
0x58):
- **Binding** (`0x894f7330` → `0x894ee680`). For the TVF: +0x150 = the Q's
  envelope, +0x154 = the Cutoff's, +0x158/+0x15c = the LFO. They are
  flagged active so they tick.
- **Note-on** (`0x894edf70`):
  - the filter states clear;
  - `0x894ec340` sets the coefficients c1/c2 (D+0xbc/+0xc0) from the
    envelopes' start levels (+0x58) and the LFO;
  - `0x894ed8f0` copies the block and sets the targets (+0xfc/+0x100) from
    the envelopes' outputs (+0x4c);
  - the step rates (+0x11c/+0x120) are 4 × the envelopes' follower
    coefficients (+0x54).
- **The 10-ms update** (vtable +0x14 = `0x894ee6a0`, type 13 at
  `0x894eecb0`). The tone update `0x894f75f0` runs it after the envelopes,
  LFOs, voices and mixer and before the TVA:
  - Q target: Q + its depths × the envelope and LFO, within −1…−1/64
    (`0x89501900`: if lo > x then lo; if x > hi then hi).
  - Cutoff target: Cutoff + its depths × the envelope and LFO. Below 0 it
    is 0. Otherwise z = y·127 and i = trunc(z) (at most 127):
    A[i]·(1 − frac) + A[i+1]·frac, where A is 129 floats at 0x897d1264
    (0.00155…1.4517, about 11 Hz…11.4 kHz as an SVF coefficient). It is
    capped at B[127 − trunc(−Q·127)], with B 128 floats at 0x897d1468.
- **The filter** (the per-type routine table, .data at 0x8987c7ec; TVF
  `0x89502890` → one of twelve kernels from 0x895028f0):
  - Per 32-sample block, each coefficient's step is rate·(target − c). It
    is added before every fourth sample, so the coefficients reach their
    targets in eight steps.
  - The kernel reads the left channel and writes the result to both.
  - **24 dB** (LP/BP/HP/NOTCH/PEAK24, `0x89502050`…), states a, b, d, e:
    - b += a·c2; t = −x·c1 − b + a·c1; a += c2·t; e += c2·d;
    - the second stage's input s is b (LP), a (BP), t (HP), t + a (NOTCH)
      or t − a (PEAK);
    - u = s − e + c1·d; d += u·c2;
    - output e, d, u, u + e or u − e.
  - **12 dB** (LP/BP/HP/NOTCH/PEAK12):
    - the input gain K = 0.5·(1 − c1), from c1 at the block's start;
    - b += a·c2; t = ∓x·K − b + a·c1 (LP12 alone keeps x's sign);
      a += t·c2;
    - output b, a, t, t + b or t − b.
  - **6 dB**: y = x − s; s += y·c2; output s (LP6) or y (HP6). Only c2
    steps.
- Arithmetic: single precision, round toward zero, FMAC as the product
  then the sum.

**A rounding finding.** XtFloat's `add` rounds the double sum toward zero.
That is exact only while the operands' exponents are within about 29 bits.
Beyond that the double sum is already rounded, and a tiny addend is lost.
The SH-4 instead moves a large value one unit toward zero. The TVF meets
this when the cutoff decays toward 0: its states then drift by a unit per
sample. `XtCosm` uses an exact round-to-zero sum (TwoSum, then the
truncation). XtFloat itself took the same exact sum in v62.

**Port and checks** (`xt_cosm`, in the interpreter, denormals flushed as
on the SH-4):
- the converter: 200,000 random cases;
- the start values, block targets and updates: 90,000 cases;
- the filter: 1,200,000 blocks (38 million samples), all twelve types, the
  coefficients stepping and the targets updated every 14 blocks.
- All 0 different.

**Plugin.**
- New: COSM 1 as a TVF:
  - switch, Filter Type, Cutoff, Resonance, Cutoff KF (±200%);
  - the Cutoff's and Resonance's Env and LFO depths;
  - Env A (the cutoff's, Env 9) and Env B (the resonance's, Env 10), each
    with ADSR, Velocity Curve/Sens, A/D/R-Sens and Time KF;
  - LFO 3, with its own free-running phase when not key-synced.
- The defaults are the XT's: Filter Type LP12, Cutoff 127, LFO 3 TRI, rate
  98, key sync off, and envelopes A 0, D 0, S 127, R 127, Velocity Sens
  +32.
- The editor's new rows: the switch, type, Cutoff, Resonance, KF; the four
  depths; Env A's ADSR; LFO 3's waveform, rate and key sync. The rest are
  host parameters.
- The COSM is latched at note-on. Parameter changes reach sounding voices
  as a new block; the envelopes and LFO update.
- With the COSM off the output is bit-identical to v60, also with mono,
  portamento and tuning set. PluginTest is bit-identical with LP24 and
  resonance, BP12 with envelope and LFO depths and a second note, and HP24
  in MONO legato.
- On the vocal:
  - LP24 at Cutoff 90 (table fc ≈ 1.97 kHz) is flat below, −18 dB at 4
    kHz and −34 dB at 6 kHz.
  - Resonance 110 adds +12 dB at the cutoff and lowers the passband by
    16 dB (the input scaling by |c1|).
  - Cutoff 127 is transparent.

**Not modelled yet:**
- COSM 2 (since modelled, v63);
- the other fifteen types (Dynamic TVF since modelled, v64; Dual Filter, v65; TB Filter, v66; Comb Filter, v67; Side Band Filters, v68; Resonator, v69; Frequency Shifter, v71; Lo-Fi, v72; Compressor, v73);
- the mixer's channel layout for stereo waves. The TVF takes the buffer's
  left channel; whether the XT's mixer leaves L or a mono sum there is not
  traced;
- the matrix and controllers;
- LFO rates above 127 (tempo sync; since v118);
- switching the COSM on or off for a sounding note.

### Mono/Poly, Legato and Octave Shift (2026-10-31, v60)

**Parameter tables.** rom.2 has two tables indexed by parameter ID:
- names: one pointer per ID from 0x89790338 (for example 0x113 "Mono/Poly",
  0x51f "Truncate Time");
- descriptors: 12 bytes per ID from 0x897a167c (through `0x89484310`):
  - a u16 and two bytes (the value's storage);
  - then int16 centre, default, minimum and maximum.
  - For example, Coarse Tune (0x110) is 64, 64, 16, 112: −48…+48, default 0.

Some defaults from the descriptors:
- Mono/Poly POLY; Legato Switch ON; Octave Shift 0;
- Portamento Time 20, where the plugin had 32;
- Bend Range 2/2; Master Tune ±1000 (1/10 cent); Master Key Shift ±24;
- Truncate Time (0x51f, a system setting) 70, in 1/100 ms.

**The patch-common block** (`0x894dbfa0`, 0x16 bytes per part from
0x89d021d0):
- +0: Octave Shift (0x112) × 12;
- +1: 1 for MONO (0x113 = 0);
- +2: Legato Switch (0x114);
- +4: Patch Tempo (0x11d);
- +6…: the zones' upper keys.

**The key state** (0x24 bytes per part and MIDI channel, from 0x89cf52d8;
reset by `0x894d6420` to key 60, nothing held, window 32). Its fields:
- +0xc: the sounding key, as played.
- +0x10: the time of the last note-on; +0x14: the chord window; +0x1e: the
  time since the note-on before.
- +0x1a/+0x1b: the POLY previous key, and whether a key was held then.
- +0x1c/+0x1d: the POLY last key, and whether a key is held.
- +0x20/+0x21 and +0x22/+0x23: the same for MONO.

The channel's held keys are a bitmap (bit k & 15 of word k >> 4).

- **Note-on** (`0x894d6330`):
  - dt = now − the last note-on, at most 0xffff, from the system clock
    (`0x89301940` → the kernel's 0x880080d0, taken to be in ms).
  - If dt is under the window, the window narrows by 1 and the POLY
    previous key stays. Otherwise the window resets to 32 and the POLY
    previous key and flag take the last ones. A chord's notes therefore
    all glide from the key before the chord.
  - The MONO previous key and flag always move on.
  - The note becomes the last key and the sounding key; both flags are set.
- **Note-off** (`0x894d63c0`), after the key left the bitmap:
  - With no key held, all four flags clear.
  - If the released key is the sounding one, the highest key still held
    (`0x894d66b0`) becomes the sounding key. The MONO previous key takes
    the released one, and the result is that key; otherwise the result is −1.

**The part's note-on** (`0x894dbcb0`, the part's vtable at 0x89833e30):
- The key is the played key plus the system's Transpose plus the Octave
  Shift, within 0…127. It picks the zone (the first upper key ≥ it).
- POLY: new tone and voices (`0x894dbc20` → the allocator `0x894d4af0`).
- MONO:
  - no key held before: the part's tones are truncated (`0x894d4a80`), then
    a new note;
  - a key held, Legato Switch OFF: the same;
  - a key held, Legato Switch ON: `0x894d4e00` calls vtable +0x20
    (`0x894dc1e0`) on each of the part's tones not released, with the new
    key.

**The part's note-off** (`0x894dbea0`): after the key state, a MONO part
whose sounding key was released while others are held calls `0x894d4e00`
with the highest of them (plus the shift). It does so whether Legato is on
or off.

**A tone taking a note** (`0x894dc1e0`):
- The tone's key, played key and velocity are updated (`0x894d5c90`).
- The re-setup `0x894eb910` runs: the portamento settings (`0x894e6690`)
  and the per-note OSC setup `0x894e4c30` (a new random offset; +0x7c = the
  MONO previous key + the shift), then the envelope, LFO and TVA
  converters.
- `0x894db8a0` hands the results to the voice without a restart:
  - the ramps take the new settings (`0x894f5e20` → `0x894f27c0` →
    `0x894f48b0`);
  - the OSC blocks are copied (`0x894f2600`); when the block's key-follow
    key (+4, `0x894e12c0`) changed, the ramps restart from +0x68 to +8
    (`0x894f4920`, the same code as `0x894f4680` at note-on);
  - the 13 envelopes, the LFOs and the TVA take their new settings.
- The tone update (`0x894ebbd0`) also recomputes the portamento settings
  and the OSC blocks. So a Portamento knob reaches a gliding note, and a
  Coarse change that moves the key-follow key restarts the glide from
  +0x7c.
- `0x894f48b0` copies the switch and time but not the type. It recomputes
  the increment from the span and leaves the ramp running or stopped as it
  was; the start then sets "running" from the switch.

**The key-follow key** (`0x894e12c0`, from the key, coarse and KF):
- KF 0: 60 + coarse;
- KF 1000: key + coarse;
- otherwise: ((60 + coarse)·1000 + KF·(key − 60) + 999)/1000, toward zero;
- in all cases within 0…127.

With Pitch KF 0 a legato note does not glide.

**Truncate** (`0x894d5ba0` → `0x894dbb70`, once → `0x894f5500`): the patch
TVA's +0x48 = 1 and +0x54 = 1.0. Then:
- The render (`0x894f5570`) holds the gains. Each sample, +0x54 falls by a
  step, and the output is the input × the gain × +0x54. At ≤ 2^-16 the TVA
  goes silent and the tone is done (`0x894f5510`).
- The step is 1/(T/100·44.1), with T the Truncate Time (`0x894ea380`: the
  system short at 0x89d2a6d0 + 0x1602). At 70 that is 0.0324: a 0.70 ms
  linear fade, 31 samples.

**Portamento's source key at a new note** (`0x894e4940`, +0x7c, within
0…127):
- POLY: the POLY previous key plus the shift. With LEGATO, the note's own
  key instead when nothing was held (+0x1b = 0) and the controller byte
  +0x11c of the channel is below 64 (perhaps Hold).
- MONO: the MONO previous key plus the shift. With LEGATO, the note's own
  key when +0x21 is 0.
- Before any note, the previous keys are 60.

**Port and checks** (`xt_osc`, in the interpreter):
- `XtKeyState` (`XtKeys.h`) against `0x894d6330`/`0x894d63c0`, after the
  reset `0x894d6420`: 400 random sequences, 120,000 note-ons and note-offs
  (chords, fast and slow), every field and result after each event. 0
  different.
- `XtPitchRamp::update`/`restart` against `0x894f48b0`/`0x894f4920`, mixed
  with starts and ticks: 2,052,895 values, 0 different.
- `xtKeyFollowKey` against `0x894e12c0`: 1,464,448 cases, 0 different.

**Plugin.**
- New controls: Mono/Poly, Legato Switch (default ON) and Octave Shift
  (−3…+3).
- Portamento Time now defaults to the XT's 20.
- The plugin's system clock is the host frames rendered, in ms.
- MONO:
  - a note played while a key is held takes over the sounding voice when
    Legato is on: a new random offset, new velocity, the envelopes and LFO
    updated not restarted, and a glide from the previous key;
  - otherwise the old voice is truncated (0.70 ms) and a new one starts;
  - releasing the sounding key while others are held returns to the
    highest of them.
- POLY, NORMAL or LEGATO: portamento starts from the POLY previous key. The
  chord window and the first note's glide from key 60 are as on the XT.
- Changing a Portamento knob reaches a voice already gliding. A
  Coarse/Pitch KF change that moves the key-follow key restarts its glide.
- Note-offs match the key as played.
- Checks:
  - at the defaults, and with portamento NORMAL or LEGATO on the A/B's two
    overlapping notes, the output is bit-identical to v59;
  - PluginTest is bit-identical with MONO legato + portamento, MONO without
    legato + Octave −1 + random, and a second note;
  - on the sine, Octave +1 on note 60 equals note 72 bit for bit, and a
    truncated voice falls linearly to silence in 31 samples.
- Not modelled:
  - Hold (the plugin ignores the pedal);
  - the system's Transpose;
  - zones;
  - the voice allocator's CPU budget and its stealing (the plugin steals
    the oldest voice outright).

### The note's tuning (2026-10-31, v59)

The OSC's Coarse (+0x68) and Fine (+0x70), which the note's pitch (v55)
reads, are sums the per-note OSC setup forms. It has three forms:
- `0x894e4940` at note-on, once per oscillator (from `0x894eb4d0`). It also
  copies the OSC's table entry into the block (+8) and sets the portamento's
  source key (+0x7c).
- `0x894e4c30` when a voice is set up again (`0x894eb910`, from
  `0x894dc1e0`).
- `0x894e4e90` on an update (`0x894ebaa0`): the same sums, keeping the
  random offset (+0x6c).

**Coarse** (semitones, int16): the system's key shift (the byte at part
base + 0xc202), plus Patch Coarse Tune, plus OSC Coarse, plus two matrix
terms (destinations 0 and 3; 0x11 for OSC 2).

**Fine** (1/10 cent, int32), the sum of:
- the Scale Tune at the key's pitch class (key % 12, by a reciprocal
  multiply);
- the system's Master Tune (the short at part base + 0xc200);
- Patch Fine Tune and OSC Fine;
- the part's pitch bend (+0x174 of the part object at +0xdc);
- the random offset;
- the matrix's fine terms (destinations 1 and 4, or 0x12) × 10.

**Where the terms come from.** The OSC table updater `0x894e6330` writes one
0xc20-byte block per part, from 0x89d1b070 (RAM):
- +0: Octave Shift (ID 0x112) × 12;
- +1: Coarse Tune (0x110);
- +2: Fine Tune (0x111) × 10;
- +4: 1 when 0x113 is 0 (probably Mono/Poly; it selects the source key
  below);
- +5: Portamento Mode (0x116);
- +6…+0x1d: Scale Tune C…B (IDs 0x136, 0x138, …, 0x14c) × 10, all 0 when
  the switch (0x134) is off;
- +0x20 + 0xc0·tone + 0x60·osc: the OSC entries (`0x894e55d0`). OSC Coarse
  is the byte at +0xe (ID 0x2ab), OSC Fine × 10 the short at +0x10 (0x2ac),
  and Random Pitch Depth the short at +0x12 (0x2ad).
- Random Pitch Depth goes through a 31-way switch, in 1/10 cent: 0…9 →
  0…90, 10 → 100, 11…19 → 200…1000, 20…29 → 2000…11000, 30 → 12000. That
  is 0…9 cents in 1s, 10…100 in 10s and 200…1200 in 100s.
- The system's terms come from IDs 0x3c (+0xc200) and 0x3d (+0xc202)
  (`0x894e5f20`). Their units are not checked; the port leaves them at 0.

**Random Pitch Depth.**
- There is one 16-bit state (0x89d1b068) for every voice and oscillator.
  A power-on initialiser seeds it with 0x7ab7 (`0x894e5010`, listed in the
  table at 0x897f0078).
- Each setup with a depth steps it: x = x·0x497d + 1, as an int16. The
  offset is ((x·depth) & 0x7fffffff) >> 15, taken as an int16: x·depth/32768
  rounded down, within ±depth.
- A depth of 0 leaves the state alone. The period is the full 65,536.
- From power-on at 1200 cents, the first offsets are +24.71, +25.24,
  −64.95, +51.39, −14.98 and +116.16 cents.

**Pitch bend** (`0x894e9fe0`, run when the part object is built, `0x894ea190`,
or rebuilt, `0x894ea290`):
- b = the bender − 8192 (the controller's u16 + 0xe000, as an int16);
- up: ((b·Up) & 0x1fffffff) >> 13;
- down: −((((−b)·Down) & 0x1fffffff) >> 13);
- Up and Down are Bend Range Up and Down (IDs 0x11e/0x11f) × 1000
  (`0x894e9330`, per part, 0x84 bytes each from 0x89d276b4).
- With a range of 2 semitones, full up gives +1999 and full down −2000
  (1/10 cent).
- How a bender move reaches a voice that is already sounding: only through the tone update
  `0x894dc0b0`, by the callers of the conversion pass (v99); the message's own path is not traced.

**Portamento's source key** (+0x7c, in `0x894e4940`; clamped to 0…127; the
note's own key means no glide). The note object at *param_2 points (+4) to
a structure that holds the previous keys:
- part +4 = 0: its +0x1a, plus the Octave Shift. With Mode LEGATO (part +5
  = 1), the note's own key instead when its +0x1b is 0 and the controller
  byte at +0x11c is below 64 (perhaps Hold).
- part +4 ≠ 0: its +0x20, plus the Octave Shift. With LEGATO and its +0x21
  = 0, the note's own key.
- These fields are traced in v60 ("Mono/Poly, Legato and Octave Shift").
  `0x894e4c30` takes +0x20 in every case.

**Port and checks** (`xt_osc`, in the interpreter):
- The power-on seed is 0x7ab7.
- `0x894e4c30` (with the converter it calls, `0x894e48a0`, returning at
  once) was run on 300,000 random cases:
  - inputs: part, key, the patch's Coarse, Fine and Scale Tune (on and
    off), OSC Coarse and Fine, every Random Pitch Depth, and bend;
  - the generator runs on from case to case;
  - coarse, fine, the random offset and the generator's state: 0 different.
- The bend, on every bender value with Up 0…48 and Down 48 − Up: 802,816
  cases, 0 different.

**Plugin.**
- New controls:
  - Patch Coarse Tune (−48…+48) and Patch Fine Tune (−50…+50);
  - Bend Range Up and Down (0…48, default 2) and the MIDI pitch wheel;
  - Random Pitch Depth (the XT's 31 steps);
  - Scale Tune: the switch and C…B (−64…+63 cents), as host parameters
    only.
- Pitch Shift now adds to the patch's Coarse and Fine, not the OSC's. The
  sum, and so the output, is the same.
- With the XT, the big PITCH knob is now the OSC's Pitch knob, as TIME and
  FORMANT already were. Random Pitch Depth takes its place in the OSC Pitch
  row (Crs, Fine, Rnd, P-KF, as on the XT's OSC page), and a Tune row holds
  the patch's Coarse, Fine and Bend Range. Pitch Shift stays a host
  parameter.
- The random offset is drawn at note-on and kept for the note. Each plugin
  instance has one generator, starting from the XT's power-on seed.
- The tuning is formed again at every 10-ms update, as the XT's update
  does, so a bend takes effect at the next update.
- With the new controls at their defaults the output is bit-identical to
  v58; it is also identical with Pitch Shift, Coarse/Fine and portamento
  set.
- Each new term renders bit-identically to an existing equivalent:
  - Patch Coarse 7 = OSC Coarse 7;
  - Scale Tune C +37 at note 60 = OSC Fine +37;
  - a bend of +4096 at range 2 = Coarse +1;
  - full down at range 12 = Coarse −12.
- Not modelled:
  - Octave Shift (since modelled, v60: the part's note-on adds it to the
    key);
  - the system's Master Tune and Key Shift;
  - the matrix;
  - a second oscillator, which would step the generator too.

### Portamento (2026-10-31, v58)

**Settings** (`0x894e66e0`). This is the tone's common block at +0xc. It is
recomputed for every note by `0x894e6690`, with the note's key and
velocity.
- Word 0 is the Portamento switch (patch parameter 0x115). Word 4 is the
  Type (0x117: 0 RATE, 1 TIME).
- Words 1 and 2 are 0. They switch the VariPhrase voice's formant and walk
  ramps, so only the pitch glides.
- Word 3 is the time in ms, as a float. For k = Time (0x118) plus
  modulation (the matrix `0x894d9750`), clamped to 127:
  - t = k²·10000/127² (1 ms at k = 1; 10 s at 127);
  - then t − t·v²·s/258064, at least 0;
  - s = |Time Velo Sens| (0x119, stored as is); v = the velocity, or 127 − v
    for a negative sensitivity.
  - The product t·v²·s is formed in 32 bits and wraps at high settings:
    Time 127, velocity 127, sensitivity +63 leaves 3911 ms instead of 0.
    The port keeps the wrap.
- The per-part table of 0x115, 0x117, 0x118 and 0x119 is written by
  `0x894e6920`. Mode (0x116) is not in it.

**The ramp** (voice +0x880). It is a 0x48-byte object built by the
constructor `0x894f4860` as type 0, additive. The voice's formant (+0x8c8,
type 2) and walk (+0x910, type 1) ramps are factors.
- `0x894f2440` runs at every note-on, after the voice setup (in
  `0x894f5e60`). It starts the pitch ramp from the converter's +0x68 to its
  +8 (`0x894f4890` copies the settings, `0x894f4680` starts):
  - +0x68 is the cents at the previous key. The per-note OSC setup sets
    +0x7c to the previous key plus the part's Octave Shift (v59, "The
    note's tuning").
  - +8 is the note's cents.
  - For VariPhrase it also starts the formant and walk ramps, which the
    settings leave off.
- **Start.**
  - span = to − from.
  - T is the time (TIME), or |span|·time/1200 (RATE: the time per octave);
    1 if T is 0.
  - The increment is 655360/T, at most 65536.
  - The phase starts at 0 (65536 if the span is 0).
  - The value starts at (phase/65536 − 1)·span, i.e. −span. With time 0 or
    no span it is done at once (value 0).
  - The ramp is active when the switch is on.
- **Tick** (`0x894f4b00`), in the 10-ms voice update `0x894f2090`, before
  the voice update. Its argument is the ticks since the last update, 1.
  - The first tick after a start only arms the ramp.
  - Each later tick adds the increment to the phase (FMAC). The value is
    (phase/65536 − 1)·span until the phase reaches 65536; then the ramp is
    done (value 0).
- **Value** (`0x894f4c00`): the ramp's value while active. Inactive, it is
  0 for the additive ramp and 1 for the factors. The pitch target adds it to
  the cents (v55).
- The note-on targets are computed before the ramp starts:
  - the note's first 10 ms play at its own pitch, plus whatever the voice's
    ramp held from its previous note;
  - the glide starts at the next update, with the voice's 70% step first
    taking the pitch toward the previous note.

**Also found:** the per-note OSC setup's Coarse and Fine (patch, OSC, Scale
Tune, bend, Random Pitch Depth), ported in v59 ("The note's tuning").

**Port and checks** (`xt_osc`):
- `xtPortaSettings` equals `0x894e66e0` on 1,664,612 cases (Time × Time Velo
  Sens × velocity × type × switch).
- `XtPitchRamp` equals the firmware's ramp (constructor, start, tick and
  value) on 4,886,610 values. These come from 4,000 random glides, each
  restarted mid-way.

**Plugin.**
- New: Portamento (switch), Mode (NORMAL/LEGATO), Type (RATE/TIME), Time
  (0..127, default 32) and Time Velo Sens.
- The glide starts from the previous note-on's key. LEGATO glides only while
  another key is held.
- Where the XT keeps the previous key and applies LEGATO (Mode, 0x116) was
  not traced; the plugin followed the manual. v60 ports the XT's own rule
  ("Mono/Poly, Legato and Octave Shift").
- With Portamento off, the output is bit-identical to v57.
- On the sine (RATE, Time 64), note 72 after note 60 glides from 440 to
  880 Hz, linearly in cents, over 2.54 s.

### Loop playback, BACKING and ENSEMBLE (2026-10-31, v57)

The multi-band players loop through the same cache and logic as SOLO (v56),
with these differences.

**Once per block.**
- The block (`0x8950e760`, for BACKING and ENSEMBLE alike) computes the
  next crossing first (`0x8950ccc0`).
- The walk list (`0x8950e860` forward, `0x8950e8f0` backward) fills its 16
  positions, then checks the crossing once (`0x8950c880`/`0x8950ca70`, type
  0x10).
- With type 0x10 the handlers set the bounce flags but don't negate the
  step. The step turns at the next block (`0x8950c280`).

**The jump** (`0x8950f710`, the loop entry of the event jump
`0x8950f6f0`):
- the cached event's index becomes current (`0x8950c830`);
- the position moves on by the cached position: the walk has had the
  crossing subtracted, so the overshoot is kept, unlike SOLO;
- the block's 16 walk positions are all set to it;
- each band restores its segment and record from its slot, the same
  {u32 segment entry, u32 record, u16 mark} as a vpdt event (band 0, an
  empty slot, bands 1..5; 12 bytes each mono, 24 stereo);
- ENSEMBLE restores band 0 only; its oscillator bands follow the walk
  position.
- Mode 3 zeroes the position before the jump, so it lands on the cached
  point.

**The cached events.** A band's slot is the segment entry containing the
point (one per event), then the record containing it, walked from the
segment's start; the event index is the last event before the point.
- Rebuilt this way at the encoder's own event positions, every slot comes
  out identical to the encoder's events (drum, vocal and chord: 0
  mismatches).
- The records tile each segment exactly.
- As for SOLO, the firmware's builder of the cached events is not found.

**Port and checks.** `xtMbLoopEvents` builds the four cached events as
bytes. `XtMbPlayer` loads them through its event-jump code (`restore`,
shared with note-on). `xt_fw bandcmp … loop:…` gives the firmware player
the same bytes. Results:
- BACKING stereo (the drum, three events), BACKING mono and ENSEMBLE mono
  and stereo, 14 cases: **bit-exact** (max difference 0);
- the cases cover One Way, Alternate and mode 3 in both directions, loops
  across and at an event, an 84-sample loop and a whole-wave loop, and walk
  reversals and holds.

**Plugin.**
- Loop, Loop Mode and Loop Start/End now apply to all three types.
- With Loop off the output is bit-identical to v56.

**Not modelled:** the event types 1–3 of STEP/EVENT playback in the event
steps (with normal playback they only clear flags that are never set).

### Loop playback, SOLO (2026-10-31, v56)

**Settings.** The OSC's Loop switch and the wave's loop:
- The switch reaches the player as byte 0 bit 0 (out+0xb4 bit 0). The
  encode-type tail sets it to (+0x5f ≠ 0) or (+0x90 = 0) in its normal
  playback branch.
- The wave's loop record (the `vplp` chunk, out+0xac) holds:
  - +4 and +8: the forward loop's start and end, in samples;
  - +0x10 and +0xc: the backward loop's start and end;
  - +0x18 and +0x19: the forward and backward Loop Modes;
  - +0, +0x1b: tuning (see v55).
- Emu_test.prj's waves loop the whole wave (end = frames − 1), in mode 1.
- With Vari off, the voice start `0x894f3110` sets up a PCM-style loop from
  the same record. That path isn't modelled.

**The cache.** The player reaches four cached loop events through +0x6c, at
+0xb4·k:
- k = 0: the forward start;
- k = 1: the forward end, whose mode is at +2;
- k = 2: the backward end;
- k = 3: the backward start, whose mode is at +2.

Each holds the same fields as a vpdt event at that point:
- {u16 event index, u16 mode, u32 position (1/16 sample);
- u32 segment-entry offset, u32 record offset;
- u16 mark}.

The event jump puts the record's start at position + mark − its first length.
- No code that writes these events was found. Literal-pool and pattern
  searches cover the player, the converter, the voice and the wave loader.
  The voice setup, run in the interpreter, leaves them untouched. So they
  are built per wave, elsewhere.
- The port builds each one as the jump needs it: the record containing the
  point, and the mark to that record's end.

**The next crossing** (`0x8950ccc0`) is computed at every block start
(`0x8950cd90`) and after each crossing. It is the nearest of:
- walking forward: the next event, the backward end if it lies ahead, and
  the forward end (mode > 0; always with Loop on, else only if ahead);
- walking backward: the mirror image, with the current event, the forward
  start and the backward start.

Inside an Alternate bounce only the bounce's far end counts.

**Crossing forward** (`0x8950c880`, from the walk `0x8950ce80`):
1. At the backward end:
   - if a backward bounce is turned round (flag 4), the walk turns back and
     the flag clears;
   - otherwise the flag "inside the backward loop" (0x10) clears.
2. At the forward end (not in a backward bounce), with Loop on:
   - flag 0x20 is set, the ended flag clears, and the rate factor becomes
     +0xa0;
   - mode 1 (One Way): the cached forward start loads (`0x8950d7d0`);
   - mode 2 (Alternate): flag 8 is set and the walk step negates;
   - mode 3: the step and position are zeroed, flag 0x40 is set, and the
     forward start loads.
3. With Loop off, flag 0x20 clears.
4. At the next event: the event step; at the last event, the note ends.

**Crossing backward** (`0x8950ca70`) mirrors it, at the forward start and
the backward start:
- One Way loads the backward end;
- mode 3 loads the backward start itself.

**The jump** (`0x8950d7d0`) loads:
- the event index, the position and the anchor;
- the segment and record pointers;
- the record end, from the mark (`0x8950d4c0`);
- the next event, from the vpdt.

The running grains and the trigger timer carry on. If the record landed in
is unvoiced, the Energy filter's cutoff (+0x148) is zeroed, as after an
event jump. The port missed that at first, and a stereo Alternate case was
off by −29 dB until it was added.

**The walk step** (`0x8950c280`, each block): ftrc(walk·2^25).
- Flag 8 (forward bounce): negated while the walk is ≥ 0; the flag clears
  once the walk itself is negative.
- Flag 4 (backward bounce): negated while the walk is negative. When the
  walk turns positive, the firmware clears flag 8, not 4 (sic). The port
  does the same.
- Flag 0x40 (mode 3): the step is 0. It is released once the walk turns
  away from the loop, which also clears 0x40, 0x20 and 0x10.

**The rate factors** +0x9c, +0xa0 and +0xa4 are 1 on a VariPhrase voice, as
the voice setup leaves them in the interpreter. So a loop region plays at
the voice's rate.

**Note-on.** In normal playback, `0x8950fa60` seeks (`0x8950fed0`) to the
start, or to the end when walk < 0, with event 0 current. It does not jump
to the end event. The SOLO harness now starts notes through it.
- The record cursor and anchor are the same either way, so v52's results
  stand: the no-loop comparisons are unchanged at −94…−98 dB.

**Port and checks** (`xt_fw solocmp … loop:on,fwdMode,bwdMode,fs,fe,bs,be`):
- `XtSoloVoice` against the firmware SOLO player run in the interpreter,
  with the same cached events: One Way, Alternate and mode 3 in both
  directions;
- also walk reversals and holds mid-loop, Robot with a loop, a 16-sample
  loop, the stereo wave and the sine;
- all at −92…−107 dB, the player's usual residual.

**Plugin:**
- Loop (switch), Loop Mode (One Shot, One Way, Alternate) and Loop
  Start/End (% of the wave; forward and backward alike; at least 16
  samples).
- One Shot keeps the switch without effect (modes 0).
- With Loop off the output is bit-identical to v55.
- On the vocal, a 10 s note sustains with One Way and Alternate; without a
  loop it ends after 3.5 s.

**Not yet:**
- loops on BACKING and ENSEMBLE (done in v57). Found so far:
  - their block (`0x8950e760`) computes the next crossing first;
  - the walk list (`0x8950e860` forward, `0x8950e8f0` backward) checks it
    once per block, after its 16 entries, calling `0x8950c880`/`0x8950ca70`
    with type 0x10. With that type the handlers don't negate the step; the
    bounce flags turn it from the next block (`0x8950c280`);
  - their jump (`0x8950f710`, the loop entry of the event jump `0x8950f6f0`)
    keeps the overshoot (position = cached position + position − crossing);
  - it fills the block's walk list with the cached position;
  - it restores six bands from per-band {segment, record, mark} entries, a
    stride of player byte 9 apart in the 0xb4-byte cached event; ENSEMBLE
    (+3 bit 0x80) restores band 0 only;
- separate backward loop points in the plugin;
- the PCM-style loop with Vari off;
- Time Trip (since v118).

### The OSC pitch (2026-10-31, v55)

The pitch, from the OSC's pitch values to the player's ratio, now follows
the firmware.

**The note's pitch** (`0x894e1250`), in 1/1000 semitone, from:
- the key;
- Coarse (+0x68, semitones);
- Pitch KF (+0x0e, ×10: 1000 = 100%);
- Fine (+0x70, tenths of a cent);
- a centre key, 60.

It is computed as:
- KF 0: (60 + coarse)·1000 + fine;
- KF 1000: (key + coarse)·1000 + fine;
- otherwise: (60 + coarse)·1000 + KF·(key − 60) + fine.

The encode-type tail `0x894e4030` runs for SOLO, BACKING and ENSEMBLE
(+0x87 = 1, 3, 5; 0 goes to `0x894e3d50`). It computes the pitch twice:
- at the key (+0x74);
- at the key in +0x7c (+0x78).

It also forms an integer key for key follow (out+4, `0x894e12c0`).

**Cents** (out+0x08; out+0x68 from the second key):
- (pitch − (Original Key − shift)·1000)/10, clamped to ±6000. The
  Original Key is the byte at +0x88; the shift is the signed byte at +0x85.
- With SOLO Robot Voice: (pitch − 69000)/10.
- The tail switches on +0x5e. Values 2 and 3 have their own branches
  (probably STEP and EVENT, by the order of the playback modes); one of
  them can move the centre key. Values 0 and 1 compute the pitch as above.

**The Pitch knob** (out+0x0c) is a ratio, 2^(c/1200 + (2·b183 − b182)/127),
computed by the firmware's powf (`0x8952ff9c`):
- c is the depth curve's cents (`0x894e0ce0`) at the knob plus
  modulation, clamped to ±63. That gives ±1200 cents at ±36 and ±6000 at
  ±63.
- b182 and b183 are the part's controller bytes; they are 0 without a
  controller.
- The pitch envelope's depth (+0x1c, times its velocity factor) and the
  LFO's depth (+0x34) go through the same curve.

**The wave's terms**, set by the voice setup `0x894f21c0` from the wave's
header (out+0xa4) and its loop record (out+0xac, the `vplp` chunk):
- +0x990 = 44000 / the header's u16 at +8. Every wave in Emu_test.prj
  stores 44000 there (in its `vpsc` chunk, which also holds the Original
  Pitch at +0xa), so the ratio is 1.
- With Robot Voice (+0x80 is 1, 6, 7 or 8, +0 is 2 and the Robot flag is
  set): +0x994 = +0x998 = 0, and +0x9a4 = 440, the robot's reference.
- Otherwise, a normal SOLO voice included:
  - +0x994 = the header's s32 at +0x18, plus 0.01 × the loop record's s8
    at +0x1b (cents);
  - +0x998 = 0.01 × the loop record's s32 at +0;
  - +0x9a4 = the wave's root frequency, 440·2^((Original Pitch − 69)/12),
    with those terms.
  - All three fields are 0 in every Emu_test.prj wave. They are where the
    wave's own fine tuning enters.
- v55 had the Robot condition the wrong way round. v56 corrects it, from
  the disassembly and from the firmware run in the interpreter. The plugin's
  numbers are unchanged: an imported wave has 0 in all three fields.

**The target**: `0x894f3a00` at note-on and `0x894f3ba0` in the update are
the same code. They compute:
1. Start from the cents.
2. Add env(+0x970)·(+0x1c), then LFO(+0x980)·(+0x34), each by FMAC.
3. Add the ramp at +0x880, then +0x994.
4. Clamp to ±6000.
5. Multiply by 0x3a5a740d (1/1200, a little under) and take exp2
   (`0x894ec090`).
6. Multiply by +0x0c, then by +0x990.
7. Clamp to ±32.
8. Multiply by exp2(−(+0x998)·0x3a5a740d).

The ramp `0x894f4c00` returns its value at +0x3c while active. When it is
inactive, it returns 1 for a factor ramp and 0 for an additive one. The
voice constructor (`0x894f1f50`) makes +0x880 additive (pitch: the
portamento) and +0x8c8 and +0x910 factors (Time Trip, I thought; v118 shows
Time Trip goes through the player's +0x8c, and these stay at rest).

**The update step** (`0x8950c1b0`): pitch (+0x144), formant, Energy, walk
and gain each become cur + (target − cur)·0.7, by FMAC (0.7 as 0x3f333333).

**What it does to the pitch.** The exp2 tables and the short 1/1200 leave
the XT slightly off equal temperament:
- +1, +4 and +7 semitones play 0.008 cents flat;
- −1 and −7 play 0.006 cents sharp;
- +12 plays 0.018 cents flat (a ratio of 1.99997914);
- −12 plays 0.020 cents sharp.

**Port and checks** (`xt_osc`, in the interpreter):
- `xtPitch10` equals `0x894e1250` on 238,080 cases (keys; Coarse
  −60…+60; KF −2000…+2000 including 0 and 1000; Fine).
- The converter with the SOLO tail gives 0 differences on 17,160 cases × 5
  values. The values are the cents at both keys, the Pitch ratio and both
  depths. The inputs cover knob + matrix sums −100…+100, KFs, keys,
  Coarse, Fine, Original Keys, shifts and Robot.
- `xtPitchTarget` equals both target functions on 117,000 cases, including
  an active ramp and set +0x990/+0x994/+0x998.
- `xtOscSmooth` equals the update's step on 13,000 cases.
- The firmware's powf differs from std::pow on 59 of the 127 Pitch ratios.
  So the firmware computes them at ROM load (`xtFwOscPitchRatios`).

**Plugin:**
- New: Coarse (−48…+48), Fine (−50…+50 cents), OSC Pitch (−63…+63) and
  Pitch KF (−200…+200%, default 100).
- Pitch Shift adds to Coarse and Fine, to the cent.
- The voice update's 70% step now rounds as the SH-4 does; it used to be
  host float.
- Measured on the sine (SOLO, 440 Hz at the Original Key):
  - Pitch KF 50 at note 72: 622.249 Hz (622.254 exact);
  - OSC Pitch +36: 880.001 Hz; −63: 13.750 Hz;
  - Coarse +7 with Fine −25: 649.83 Hz;
  - Robot at note 57: 220.004 Hz.
- Against v54:
  - a note at the Original Key differs at −141…−189 dB (the step's
    rounding);
  - other notes drift apart at −31…−41 dB over a second, from the XT's
    slightly flat and sharp ratios.

**Not modelled:**
- Random Pitch Depth (since modelled, v59);
- realtime controllers and the matrix;
- portamento (the ramp; since modelled, v58);
- the patch's own Coarse/Fine Tune path, where Pitch Shift stood in (since
  modelled, v59);
- the wave's own tuning fields (the header's +0x18, the loop record's +0
  and +0x1b): the plugin's imported waves have them at 0.

### Robot Voice (2026-10-31, v54)

Robot Voice (SOLO, with Vari on) plays the voiced parts at one steady pitch,
the note's.

**Converter.** In the SOLO tail `0x894e4030`, Robot and Vari on set flag
0x10 at +0xb4. The pitch (+0x08) then becomes the note's against A4:
(key·1000 + coarse/fine − 69000)/10 cents, clamped to ±6000, instead of
against the wave's Original Key.

**Player:**
- The flag reaches the player as byte 0 bit 0x10.
- The event jump's `0x8950c7e0` then sets the grain routine's pitch term
  (player +0x124) to 0 instead of 256.
- The block step (`0x8950cde0`) takes the rate from +0x70 without the
  sample-rate factor +0x9c.

**Grains** (`0x8950d5a0`), with that term 0:
- A voiced record's period is 1603.6364/A, i.e. A·440 Hz. The constant is
  0x44c8745d, 44100·16/440 as a float. The rate is the record period over
  that.
- An unvoiced record plays at rate 1, still gated by its info bits, times
  the formant.

So intonation, vibrato and pitch drift in the voiced parts disappear. Time,
formant and Energy work as before.

**Port and checks:**
- `XtSoloVoice::setRobot`, compared against the firmware player in the
  interpreter (`xt_fw solocmp … robot`):
  - ratios 0.297…1, with formant and walk changes, reverse and Energy;
  - the stereo wave and the sine;
  - all at −94.5…−110 dB, the port's usual residual.
- Measured on the vocal: normally 127–135 Hz and wandering; with Robot,
  note 57 holds 220 Hz and note 69 holds 440 Hz.

**Plugin:**
- The existing Robot button is the XT's Robot Voice when a ROM is loaded,
  for SOLO waves (the firmware applies it only in the SOLO tail).
- It is latched per voice at note-on, as the player latches it. Toggling it
  mid-note on the XT would switch the pitch reference but not the player.

### The OSC level, its amp envelope, and velocity (2026-10-30, v53)

**From the patch to the converters.** On an update, a tone (`0x894dc0b0`)
runs three steps:
- `0x894ebaa0` copies the per-part tables into the converters' input
  blocks (`0x894e4e90` the OSC, `0x894df600` the envelopes, `0x894eaae0`
  the TVA);
- the conversion pass `0x894ebbd0` converts them;
- the result goes to the voices.

The tables are written from the patch by `0x894dfe10`, one updater per
table, which read parameters by ID (`0x8947d310`):
- **OSC** (`0x894e6330` → `0x894e55d0` per oscillator, IDs 0x2a4 + 0x34·osc):
  - the input block is the table entry plus 8;
  - Wave Gain in dB → +0xa;
  - Level → +0x1c, Level KF ×10 → +0x1e, Level LFO depth → +0x20;
  - Time → +0x48, Time KF ×10 → +0x4a;
  - Time Offset → +0x61 and Energy → +0x62, both raw;
  - Formant KF ×10 → +0x54; Pitch KF ×100 → +0xe.

  The ×10 makes the converters see the displayed ±200: the patch stores
  KFs in steps of 10.
- **Velocity pairs** (`0x894e5c70`): each oscillator envelope's Velocity
  Curve and Sens go through the converters below. The level envelope's pair
  goes to +0x21/+0x22, the time envelope's to +0x4e/+0x50, the formant
  envelope's to +0x58/+0x5a.
- **Envelopes** (`0x894dfac0` → `0x894df8b0`, 16 tones × 13 envelopes, 10
  bytes each, 13 parameter IDs per envelope from 0x315 + 13e):
  - the group is [A, ·, D, S, ·, R, curve, sens, A-Sens, D-Sens, R-Sens,
    Time KF, …];
  - A-, D- and R-Sens are stored as trunc(knob·100/63);
  - Time KF is stored ×10.
- **TVA** (`0x894eb360` → `0x894eb170`): IDs 0x299…0x2a3; Velocity Curve
  0x3b7 and Sens 0x3b8 go through the converters.

**Velocity converters:**
- Sens (`0x894ebe20`) maps ±63 to ±200: s·100/32 up to ±32, then
  ±(100 + (|s| − 32)·100/31).
- Curve (`0x894ebed0`) maps 1…7 to the internal curves 0…6, and 0 to 7
  (no velocity effect).
- The curve function `0x894ebd60` (v48) then gives c, and the gain is
  (c/32768)². That holds for the patch TVA (`0x894ea990`, TVA +0x1c) and
  for the OSC level.

**The OSC level** (the converter's level slots):
- +0x54 = the level curve at L, times the velocity gain, where L comes from
  `0x894e0c90`:
  - L = level + trunc(127·KF·(key − 60)/6000), by a reciprocal multiply,
    within 0…127.
- +0x58, the level LFO depth: (d/63)·|d/63|.
- +0x5c, the gain: 2^(Wave Gain/6) (the firmware's powf), times the level
  curve at +0x84 and at +0x86. Those two bytes are set with the wave, are
  not decoded, and are taken as full.

**The voice:**
- The gain target is `0x894f3dc0`:
  - +0x5c · clamp((1 + LFO·+0x58) · env · +0x54, ±1);
  - env is the oscillator's amp envelope (+0x97c, envelope 4 or 8, curve 2
    like the TVA's);
  - the LFO is the oscillator's LFO (+0x98c).
- At note-on (`0x894f3e20`) the envelope term is its start level, or its
  output when the attack time is 0, and the player's gain starts there.
- The voice update moves the player's target (+0x80; the player sits at
  voice +0xd4) 70% of the way per 10 ms.
- Per sample, the player's gain moves 1/64 of the way to the target, then
  scales the output (×65536 for the int16 sample scale; `0x8950d1c2`).

**Port and checks** (`xt_osc`, in the interpreter):
- the curve function on every velocity, sens −210…+210 and curve 0…8:
  485,209 cases, identical;
- both converters: identical;
- the converter's level slots: 179,712 cases × 3 values, identical;
- the gain target and its note-on form: 6,000 cases, identical;
- the gain stage against the firmware players with gain changes: SOLO
  −95.6 / −100 dB (its usual residual), BACKING and ENSEMBLE bit-exact.

**A fix from this:** the plugin passed the A/D/R-Sens knobs (±63) to the
time converter, which expects the stored trunc(knob·100/63). So velocity
changed envelope times about 1.6× too little. Now fixed, for every
envelope.

**Plugin:**
- OSC TVA: Level, Level KF, Level LFO depth and Wave Gain; its amp envelope
  (A/D/S/R, A/D/R-Sens, Time KF) in series with the patch TVA; OSC
  Velocity Sens and Curve.
- The patch TVA's velocity now uses the curve, with Velocity Sens and Curve
  (default +16 and curve 1, close to the former linear gain).
- Defaults leave the level unchanged. Against v52 the output differs only
  in release tails, by −89…−95 dB, because the OSC envelope (release 127)
  now runs under the TVA's release.

**Open:**
- +0x84/+0x86;
- the patch TVA's own Level, Level KF and LFO;
- the velocity sens of the pitch, time and formant envelope depths;
- matrix and controllers.

### Reverse and held playback (2026-10-30, v52)

The XT plays a VariPhrase wave backward when the walk is negative. That
happens with Time Offset BWD, or Time below −20 with FWD, or a time
envelope or LFO crossing zero. All three players are now ported both ways.

**Reference: the firmware's players in the SH-4 interpreter.** The players
used to be checked in Ghidra's P-code emulator (EmuSolo.java,
EmuBacking.java) at about 1 ms per sample. `XtFirmware.cpp` now runs the
same setups natively, in under a second per note:
- `xtFwSoloPlay` writes the records as the XT lays out a one-segment SOLO
  vpdt and calls the firmware's own event jump for note-on;
- `xtFwBandPlay` builds the bands in place and calls the BACKING or
  ENSEMBLE note-on.

Both take walk changes at block starts. `xt_fw solocmp` and `xt_fw
bandcmp` compare them with the ports (notes.txt).

**SOLO vpdt events** (single segment):
- Event e sits at (stride·e + 8)·2, where the stride is 10 u16 mono and 16
  stereo. Each is {u32 flags, u32 position, u32 segment-list offset, u32
  record offset, u16 first mark}.
- Event 0 is at position 0. It points at record 0, with mark = that
  record's length.
- Event 1 is at the wave's end. It points at the stream's (256, 1)
  terminator, with mark 256.
- The segment entries {record offset, 1, start} follow, then the record
  count and the records. The XT's waves 003, 004 and 358 have exactly this
  layout.
- Note-on (`0x8950fa60`) takes event 0, or event 1 when walk < 0. The event
  jump (`0x8950d7b0`) puts the position and the anchor at the event and
  places its first record to end at position + mark. So a backward note
  starts at the end, on the terminator (unlimited budget, length 16
  samples).

**SOLO walk** (`0x8950ce80`, per sample). The position steps by
(walk step >> 1) in 12.20, with walk step = trunc(walk·2^25) per block
(`0x8950c280`). There are three branches:
- **Forward** (as ported before): the anchor advances. Past the record's
  end it repeats the record, or the next records load.
- **Backward:** the position stops at the start (+0x18, ended −1). The
  anchor moves back. Below the record's start it repeats the record if the
  position is still inside it and the budget allows (or it is the first
  record), else the previous records load (`0x8950d530`). That reader
  steps back over 0xFFFF continuation entries and keeps the following
  record's length as the next period.
- **Walk step 0:** the position holds, and the anchor steps toward it from
  either side, forward when at or below it, backward when above. So a
  frozen note keeps its record. Before v52 the port always advanced the
  anchor, and that could walk through records with no repeat budget (−8 dB
  against the firmware on a hold mid-note).

Details the port now follows:
- A budget of 0x7E00 means unlimited.
- The last record repeats past the wave's end whatever its budget (its real
  budget matters backward).
- The repeat cost is computed in floats from the integer anchor and walk
  steps (`0x8950d6f0`).
- The anchor wraps with the firmware's loop. It differs from "minus the
  length" only when one step spans more than a record.
- The trigger part (`0x8950d080`…) and the grain parameters do not depend
  on direction. Reversed SOLO grains still play forward.

**BACKING / ENSEMBLE walk.**
- The walk list is filled backward by `0x8950e8f0` when the walk step is
  negative. It stops at the start bound with ended −1.
- Each band's gain field is its direction. Note-on sets it to the sign of
  walk. The stepper sets it to 1 or 0 going forward, and to −1 going
  backward, or just below 0 (0xb3d6bf94, about −1e−7) once stopped at the
  start.
- With a negative gain the band:
  - moves its segment cursor back one when the walk passes its grain's
    start (`0x8950e820` / `0x8950eb20`);
  - reads with step (−rate) >> shift;
  - steps with `0x8950f4c0`, the mirror of `0x8950f400`, repeating below
    the record start or loading previous records (`0x8950f5d0`).
- The reads are multiplied by the gain, so reversed bands come out
  inverted.
- A band that reaches a record edge running against the walk turns: gain
  ±1, read position reflected about the edge (`0x8950f4b0`, `0x8950f570`).
  That is how a direction change spreads through the bands.
- ENSEMBLE's frame loader (`0x8950dcb0`) builds each {delta, base} from
  frame f when the walk step is negative: base = value(f), delta = old
  base − base.

**Ports and checks:**
- `XtSoloVoice`:
  - now in the firmware's terms: fixed-point position, a record cursor
    with the terminator, both anchor directions and the hold;
  - against the firmware on vocal, sine and stereo wave 358:
    - forward, held and backward walks;
    - schedules 1 → 0 → 1, 1 → −1, −1 → 0 → 1;
    - a sine sweep through zero;
  - all agree at −94…−109 dB, the same level as forward before (the port's
    output stage runs in double);
  - the hold mid-note goes from −8 dB to −95 dB.
- `XtMbPlayer`:
  - **bit-exact** against the firmware for BACKING and ENSEMBLE: forward,
    backward, held, schedules and the sweep, on the drum and the chord;
  - the only exceptions are differences of about 7e−46: the interpreter
    flushes denormals, as the SH-4 does with FPSCR.DN, and the test CLI
    does not.
- At the default controls the plugin's output is bit-identical to v51, for
  every type.
- The engine's SOLO render moves only where its walk is not a float: the
  firmware stores walk as a float. The suite's time cases are unaffected.

**Plugin:** the walk is no longer held at 0. Time Offset BWD, or Time
below −20 with FWD, starts the note at the wave's end and plays it
backward, and the time envelope or LFO can swing it through zero.

### The VariPhrase converter: time, formant and Energy (2026-10-30, v51)

**Where.** The OSC converter `0x894e48a0` (called for OSC1 and OSC2 by the
conversion pass `0x894ebbd0`) hands a VariPhrase OSC to `0x894e2f20(input,
OSC block)`. That routine fills:
- the pitch, time and formant slots;
- their envelope and LFO depths;
- the gain and level.

It then calls a tail chosen by the encode type (+0x87):
- `0x894e4030` for SOLO, BACKING and ENSEMBLE;
- `0x894e3d50` for LITE.

The tail writes the pitch offsets (+0x08, +0x68), the flags (+0xb4) and
Energy.

**Input block** (bytes unless noted). Matrix destinations are given as
OSC1/OSC2:

| Offset | Parameter | Matrix |
|---|---|---|
| +0x48 | Time | 7/21 |
| +0x4a (s16) | Time KF | |
| +0x4c | Time LFO Depth | 13/27 |
| +0x4d | Time Envelope Depth | 8/22 |
| +0x4e, +0x50 (s16) | time envelope velocity curve, sens | |
| +0x52 | Formant | 9/23 |
| +0x54 (s16) | Formant KF | |
| +0x56 | Formant LFO Depth | 14/28 |
| +0x57 | Formant Envelope Depth | 10/24 |
| +0x58, +0x5a (s16) | formant envelope velocity curve, sens | |
| +0x61 | Time Offset: 0 BWD, 1 ZERO, 2 FWD (labels at 0x8977c4dc) | |
| +0x62 | Energy (OFF, 1–127) | none |

Each knob is added to its matrix value (`0x894d96d0`, a 16-bit sum). It
also gets a realtime-controller term:
- the structs behind `0x894e7010`/`0x894e7320`;
- the part's bytes at +0x180…+0x183.

These terms are 0 with no controller assigned.

**Time** (+0x10): `0x894e0da0(t, controller, part bytes, Time KF, key −
60, base)`.
1. base = −1, 0, +1 for BWD, ZERO, FWD.
2. x = t + controller·63/12000, clamped to ±63.
3. For |x| ≤ 60: x/20 + base. Beyond that, a line on to ±5 at ±63:
   ((5 − (base + 3))·(x − 60))/3 + (base + 3), and its mirror below −60.
4. Add (b1 − 3·b0)/127 from the part bytes, multiply by
   2^(KF·(key − 60)/1200) (the firmware's powf `0x8952ff9c`), clamp to ±5.

This is the player's walk speed. With FWD:
- Time 0 plays as recorded;
- +20 plays at double speed, and −10 at half speed;
- −20 holds (the VariPhrase freeze);
- −40 plays backward at normal speed.

**Time envelope and LFO depths** (+0x24, +0x3c): d/20 within ±60, then
3 + (2d − 120)/3 out to ±5 at ±63.
- The envelope's d is not clamped. It is multiplied by its velocity factor
  (the curve function `0x894ebd60`, /32768).
- The LFO's d is clamped to ±63 first.

**Formant** (+0x14), in octaves:
1. f·12000/63 + controller, clamped to ±12000.
2. Then /12000·63/63, plus KF·(key − 60)/1200.
3. Clamp to ±1.

**Formant envelope and LFO depths** (+0x2c, +0x44): d/63 octaves. The
envelope's is multiplied by its velocity factor; the LFO's is clamped to ±63
first.

**Energy** (+0x18, in `0x894e4030`): the u16 level curve at 0x897cfcb8 at
the Energy byte, divided by 32768.
- The curve is 32768·(k/127)², truncated, so g = (k/127)²: OFF → 0,
  64 → 0.254, 127 → 1.
- The vocal references' g ≈ 0.05 is Energy ≈ 28.
- The same curve scales the OSC's level and gain (+0x54, +0x5c).
- No matrix value is added.

**The other slots it writes:**
- +0x0c, the pitch ratio: 2^(curve(Pitch)/1200 + a part term);
- the pitch envelope and LFO depths (+0x1c, +0x34), through the depth curve
  `0x894e0ce0`, as already ported;
- the gain (+0x5c): 2^(Wave Gain/6) times the level curve at +0x84 and at
  +0x86;
- the level (+0x54) and its LFO depth (+0x58, the signed square of d/63);
- the envelope indices: pitch 1/5 (+0x20), time 2/6 (+0x28), formant 3/7
  (+0x30), level 4/8 (+0x4c);
- the LFO index 1/2.

All three depths ride the same OSC LFO.

**The voice's targets.** Note-on uses `0x894f3ac0`/`0x894f3b30`; the
update uses `0x894f3c90`/`0x894f3d40`.
- walk = (time + env(+0x974)·time env depth + LFO(+0x984)·time LFO depth)·k,
  clamped to ±5. For LITE with Vari on (the LITE tail sets +0x80 = 2; 0
  with Vari off) and with voice flag +0xd4 bit 1, it is also × voice +0x99c.
- formant ratio = 2^(formant + env(+0x978)·depth + LFO(+0x988)·depth)·k,
  clamped to [0.5, 2], × voice +0x9a0.
- k (`0x894f4c00`) is 1 unless the object at +0x910, +0x8c8 is active. Then
  k is that object's value at +0x3c. (This called it Time Trip's; it is not, v118.)

The update moves the player 70% of the way to these every 10 ms (v49).

**exp2** (`0x894ec090`, for the formant and pitch ratios):
- n = trunc(|x|) and q = trunc(frac·65536);
- 2^x = hi[q >> 8]·int[n]·lo[q & 0xff].

The tables are at 0x897d0228 (hi), 0x897d0628 (lo) and 0x897d1228
(2^0…2^6). Their negative-argument counterparts are at 0x897d0a28,
0x897d0e28 and 0x897d1244.

**Port.** `plugin/Source/XtOsc.cpp` equals the firmware's `0x894e2f20` on
all 310,545 cases, six values each (`xt_osc`, notes.txt):
- knob + matrix sums −100…+100;
- the three offsets;
- ten KFs;
- every key.

The firmware runs in the SH-4 interpreter with the matrix fetch hooked. The
port's exp2 equals `0x894ec090` on 17,143 arguments. The Time KF scales come
from the firmware's powf at ROM load (`xtFwOscTimeKeyScales`).

**Plugin.**
- With a ROM, the TIME and FORMANT knobs are the XT's Time and Formant
  (−63…+63).
- New: Time Offset, Time KF, Formant KF, and Energy OFF/1–127.
- New: the time and formant envelopes (curve 0, like the pitch envelope)
  and the LFO's time and formant depths.
- At the defaults the output is bit-identical to v50 (SOLO, BACKING,
  ENSEMBLE, stereo).
- On the vocal: Time 0 plays 3.55 s, +20 plays 1.79 s, −10 plays 7.07 s,
  and −20 holds.
- The band players now accept walk 0 (they rejected it): their forward path
  holds the walk position.

**Not modelled:**
- backward play: walk < 0 holds at 0, because the players' backward paths
  (`0x8950e8f0`, `0x8950f4b0`) are not ported — ported in v52;
- the time and formant envelope depths' velocity factor (their curve and
  sens are not exposed);
- controllers, the matrix, and Time Trip (Time Trip since v118).

### The LFO (2026-10-29, v50)

**Converter** (`0x894e0290`, patch → block):
- waveform: SIN, TRI, SAW, SQR, RND, TRP, S&H, CHAOS map to internal 1, 0,
  2, 3, 6, 4, 5, 7 (`0x894e0010`);
- rate: the float table at 0x897ced34, in Hz (0.001…40). Tempo sync takes
  the clock table instead;
- delay and fade: (k/127)²·10000 ms;
- offset index 0–4, fade mode 0–3, key sync.

**Object** (`0x894f0b40`, 0x98 bytes). A delay/fade base (vtable
0x89833ef8) holds:
- state (+0), direction (+1), a delay counter in ms (+2);
- fade times (+4/+8), delay (+0xc), rates (+0x10/+0x14);
- rest and target levels (+0x18/+0x1c) and the amplitude (+0x20,
  0…131072).

Its methods (some reached only through the vtable):
- `0x894f04a0` init;
- `0x894f04d0` start: delay, then fade, then full;
- `0x894f0520` direction: in 0 → full, or out full → 0;
- `0x894f0560` delay;
- `0x894f05a0` and `0x894f0570`, the fade rates: 131072/(T/10) per tick,
  signed by the direction;
- `0x894f05d0` tick: the delay counts n·10 ms, then the fade moves by n·rate
  to its target, clamped to 0…131072.

The LFO's own fields: phase +0x40, held values +0x44/+0x48, chaos state
+0x6c, output +0x38.

**Note-on** `0x894f0cf0`:
- phase 0;
- trigger `0x894f0bf0`: direction by fade mode (`0x894f0780`), then the
  times;
- ON modes start the fade now, OFF modes at note-off (`0x894f0810`);
- then one tick with n = 0.

A key-unsynced LFO takes its phase from a shared one (`0x894f0e60`, +0x7c).

**Tick** `0x894f1050`:
1. phase += n·rate, with rate = Hz·65535/100 clamped to 1…65535
   (`0x894f1320`); wrap at 65535.
2. The waveform, on the 16-bit phase:
   - triangle and trapezoid: piecewise lines;
   - sine: the .data table of 257 floats, interpolated on the low byte;
   - saw: (p − 32768)/32768; square: ±1;
   - S&H, RND and CHAOS draw from a logistic map, x ← 3.997·x·(1 − x),
     output 2x − 1 (`0x894f4cd0`). S&H holds a value per cycle, RND ramps to
     the next, CHAOS takes one per tick.
3. Add the offset (−1, −0.5, 0, +0.5, +1).
4. Times amplitude/131072.

The constructor seeds +0x6c with |2x − 1| from a global logistic map.

**Port.** `plugin/Source/XtLfo.cpp` is tick-for-tick identical to the
firmware's LFO built and run in the SH-4 interpreter (`xt_lfo`, 18 cases):
output, phase and amplitude. The cases cover every waveform, rates 0–127,
the offsets, delay and fade, and all four fade modes with note-off. The 3.997
is the ROM's 0x407fced8, one unit in the last place below the nearest float.

**Plugin.**
- An LFO per voice, stepped with the envelopes, drives pitch through the
  depth curve `0x894e0ce0`. A rate-64 triangle at depth 13 measures ±47
  cents at a 1.03 s period.
- Key Sync off follows a free-running LFO.
- Edits take effect while notes play: new times and rates, keeping the phase
  and the fade state.

Not modelled: tempo-synced rates (since v118), and the LFO's other destinations (time,
formant, filter, level, pan).

### The voice update and the pitch envelope (2026-10-29, v49)

**Voice update** (`0x8950c1b0`, every 10 ms from the tone update). Each
player value moves 70% of the way to its new target (0x3f333333 = 0.7):
- the pitch ratio (+0x70), from `0x894f3ba0`;
- the formant ratio (+0x74), from `0x894f3d40`;
- Energy (+0x78), toward voice +0x18;
- the walk (+0x7c), from `0x894f3c90`;
- the gain target (+0x80), from `0x894f3dc0`.

Note-on sets them directly (`0x894f3a00`…). So knob changes and
modulation reach the player in 10 ms steps, 70% per step.

**Pitch** (`0x894f3a00`, update twin `0x894f3ba0`):
1. cents = V8 + env(+0x970)·V1c + LFO(+0x980)·V34 + ramp(+0x880) + V+0x994.
2. Clamp to ±6000.
3. Ratio = exp2(cents/1200) (`0x894ec090`, with 1/1200 as 0x3a5a740d),
   × V+0xc × V+0x990, clamped to ±32.

Time (`0x894f3ac0`) and formant (`0x894f3b30`) have the same shape with
their envelopes and LFOs at +0x974/+0x984 and +0x978/+0x988.

**Depth knob → cents** (`0x894e0ce0`; the pitch envelope's V1c and the
pitch LFO's V34):
- |d| ≤ 12: a 13-byte table at 0x897cef44 (0 to 40 cents);
- then 50·|d| − 600 (to 36), 100·|d| − 2400 (to 48), 200·|d| − 7200
  (to 60), and 400·|d| − 19200, with d's sign (±1200 cents at 36, ±6000 at
  63).

The converter `0x894e1920` multiplies the envelope depth by the envelope's
velocity factor (the curve function `0x894ebd60`, whose Velocity Sens unit
is open, see v48).

**Port.**
- `xtPitchDepthCents` equals `0x894e0ce0` on all 127 depths (`xt_env rom
  conv`), with the table read from rom.2.
- The plugin has a pitch envelope (ADSR through the same generator, curve 0,
  and Depth), and its voices are updated as the XT updates them: the
  envelopes step, then pitch, formant, walk and Energy move 70% toward their
  targets, then the TVA.

**Not yet:**
- the time and formant envelopes and the OSC LFO depths: their converter,
  the VariPhrase one that also writes time, formant and Energy, is not
  found (see v48, Energy). Found in v51 (`0x894e2f20`);
- the LFO itself (`0x894f1050` tick, rate `0x894f1320` = f·65535/100 phase
  per tick with f in Hz, setup `0x894f0cf0`; tempo sync, delay and fade, and
  a shared-LFO link at +0x7c).

### Envelope velocity and key follow; velocity to level; Energy (2026-10-29, v48)

**Times** (`0x894df7b0`, knob, velocity, sens, 16.16 scale):
1. T = the time table at the knob (a sign-extended u16).
2. If sens ≠ 0, T −= ((velocity − 64)·q) >> 8, with q = sens·40 (a
   reciprocal multiply rounded toward zero). T ≤ 0 means no time, and T is
   then capped at 12000.
3. The 16.16 scale applies: (T·frac) >> 16 + T·int, capped at 12000, at
   least 1.

The converter `0x894df240` calls it three times:
- attack with the note-on velocity, A-Sens and no scale;
- decay with the note-on velocity, D-Sens and the Time KF scale;
- release with the note-off velocity, R-Sens and the Time KF scale.

**Time KF scale** (`0x894df760`): 2^(−kf·(key − 60)/1200)·65536,
truncated, with kf −200…+200. The firmware's powf is not correctly rounded:
251 of the 5248 (kf, key) values differ from std::pow by a few units.

**Port.**
- `xtEnvTime` matches `0x894df7b0` on all 1,045,760 cases tried in the
  interpreter (`xt_env rom conv`).
- The KF scales are the firmware's own, computed at ROM load
  (`xtFwEnvKeyScales`, 41 × 128).
- The plugin has A/D/R-Sens and Time KF; a note-off converts the times
  again with its velocity before the release starts.

**Velocity to level** (the TVA):
- The TVA object (`0x894ea990`) computes gain = (c(velocity, sens,
  curve)/32768)².
- c is `0x894ebd60`:
  - s comes from a linear table (0x897cfeda, 200 entries, 65536 at 200);
  - curve is one of 7 tables of 128 bytes (pointers at 0x897cfb9c; about
    255 at velocity 0, falling to 0 at 127);
  - c = 32768 + s·(256 − curve[v])/256, minus s when s > 0, clamped to
    0…32768. Sens 0 or curve ≥ 7 gives 32768.
- The level converter (`0x894eab40`) multiplies this by three level-table
  entries: the Level knob with matrix and Level KF·(key − 60), and two
  patch levels.

Open: the sens the TVA reads ranges over ±200, while the patch's Velocity
Sens is −63…+63. The scaling between them, in the code that fills the
per-part table at 0x89d2a6d0, is not found. So the plugin keeps a linear
velocity gain for now. — Found in v53 (`0x894ebe20`).

**Energy: found in v51** (`0x894e4030`, g = (k/127)²). What v48 knew:
- The OSC parameter block lives at conversion context +0x434 (OSC2 at
  +0x508). The glue `0x894db8a0` passes it on, and `0x894f2600` copies it
  into the voice; +0x18 is the Energy target that `0x8950c1b0` smooths.
- The OSC converter `0x894e1920` writes +8/+0xc and the envelope and LFO
  depths, but not +0x10, +0x14 or +0x18 (time, formant, Energy).
- The parameter names ("OSC 1 Energy" at 0x8975e3d4) sit in descriptors
  {name, short name, id} at 0x89835b5c, not in the patch layout. (v51: their
  ids, 0x2ae…0x370, are not the converter's matrix destinations, and the
  converter fetches none for Energy.)
- The routine that writes +0x18 is not found yet.

### The WAV import converter (2026-10-29, v47)

The resampler found in pass 1 (`0x8952dc60` mono, `0x8952dd20` stereo) is
the XT's WAV import converter to 44.1 kHz.

**Format drivers.** One per input format, each taking (context, bytes,
data); a call with no data flushes 8 zero samples per channel. Each turns
the input into int16:

| Driver | Input | Conversion to int16 |
|---|---|---|
| `0x8952e4d0` | 16-bit | as is |
| `0x8952e620` | 8-bit unsigned | (b − 0x80)·256 |
| `0x8952e740` | 24-bit | the top two bytes (a truncation) |
| `0x8952eab0` | 32-bit | the top two bytes (a truncation) |

The int16 samples go into a 1024-sample buffer (`0x8952dbf0` makes room),
and the resampler runs while 8 samples per channel are available beyond
the read point. Output goes to the context's output pointer.

**Setup** `0x8952dec0` (state at RAM 0x8a0154f0):
- step = rate·2^24/44100, in 8.24 fixed point, with the phase at 0;
- write index 7·ch and read index 8·ch;
- the buffer cleared.

**Resampler**, per output sample:
1. 16 taps around the read point, at two adjacent phases of the 129-tap
   table at 0x897eebb0 (8 phases, phase from bits 21–23).
2. The two dot products are blended by the remaining 21 bits.
3. The result is clipped to ±32767 and truncated to int16.
4. The phase advances by the step and the read index by its integer part.

**Measured** on our 48 kHz sources against scipy's resample_poly:
- no delay, +0.08 dB;
- ±0.2 dB to 15 kHz, then +0.6 to +5 dB above 19.5 kHz (a short filter);
- −42 to −48 dB of residual.

44.1 kHz files are not converted: the XT's waves 003/004 are
sample-identical to their sources.

**Use.** `xtFwImport` runs setup, the 16-bit driver and the flush from
rom.2 in the interpreter (0.1–0.2 s for a few seconds of audio). The plugin
imports through it, keeping a wider source's top 16 bits first, as the
24-bit and 32-bit drivers do. The engine's BACKING/ENSEMBLE path does the
same. Suite 76.601 → 76.765: chord +0.3 to +2.5, drum within ±0.2.

## Next steps
1. ~~Obtain firmware~~ — done (XT v2.02, user's copy, in `firmware/`).
2. ~~First-pass triage~~ — done (above).
3. ~~Install Ghidra, import rom.2~~ — done (pass 1 above).
4. Trace: (a) who reads the 64×6 interpolation kernel at rom.2+0x20ee00;
   (b) the routine behind "SAMPLE Encode" / "Encode Type :" — pitch
   detection, grain cutting, LITE/SOLO/ENSEMBLE/BACKING handling; (c) the
   formant-knob mapping; (d) what "Encode Depth" does.
5. ~~SH-4 vs DSP~~ — SH-4 runs float audio DSP (pass 1); locate the
   VariPhrase voice render itself (likely in 0x894D–0x8953 cluster; look
   for code reading encoded sample data and calling the per-voice chain).
6. Read the XT circuit diagram (Main 1/5, printed p. 40) page image for the
   DSP part number and confirm IC10.
7. Examine `V-SynthXTDiskImage.bin` (FAT image) for preset/wave content.

Rule: firmware is Roland copyright. Record facts and measurements here;
never commit firmware files, disassembly listings, or copied tables.

## Sources
- V-Synth service notes: https://www.manualslib.com/manual/1339839/Roland-V-Synth.html
- V-Synth XT service notes: https://www.manualslib.com/manual/1713479/Roland-V-Synth-Xt.html
- V-Synth GT service notes: https://www.manualslib.com/manual/1853354/Roland-V-Synth-Gt.html
- V-Synth FAQ (encode types, update method, VC-1 boot): http://www.rolandclan.com/media/17/V-Synth%20FAQ.html
- VariOS downloads: https://www.roland.com/fr-ca/products/varios/downloads/
- VariOS review (processing in module): https://www.soundonsound.com/reviews/roland-varios
- JE-8086 precedent: https://synthanatomy.com/2026/01/je-8086-roland-dsp-emulator-insights-and-the-supersaw-is-no-longer-a-mystery.html
