# V-Synth Test Recording Guide

This file is the master recording reference.  After each session add new files to the
checklist and update `RESEARCH_LOG.md` with results.

---

## Recording Chain

```
DAW (playback) → Import and encode PCM sample →  V-Synth PCM oscillator → V-Synth audio output → DAW (capture)
```

- Sample rate: **48000 Hz** (V-Synth native)
- Bit depth: **24-bit PCM** (current test files are 24-bit; 16-bit also handled)
- Channels: **stereo** (capture as-is; renderer downmixes to mono automatically)
- Trim: set so the passthrough signal peaks around **−3 to −6 dBFS** — not too quiet, not clipping

---

## V-Synth Patch: [EMU_TEST]

Document current settings here; update if anything changes between sessions.

| Parameter | Setting | Note |
|---|---|---|
| Oscillator 1 type | **External (EXT)** | Routes audio input through VariPhrase |
| Oscillator 2 | Off | Avoids contamination from onboard PCM |
| VariPhrase mode | **SOLO** | Single-voice; deterministic behavior |
| Effects (FX) | **ALL OFF** | Isolates VariPhrase algorithm |
| COSM | **OFF** | |
| Reverb / Chorus | **OFF** | |
| TVF (filter) | Open / flat | Set cutoff to max, resonance 0 |
| TVA (amp) | Unity gain | No envelope modulation |
| VariPhrase Pitch | As needed | Set per test case |
| VariPhrase Time | As needed | Set per test case |
| VariPhrase Formant | As needed | Set per test case |
| Formant (PCM) page: Energy | **unknown — please read and note** | The references fit Energy **on**, at a small amount (see "Energy check and calibration" below) |
| Formant (PCM) page: Formant KF, Fmt LFO Depth, formant envelope depth | **unknown — please read and note** | A non-zero value would explain the formant offset in the May sine takes |

**Critical:** zero all VariPhrase parameters first, confirm passthrough is clean (null test
should show > 40 dB null when comparing passthrough to input), THEN set the test parameter.

---

## File Naming Convention

```
{signal}_{parameter}_{value}.wav
```

Examples:
- `vocal_aah_formant_upmax.wav`    (formant +12 st)
- `drum_hit_time_2x.wav`           (time stretch × 2)
- `chord_Cmaj_pitch_up7st.wav`     (pitch +7 st)

For the matching passthrough (V-Synth bypassed or all params at zero), use the same base name
with `_passthrough` suffix:
- `vocal_aah_passthrough.wav`
- `drum_hit_passthrough.wav`
- `chord_Cmaj_passthrough.wav`

---

## Directory Layout

```
analysis/test_files/
├── passthrough/      ← all unprocessed reference inputs (one per signal type)
└── sustained/        ← all V-Synth processed outputs
```

Every processed file in `sustained/` must have a matching passthrough file in `passthrough/`
with the same base name.  The offline renderer takes the passthrough as its `--input`.

---

## Completed Recordings

### Sustained sine (6 files — done)

| File | Location | Status |
|---|---|---|
| sine_440_pitch_up7st.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_pitch_down12st.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_time_2x.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_time_halfspeed.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_formant_upmax.wav (+12 st) | sustained/ + passthrough/ | ✅ done |
| sine_440_formant_downmax.wav (−12 st) | sustained/ + passthrough/ | ✅ done |

### Held vowel "aah" (7 files — done)

| File | Pitch | Time | Formant | Status |
|---|---|---|---|---|
| vocal_aah_passthrough.wav | 0 | 1× | 0 | ✅ done |
| vocal_aah_time_2x.wav | 0 | **2×** | 0 | ✅ done |
| vocal_aah_time_halfspeed.wav | 0 | **0.5×** | 0 | ✅ done |
| vocal_aah_pitch_up7st.wav | **+7 st** | 1× | 0 | ✅ done |
| vocal_aah_pitch_down12st.wav | **−12 st** | 1× | 0 | ✅ done |
| vocal_aah_formant_up4st.wav | 0 | 1× | **+4 st** | ✅ done |
| vocal_aah_formant_upmax.wav | 0 | 1× | **+12 st** | ✅ done |
| vocal_aah_formant_downmax.wav | 0 | 1× | **−12 st** | ✅ done |

### Single drum hit (4 files — done)

| File | Pitch | Time | Formant | Status |
|---|---|---|---|---|
| drum_hit_passthrough.wav | 0 | 1× | 0 | ✅ done |
| drum_hit_time_2x.wav | 0 | **2×** | 0 | ✅ done |
| drum_hit_time_4x.wav | 0 | **4×** | 0 | ✅ done |
| drum_hit_time_halfspeed.wav | 0 | **0.5×** | 0 | ✅ done |
| drum_hit_pitch_up7st.wav | **+7 st** | 1× | 0 | ✅ done |

### Chord C major (3 files — done)

| File | Pitch | Time | Formant | Status |
|---|---|---|---|---|
| chord_Cmaj_passthrough.wav | 0 | 1× | 0 | ✅ done |
| chord_Cmaj_time_2x.wav | 0 | **2×** | 0 | ✅ done |
| chord_Cmaj_pitch_up7st.wav | **+7 st** | 1× | 0 | ✅ done |
| chord_Cmaj_formant_max.wav | 0 | 1× | **+12 st** | ✅ done |

---

## Sine Re-record (added 2026-10-18) — do this first

**Why:** in 5 of the 6 May sine takes (all except formant +63), the decoded
firmware SOLO player only matches if the formant sits an octave low (read
speed ½). Pitch −12 came out as a *pure* 220 Hz tone, which the SOLO player
produces only with formant at its minimum; at formant 0 it keeps the 440 Hz
content. The takes were made changing one parameter at a time, so something
else was probably lowering the formant: **Formant KF** (key follow,
relative to C4), the formant envelope or the formant LFO (PCM Formant
screen, manual p. 92). The vocal takes show no such offset.
(notes.txt, "SINE REFERENCES EXPLAINED")

**Setup:** wave 003 (SOLO), PCM oscillator with Vari ON. On the Formant (PCM)
screen set **Formant 0, Formant KF 0, Fmt LFO Depth 0**, formant envelope depth
0, Energy OFF; Robot voice off. Play the key that sounds 440 Hz and **note
which key**. Same capture chain as the vocal takes (48 kHz stereo); ~0.5 s
lead-in; record each take to its natural end (time ½ runs about 8 s; the May take
stopped at 4 s). Save with an `_r2` suffix so the May takes stay for
comparison.

| Take | Pitch | Time | Formant | Note |
|---|---|---|---|---|
| `sine_440_identity_r2` | 0 | 1× | 0 | **Decisive.** Should be a pure 440 Hz sine; overtones (2nd harmonic ≈ −18 dB) mean the formant is still offset |
| `sine_440_pitch_down12st_r2` | −12 | 1× | 0 | |
| `sine_440_pitch_up7st_r2` | +7 | 1× | 0 | |
| `sine_440_time_2x_r2` | 0 | 2× faster | 0 | |
| `sine_440_time_halfspeed_r2` | 0 | ½ speed | 0 | full length |
| `sine_440_formant_downmax_r2` | 0 | 1× | −63 | optional: same chain as the rest |
| `sine_440_formant_upmax_r2` | 0 | 1× | +63 | optional |

If the identity take is clean but the May takes are reproduced when you put
back a setting (e.g. Formant KF), record which setting it was: that tells us
how the firmware combines KF with the knob.

## Energy check and calibration (added 2026-10-28) — no new takes needed for step 1

**Why:** the output-chain study (v40) found that every vocal reference has
its fundamental 1.6–1.9 dB low and the content below it cut. That pattern
follows F0 under pitch shift, which is what the SOLO player's **Energy**
high-pass does when Energy is on. The engine reproduces it with a small
internal amount (g ≈ 0.05), but the mapping from the XT's Energy knob
(OFF, 1–127) to g is not decoded yet. (research/HARDWARE_FIRMWARE.md,
"Output chain")

**Step 1 — read the EMU-TEST patch (no recording).** On the XT, open the
EMU-TEST patch, then OSC → **Formant (PCM)** page (manual p. 92). Note these
values in the patch table above:

| Parameter | Value |
|---|---|
| Energy | |
| Formant KF | |
| Fmt LFO Depth | |
| Formant envelope depth (formant envelope screen) | |

**Step 2 — Energy calibration takes.** Use wave 004 (the SOLO vocal), the
same capture chain as the other vocal takes (48 kHz stereo), and ~0.5 s of
lead-in. Pitch **−12** for every take, because the Energy amount only
changes the sound when the pitch is shifted. Keep every other setting as in
the original vocal takes and change only Energy:

| Take | Pitch | Time | Formant | Energy |
|---|---|---|---|---|
| `vocal_aah_pitch_down12st_energy_off` | −12 | 1× | 0 | OFF |
| `vocal_aah_pitch_down12st_energy_032` | −12 | 1× | 0 | 32 |
| `vocal_aah_pitch_down12st_energy_064` | −12 | 1× | 0 | 64 |
| `vocal_aah_pitch_down12st_energy_127` | −12 | 1× | 0 | 127 |

Optional: repeat at pitch **+7** with the same four Energy values. The +7
damping depends on g differently (1.2 − 0.5g against 1.2 − g at −12), so
those takes cross-check the mapping.

Save them in `sustained/`. They need no renderer mapping yet; they're used to
fit the knob → g curve.

## Repeat-budget check (added 2026-10-28) — one take

**Why:** each SOLO record carries a repeat budget (v38: the XT's encoder
writes it; our encoder now reproduces it bit-exactly, v39). A budget only
changes the sound when a record has to repeat three or more times, i.e. at
slowdowns of about 4× or more. Every take so far is at ½ speed or faster, so
the budget behaviour has only been checked against the firmware emulator,
not the hardware. (research/HARDWARE_FIRMWARE.md, "How the encoder sets the
repeat budget")

**Setup:** wave 004 (SOLO vocal), with the same patch settings and capture
chain as the other vocal takes (including the Energy setting, whatever it
is). Pitch 0, Formant 0, Time at **¼ speed (0.25×, 4× slower)**. Record to
the natural end: about 14 s of audio for the 3.5 s source.

| Take | Pitch | Time | Formant | Note |
|---|---|---|---|---|
| `vocal_aah_time_quarterspeed` | 0 | **0.25×** | 0 | full length |

`render_suite.py` already maps it (`--time 4.0`), so it scores automatically
once it's in `sustained/`.

## XT encodes of the chord and drum (added 2026-10-28) — no audio capture needed

**Why:** the BACKING/ENSEMBLE work needs the XT's own analysis of our exact
inputs, as waves 003/004 gave for SOLO (where the encoder port now
reproduces the XT bit-exactly). Every BACKING/ENSEMBLE wave in Emu_test.prj
(362, 376, 377, 380, 381, 415) is factory content stamped "V-Synth Ver.1.50"
— encoded on an original V-Synth in 2003, not on the XT — and that version's
encoder is measurably different (see "Factory waves" in
research/HARDWARE_FIRMWARE.md). So these four encodes are the only possible
ground truth for the XT's multi-band encoder. With them we can:
- run the XT's BACKING and ENSEMBLE players on exactly what the hardware
  stored, and score them against the existing chord and drum references;
- validate the emulated multi-band encoder.

(research/HARDWARE_FIRMWARE.md, "BACKING / ENSEMBLE: first findings")

**Question first:** which **Encode Type** did you use for the existing chord
and drum takes (BACKING or ENSEMBLE)? The formant +12 chord take shows no
formant change, so it was one of the two.

**Steps:** import `raw/chord_Cmaj_original.wav` and
`raw/drum_hit_original.wav` into the XT (note whether the XT converts them
from 48 kHz). Encode each **twice**, once as BACKING and once as ENSEMBLE,
keeping Encode Depth at whatever you normally use (note the value). Then save
the project, as with Emu_test.prj, and copy it into `analysis/projects/`.

| Wave | Source | Encode Type |
|---|---|---|
| `chord_backing` | raw/chord_Cmaj_original.wav | BACKING |
| `chord_ensemble` | raw/chord_Cmaj_original.wav | ENSEMBLE |
| `drum_backing` | raw/drum_hit_original.wav | BACKING |
| `drum_ensemble` | raw/drum_hit_original.wav | ENSEMBLE |

## Next Recording Session — Priorities

**Superseded by the "Round 2 — Recording Wishlist" section at the end of this
file** (updated after v27).  Optional extras kept from the earlier list, worth
grabbing only if time permits after the Round-2 priorities:

| File | Pitch | Time | Formant | Why |
|---|---|---|---|---|
| `vocal_aah_time_1_5x_dry`/ref | 0 | **1.5×** | 0 | mid-ratio quality curve |
| `chord_Cmaj_time_halfspeed` | 0 | **0.5×** | 0 | subband engine at compression |
| `chord_Cmaj_pitch_down12st` | **−12 st** | 1× | 0 | ENSEMBLE pitch, down direction |

---

## Rendering Plugin Outputs

After recording, rebuild the renderer (from the project root):

```bash
cd plugin/Source && g++ -std=c++20 -O2 -DOFFLINE_RENDERER_MAIN \
    OfflineRenderer.cpp VariphraseEngine.cpp \
    PhaseVocoder.cpp SourceFilterModel.cpp \
    -o ../../analysis/variphrase_render
```

Then render each new test case.  **Always use `--algo hybrid`** — the hybrid algorithm
runs the offline encode pass (`analyzeContent`) automatically and picks the best
sub-algorithm for the content type:

```bash
BASE=/path/to/vsynth-emu/analysis
R="$BASE/variphrase_render"
VER=hybrid_v17c      # increment for each new batch

mkdir -p "$BASE/plugin_outputs/$VER/sustained"

# Time stretch — WSOLA will be selected automatically for BACKING/ENSEMBLE content
"$R" --input  "$BASE/test_files/passthrough/chord_Cmaj_passthrough.wav" \
     --output "$BASE/plugin_outputs/$VER/sustained/chord_Cmaj_time_2x.wav" \
     --time 2.0 --algo hybrid

# Pitch shift — PV or LPC selected based on content type + voiced detection
"$R" --input  "$BASE/test_files/passthrough/vocal_aah_passthrough.wav" \
     --output "$BASE/plugin_outputs/$VER/sustained/vocal_aah_pitch_up7st.wav" \
     --pitch 7 --algo hybrid

# Formant shift — LPC always selected when |formant| > 0.5 st
"$R" --input  "$BASE/test_files/passthrough/vocal_aah_passthrough.wav" \
     --output "$BASE/plugin_outputs/$VER/sustained/vocal_aah_formant_upmax.wav" \
     --formant 12 --algo hybrid
```

**Current hybrid routing summary (v27 — see research/PATENTS.md):**
| Content type | Operation | Engine |
|---|---|---|
| SOLO (speech/melody) | any | pitch-synchronous granular (US6421642) |
| LITE (pure tones) | time, pitch-up | LPC resynthesis |
| LITE | pitch-down | Phase Vocoder |
| LITE | formant | LPC source-filter |
| ENSEMBLE (chords) | time-only | subband stretch (US6564187) |
| ENSEMBLE | pitch/formant | Phase Vocoder / LPC |
| BACKING (drums) | time-only | event-based stretch (attacks verbatim) |
| BACKING | pitch | Phase Vocoder |

The content type (LITE / SOLO / ENSEMBLE / BACKING) is printed to stdout during render:
```
Content: BACKING  medianConf=0.895  peakToMean=7.80
```

Then run the full batch test:

```bash
cd "$BASE" && python3 batch_test.py \
    --ref-dir    test_files/sustained \
    --plugin-dir plugin_outputs/$VER/sustained \
    --output     results/$VER
```

---

## Tips for Clean Recordings

1. **Zero all VariPhrase parameters before each take.** Confirm with a passthrough null
   test before setting the test parameter.
2. **Record a few seconds of silence before playing the signal.** This lets the batch
   test measure the noise floor and gives the analyzeContent pass enough samples to
   classify content correctly.
3. **Use the same audio interface and cable for all takes in a session** so the output
   chain is consistent.
4. **Label takes immediately** — do not rely on memory.  Record the V-Synth panel display
   settings in a photo alongside the audio file.
5. **Check the recording with compare.py before moving on** — a quick null test against
   the passthrough reveals miswired settings immediately.
6. **Verify content classification** after recording by running a quick render and checking
   the `Content:` line in the output.  If classification looks wrong, check signal length
   (< 2048 samples returns LITE by default) and silence threshold.

---

## Round 2 — Recording Wishlist (updated after v27, score 51.5)

Session 15 ended with all three Roland VariPhrase patents implemented in
their correct roles (see research/PATENTS.md): pitch-synchronous granular for
SOLO (US6421642), quarter-octave subband stretch for ENSEMBLE (US6564187),
and event stamps for BACKING.  The remaining ceiling is **coverage- and
metric-bound**: the suite is 20 cases from 4 source recordings, several
behaviours are calibrated against a single example, and the open questions
below can only be answered with new hardware captures.

**If hardware time is limited, record ① + ⑥ only** (nine takes of one vocal
source) — they answer the two most important open questions: the true
formant transfer curve and the metric ceiling.

### Ground rules for every take

- **Keep the RAW source of every sample in `test_files/raw/`** — the file
  you sampled into the V-Synth.  The engine must render from the raw source,
  never from a V-Synth capture: the V-Synth is not transparent at zero
  settings (Session 16: using the vocal passthrough capture as input cost
  ~9 points).  Chord and drum originals: `raw/*_original.wav` (identical to
  their old passthrough files).  Still needed: a real V-Synth LITE capture
  of the sine at zero settings (`passthrough/sine_400hz_raw_lite.wav` is a
  copy of the raw sine).
- **Capture the passthrough first** for every new source (all VariPhrase
  parameters zeroed).  Reference levels are recording-chain gain staging
  (the sine refs sit ~7 dB below their dry input; vocal/chord at unity);
  a passthrough per source pins down the unit's level law.
- **Name dry inputs with a `_dry` suffix.**  The sine set's identical names
  across passthrough/ and sustained/ caused a wrong-input render and an
  inflated score in Session 15.
- **Short, consistent lead-ins (~0.5 s)** and **≥ 4 s of steady content** for
  sustained sources (analysis windows reach 65 k samples).
- **Note the exact displayed parameter values** — the panel-to-DSP mapping is
  part of what is being reverse-engineered.

### Priority ① — Formant-knob calibration sweep (7 takes)

Vocal "aah" at formant settings:
```
−9, −6, −3, +2, +3, +6, +9   (semitone display values, everything else zero)
```
Now does double duty:
- Maps the full formant transfer curve (currently measured from only two
  points: "+12 st" moves F1 ~×1.67; downward tracks nominal).
- **Directly tests the patent's window clamp** (`wl ≤ ppw`, US6421642), which
  replaced the empirical 0.75× calibration in the granular engine — the
  engine now makes a specific, falsifiable prediction at every knob setting.
- Resolves whether the down-shift's odd upper structure (reference peaks at
  3170/4172 Hz that don't fit ×0.5 scaling) is real behaviour or artifact.

### Priority ② — A second vowel and a higher voice (6 takes)

All vocal behaviour is tuned on ONE low-pitched "aah".
- **"ee" vowel, sustained** (high F2 ≈ 2.3 kHz, low F1 — the hardest envelope
  configuration): formant +12, −12, pitch +7.
- **Higher-F0 voice** (female or falsetto, F0 ≈ 200–250 Hz), sustained "aah":
  same three takes.  Stresses the granular engine's period tracker and
  integer-subharmonic octave guard.

### Priority ③ — Drum loop with multiple onsets (4 takes)

A 2–4 bar kick/snare/hat loop at a known tempo: time 2×, time 0.5×,
pitch +7 st, passthrough.  The event-stamp system has only ever been tested
against a SINGLE drum hit; a loop exercises multi-onset detection, the
2048-sample refractory window, inter-event segment stretching, and cumulative
timing placement.

### Priority ④ — Sustained pad chord (4 takes)

A slow-attack polyphonic sustain (organ/strings patch): time 2×, pitch +7,
formant +12, passthrough.  Two jobs:
- Tests the ENSEMBLE/BACKING classifier boundary (peakToMean > 9, set from
  one strummed chord at 7.8 vs one drum at 11.4) without the borderline
  attack transient.
- **Direct validation for the new subband engine** (US6564187, v27) on clean
  polyphony; the pitch take informs the planned subband-pitch extension
  (chord_pitch is a queued target).

### Priority ⑤ — Moving-pitch phrase (4 takes)

A 2–4 s sung or spoken phrase with natural pitch movement: time 2×,
time 0.5×, pitch +7, passthrough.  This is what VariPhrase exists for, and
the real test of the granular encoder's grain tracking — everything in the
current suite is stationary.

### Priority ⑥ — Duplicate takes (cheap; do alongside anything)

Record the SAME setting twice without touching anything (e.g. vocal formant
+12, two takes).  Scoring take A against take B through compare.py measures
the recording-chain + metric noise floor — the best score ANY render can
achieve.  With the suite at 51.5, knowing whether the ceiling is 65 or 85
determines how much algorithm work actually remains.
