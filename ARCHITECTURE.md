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
├── analysis/              # Python analysis pipeline (independent of plugin)
│   ├── compare.py         # null test + spectrogram + LPC comparison
│   ├── lpc.py             # formant extraction utilities
│   ├── batch_test.py      # automated regression runner across all test files
│   ├── variphrase_render  # compiled C++ offline renderer (no JUCE required)
│   ├── test_files/        # V-Synth reference WAV recordings
│   │   ├── passthrough/   # unprocessed input signals (sine, sawtooth)
│   │   ├── sustained/     # V-Synth processed outputs (PCM16, 44.1 kHz)
│   │   ├── transients/    # drum hits, plucks (not yet populated)
│   │   ├── polyphonic/    # chords, multi-pitch (not yet populated)
│   │   └── edge_cases/    # whisper, noise, extreme settings (not yet populated)
│   ├── plugin_outputs/    # engine render outputs for batch comparison
│   └── results/           # batch_test.py JSON + HTML reports
├── plugin/                # JUCE project
│   ├── CMakeLists.txt
│   └── Source/
│       ├── PluginProcessor.h/cpp      # JUCE boilerplate
│       ├── PluginEditor.h/cpp         # UI
│       ├── VariphraseEngine.h/cpp     # core algorithm router — isolated + testable
│       ├── PhaseVocoder.h/cpp         # phase vocoder (pitch + time)
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
a float32 WAV file, processes it through the engine, and writes the output — no JUCE,
no libsndfile, no DAW required.  Rebuild command:
```
clang++ -std=c++17 -O2 -DOFFLINE_RENDERER_MAIN \
    plugin/Source/OfflineRenderer.cpp \
    plugin/Source/VariphraseEngine.cpp \
    plugin/Source/PhaseVocoder.cpp \
    plugin/Source/SourceFilterModel.cpp \
    -o analysis/variphrase_render
```

## Variphrase Parameters (matching V-Synth controls)
- **Pitch Shift** — semitones, range -24 to +24
- **Time Stretch** — ratio, range 0.25x to 4.0x  
- **Formant Shift** — semitones, range -12 to +12
- **Robot** — forces monophonic voiced analysis (V-Synth-style "Robot" mode)

## Algorithm Candidates — Status

| # | Algorithm | Status | Avg Score | Notes |
|---|-----------|--------|-----------|-------|
| 1 | Phase Vocoder (`--algo pv`) | ✅ Implemented | 26.7 | Good for pitch/time; fails on formant shift (spectral warp ≠ source-filter) |
| 2 | Sinusoidal + residual (SMS) | ⬜ Not started | — | Skipped; LPC approach more faithful to V-Synth architecture |
| 3 | LPC Source-Filter (`--algo lpc`) | ✅ Implemented | 24.0 | Correct architecture; biquad synthesis stable; weaker than PV for pitch/time |
| 4 | Hybrid (`--algo hybrid`) | ✅ Calibrated | **28.7** | hasFormant → LPC, else → PV; oracle routing verified Session 5 |

### Current Best Routing (per case) — Session 5 calibrated
| Case | Algorithm | Score | vs. alternative |
|---|---|---|---|
| formant_downmax | lpc | 25.9 | PV: 16.0 |
| formant_upmax | lpc | 34.6 | PV: 32.3 |
| pitch_down12st | pv | 26.7 | LPC: 17.3 |
| pitch_up7st | pv | 36.3 | LPC: 34.3 |
| time_2x | pv | 20.1 | LPC: 15.6 |
| time_halfspeed | pv | 28.6 | LPC: 15.7 |

**Hybrid routing rule:** `hasFormant (|formantShift| > 0.5 st) → LPC, else → PV`

## Measurement & Scoring
Every algorithm version is scored by `batch_test.py`:
- **Null test residual (dBFS)** — lower is better; target < -30 dB
- **SNR (dB)** — signal-to-noise of residual vs reference
- **Formant accuracy** — LPC formant trajectory comparison (0–1)
- **Transient smearing** — energy spread around detected transients (0–1)
- **Composite score** — weighted sum; target > 60/100

### Score Progression
| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Baseline | Passthrough | 13.2 | |
| Session 2 | Phase Vocoder v1 | 21.6 | |
| Session 3 | Mixed (PV+LPC per case) | 26.5 | PV for pitch/time, LPC for formant — stale outputs |
| Session 4 | LPC Biquad (NaN fix) | *(corrected: 24.0)* | Energy normalization incorrectly global |
| Session 5 | **Hybrid v3 (oracle routing)** | **28.7** | Calibrated rule: formant→LPC, else→PV |
| Session 6 | **Hybrid v5 (adaptive LPC)** | **28.3** | Guard 3 min-order-2; formant_downmax +2, upmax −4.5 on pure sine |
| **Target** | — | **> 60** | |

## Confirmed V-Synth Architecture (as of Session 3)

VariPhrase operates as a **source-filter vocoder**, not a spectral manipulator:

1. **F0 detection** — ACF-based, per analysis frame (1024 samples, 256-sample hop)
2. **Voiced/unvoiced** — ZCR + energy threshold; voiced → sawtooth excitation, unvoiced → noise
3. **LPC analysis** — Levinson-Durbin, order 16 (≈ 8 formant pairs), bandwidth-expanded (λ=0.994)
4. **Formant shift** — Laguerre root-finding on LPC polynomial → scale each pole's angle by 2^(st/12) → cascade biquad reconstruction (float32-safe)
5. **Excitation synthesis** — band-limited sawtooth at (pitch-shifted) F0; harmonics 1/k up to Nyquist; phase continuous across frames
6. **LPC synthesis filter** — cascade of 8 all-pole biquad sections (when formant shift active) or direct-form 16-tap IIR; per-frame energy normalization
7. **OLA output** — Hann-window overlap-add; synthesis hop = kHopSize × timeStretch for time stretch

**Key confirmed facts from V-Synth recordings:**
- F0 is preserved exactly during formant shift (F0 in = F0 out)
- Excitation is a 1/k harmonic sawtooth at the input F0, not the original waveform
- Pitch shift moves the excitation F0; formant filter is unchanged (independent axes confirmed)

## Known Gaps / Next Work

### Immediate — Recording session incoming
New V-Synth recordings planned for three signal types.  See `RECORDING_GUIDE.md`
for the full file list and V-Synth patch settings.

| Signal | Files | Expected benefit |
|---|---|---|
| Held vowel "aah" (F0 ≈ 220 Hz) | 7 processed + 1 passthrough | LPC formant quality on real speech (currently untestable) |
| Single drum hit | 4 processed + 1 passthrough | First real transient_score data; exercises onset detection |
| C major chord | 3 processed + 1 passthrough | Polyphonic stress test for both PV and LPC paths |

These files plug directly into the existing `batch_test.py` pipeline — no code changes needed.
Output directory for new renders: `plugin_outputs/hybrid_v6/`.

### Software gaps (next after batch_test reveals new failure modes)

1. **Transient onset detection — untested on real material** — infrastructure is in
   place (12 dB threshold, 85% blend, filter state reset), but has never fired on the
   current test suite (sustained sines). Drum-hit recordings will expose actual behavior.

2. **Adaptive LPC — pure-sine regression on upmax** — Guard 3 (min order 2) improved
   formant_downmax (+2.0) but regressed formant_upmax (−4.5) because a 2-pole model shifts
   only one resonance to 880 Hz while the 16-pole model accidentally produced richer spectral
   content. On real speech (order-2 residual 10–60%) the adaptive stop will not fire early.
   Possible fix: parameter-aware LPC order (don't stop before `formantShift / 12` additional
   iterations for large positive shifts). Defer until batch test on real speech confirms the
   magnitude of the issue.

3. **Phase vocoder time-stretch quality** — time_2x (20.1) and time_halfspeed (28.6) are the
   weakest PV cases. Phase locking (lock the phases of spectral peaks to their harmonics) or
   transient-aware PV (suppress phase unwrapping at transients) could improve these.

4. **Score gap: 28.3 → 60 target** — ~31.7 pts needed. Expected breakdown once new recordings
   are added: transient_score dimension newly measurable (+10–20 pts weight uplift), formant
   similarity on real speech higher than 0.641–0.744 (real vowel may reach 0.90+), time-stretch
   on real material likely better than on pure sine (OLA artifacts less audible on noise-like
   content).

## Known Constraints
- Cannot license Roland's PCM ROM content — custom samples required for oscillators
- D-Beam controller mapped to MIDI CC in plugin context
- Ribbon controllers mapped to MPE or standard CC
