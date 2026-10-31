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
├── CONTROLLERS.md         # controller MIDI surface + emulation requirements
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
│       ├── PluginProcessor.h/cpp      # JUCE processor: XT voices, encode thread, MIDI
│       ├── PluginEditor.h/cpp         # UI (ROM, Encode Type, Key, Depth, knobs)
│       ├── XtSynth.h/cpp              # polyphonic XT playback (no JUCE): encode + voices + resampler
│       ├── XtSoloEncoder.h/cpp        # the XT's SOLO encoder, ported (bit-exact)
│       ├── XtSoloVoice.h/cpp          # the XT's SOLO player as a block voice
│       ├── XtMultiBandPlayer.h/cpp    # the XT's BACKING / ENSEMBLE players, ported (bit-exact)
│       ├── Sh4Interp.h/cpp            # SH-4 interpreter: runs rom.2's load/encode-time code
│       ├── XtFirmware.h/cpp           # firmware harnesses: multi-band encoder, band rebuild
│       ├── VariphraseEngine.h/cpp     # offline router for the test suite — isolated + testable
│       ├── PhaseVocoder.h/cpp         # phase vocoder + WSOLA time-stretch
│       ├── SourceFilterModel.h/cpp    # LPC source-filter (formant shift)
│       └── OfflineRenderer.h/cpp      # WAV file renderer for batch testing
│   └── Tests/PluginTest.cpp           # processor end-to-end test (target VSynthEmuTest)
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
    PhaseVocoder.cpp SourceFilterModel.cpp XtSoloEncoder.cpp XtSoloVoice.cpp \
    XtMultiBandPlayer.cpp Sh4Interp.cpp XtFirmware.cpp \
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
| SOLO (speech/melody) | any | **firmware SOLO player** (v31/v32: two-channel grains, velocity = formant, epoch-aligned records — transcribed from V-Synth XT firmware; US6421642) |
| LITE (pure tones) | time, pitch-up | LPC resynthesis |
| LITE | pitch-down | Phase Vocoder |
| LITE | formant | LPC source-filter (pole shift) |
| ENSEMBLE (chords) | time-only | **subband stretch** (US6564187) |
| ENSEMBLE | pitch | **subband** (per-band inst-freq × ratio; v30) |
| ENSEMBLE, BACKING | formant | **ignored** — no formant control on these encode types (v29) |
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

## Measurement & Scoring (metric v6)

`analysis/compare.py` per test pair (see PROJECT_REVIEW.md for rationale):
- **Spectral similarity** — phase-blind multi-resolution STFT log-magnitude
  L1 + log-mel L1, time-aligned (envelope cross-correlation) and level-
  matched by RMS ratio (v5; v4 used a phase-sensitive least-squares gain).  The V-Synth resynthesizes, so phase-sensitive
  null testing cannot measure closeness; this term carries most of the score.
- **Pitch similarity** (v6) — correlation of the harmonic comb (long-term
  spectrum at n_fft 16384, envelope removed); pitched content only.
- **Formant trajectory similarity** (order-50 LPC) — speech content.
- **Transient preservation** — percussive content.
- **SNR** (gain-matched null) — small weight, 30 dB = full marks.

Content-aware composite weights (spectral/pitch/formant/transient/snr):
speech .35/.20/.20/.15/.10 · percussive .45/0/0/.45/.10 ·
tonal .50/.30/0/.10/.10.

The HTML batch report embeds **A/B audio players** per case.
Fast engine invariants: `plugin/Source/EngineTests.cpp`
(`-DENGINE_TESTS_MAIN`, 15 checks, runs in seconds).

## Current Scores (v47, metric v6)

**76.8 / 100 average** (v47 76.765: BACKING/ENSEMBLE inputs imported
through the XT's own WAV converter, chord time 2× 85.4 → 87.9; v44 76.601:
SOLO player corrections; v42 76.600: BACKING and ENSEMBLE through the XT's
own firmware chain, chord 75.4 → 87.3; v40 74.8; full history in
notes.txt).
Earlier: **73.9 / 100 average** (v35 — SOLO encoder epochs matched to the V-Synth XT's own grain records, vocal 74.6 → 77.4; v33 = 73.0; four sine references show the formant an octave low although the Formant knob was 0 (likely Formant KF / envelope / LFO in the sine patch; re-record pending, see notes.txt); render with `analysis/render_suite.py`) under metric v6 (v28: 66.6 v6 / 68.1 v5 / 52.5 v4;
history in notes.txt).  Per-case v4 detail below is historical.  Earlier text:
51.8 / 100 under v4 (re-baselined v17 = 34.0).  Strong: drum_time_4x 64.7,
chord_formant 63.3, sine_formant_upmax 65.6, drum_pitch 62.9, chord_time
62.7, drum_2x 57.2, vocal_time_2x 53.2.  Weak: vocal_formant_downmax 35.7,
vocal_pitch_up7st 37.5, chord pitch/time-residual ~40.
Full history in `notes.txt`; per-case tables in `analysis/results/`.

## The plugin: the XT's own players in real time (v43)

The plugin plays a loaded sample the way the XT plays an encoded wave:

1. **Encode at load, off the audio thread** (`VSynthEmuProcessor::reencode`
   → `xtSynthEncode`):
   - import as the XT imports a WAV (v47): to int16, a 24-bit source keeping
     its top 16 bits, then the XT's own converter to 44.1 kHz (`xtFwImport`,
     run from rom.2), unless the file is already 44.1 kHz;
   - encode as the chosen **Encode Type**. Auto uses the engine's content
     classifier; LITE plays on the SOLO player.
   - SOLO: the XT's SOLO encoder port. A stereo wave has one record stream,
     encoded from (L+R) >> 1, and the player reads both channels through it
     (v44).
   - BACKING / ENSEMBLE: the firmware's multi-band encoder, with attack
     detection at the **Encode Depth**, and the load-time band rebuild. Both
     run from `rom.2` in the SH-4 interpreter, about 1–2 s.
2. **One firmware player per note** (`XtSynth`, 16 voices, oldest stolen):
   - `XtSoloVoice` or `XtMbPlayer` render 32-frame blocks at 44.1 kHz;
   - pitch = 2^((note − Original Key + Pitch)/12); Time, Formant (SOLO) and
     Energy (SOLO) apply live, from the XT's own knobs (v51): Time −63…+63
     with Time Offset BWD/ZERO/FWD and Time KF; Formant −63…+63 (±1
     octave) with Formant KF; Energy OFF/1–127, g = (k/127)². These go
     through the firmware's VariPhrase converter (`XtOsc`, `0x894e2f20`);
     with a ROM, the TIME and FORMANT knobs are the XT's;
   - the amp envelope is the XT's own generator (`XtEnvelope`, v45): A/D/S/R
     knobs 0–127 through the firmware's time and sustain tables, and its
     attack and segment curves. It is applied as the patch TVA applies it
     (v46): an update every 441 frames, linear gain ramps with the TVA's
     ramp-length and landing rules, and voices end at the TVA's done check.
     Velocity shortens or lengthens the times as the XT's converter does
     (A/D/R-Sens, stored as the XT stores them since v53), and Time KF
     scales decay and release by key (v48). Velocity sets the level through
     the XT's velocity curves (v53: Velocity Sens and Curve).
   - a pitch envelope (v49): the same generator, linear, with the XT's depth
     curve (±6000 cents). Voices are updated as on the XT: every 10 ms the
     envelopes step and pitch, formant, time and Energy move 70% toward
     their targets, so knob changes glide in 10 ms steps.
   - an LFO on pitch (v50): the XT's LFO with all eight waveforms, delay,
     fade modes, offset and key sync, tick-for-tick identical to the
     firmware's, and the same depth curve as the pitch envelope.
   - time and formant envelopes and the LFO's time and formant depths
     (v51), as the converter scales them: time depth ±3 at ±60 and ±5 at
     ±63 (walk speed), formant depth d/63 octaves. The voice's walk is
     clamped to ±5, its formant ratio to [0.5, 2].
   - reverse (v52): a negative walk (Time Offset BWD, Time below −20 with
     FWD, or the time envelope or LFO) plays backward, a note starting at
     the wave's end, and walk 0 holds, as the XT's players do.
   - the OSC TVA (v53): Level, Level KF, Level LFO depth, Wave Gain, the
     oscillator's own amp envelope in series with the patch TVA, and
     velocity to level, through the firmware's level path and the player's
     per-sample gain.
   - Robot Voice (v54, SOLO): voiced grains at the note's fixed pitch
     against A440, from the plugin's Robot button.
   - the OSC pitch (v55): Coarse, Fine, the Pitch knob and Pitch KF, with
     the pitch envelope and LFO, through the firmware's pitch functions and
     exp2, so the XT's tiny departures from equal temperament (±0.02 cents)
     come out too. Pitch Shift adds to the patch's Coarse and Fine.
   - loop playback (v56 SOLO, v57 BACKING/ENSEMBLE): Loop, Loop Mode (One
     Shot, One Way, Alternate) and a loop region, played by the firmware's
     loop logic in each player (crossings, jumps, Alternate bounces).
   - portamento (v58): Mode, Type, Time and Time Velo Sens through the
     firmware's settings converter and the voice's pitch ramp.
   - the note's tuning (v59): the patch's Coarse/Fine Tune and Scale Tune,
     Random Pitch Depth (the XT's own generator) and the pitch wheel with
     Bend Range Up/Down, summed as the per-note OSC setup does.
   - key assign (v60): Mono/Poly, Legato Switch and Octave Shift through the
     XT's key state; a MONO legato note takes over the sounding voice,
     otherwise the old voice is truncated (0.70 ms).
3. **The 44.1 kHz mix is resampled to the host rate** with a streaming
   polyphase Kaiser sinc (16 taps each side). At 44.1 kHz it is bypassed.

**The firmware image.** The plugin looks for `VSE_XT_ROM`, then
`~/Library/Application Support/V-Synth Emu/V-SynthXT.rom.2.bin`; the
**ROM...** button sets a path, saved with the session. Without it, the
plugin falls back to the engine's streaming path (pre-v43 behaviour).

**Checks:**
- At a 44.1 kHz host rate, `XtSynth` output is bit-exact against the
  firmware chain: `xt_fw play`, and the engine's SOLO render.
- `XtEnvelope` is tick-for-tick identical to the firmware's envelope
  routines run in the SH-4 interpreter (`xt_env`, 18 cases).
- The SOLO player (`XtSoloVoice`, mono and stereo) is within −101…−118 dB
  of the firmware player in the P-code emulator, on 13 mono cases and 4
  stereo ones (v44).
- `Tests/PluginTest.cpp` drives the real processor (ROM, file load,
  background encode, sample-accurate MIDI) at 48 kHz. Its output is
  **bit-identical** to `XtSynth` on the chord (ENSEMBLE), drum (BACKING),
  vocal (SOLO) and the stereo factory wave 358 (SOLO), with any envelope
  and a note-off (`TEST_ADSR`), and with the OSC knobs and the time and
  formant envelopes set (`TEST_OSC`, `TEST_TENV`, `TEST_FENV`).
  `TEST_SNAPSHOT=file.png` draws the editor offscreen.
- `XtOsc` equals the firmware's VariPhrase converter on 310,545 cases × 6
  values and its exp2 on 17,143 arguments (`xt_osc`, v51).
- The level path (v53): the velocity curve, the OSC level slots and the
  gain target equal the firmware (`xt_osc`); the players' gain stages
  match the firmware players with gain changes.
- Robot Voice (v54) matches the firmware's SOLO player at −94.5…−110 dB.
- The pitch path (v55): the note's pitch, the cents, the Pitch ratio, the
  target and the voice update's step equal the firmware (`xt_osc`, 385,240
  cases).
- SOLO loops (v56) match the firmware's SOLO player, given the same cached
  loop events, at −92…−107 dB (`xt_fw solocmp … loop:…`). BACKING and
  ENSEMBLE loops (v57) are bit-exact against the firmware players
  (`xt_fw bandcmp … loop:…`).
- Portamento (v58): the settings converter and the pitch ramp equal the
  firmware (`xt_osc`, 6.5 million values).
- The note's tuning (v59): the per-note OSC setup's coarse, fine and random
  offset (300,000 cases, the generator running on) and the bend (802,816
  cases) equal the firmware (`xt_osc`).
- Mono/Poly (v60): the key state (120,000 events), the ramp's legato update
  and restart (2.05 million values) and the key-follow key equal the
  firmware (`xt_osc`).
- Reverse and held playback (v52), against the firmware's own players run
  in the SH-4 interpreter (`xt_fw solocmp` / `bandcmp`). BACKING and
  ENSEMBLE are bit-exact forward, backward, held and through walk changes.
  SOLO agrees at −94…−109 dB in every direction, the port's usual
  output-stage residual.
- `auval -v aumu VsEm Ystu` passes.

**Not yet:**
- the patch TVA's own Level, Level KF, pan and LFO terms are not
  modelled;
- no V-Synth voice layer (filters, LFOs, effects);
- no step/event playback or Time Trip;
- no matrix or realtime controllers on the OSC knobs, and the time and
  formant envelope depths' velocity sensitivity is not exposed;
- no Random Pitch Depth, scale tune or patch Coarse/Fine path.

## Known Gaps / Next Work

See `notes.txt` (always current) and `RESEARCH_LOG.md` Session 15 final
next-steps.  Headlines: listening pass (A/B players ready), Round-2
recordings (`RECORDING_GUIDE.md`), real-time parity above.

## Known Constraints
- Cannot license Roland's PCM ROM content — custom samples required for oscillators
- Expressive controllers are the Time Trip Pad, twin D Beam and C1/C2 knobs (E1–E8 on
  the XT); there is **no ribbon**. MIDI surface and the MPE recommendation for the pad:
  `CONTROLLERS.md`
- Audio files from V-Synth: 48 kHz, 24-bit PCM, stereo — renderer downmixes to mono and
  handles both sample rates transparently
