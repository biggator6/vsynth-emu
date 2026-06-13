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

## Next Recording Session — Priorities

These would extend the test battery into currently under-tested territory.

### Priority 1 — Additional vocal time cases

The vocal_aah_time_halfspeed case (32.5) lags behind time_2x (45.7). An additional
mid-ratio case would help characterise the quality curve:

| File | Pitch | Time | Formant | Dir |
|---|---|---|---|---|
| `vocal_aah_time_1_5x.wav` | 0 | **1.5×** | 0 | sustained/ |

### Priority 2 — Chord compression and pitch

The chord_Cmaj_pitch_up7st (25.1) is the weakest non-formant chord case.  An additional
time-compression case would reveal how BACKING WSOLA handles short-stretch ratios:

| File | Pitch | Time | Formant | Dir |
|---|---|---|---|---|
| `chord_Cmaj_time_halfspeed.wav` | 0 | **0.5×** | 0 | sustained/ |
| `chord_Cmaj_pitch_down12st.wav` | **−12 st** | 1× | 0 | sustained/ |

### Priority 3 — Second vocal material (female / higher F0)

All current vocal tests use a male-range "aah" (~120 Hz F0). A higher-F0 source
would stress-test the LPC formant extractor at a different range:

| File | Pitch | Time | Formant | Dir |
|---|---|---|---|---|
| `vocal_female_passthrough.wav` | 0 | 1× | 0 | passthrough/ |
| `vocal_female_time_2x.wav` | 0 | **2×** | 0 | sustained/ |
| `vocal_female_formant_upmax.wav` | 0 | 1× | **+12 st** | sustained/ |

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

**Current hybrid routing summary:**
| Condition | Sub-algorithm |
|---|---|
| `|formantShift| > 0.5 st` | LPC source-filter |
| `hasPitch AND voiced speech` | LPC source-filter |
| `ENSEMBLE or BACKING, time ≥ 1×, !hasPitch` | WSOLA |
| everything else | Phase Vocoder |

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

## Round 2 — Recording Wishlist (after Session 15, score 36.5)

Session 15 implemented every V-Synth architectural element identified so far
(source-filter resynthesis with content routing, downsampled formant analysis,
formant-knob saturation, BACKING event stamps).  The remaining score ceiling is
increasingly **coverage- and metric-bound**: the suite is 20 cases from 4 source
recordings, several behaviours are calibrated against a single example, and a
few open questions can only be answered with new hardware captures.

Priority order below.  For every new source, **always capture the passthrough
first** (all VariPhrase parameters zeroed) — Session 15 found the existing
references carry recording-chain gain differences (the sine refs sit ~7 dB
below their dry input; vocal/chord are at unity), and a passthrough per source
pins down the unit's level law.

### Priority 1 — Formant-knob calibration sweep (one source, many settings)

The single highest-value set.  Session 15 measured the V-Synth's formant
mapping from just two points: "+12 st" moves F1 by only ~×1.67 (≈ +9 st, i.e.
the upward mapping saturates at ~0.75× nominal) while the downward direction
appears to track nominal.  The current code hard-codes 0.75× for upward speech
shifts off this one measurement.

Record the **vocal "aah"** at formant settings:
```
−9, −6, −3, +2, +3, +6, +9   (semitone display values, everything else zero)
```
Seven takes map the full transfer curve, replace the hard-coded 0.75 with a
measured function, and resolve whether the down-shift's odd upper structure
(reference peaks at 3170/4172 Hz that don't match ×0.5 scaling — Session 15
could not explain these) is real behaviour or measurement artifact.

### Priority 2 — A second vowel and a higher voice

All vocal behaviour (speech discriminant, downsampled LPC, F0 estimation,
pre-emphasis routing) is tuned on ONE low-pitched "aah".
- **"ee" vowel, sustained** (high F2 ≈ 2.3 kHz, low F1): the hardest formant
  configuration for LPC capture; one take each of formant +12, −12, pitch +7.
- **Higher-F0 voice** (female or falsetto, F0 ≈ 200–250 Hz), sustained "aah":
  stresses the F0 octave guard and the pre-emphasis corner; same three takes.

### Priority 3 — Drum loop (multiple onsets)

The event-stamp system (onset detection, verbatim attack placement, segmented
stretching) shipped in Session 15 tested against a SINGLE drum hit.  A 2–4 bar
kit loop (kick/snare/hat) at a known tempo exercises:
multi-onset detection, the 2048-sample refractory window, inter-event segment
stretching, and cumulative timing placement.
Takes: time 2×, time 0.5×, pitch +7 st, plus passthrough.

### Priority 4 — Sustained polyphony without an attack

The chord/drum classifier boundary (peakToMean > 9 → BACKING) was set from one
strummed chord (7.8) and one drum hit (11.4).  A **pad-style sustained chord**
(slow attack, e.g. organ or strings patch through the V-Synth) tests ENSEMBLE
classification and polyphonic processing without the attack transient that
made the strummed chord borderline.  Takes: time 2×, pitch +7, formant +12,
passthrough.

### Priority 5 — Moving pitch (the real VariPhrase use case)

Everything in the current suite is stationary.  A short **sung or spoken
phrase** (2–4 s, natural pitch movement) is what VariPhrase exists for, and
tests: per-frame F0 tracking under movement, dsLPC envelope smoothing
(currently α = 0.7, tuned on stationary content), and SOLO routing stability.
Takes: time 2×, time 0.5×, pitch +7, passthrough.

### Priority 6 — Metric noise floor (cheap, do alongside any session)

Record the SAME setting twice in a row (e.g. vocal formant +12, two takes
without touching anything).  Scoring take A against take B through compare.py
measures the recording chain + metric noise floor — i.e. the best score any
render could achieve.  Currently unknown; it bounds how much of the remaining
gap to 100 is even reachable.

### Capture conventions (lessons from Session 15)

- **Name dry inputs distinctly** — the sine set's per-case dry files share
  names with their references across directories
  (passthrough/sine_440_formant_downmax.wav = 16-bit DRY,
  sustained/sine_440_formant_downmax.wav = 24-bit REFERENCE), which caused a
  wrong-input render and an inflated score.  Use a `_dry` suffix for new
  material.
- **Keep lead-ins short and consistent** (~0.5 s).  The old drum references
  carried up to 2.6 s of lead-in silence, which combined with a then-unaligned
  metric to make all drum scores measurement noise for 14 sessions.  The
  metric now aligns, but short lead-ins maximise the comparable region.
- **Record ≥ 4 s of steady content** for sustained sources (the analysis uses
  windows up to 65 k samples), and note the displayed parameter value
  (semitones/ratio) exactly — the encode/UI mapping is part of what is being
  reverse-engineered.
