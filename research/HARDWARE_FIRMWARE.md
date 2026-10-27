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

Next targets: the encoder's per-record budget field (XT vocal info >> 3 =
11–20; ours unlimited), and the LITE player.

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
