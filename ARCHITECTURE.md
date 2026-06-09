# V-Synth VariPhrase Emulator — Architecture

## Project Goal
Reverse-engineer Roland's VariPhrase DSP algorithm by black-box testing against a real V-Synth,
then implement a faithful emulation as a JUCE VST3/AU plugin.

## What VariPhrase Does
VariPhrase decouples three axes that are normally entangled in audio processing:

| Parameter   | Normal Pitch Shift      | VariPhrase         |
|-------------|-------------------------|--------------------|
| Pitch       | Locked to speed         | Fully independent  |
| Time/Duration | Locked to pitch       | Fully independent  |
| Formants    | Shift with pitch        | Fully independent  |

## Methodology: Black-Box Reverse Engineering
1. Feed controlled inputs to the real V-Synth, capture outputs (WAV)
2. Run the same inputs through our plugin
3. Null test + spectrogram compare to measure the difference
4. Refine the algorithm based on what the residual reveals
5. Repeat until perceptually indistinguishable on most material

## Repository Structure
```
vsynth-emu/
├── ARCHITECTURE.md        # this file — living design document
├── RESEARCH_LOG.md        # session-by-session findings and hypotheses
├── RECORDING_GUIDE.md     # V-Synth recording setup, file naming, completed checklist
├── analysis/              # Python analysis pipeline (independent of plugin)
│   ├── compare.py         # null test + spectrogram + LPC comparison
│   ├── lpc.py             # formant extraction utilities
│   ├── batch_test.py      # automated regression runner across all test files
│   ├── variphrase_render  # compiled C++ offline renderer (no JUCE required)
│   ├── test_files/        # V-Synth reference WAV recordings
│   │   ├── passthrough/   # unprocessed input signals (raw recordings)
│   │   └── sustained/     # V-Synth processed outputs (PCM, 48 kHz stereo)
│   ├── plugin_outputs/    # engine render outputs for batch comparison
│   └── results/           # batch_test.py JSON + HTML reports
├── plugin/                # JUCE project
│   ├── CMakeLists.txt
│   └── Source/
│       ├── PluginProcessor.h/cpp      # JUCE boilerplate
│       ├── PluginEditor.h/cpp         # UI
│       ├── VariphraseEngine.h/cpp     # core algorithm router — isolated + testable
│       ├── PhaseVocoder.h/cpp         # phase vocoder + WSOLA time-stretch
│       ├── SourceFilterModel.h/cpp    # LPC source-filter (formant shift)
│       └── OfflineRenderer.h/cpp      # WAV file renderer for batch testing
└── research/              # papers, notes, spectrogram images, session exports
```

## Key Architectural Decision: Isolated Engine
`VariphraseEngine` is a completely standalone C++ class with no JUCE dependencies.
It takes a buffer of floats in, returns a buffer of floats out.
This allows:
- Python to call it via subprocess (`analysis/variphrase_render`) for automated testing
- Unit testing without a VST host
- Swapping algorithm implementations without touching plugin scaffolding

### Offline Renderer
`analysis/variphrase_render` is compiled directly from the C++ source files with
`-DOFFLINE_RENDERER_MAIN`, activating a `main()` in `OfflineRenderer.cpp`.  It reads
a WAV file (16-bit, 24-bit PCM or 32-bit float; mono or stereo), processes it through
the engine, and writes the output — no JUCE, no libsndfile, no DAW required.

**Rebuild command:**
```bash
cd plugin/Source && g++ -std=c++20 -O2 -DOFFLINE_RENDERER_MAIN \
    OfflineRenderer.cpp VariphraseEngine.cpp \
    PhaseVocoder.cpp SourceFilterModel.cpp \
    -o ../../analysis/variphrase_render
```

**Usage:**
```bash
./variphrase_render \
    --input  test_files/passthrough/vocal_aah_passthrough.wav \
    --output plugin_outputs/hybrid_v17c/sustained/vocal_aah_time_2x.wav \
    --time 2.0 --algo hybrid
```

Full options: `./variphrase_render --help`

## Variphrase Parameters (matching V-Synth controls)
- **Pitch Shift** — semitones, range -24 to +24
- **Time Stretch** — ratio, range 0.25× to 4.0×
- **Formant Shift** — semitones, range -12 to +12
- **Robot** — forces monophonic voiced analysis (V-Synth-style "Robot" mode)

---

## Algorithm Candidates — Current Status

| # | Algorithm | `--algo` flag | Status | Avg Score | Notes |
|---|-----------|--------------|--------|-----------|-------|
| 1 | Phase Vocoder | `pv` | ✅ Implemented | 26.7 | Good for pitch/time; formant shift = spectral warp (incorrect architecture) |
| 2 | Sinusoidal + Residual (SMS) | `sms` | ⬜ Not started | — | Skipped; LPC approach more faithful to V-Synth architecture |
| 3 | LPC Source-Filter | `lpc` | ✅ Implemented | 24.0 | Correct architecture for voiced speech; weaker on pitch/time; LPC order stability issues |
| 4 | Hybrid | `hybrid` | ✅ Active | **27.2** | Content-adaptive routing via offline encode pass (Session 12) |

---

## Hybrid Algorithm — Routing Logic (current, Session 12)

### Step 1 — Offline encode pass (runs once before block processing)

`VariphraseEngine::analyzeContent()` classifies the full input buffer into one of four
V-Synth encode types using global ACF statistics:

```
medianUnbiasedACF > 0.95 AND 1–4 kHz band energy > 5% of total  →  SOLO
medianUnbiasedACF > 0.95 AND low 1–4 kHz band energy             →  LITE
medianUnbiasedACF ≤ 0.95 AND peakToMeanEnergy > 5.0              →  BACKING
medianUnbiasedACF ≤ 0.95 AND peakToMeanEnergy ≤ 5.0              →  ENSEMBLE
```

ACF parameters: frame = 2048 samples, step = 512, lag range = sr/500 to sr/60 (88–735 samples @ 44.1 kHz).
Unbiased normalisation: biased[lag] × N/(N−lag) / ACF[0].

Observed values from test material:

| Signal | medianConf | peakToMean | ContentType |
|--------|-----------|------------|-------------|
| vocal_aah | 0.985 | 2.0 | **SOLO** |
| chord_Cmaj | 0.895 | 7.8 | **BACKING** |
| drum_hit | 0.489 | 11.4 | **BACKING** |
| sine_440 | 1.000 | 1.0 | **LITE** |

The result is stored in `VariphraseEngine::Impl::analysis_` via `setAnalysis()`.

### Step 2 — Per-block routing (real-time)

```
hasFormant (|formantShift| > 0.5 st)            →  LPC source-filter
hasPitch AND isVoicedSpeech                      →  LPC source-filter
  (voiced = ZCR < 0.15 AND 1–4 kHz band > 5%)
isEnsembleOrBacking AND !hasPitch AND time ≥ 1×  →  WSOLA (time-domain OLA)
else                                             →  Phase Vocoder
```

WSOLA is only applied for time EXTENSION (timeStretch ≥ 1.0). Compression falls back
to PV because the WSOLA synthesis hop would be smaller than the analysis hop, causing
the output buffer to drain faster than it fills.

### WSOLA Implementation Notes

`PhaseVocoder::processWSOLA(timeStretch)` — called when `forceWSOLA_` is set:
- Guard: `inputFill_ >= kFFTSize + kWsolaSearchLen` (2048 + 256 = 2304 samples)
- Similarity search: normalised cross-correlation over ±256 samples
- Overlap region: kFFTSize − kHopSize = 1536 samples compared
- Synthesis hop: kHopSize × timeStretch; OLA normalisation: totalStretch / 2.0

---

## Confirmed V-Synth Architecture (from analysis)

VariPhrase operates as a **source-filter vocoder** for voiced content, not a spectral manipulator:

1. **F0 detection** — ACF-based, per analysis frame
2. **Voiced/unvoiced decision** — ZCR + energy threshold
3. **LPC analysis** — Levinson-Durbin, order 16, bandwidth-expanded (λ=0.994)
4. **Formant shift** — Laguerre root-finding → scale pole angles by 2^(st/12) → cascade biquad
5. **Excitation synthesis** — band-limited sawtooth at pitch-shifted F0; harmonics 1/k
6. **LPC synthesis filter** — cascade of 8 all-pole biquad sections
7. **OLA output** — Hann-window overlap-add; synthesis hop = kHopSize × timeStretch

**For polyphonic / ENSEMBLE content** the V-Synth uses WSOLA (waveform similarity OLA)
rather than the source-filter model — confirmed by improved chord_Cmaj_time_2x scores
when WSOLA is applied (+3.8 pts vs PV).

**Key confirmed facts:**
- F0 is preserved exactly during formant shift
- Pitch shift moves only the excitation F0; formant filter poles are unchanged
- Polyphonic content (chords) uses WSOLA encode type, not LPC

---

## Measurement & Scoring

Every algorithm version is scored by `batch_test.py`:
- **Null test residual (dBFS)** — lower (more negative) is better; target < −30 dBFS
- **SNR (dB)** — signal-to-noise of residual vs reference
- **Formant accuracy** — LPC formant trajectory comparison (0–1)
- **Transient smearing** — energy spread around detected transients (0–1)
- **Composite score** — weighted sum; target > 60/100

### Score Progression

| Session | Version | Avg Score | Notes |
|---------|---------|-----------|-------|
| Baseline | Passthrough | 13.2 | No processing |
| Session 2 | Phase Vocoder v1 | 21.6 | |
| Session 3 | Mixed PV+LPC | 26.5 | Per-case oracle routing |
| Session 4 | LPC Biquad (NaN fix) | 24.0 | Energy normalisation fix |
| Session 5 | Hybrid v3 | 28.7 | Calibrated rule: formant→LPC, else→PV |
| Session 6 | Hybrid v5 | 28.3 | Adaptive LPC; 6 sine-only cases |
| Session 7 | Hybrid v6 | 25.8 | 20-case battery; ceiling 45.7 (vocal time_2x) |
| Session 8 | Hybrid v8 | 26.8 | Voiced-pitch routing: vocal pitch cases +9 pts |
| Session 9 | Hybrid v10b | 26.9 | Voiced-formant LPC order fix (+2.8 chord/sine) |
| Sessions 10–11 | hybrid_v11_clean | 26.9 | WSOLA routing attempts failed (per-frame ACF) |
| **Session 12** | **hybrid_v17c** | **27.2** | Offline encode pass; chord_Cmaj_time_2x +3.8 |
| **Target** | — | **> 60** | |

---

## Current Per-Case Scores (hybrid_v17c, 20 test files)

| Test File | Score | Routing Used |
|-----------|-------|-------------|
| vocal_aah_time_2x | **45.7** | PV (SOLO, time-only) |
| vocal_aah_time_halfspeed | 32.5 | PV (SOLO, time-only) |
| vocal_aah_formant_upmax | 34.4 | LPC (formant shift) |
| vocal_aah_formant_up4st | 28.5 | LPC (formant shift) |
| vocal_aah_formant_downmax | 17.0 | LPC (formant shift) |
| vocal_aah_pitch_up7st | 19.0 | LPC (voiced pitch) |
| vocal_aah_pitch_down12st | 27.5 | LPC (voiced pitch) |
| sine_440_pitch_up7st | 36.3 | PV (LITE) |
| sine_440_pitch_down12st | 26.7 | PV (LITE) |
| sine_440_time_2x | 20.1 | PV (LITE) |
| sine_440_time_halfspeed | 28.6 | PV (LITE) |
| sine_440_formant_upmax | 32.8 | PV (LITE, formant via PV envelope) |
| sine_440_formant_downmax | 25.4 | PV (LITE) |
| chord_Cmaj_time_2x | **35.0** | **WSOLA (BACKING, time ext.)** |
| chord_Cmaj_pitch_up7st | 25.1 | PV (BACKING, hasPitch) |
| chord_Cmaj_formant_max | 17.9 | LPC (formant shift) |
| drum_hit_time_2x | 19.7 | WSOLA (BACKING, time ext.) |
| drum_hit_time_4x | 19.8 | WSOLA (BACKING, time ext.) |
| drum_hit_time_halfspeed | 36.1 | PV (BACKING, time < 1× → fallback) |
| drum_hit_pitch_up7st | 15.4 | PV (BACKING, hasPitch) |
| **AVERAGE** | **27.2** | |

---

## Known Gaps / Next Work

### Largest score levers remaining

1. **LPC order-collapse on voiced speech** — highest priority  
   The strong fundamental of voiced vowels dominates the LPC MSE, causing the Levinson-Durbin
   solver to converge to a 2-pole model (pitch resonance) rather than the vocal-tract formants
   (F1–F4). This limits ALL vocal formant and pitch cases.  
   - `vocal_aah_formant_downmax` = 17.0 (formant_sim 0.329) — formants not captured  
   - `vocal_aah_pitch_up7st` = 19.0 — LPC pitch shift with 2-pole model  
   **Planned fix:** pre-emphasis (`x[n] − 0.97×x[n−1]`) flattens the spectrum before LPC
   analysis, making the fundamental less dominant relative to F1/F2/F3 peaks.

2. **Chord time-compression and pitch shift** — medium priority  
   - `chord_Cmaj_pitch_up7st` = 25.1 — PV used (hasPitch prevents WSOLA), but PV on
     polyphonic content is architecturally wrong. WSOLA cannot shift pitch; a proper
     polyphonic pitch-shifter is needed for ENSEMBLE content.  
   - `chord_Cmaj_formant_max` = 17.9 — LPC formant shift on a chord is incoherent (LPC
     designed for monophonic voiced speech). No improvement possible without a separate
     polyphonic formant model.

3. **Drum time-compression (timeStretch < 1)** — medium priority  
   WSOLA does not handle compression: synthesis hop < analysis hop → output read drains
   buffer faster than write fills it → silence. Falls back to PV (36.1 on drum_hit_time_halfspeed
   is deceivingly high — output is near-silence that happens to null-test well against a
   near-silence reference).  
   **Planned fix:** discard-frame approach — for BACKING + compression, use WSOLA similarity
   search to select which input frames to keep, discarding the worst-matching ones.

4. **Score gap: 27.2 → 60 target** — ~33 pts needed  
   Estimated levers:
   - Pre-emphasis LPC fix: ~8–10 pts (vocal formant + pitch cases)
   - Polyphonic pitch shift for ENSEMBLE content: ~4 pts
   - WSOLA compression for BACKING content: ~2 pts
   - Drum transient timing: ~2 pts

---

## Known Constraints
- Cannot license Roland's PCM ROM content — custom samples required for oscillators
- D-Beam controller mapped to MIDI CC in plugin context
- Ribbon controllers mapped to MPE or standard CC
- Audio files from V-Synth: 48 kHz, 24-bit PCM, stereo — renderer downmixes to mono and
  handles both sample rates transparently
