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

**Where the XT differs from vpenc** (rom.2 addresses):

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
- The knob → g mapping. g is smoothed ×0.7 per update toward a voice float
  at +0x18 (`0x8950c1b0`); its converter is probably inside `0x894e1920`.
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
- reverse playback (`0x8950e8f0`, `0x8950f4c0`, `0x8950f5d0`);
- loops and the event walker at the bounds (`0x8950c880` / `0x8950ca70`);
- event seeks (`0x89510bf0`);
- the voice flags 0x2 / 0x4 / 0x8 / 0x20;
- the loop struct's cache bounds.

**Not yet in the engine.** The player needs a loaded wave's band data and
its vpdt. Both come from firmware not yet ported: the load-time rebuild
(`0x894a9940`: the filter bank for BACKING, the sinusoidal analysis for
ENSEMBLE) and the multi-band encoder (`0x89516000` and the step that writes
the vpdt).

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
