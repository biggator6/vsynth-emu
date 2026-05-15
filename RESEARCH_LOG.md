# Research Log — V-Synth VariPhrase Emulator

---

## Session 0 — Project Kickoff
**Date:** 2026-05-15  
**Phase:** Foundation  
**Model:** Claude Sonnet 4.6

### What Was Done
- Established project methodology: black-box reverse engineering via controlled test signal battery
- Designed repository structure and architectural principles
- Created Python analysis pipeline (compare.py, lpc.py, batch_test.py)
- Created JUCE plugin scaffold with stub VariphraseEngine
- Established scoring methodology for algorithm comparison

### Key Decisions
- `VariphraseEngine` will be isolated from JUCE — testable as standalone C++ or via Python
- Analysis pipeline built first, before any DSP implementation
- Algorithm progression: phase vocoder → sinusoidal+residual → LPC source-filter → hybrid

### Current Status
- [ ] Analysis pipeline: **built, needs validation with real files**
- [ ] JUCE plugin shell: **built, stub passthrough only**
- [ ] V-Synth test recordings: **NOT YET RECORDED** — this is the next bottleneck
- [ ] First algorithm iteration: **not started**

### Next Steps
1. **Record V-Synth test battery** (see test plan in ARCHITECTURE.md)
   - Priority order: sustained sine → held vowels → single transients → polyphonic
   - Document V-Synth patch settings for every recording
2. Validate `compare.py` using two copies of the same WAV (should show ~-inf null test)
3. Run baseline: passthrough plugin through batch_test.py to establish a floor

### Open Questions
- What internal V-Synth patch settings produce the "purest" VariPhrase behavior
  (minimal COSM, minimal effects chain) for cleaner reverse engineering?
- Does VariPhrase process pre-effects or post? (affects test signal design)

---

## Session Template (copy for each new session)

## Session N — [Title]
**Date:** YYYY-MM-DD  
**Phase:** [Foundation / Analysis / Algorithm / Polish]  
**Model:** [Sonnet / Opus]

### What Was Done

### Batch Test Results
| Test File | Null dBFS | Formant Score | Transient Score | Composite |
|-----------|-----------|---------------|-----------------|-----------|
|           |           |               |                 |           |

### Findings / Hypothesis Update

### Next Steps

---
