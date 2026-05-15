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
│   └── test_files/        # V-Synth WAV recordings go here
│       ├── sustained/     # sustained tones, held vowels
│       ├── transients/    # drum hits, plucks
│       ├── polyphonic/    # chords, multi-pitch material
│       └── edge_cases/    # whisper, noise, extreme settings
├── plugin/                # JUCE project
│   ├── CMakeLists.txt
│   └── Source/
│       ├── PluginProcessor.h/cpp      # JUCE boilerplate
│       ├── PluginEditor.h/cpp         # UI
│       ├── VariphraseEngine.h/cpp     # THE core algorithm — isolated + testable
│       ├── PhaseVocoder.h/cpp         # phase vocoder implementation
│       ├── SourceFilterModel.h/cpp    # LPC-based source/filter separation
│       └── OfflineRenderer.h/cpp      # WAV file render for batch testing
└── research/              # papers, notes, spectrogram images, session exports
```

## Key Architectural Decision: Isolated Engine
`VariphraseEngine` is a completely standalone C++ class with no JUCE dependencies.
It takes a buffer of floats in, returns a buffer of floats out.
This allows:
- Python to call it via pybind11 for automated testing
- Unit testing without a VST host
- Swapping algorithm implementations without touching plugin scaffolding

## Variphrase Parameters (matching V-Synth controls)
- **Pitch Shift** — semitones, range -24 to +24
- **Time Stretch** — ratio, range 0.25x to 4.0x  
- **Formant Shift** — semitones, range -12 to +12
- **Robot** — forces monophonic voiced analysis (V-Synth-style "Robot" mode)

## Algorithm Candidates (to be tested in order of complexity)
1. Phase vocoder with formant preservation (baseline)
2. Sinusoidal + residual (SMS) model
3. LPC source-filter separation + independent manipulation
4. Hybrid: LPC for voiced, phase vocoder for residual/noise

## Measurement & Scoring
Every algorithm version is scored by `batch_test.py`:
- **Null test residual (dBFS)** — lower is better; target < -30 dB
- **Formant accuracy** — LPC formant trajectory comparison
- **Transient smearing** — energy spread around detected transients
- **Composite score** — weighted sum used to track overall progress

## Current Hypothesis
**Status: Pre-data.** Hypothesis TBD pending first V-Synth test recordings.

## Known Constraints
- Cannot license Roland's PCM ROM content — custom samples required for oscillators
- D-Beam controller mapped to MIDI CC in plugin context
- Ribbon controllers mapped to MPE or standard CC
