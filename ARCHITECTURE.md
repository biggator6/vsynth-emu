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
| 1 | Phase Vocoder (`--algo pv`) | ✅ Implemented | 21.6 | Good for pitch/time; fails on formant shift (spectral warp ≠ source-filter) |
| 2 | Sinusoidal + residual (SMS) | ⬜ Not started | — | Skipped; LPC approach more faithful to V-Synth architecture |
| 3 | LPC Source-Filter (`--algo lpc`) | ✅ Implemented | 28.7 | Correct architecture; biquad synthesis stable; formant shift working |
| 4 | Hybrid (`--algo hybrid`) | 🔄 Partial | ~26.5* | Routes pitch/time → PV, formant → LPC; scores need re-verify post-biquad-fix |

*Hybrid score from Session 3, before biquad fix; retest pending.

### Current Best Routing (per case)
| Case | Best algorithm | Score |
|---|---|---|
| formant_downmax | lpc | 25.9 |
| formant_upmax | lpc (biquad) | 34.6 |
| pitch_down12st | pv | 37.5 |
| pitch_up7st | lpc | 36.3 |
| time_2x | lpc | 20.1 |
| time_halfspeed | lpc | 28.6 |

## Measurement & Scoring
Every algorithm version is scored by `batch_test.py`:
- **Null test residual (dBFS)** — lower is better; target < -30 dB
- **SNR (dB)** — signal-to-noise of residual vs reference
- **Formant accuracy** — LPC formant trajectory comparison (0–1)
- **Transient smearing** — energy spread around detected transients (0–1)
- **Composite score** — weighted sum; target > 60/100

### Score Progression
| Session | Algorithm | Avg Score |
|---|---|---|
| Baseline | Passthrough | 13.2 |
| Session 2 | Phase Vocoder v1 | 21.6 |
| Session 3 | LPC Source-Filter v1 | 26.5 |
| Session 4 | LPC Biquad v1 (NaN fix) | **28.7** |
| **Target** | — | **> 60** |

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
1. **Transient handling** — transient frames score 0.000; needs onset detection + bypass
2. **Hybrid routing optimization** — per-case routing (formant→LPC, pitch/time→PV) has not been verified post-biquad-fix
3. **Harmonically-rich test material** — all current tests use pure 440 Hz sine, a worst-case for LPC pole clustering; speech/sawtooth/guitar material would be less degenerate
4. **formant_downmax score** — energy normalization is a blunt fix for pole clustering; a minimum-angle-gap guard in shiftFormants may recover the 2.4 pt regression

## Known Constraints
- Cannot license Roland's PCM ROM content — custom samples required for oscillators
- D-Beam controller mapped to MIDI CC in plugin context
- Ribbon controllers mapped to MPE or standard CC
