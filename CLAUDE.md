# CLAUDE.md — vsynth-emu

Operational guide for AI agents working on this project. Read this **before** touching code.

## What this project is

A black-box reverse-engineering effort to clone Roland V-Synth's **VariPhrase** DSP (independent pitch / time / formant axes) as a JUCE VST3/AU plugin.

Methodology: feed controlled signals into a real V-Synth, capture WAV outputs, run the same inputs through this plugin, null-test and score the difference. Iterate on the algorithm until the residual is small.

**Current state:** Hybrid algorithm (LPC for formant shift, phase vocoder for pitch/time) scoring **~28/100** against a 60-point target. Six sessions in. Test corpus is currently six pure sine waves — a real-material recording session (vowel/drum/chord) is the next planned input.

## Start every session by reading

1. [ARCHITECTURE.md](ARCHITECTURE.md) — algorithm status, per-case scores, routing table, known gaps
2. [RESEARCH_LOG.md](RESEARCH_LOG.md) — session-by-session findings, hypotheses, what was tried
3. [notes.txt](notes.txt) — current score snapshot and rebuild command quick-reference
4. [RECORDING_GUIDE.md](RECORDING_GUIDE.md) — V-Synth patch settings and file list for the pending recording session

These four files are the project's brain. Update them as state changes — don't let them drift.

## Where context lives — the only-two-files rule

**Persistent guidance and findings go in exactly one of two top-level docs:**

- **`CLAUDE.md`** (this file) — operational rules, build commands, architectural invariants, gotchas. Things an agent must read *before* acting. Keep terse and imperative.
- **`CONTEXT.md`** — longer-form findings, decisions, dead ends, accumulated lore that didn't fit cleanly into a session log entry but is too durable for `.remember/`. Create it the first time you need it; do not pre-create empty.

**Do not create new top-level `*.md` files** to capture context (no `NOTES_2.md`, no `LEARNINGS.md`, no `TODO.md`). The four brain-files listed above (ARCHITECTURE / RESEARCH_LOG / RECORDING_GUIDE / notes) are the *only* other docs that should grow — each has a defined purpose, stay inside it:

- New algorithm status, score, or routing change → `ARCHITECTURE.md`
- New session-level finding or hypothesis-tested → `RESEARCH_LOG.md`
- Recording-session logistics → `RECORDING_GUIDE.md`
- Current-score snapshot or command quick-ref → `notes.txt`
- Anything else that's durable → `CLAUDE.md` or `CONTEXT.md`

If you find yourself wanting a new doc, ask: does it fit one of the above? If not, it probably belongs as a section in `CONTEXT.md`. README.md is for human visitors landing on the repo and should stay short — do not pile context into it.

## Repository layout

```
plugin/Source/          C++ DSP — JUCE-free engine + JUCE plugin wrapper
  VariphraseEngine.*    Algorithm router (PIMPL, JUCE-free, the public API)
  PhaseVocoder.*        Pitch + time (good) + formant (poor — single peak, not harmonics)
  SourceFilterModel.*   LPC source-filter — sawtooth excitation + biquad cascade
  OfflineRenderer.*     CLI tool; compiles with -DOFFLINE_RENDERER_MAIN
  PluginProcessor.*     JUCE boilerplate
  PluginEditor.*        UI

analysis/               Python test harness (independent of JUCE)
  variphrase_render     Compiled CLI binary the harness invokes
  batch_test.py         Runs full regression across test_files, emits JSON+HTML
  compare.py            Null-test + spectrogram + composite scoring
  lpc.py                Formant extraction (Levinson-Durbin, reference impl)
  test_files/           V-Synth reference WAVs (passthrough/sustained/transients/polyphonic/edge_cases)
  plugin_outputs/       Engine renders, one subdir per algorithm version (hybrid_v5, hybrid_v6, …)
  results/              batch_test.py output (JSON + HTML reports)
```

## Build & run commands

**Rebuild the offline renderer** (the binary the Python harness calls). Do this after editing any DSP source:
```bash
clang++ -std=c++17 -O2 -DOFFLINE_RENDERER_MAIN \
  plugin/Source/OfflineRenderer.cpp \
  plugin/Source/VariphraseEngine.cpp \
  plugin/Source/PhaseVocoder.cpp \
  plugin/Source/SourceFilterModel.cpp \
  -o analysis/variphrase_render
```

**Run one test case:**
```bash
./analysis/variphrase_render \
  --input  analysis/test_files/passthrough/sine_440_formant_downmax.wav \
  --output /tmp/out.wav \
  --algo hybrid --formant -12
```

**Run the full batch test** (always from project root — paths are relative):
```bash
python3 analysis/batch_test.py \
  --ref-dir    analysis/test_files \
  --plugin-dir analysis/plugin_outputs/hybrid_vN \
  --output     analysis/results/hybrid_vN
```
Bump `vN` for each iteration so results are comparable across versions.

**Build the JUCE plugin** (requires JUCE cloned as a sibling dir, or set `-DJUCE_PATH=...`):
```bash
cmake -S plugin -B build && cmake --build build
```

## Architectural rules — don't break these

- **`VariphraseEngine` must stay JUCE-free.** It's a pure C++ class so the offline renderer, unit tests, and future Python bindings can use it without a VST host. If you find yourself reaching for `juce::` inside the engine or its dependencies, stop.
- **The offline renderer is the source of truth for batch testing.** Every algorithm change must rebuild it (see command above) — the harness will silently use stale binaries otherwise. Multiple sessions have lost time to this.
- **Algorithm versions are immutable on disk.** New iteration → new `plugin_outputs/hybrid_vN+1/` directory. Don't overwrite old outputs; comparison across versions depends on them.
- **No allocation in the audio thread.** All buffers are preallocated in `prepare()`. The audio path is `processMono()` and what it calls; if you add a `std::vector` ctor or `new` in there, you'll get dropouts in a DAW.

## Layer boundaries

Treat the project as three related but distinct layers:

- **Audio engine / research core** — `VariphraseEngine`, `PhaseVocoder`, `SourceFilterModel`, and `OfflineRenderer`. This layer must remain JUCE-free, deterministic, offline-renderable, and scoreable without a DAW.
- **Plugin integration** — `PluginProcessor` and host-facing behavior: parameter mapping, MIDI/file playback, sample-rate handling, state restore, latency/tail drain, and real-time safety. This is not UI; bugs here can change audible behavior even when the DSP core is correct.
- **UI** — `PluginEditor` and user workflow. The current UI is intentionally minimal and should not drive algorithm decisions.

UI enhancements and modernization cleanup are needed eventually: clearer controls, better state feedback, cleaner file-loading workflow, and general visual polish. Defer that work until the measurement loop and engine behavior are trustworthy, unless a UI issue blocks testing.

## Build cleanup notes

- **C++ standard mismatch:** `plugin/CMakeLists.txt` currently requests C++20, while the documented offline-renderer commands use `-std=c++17` and `OfflineRenderer.h` still mentions `-std=c++20`. Current engine code does not appear to require C++20. Downstream review should standardize the project on one version, preferably C++17 unless a specific C++20 feature is intentionally adopted.

## Known critical bugs (verified during code review, not yet fixed)

These should be fixed *before* any further algorithm tuning, because they silently corrupt scores.

1. **Offline-render latency compensation is wrong** — [VariphraseEngine.cpp:170-194](plugin/Source/VariphraseEngine.cpp:170). Allocates `n + latency`, only writes `n` samples, then erases the leading `latency` (which is zeros, not the buffered output) and resizes back to `n`. Every batch score is misaligned by `latencySamples`. Fix: run extra zero-padded blocks through the engine to flush its delay line, *then* trim leading latency.
2. **Biquad cascade state not cleared when switching to formant-shift path** — [SourceFilterModel.cpp:325,350](plugin/Source/SourceFilterModel.cpp:325). Old direct-form IIR state corrupts the first samples of cascade synthesis. Add `std::fill(biquadState_.begin(), biquadState_.end(), {0.0f, 0.0f})` after assigning `biquads_`.
3. **Levinson-Durbin early-stop leaves low-damping poles unchecked** — reflection-coefficient guard `|λ| < 1` doesn't catch poles with radius ~0.99 that formant shift can then push outside the unit circle. Add a post-hoc pole-radius check and re-run Levinson at higher order if any radius > 0.99.

## Scoring caveats — don't be fooled by your own metrics

- **Composite-score weights (40% formant / 35% SNR / 25% transient) are unvalidated guesses.** A score delta of < 2–3 points is below measurement noise. Don't celebrate a 28.7 → 29.1 "improvement" or panic over a 28.7 → 28.3 "regression."
- **The Session 6 formant_upmax regression is on a pure 440 Hz sine.** A 2-pole LPC model shifting a single resonance to 880 Hz is the *correct* behavior for the algorithm; the 16-pole baseline only "won" by accidentally producing richer spectral content on a degenerate input. Do not tune the algorithm to recover that score.
- **The current test corpus rewards the wrong things.** Sustained sines don't exercise transient handling, voiced/unvoiced detection, polyphony, or realistic formant structure. The single highest-leverage next step is not algorithm work — it's getting the planned vowel/drum/chord recordings into `test_files/`.

## Workflow rules of thumb

- **Defer algorithm tuning until the real-material recordings are in the suite.** Iterating on sines is overfitting.
- **Update [ARCHITECTURE.md](ARCHITECTURE.md)'s score table and [notes.txt](notes.txt) every session.** Future-you (or future-Claude) reads these to bootstrap context.
- **When proposing an algorithm change, predict the score impact first**, then run the batch test, then update the log with predicted-vs-actual. This is what makes the iteration loop honest.
- **Don't add SMS / sinusoidal+residual.** It's in the enum as a placeholder but [ARCHITECTURE.md](ARCHITECTURE.md) marks it skipped — LPC source-filter is closer to the confirmed V-Synth architecture.

## V-Synth ground truth (confirmed from recordings)

VariPhrase is a **source-filter vocoder**, not a spectral manipulator:
- F0 detection (ACF), per-frame voiced/unvoiced decision (ZCR + energy)
- LPC analysis (Levinson-Durbin, order 16, bandwidth-expanded λ=0.994)
- Formant shift = Laguerre root-finding on LPC polynomial → scale each pole's angle by `2^(st/12)` → reconstruct cascade biquads
- Excitation = band-limited 1/k sawtooth at (pitch-shifted) F0; harmonics up to Nyquist; phase-continuous across frames
- LPC synthesis filter + Hann-window OLA (synthesis hop = `kHopSize × timeStretch`)

Confirmed facts to preserve in any implementation:
- F0 is preserved exactly under formant shift (independent axes)
- Excitation is a synthetic 1/k sawtooth at the input F0 — *not* the original waveform
- Pitch shift moves excitation F0; formant filter is unchanged

## When in doubt

- The author treats this as a research project, not a product. Honesty about what's broken beats polish.
- Prefer fixing the measurement loop over chasing scores.
- If you find yourself about to write a "this should work" — instrument it and prove it instead.
