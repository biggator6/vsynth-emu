# vsynth-emu

A reverse-engineering effort to clone Roland V-Synth's **VariPhrase** DSP — the engine that lets you shift pitch, time, and formants as three independent axes — as a JUCE VST3/AU plugin.

This is a research project, not a product. It is six sessions in and currently scores **~28/100** against a 60-point similarity target on a small sine-wave test corpus. The next milestone is real-material recordings (voice, drums, chords) entering the test suite.

## How it works

The project is a tight black-box reverse-engineering loop:

1. Play a controlled signal (sine, sawtooth, vowel, etc.) into a real Roland V-Synth and record the output.
2. Run the same input through this plugin's offline renderer.
3. Null-test the two outputs, score the residual on several dimensions (formant accuracy, SNR, transient smearing), and write the result to a versioned report.
4. Form a hypothesis about what the residual reveals, change the algorithm, and re-run.

The DSP engine is intentionally JUCE-free C++ so the same code can be driven from a CLI binary that the Python test harness invokes via subprocess — no DAW required to iterate.

## Current algorithm

**Hybrid** routing:
- **Formant shift requested** → LPC source-filter model (sawtooth excitation + cascaded biquads from shifted LPC poles)
- **Otherwise** → Phase vocoder (pitch and time stretch)

V-Synth's real architecture has been confirmed (from recordings) to be a source-filter vocoder, not a spectral manipulator — see [ARCHITECTURE.md](ARCHITECTURE.md) for the details.

## Quick start

Clone JUCE alongside this directory:
```bash
git clone https://github.com/juce-framework/JUCE.git ../JUCE
```

Build the offline renderer (the binary the Python harness drives):
```bash
clang++ -std=c++17 -O2 -DOFFLINE_RENDERER_MAIN \
  plugin/Source/OfflineRenderer.cpp \
  plugin/Source/VariphraseEngine.cpp \
  plugin/Source/PhaseVocoder.cpp \
  plugin/Source/SourceFilterModel.cpp \
  -o analysis/variphrase_render
```

Render a single test case:
```bash
./analysis/variphrase_render \
  --input  analysis/test_files/passthrough/sine_440_formant_downmax.wav \
  --output /tmp/out.wav \
  --algo hybrid --formant -12
```

Run the full batch test (from the project root):
```bash
python3 analysis/batch_test.py \
  --ref-dir    analysis/test_files \
  --plugin-dir analysis/plugin_outputs/hybrid_vN \
  --output     analysis/results/hybrid_vN
```

Build the JUCE plugin (VST3 / AU / Standalone):
```bash
cmake -S plugin -B build && cmake --build build
```

## Repository layout

```
plugin/Source/      C++ DSP — JUCE-free engine + JUCE plugin wrapper
analysis/           Python test harness, reference recordings, scored reports
ARCHITECTURE.md     Algorithm status, per-case scores, routing table
RESEARCH_LOG.md     Session-by-session findings and hypotheses
RECORDING_GUIDE.md  V-Synth patch settings and file list for the next recording session
notes.txt           Current score snapshot and rebuild quick-reference
CLAUDE.md           Operational guide for AI agents
```

## For human readers — where to start

- New to the project? Read [ARCHITECTURE.md](ARCHITECTURE.md), then skim the latest entries in [RESEARCH_LOG.md](RESEARCH_LOG.md).
- Looking for the current score and quick commands? [notes.txt](notes.txt).
- Helping with the next recording session? [RECORDING_GUIDE.md](RECORDING_GUIDE.md).

## For AI agents — read this first

If you are an AI coding agent (Claude Code, Cursor, etc.) opening this repo:

1. **Read [CLAUDE.md](CLAUDE.md) before doing anything.** It contains operational rules, known critical bugs that silently corrupt scores, and workflow guardrails that have been learned the hard way.
2. **Then read [ARCHITECTURE.md](ARCHITECTURE.md), [RESEARCH_LOG.md](RESEARCH_LOG.md), and [notes.txt](notes.txt) in that order** — those are the project's working memory.
3. **Do not start a new top-level documentation file.** Persistent context belongs in `CLAUDE.md` (instructions for agents) or `CONTEXT.md` (longer-form findings and decisions). The four existing brain-files (ARCHITECTURE / RESEARCH_LOG / RECORDING_GUIDE / notes) are the only other docs that should grow.
4. **Update the score table in [ARCHITECTURE.md](ARCHITECTURE.md) and [notes.txt](notes.txt) at the end of every session** that changes the algorithm or test corpus. Future-you reads these to bootstrap context.
5. **Rebuild the offline renderer after editing any DSP source** before running the batch test. The harness will silently use the stale binary otherwise — multiple sessions have lost hours to this.

The single most important piece of judgment to bring: **honesty about what is broken beats polish.** Score deltas under 2–3 points are below measurement noise; do not celebrate or panic about them. Prefer fixing the measurement loop over chasing scores.

## Status

Active research; expect breakage. Not licensed for redistribution of any Roland-derived material (PCM ROM content, patches, etc.) — custom samples only.
