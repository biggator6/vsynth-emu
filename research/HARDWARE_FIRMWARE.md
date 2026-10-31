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
  - +0x68 is the cents at the previous key. The per-note OSC setup
    `0x894e4c30` sets +0x7c to the part's previous key plus the part's key
    shift.
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

**Also found, not modelled yet.** The per-note OSC setup `0x894e4c30`
forms:
- the coarse (+0x68): patch and OSC Coarse plus the matrix;
- the fine (+0x70):
  - the scale tune for the key's pitch class;
  - patch and OSC Fine;
  - the part's tuning;
  - the matrix;
- Random Pitch Depth (+0x1a, into +0x6c), from a linear congruential
  generator (x·0x497d + 1).

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
- Where the XT keeps the previous key (the part's +0x20) and applies LEGATO
  (Mode, 0x116) is not traced; the plugin follows the manual.
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
- Random Pitch Depth;
- realtime controllers and the matrix;
- portamento (the ramp);
- the patch's own Coarse/Fine Tune path (Pitch Shift stands in for it);
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
