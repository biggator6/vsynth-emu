# V-Synth VariPhrase Emulator — Architecture

## Project Goal
A JUCE plugin (AU, VST3, Standalone) that plays samples through ports of the Roland V-Synth XT's
own firmware: its VariPhrase encoders and players, the OSC converters, envelopes, LFOs, pitch path,
key assign, COSM filters, TVA and effects. The algorithms are taken from the owner's `rom.2`, read
at run time and never shipped. `CLAUDE.md` is the working reference, and
`research/HARDWARE_FIRMWARE.md` holds the findings.

Until Session 15 the goal was to reverse-engineer VariPhrase by black-box testing against a real
V-Synth, then write an emulation from what the differences showed. Session 16 (October 2026)
turned to the firmware itself. The engine from that earlier work stayed in the plugin as the
legacy engine (`VariphraseEngine`) until v191 removed it, with its Algorithm menu: without a
firmware image the plugin now plays a loaded file as it is. Only its classifier is left, as Auto's
(`ContentClassifier`).

## Methodology: Ports Checked Against the Firmware
1. Read the routine in `rom.2` (Ghidra, and the disassembly for DSP code).
2. Port it to C++ in `plugin/Source/`.
3. Run the firmware's own routine in the SH-4 interpreter (`plugin/Source/Sh4Interp.*`) beside the
   port, on fixed and random cases, in the port's harness (its `XT_*_MAIN` block).
4. A port is done when the two are equal (0 different), and planted errors show the harness
   catches a mistake (the `mutate_*` scripts in `research/tools/`).

The effect chip runs microcode the SH-4 can't run. `XtFxChip` is checked the same way against
JE-8086's interpreter, on the XT's own programs, and the effects' sound against captures from the
owner's XT.

## What VariPhrase Does
VariPhrase decouples three axes that are normally entangled in audio processing:

| Parameter   | Normal Pitch Shift      | VariPhrase         |
|-------------|-------------------------|--------------------|
| Pitch       | Locked to speed         | Fully independent  |
| Time/Duration | Locked to pitch       | Fully independent  |
| Formants    | Shift with pitch        | Fully independent  |

## Earlier Methodology: Black-Box Reverse Engineering (Sessions 1–15)
The method before Session 16, kept as it was; the analysis in `analysis/` and the scores below come from it.
1. Feed controlled inputs to the real V-Synth, capture outputs (WAV)
2. Run the same inputs through our plugin
3. Null test + spectrogram compare to measure the difference
4. Refine the algorithm based on what the residual reveals
5. Repeat until perceptually indistinguishable on most material

## Repository Structure
By folder; what each file in `plugin/Source/` does is in `CLAUDE.md`, "Where things are".
```
vsynth-emu/
├── CLAUDE.md              # the working reference: where things are, the tools, what to run when
├── ARCHITECTURE.md        # this file
├── notes.txt              # each version's note, and how to build
├── RESEARCH_LOG.md        # the sessions
├── RECORDING_GUIDE.md     # recording the V-Synth: the black-box takes, and the effect captures
├── CONTROLLERS.md         # the controllers' MIDI surface, and what the plugin needs for them
├── PROJECT_REVIEW.md      # Session 15's step back, before the turn to the firmware
├── LICENSE, NOTICE.md     # GPL-3.0, and the credits
├── plugin/                # the JUCE plugin: CMakeLists.txt, BuildId.cmake
│   ├── Source/            # the processor, editor and XtSynth; the firmware's ports (Xt*), each
│   │                      #   checked in its own XT_*_MAIN block; Sh4Interp, the SH-4 interpreter;
│   │                      #   Auto's classifier (ContentClassifier), all v191 kept of the legacy engine
│   └── Tests/             # PluginTest and the processor's other tests (VSynthEmu*Test)
├── research/
│   ├── HARDWARE_FIRMWARE.md  # the findings, newest version first
│   ├── UI_DESIGN.md       # the editor's design
│   ├── tools/             # build.sh, fcheck.sh, verify.sh, the mutate runners and the other scripts
│   ├── ghidra_scripts/    # Ghidra's scripts, run headless on rom.2
│   ├── third_party/       # JE-8086's ESP core (gearmulator, GPL-3.0): the effect chip's reference
│   └── README.md, PATENTS.md, VPENC_ABI.md   # the sources, Roland's patents, V-Trainer's encoder
├── analysis/              # the black-box pipeline (Sessions 1-15): compare.py, lpc.py,
│                          #   batch_test.py, variphrase_render; test_files/, plugin_outputs/,
│                          #   results/; projects/, the owner's V-Synth projects (gitignored)
├── IOS_companion/         # the iOS controller, built elsewhere
├── JUCE/                  # JUCE 8.0.7, a gitlink
├── firmware/              # the owner's rom.2 and disk image (gitignored, never committed)
└── build/                 # the plugin's build and the tools' work (gitignored)
```

## Key Architectural Decision: Isolated Engine
(Withdrawn in v191: the engine and its offline renderer were removed. This section and the
renderer's below describe the Sessions 1-15 pipeline as it was; their sources are in v190, 164ac15,
and `analysis/variphrase_render` is the last binary built from them.)

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
(`-DENGINE_TESTS_MAIN`, 15 checks, runs in seconds), until v191; its classifier check is now
`plugin/Tests/ClassifierTest.cpp` (`classifier_test`).

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
   - the COSM sections as TVFs or Dynamic TVFs (v61 COSM 1, v63 COSM 2 in
     series, v64 the Dynamic TVF's level follower; XtCosm):
     the twelve filter types, Cutoff, Resonance and Keyfollow, each with its
     own envelopes (Env 9/10, 11/12) and LFO (3, 4), through the XT's
     converter, control and filter kernels, between the player and the
     TVA. Since v78 all sixteen COSM types, each with its stereo routine
     since v104.
   - the analog oscillator (v79–v87, XtAnalog): OSC Type ANALOG plays the
     XT's SAW, SQUARE, TRIANGLE, SINE, RAMP, JUNO, NOISE, HQ-SAW and
     HQ-SQUARE (band-limited, from rom.2's tables), LA-SAW, LA-SQUARE,
     SUPER-SAW, FEEDBACK-OSC and X-MOD, most with a sub oscillator, with Impact,
     Pulse Width and Fat (their KFs, LFO and envelope
     depths, on the time and formant envelopes), through the analog
     converter, note-on, update and routine, its level from the XT's hidden
     analog calibration. It needs no sample. Its pitch is against A4 and has
     no glide.
   - OSC 2 (v86, PCM v103): a second oscillator on the same note, analog or PCM on
     its own wave and encode, with its own pitch, level, envelopes (Env 5…8) and
     LFO 2, summed with OSC 1 before the COSM sections, as the MOD section does
     when off or set to MIX. X-MOD on OSC 1 (v87) reads OSC 2's output instead, so
     OSC 2 is not heard.
   - the MOD section (v88–v91, XtMod): between the OSCs and the COSMs, MIX, RING, FM,
     ENV RING and OSC SYNC, with the Original Level, Attack and Release; a held note keeps
     its routine when the knobs change, as the firmware's does. FM's delay lines are one
     pool for all voices, as on the XT.
   - the Structure Type (v92): the order of the sections, latched at note-on (`xtToneOrder`).
     Type1: OSC 2, OSC 1, MOD, COSM 1, COSM 2. Type2: COSM 1 on OSC 1 before the MOD. Type3:
     COSM 2 on OSC 2, COSM 1 on OSC 1, then the MOD, with COSM 2 following OSC 2's pitch and
     root (`xtCosmOsc`), and X-MOD reading OSC 2 after COSM 2.
3. **The 44.1 kHz mix is resampled to the host rate** with a streaming
   polyphase Kaiser sinc (16 taps each side). At 44.1 kHz it is bypassed.

**The firmware image.** A new instance looks for `VSE_XT_ROM`, then
`V-SynthXT.rom.2.bin` beside the plugin's bundle (not in it: an added file
breaks the signature), then in `~/Library/Application Support/V-Synth Emu/`,
then the file last chosen with the **ROM...** button, which `settings.xml` in
that folder keeps for every project and plugin format. A project also saves
its path, used only if none of those is found, and not kept for other projects.
Without any, the plugin falls back to the engine's streaming path (pre-v43
behaviour). The tables belong to the process: an image already in memory isn't
read again under the instances playing from it, and one that won't load leaves
the last in place.

**V-Synth's own files.** A `.vpw` loads wherever a WAV does (`XtVpwFormat`), its
RDAC audio unpacked by the firmware's own routine; on Auto it encodes as the type
the wave says it is. `XtFiles` also reads a `.p00` into the firmware's store, and
`XtPatch` turns one zone of that into `XtSynth::Controls`: `loadPatch` sets the parameters
and effects from it and loads its waves from the project's `Wave` folder.

**Checks:**
- At a 44.1 kHz host rate, `XtSynth` output is bit-exact against the
  firmware chain: `xt_fw play`, and the engine's SOLO render.
- `XtEnvelope` is tick-for-tick identical to the firmware's envelope
  routines run in the SH-4 interpreter (`xt_env`, 18 cases). fcheck now runs
  six of them, and the time converter on 1,045,760 cases, 0 different.
- The SOLO player (`XtSoloVoice`, mono and stereo) is within −101…−118 dB
  of the firmware player in the P-code emulator, on 13 mono cases and 4
  stereo ones (v44). Since v122 it is bit for bit: `xt_fw solocmp`, 32 cases,
  every one 0 different (HARDWARE_FIRMWARE.md, "SOLO bit for bit").
- `Tests/PluginTest.cpp` drives the real processor (ROM, file load,
  background encode, sample-accurate MIDI) at 48 kHz. Its output is
  **bit-identical** to `XtSynth` on the chord (ENSEMBLE), drum (BACKING),
  vocal (SOLO) and the stereo factory wave 358 (SOLO), with any envelope
  and a note-off (`TEST_ADSR`), and with the OSC knobs and the time and
  formant envelopes set (`TEST_OSC`, `TEST_TENV`, `TEST_FENV`).
  `TEST_SNAPSHOT=file.png` draws the editor offscreen: D from v196
  (research/UI_DESIGN.md), whose layout and controls `VSynthEmuEditorTest`
  checks.
- `XtOsc` equals the firmware's VariPhrase converter on 310,545 cases × 6
  values and its exp2 on 17,143 arguments (`xt_osc`, v51).
- The level path (v53): the velocity curve, the OSC level slots and the
  gain target equal the firmware (`xt_osc`); the players' gain stages
  match the firmware players with gain changes.
- Robot Voice (v54) matches the firmware's SOLO player at −94.5…−110 dB. Since
  v122, 0 different (`solocmp`'s three Robot cases).
- The pitch path (v55): the note's pitch, the cents, the Pitch ratio, the
  target and the voice update's step equal the firmware (`xt_osc`, 385,240
  cases).
- SOLO loops (v56) match the firmware's SOLO player, given the same cached
  loop events, at −92…−107 dB (`xt_fw solocmp … loop:…`); since v122,
  0 different, One Way and Alternate among the cases. BACKING and
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
- The analog oscillator (v79–v85): the converter's block, the note-on, the
  update, the install and the routines, the sub oscillator's eight too (outputs,
  state and wrap masks), equal the firmware (`xt_analog`).
- The MOD section (v88–v91): the converter's block, the note-on, a new block on a
  held note, all five Types, FM's lines and OSC SYNC's mask and note-on rule, alone and in
  the firmware's own render and tone note-on, equal the firmware (`xt_analog`).
- The Structure Types (v92): a tone run by the firmware's own note-on, update and three
  renders, the MOD and both COSM sections live on all sixteen types, equals the port block by
  block (`xt_analog`); `mixBlock` walks the same order table as the check.
- X-MOD (v87): its converter case on both OSCs, and the routine alone and under
  the firmware's own OSC 2 in the tone's render, equal the firmware (`xt_analog`).
- PCM on OSC 2 (v103): `xt_tone`, a whole firmware tone against XtSynth with a wave on OSC 2; and in `xt_analog`
  real encoded waves through the firmware's converter, note-on, updates and render: BACKING and ENSEMBLE equal
  the firmware, SOLO within 2e−4 of full scale. Notes whose firmware reads leave the wave are not compared.
  Now (fcheck's baseline) SOLO equals the firmware too: `xt_analog`'s PCM tone is 0 different on every kind of
  wave, SOLO mono and stereo among them.
- OSC 2 (v86): the converter with the input's OSC set, and both voices in a tone
  object through the firmware's binding, setup, install, updates and render
  (both buffers, the mix, the masks, the fields and the shared delay lines),
  equal the firmware (`xt_analog`).
- COSM TVF (v61) and Dynamic TVF (v64): the converters, start values,
  targets and updates, the follower and the twelve filter kernels equal the
  firmware (`xt_cosm`). All sixteen COSM types since v78, and their stereo
  routines since v104, the same way (CLAUDE.md, "Porting a COSM type").
- Reverse and held playback (v52), against the firmware's own players run
  in the SH-4 interpreter (`xt_fw solocmp` / `bandcmp`). BACKING and
  ENSEMBLE are bit-exact forward, backward, held and through walk changes.
  SOLO agrees at −94…−109 dB in every direction, the port's usual
  output-stage residual; since v122, 0 different in every direction too.
- XtSynth's wiring (v93–v100): a whole firmware tone, from the parameter IDs on, equals XtSynth inside its
  block and at its output, bit for bit but for a SOLO wave's buffers, over random settings and a sweep of
  every field in its change lists (`xt_tone`). Not the voice allocator, and not a real unit. Since v122 a
  SOLO wave's buffers too: `xt_tone` has no tolerance left.
- The TVA Switch (v101): off, the TVA has no envelope, as the firmware (`xt_tone`). Off by default, as on the XT (v102).
- The patch TVA's level and pan (v114): Level, Pan, their Keyfollows and LFO Depths, LFO 5, the MFX Send Level and CC 7, 10, 11, as the firmware (`xt_tone`).
- Hold, Sostenuto and the mode messages (v117): the note's three reasons to sound, as the firmware (`xt_tone`). Any controller is a tone update.
- Tempo sync and Time Trip (v118): the Patch Tempo, note values on envelope times and LFO rates, Tempo Sync, the Time Trip pad, its Switch and Beat Keep, as the firmware (`xt_tone`).
- The Matrix (v119): the eight controls, every source, the D-Beam's controllers and pressures, as the firmware (`xt_tone`), each slot's sum compared. MFX-PRM1…3 wait for the effects (they reach the effects' driver since v137). The Multi Step Modulator is not ported. The COSMs' PRMs in v120. TRIP-R from the Time Trip pad, and what a source moved with no update reaches, in v124.
- Time Trip by its controllers (v132): Time Trip Assign X and Y, on CC82 and CC83, move the pad, as the firmware's Control Change case (`xt_tone`).
- The pad's X-Y pair (v134): Pad Assign X and Y default to CC80 and CC81, the Matrix's PAD sources reading them, as the firmware (`xt_tone`). Both pairs are always live: Pad Mode is only what the XT's own pad sends.
- The host's tempo (v125): Clock Source HOST is the firmware's EXTERNAL on the host's transport, a clock on each 24th of a quarter it passes, the tempo read from the ms over six of them, as the firmware (`xt_tone`). Default INTERNAL, the Patch Tempo.
- The effects (v136–v138): the chip (v126) and the firmware's own driver (v131) in the sound behind the Effects switch, off by default, with MFX, Chorus, Reverb, the system's EQ, Master Level and Output Gain on the editor's panel (`FxPanel`, each value's text by the firmware's own routine, v138), and the tone's Chorus and Reverb Send, Output Assign and Output Mode (v133). Exact against JE-8086's interpreter and the firmware's driver in `Sh4Interp`; no reference yet beyond those.
- `auval -v aumu VsEm Gato` passes (the codes Ystu until v186).
- Each line above is as its version wrote it, with what holds now after it where that changed (v183).
  The checks of v139 on are in each version's note in `notes.txt`; fcheck and `verify.sh` run them all.

**Not yet** (checked at v182):
- the effects against a real XT: no capture is recorded yet (HARDWARE_FIRMWARE.md, "The effect
  captures"; the steps in `RECORDING_GUIDE.md`, "Effects captures");
- host automation of the effects that go by ID: each type's parameters and the system's EQ are
  saved as the project's `fxEdits`, not host parameters (v137);
- the arpeggio, the Multi Step Modulator and an OSC's external input: `xtPatchMissing` names them
  when a patch asks for one;
- the output level against a real XT: it is set by the firmware's numbers (v149), not measured.

Done since this list was first written: STEP and EVENT playback (v155 band waves, v156 SOLO), the
Matrix on every source, the OSC knobs among its destinations (v119, v120, v124), and the time and
formant envelopes' velocity settings, each OSC's, as host parameters.

## Known Gaps / Next Work

"Not yet" above, and each version's own in `notes.txt`. The listening pass, Round-2 recordings and
real-time parity named here at Session 15 belonged to the black-box engine.

## Known Constraints
- Cannot license Roland's PCM ROM content — custom samples required for oscillators
- Expressive controllers are the Time Trip Pad, twin D Beam and C1/C2 knobs (E1–E8 on
  the XT); there is **no ribbon**. MIDI surface and the MPE recommendation for the pad:
  `CONTROLLERS.md`
- Audio files from V-Synth: 48 kHz, 24-bit PCM, stereo — renderer downmixes to mono and
  handles both sample rates transparently
