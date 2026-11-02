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
- the FPU: single and double precision, both banks, SZ = 1 pair moves,
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
- `xtFwSoloEncode()` runs the SOLO encoder; its records match
  XtSoloEncoder's.
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
itself (a virtual call) is still not located, but nothing about the clock
is left to infer.

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
  no LFO; velocity stays a linear gain).

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
  the other oscillator's OSC SYNC input.
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
  above: 50d − 1950), /1200. Not ported.
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
when Alias Cancel is on, else 0. The port uses the descriptors' defaults. A
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
inline as first noted.

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
- LFO rates above 127 (tempo sync);
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
- How a bender move reaches a voice that is already sounding is not traced.

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
- Time Trip.

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
portamento) and +0x8c8 and +0x910 factors (Time Trip).

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
- k (`0x894f4c00`) is 1 unless the Time Trip object (+0x910, +0x8c8) is
  active. Then k is that object's value at +0x3c.

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
- controllers, the matrix, and Time Trip.

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

Not modelled: tempo-synced rates, and the LFO's other destinations (time,
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
