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

## Hybrid Algorithm — Routing Logic (current, v28 / Session 15)

The offline encode pass (`analyzeContent`) classifies content once per sample
(LITE / SOLO / ENSEMBLE / BACKING) and detects onset stamps.  Offline
rendering then routes by content type and operation — each engine implements
the corresponding Roland patent (see `research/PATENTS.md`):

| Content type | Operation | Engine |
|---|---|---|
| SOLO (speech/melody) | any | **pitch-synchronous granular** (US6421642) |
| LITE (pure tones) | time, pitch-up | LPC resynthesis |
| LITE | pitch-down | Phase Vocoder |
| LITE | formant | LPC source-filter (pole shift) |
| ENSEMBLE (chords) | time-only | **subband stretch** (US6564187) |
| ENSEMBLE | pitch / formant | Phase Vocoder / LPC |
| BACKING (drums) | time-only | **event-based stretch** (attacks verbatim, subband tails) |
| BACKING | pitch | Phase Vocoder |

The real-time (`processMono`) path still runs the older streaming PV/LPC
routing — see "Real-Time Parity Plan" below.

## Confirmed V-Synth Architecture (patents + black-box, Session 15)

Roland's three patents (filed Feb–May 2000) describe the system completely:

1. **US6421642** — the VariPhrase playback engine: the phrase is cut into
   one-pitch-period grains at encode time (per-grain pitch + syllable marks);
   playback walks the grain list at the time-stretch rate, re-triggers grains
   at the target pitch period (two channels, half-cycle offset, triangular
   windows), and scales the formant envelope via grain read velocity with the
   window clamped to the re-trigger period.
2. **US6564187** — subband time stretch for polyphony: log-spaced sub-bands
   (~one partial each), per-band amplitude/instantaneous-frequency
   trajectories, time-modified and resynthesised.
3. **US6201175** — band-wise sinusoidal variant (not currently used).

Black-box findings that led to / confirm this: the V-Synth RESYNTHESIZES all
content (a time-stretched pure sine reference carries sawtooth-like grain
harmonics); the formant knob saturates upward (~×1.67 at "+12") — explained
by the patent's window clamp; syllable marks = onset event stamps.

## Measurement & Scoring (metric v4)

`analysis/compare.py` per test pair (see PROJECT_REVIEW.md for rationale):
- **Spectral similarity** — phase-blind multi-resolution STFT log-magnitude
  L1 + log-mel L1, time-aligned (envelope cross-correlation) and gain-matched
  (least-squares optimal gain).  The V-Synth resynthesizes, so phase-sensitive
  null testing cannot measure closeness; this term carries most of the score.
- **Formant trajectory similarity** (order-50 LPC) — speech content.
- **Transient preservation** — percussive content.
- **SNR** (gain-matched null) — small weight, 30 dB = full marks.

Content-aware composite weights (spectral/formant/transient/snr):
speech .45/.30/.15/.10 · percussive .45/0/.45/.10 · tonal .70/.10/.10/.10.

The HTML batch report embeds **A/B audio players** per case.
Fast engine invariants: `plugin/Source/EngineTests.cpp`
(`-DENGINE_TESTS_MAIN`, 10 checks, runs in seconds).

## Current Scores (v28, metric v4)

**51.8 / 100 average** (re-baselined v17 = 34.0).  Strong: drum_time_4x 64.7,
chord_formant 63.3, sine_formant_upmax 65.6, drum_pitch 62.9, chord_time
62.7, drum_2x 57.2, vocal_time_2x 53.2.  Weak: vocal_formant_downmax 35.7,
vocal_pitch_up7st 37.5, chord pitch/time-residual ~40.
Full history in `notes.txt`; per-case tables in `analysis/results/`.

## Real-Time Parity Plan (next major work)

The offline path has diverged from real-time: granular, subband, event-based
stretch, and drain mode are offline-only.  Plan:

1. **Granular real-time is the easy one** (the V-Synth did it on 2000-era
   hardware): run the encode (grain cutting) when a sample is loaded — or
   incrementally, ~1 frame behind the write head for live input — then
   playback is O(2 grains)/sample.  Port `granularResynthOffline` to a
   streaming class with a grain-list ring.
2. **Subband real-time** needs the patent's actual octave-cascade filter bank
   (the offline version uses a whole-file FFT); implement per US6564187's
   halfband cascade, or accept PV for live ENSEMBLE.
3. **Event stamps** work live with lookahead latency (onsets detected
   ~2048 samples behind the write head).
4. Keep the current streaming PV/LPC as the fallback for unclassified /
   low-latency modes.

## Known Gaps / Next Work

See `notes.txt` (always current) and `RESEARCH_LOG.md` Session 15 final
next-steps.  Headlines: listening pass (A/B players ready), Round-2
recordings (`RECORDING_GUIDE.md`), real-time parity above.

## Known Constraints
- Cannot license Roland's PCM ROM content — custom samples required for oscillators
- D-Beam controller mapped to MIDI CC in plugin context
- Ribbon controllers mapped to MPE or standard CC
- Audio files from V-Synth: 48 kHz, 24-bit PCM, stereo — renderer downmixes to mono and
  handles both sample rates transparently
