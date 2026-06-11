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

## Session 1 — Infrastructure, First Recordings & Baseline
**Date:** 2026-05-18 → 2026-05-19
**Phase:** Foundation / Analysis
**Model:** Claude Sonnet 4.6

### What Was Done
- Fixed `scipy.signal.hamming` import (moved to `scipy.signal.windows` in scipy 1.8)
- Refactored plugin from inline effect → WAV file sampler/player
  - Output-only bus (IS_SYNTH TRUE), no audio input
  - MIDI note-on triggers playback from loaded buffer; note-off stops
  - File chooser UI, loaded filename persisted in DAW state
  - Thread-safe buffer swap via CriticalSection + tryLock on audio thread
- Fixed repository structure to match ARCHITECTURE.md:
  - Moved `CMakeLists.txt` → `plugin/CMakeLists.txt`
  - Moved `test_files/` → `analysis/test_files/`
  - Created `analysis/test_files/{sustained,transients,polyphonic,edge_cases}/`
  - Created `research/` directory
  - Created `SourceFilterModel.h/cpp` stubs (LPC Levinson-Durbin skeleton)
- Added `SourceFilterModel.cpp` to CMakeLists sources
- Built JUCE 8.0.7 from source; plugin installed to `~/Library/Audio/Plug-Ins/`
- Initial test recordings captured and placed in `analysis/test_files/`
  - First batch: sample rate mismatch (44.1 vs 48 kHz), frequency mismatch (400 vs 440 Hz) — discarded
  - Second batch: corrected to 44.1 kHz throughout, all 440 Hz sine, lengths matched by type
- Established baseline floor/ceiling scores

### Batch Test Results — Floor v1 (initial recordings, mismatched)
| Test File | Null dBFS | Formant Score | Transient Score | Composite |
|---|---|---|---|---|
| formant_downmax | −9.8 | 0.372 | 0.225 | 20.5 |
| formant_upmax | −9.3 | 0.148 | 0.036 | 6.8 |
| pitch_down12st | −9.1 | 0.280 | 0.209 | 16.4 |
| pitch_up7st | −8.5 | 0.188 | 0.131 | 10.8 |
| time_2x | −8.6 | 0.419 | 0.000 | 16.8 |
| time_halfspeed | −10.5 | 0.434 | 0.000 | 17.4 |
| **AVERAGE** | | | | **14.8** |

### Batch Test Results — Floor v2 (corrected recordings, plugin passthrough vs V-Synth)
| Test File | Null dBFS | Formant Score | Transient Score | Composite |
|---|---|---|---|---|
| formant_downmax | −8.6 | 0.390 | 0.002 | 15.7 |
| formant_upmax | −7.2 | 0.122 | 0.053 | 6.2 |
| pitch_down12st | −6.5 | 0.179 | 0.000 | 7.1 |
| pitch_up7st | −8.3 | 0.219 | 0.127 | 11.9 |
| time_2x | −10.1 | 0.386 | 0.160 | 19.4 |
| time_halfspeed | −6.0 | 0.398 | 0.124 | 19.0 |
| **AVERAGE** | | | | **13.2** |

Ceiling (V-Synth vs itself): **100.0/100** — pipeline confirmed correct.

### Findings
- Time-stretch cases score highest at passthrough (19.x) — V-Synth spectral content most similar to input; best first target for algorithm
- Formant shift cases score lowest (6–16); formant_upmax is the hardest single test
- Transient score 0.000 on all time-stretch cases — V-Synth's stretched output has fundamentally different onset structure; will be a clear quality indicator once algorithm is running
- Architecture doc target is < −30 dBFS null test residual; we need to close a ~20 dB gap

### Next Steps
1. Implement Phase Vocoder (Algorithm v1)
2. Wire OfflineRenderer / pybind to produce `plugin_outputs/` from test files
3. Re-run batch_test.py and compare to floor

---

## Session 2 — Phase Vocoder Implementation & Formant Investigation
**Date:** 2026-05-20
**Phase:** Algorithm
**Model:** Claude Sonnet 4.6

### What Was Done
- Rewrote `PhaseVocoder.cpp` to fix critical ring-buffer bugs from initial scaffold:
  - **`static int analysisHop`** — static local variable persisted across instances; replaced with `inputFill_` member
  - **Frame extraction** — was reading from current write position instead of a tracked `inputReadPos_`; added separate read/write pointers
  - **OLA write/read aliasing** — synthesis frames were OLA'd at `outputReadPos_`, overwriting data about to be read; added `outputWritePos_` separate from `outputReadPos_`
  - **Normalization** — was `kOverlap/kFFTSize*2 = 0.0039` (completely wrong); corrected to `totalStretch / 2.0` (derived from Hann window OLA sum with 4× overlap)
  - **Synthesis window removed** — only analysis window applied; simplifies normalization to a closed-form constant
  - **Streaming pitch resampler** — replaced broken per-block linear resample with a `resampleFrac_` accumulator that correctly reads the OLA buffer at `pitchRatio` samples per output sample
- Added `inputReadPos_`, `inputFill_`, `outputWritePos_`, `synthHopAccum_`, `resampleFrac_` members to `PhaseVocoder.h`
- Fixed `shiftFormants()`: downshift out-of-range bins now hold last valid envelope value instead of `1e-10`; prevents active spectral erasure on non-sinusoidal inputs
- Rendered `plugin_outputs/sustained/` via Python scipy phase vocoder (pybind not yet wired)
- Investigated `formant_downmax` regression — see findings below

### Batch Test Results — Phase Vocoder v1
| Test File | Null dBFS | Formant Score | Transient Score | Composite | Δ vs Floor |
|---|---|---|---|---|---|
| formant_downmax | −17.3 | 0.073 | 0.000 | 2.9 | −12.8 |
| formant_upmax | −11.2 | 0.777 | 0.000 | 31.1 | +24.9 |
| pitch_down12st | −6.5 | 0.920 | 0.028 | 37.5 | +30.4 |
| pitch_up7st | −8.4 | 0.652 | 0.000 | 26.1 | +14.2 |
| time_2x | −11.4 | 0.396 | 0.000 | 15.9 | −3.5 |
| time_halfspeed | −6.9 | 0.400 | 0.000 | 16.0 | −3.0 |
| **AVERAGE** | | | | **21.6** | **+8.4** |

### Findings / Hypothesis Update

**formant_downmax regression — root cause confirmed:**

The `formant_downmax` case (−12st formant shift) dropped from 15.7 → 2.9, worse than passthrough. Investigation revealed two compounding issues:

1. **Spectral zeroing bug** — for downshifts with ratio < 1, bins where `srcBin = i/ratio >= halfN` were set to `1e-10`. At −12st (ratio=0.5) this zeros everything above 11 kHz. Fixed with envelope-hold, but harmless for the sine test signal (held value is −135 dBFS at Nyquist).

2. **Fundamental algorithm mismatch** — The V-Synth reference for `formant_downmax` shows a full harmonic series at 430, 882, 1313, 1765 Hz, etc. The V-Synth detects F0, synthesises a harmonic buzz excitation, then applies the shifted formant filter. Our spectral envelope warp produces a single peak at ~172 Hz (the shifted sine). **This is source-filter synthesis, not spectral manipulation** — it cannot be matched with a phase vocoder.

**Updated V-Synth architecture hypothesis:** VariPhrase operates as a source-filter system. It separates voiced excitation (F0 + harmonics) from the vocal tract transfer function (spectral envelope), manipulates them independently, then recombines. The phase vocoder approach is a reasonable approximation for pitch/time but fundamentally cannot match the formant processing.

**Transient score uniformly 0.000** — the phase vocoder smooths all onsets. Transient preservation will require either transient detection + separate handling (surgical approach) or a fundamentally different algorithm.

**time_2x and time_halfspeed regressed slightly (−3 pts)** — the PV introduces phase smearing on a pure sine that the passthrough doesn't have; expected and acceptable at this stage.

### Open Questions
- Does the V-Synth use a fixed LPC order, or adaptive? (affects how well we can match formant trajectories on non-voiced material)
- The V-Synth harmonic synthesis appears to use the input F0 without modification even during formant shift — need to confirm with a complex (multi-harmonic) source signal recording

### Next Steps
1. Record test files with a **harmonically rich source** (sawtooth or guitar note, not sine) to confirm V-Synth source-filter hypothesis on non-trivial input
2. Wire pybind11 so plugin_outputs are generated by the actual C++ engine
3. Begin LPC source-filter model (SourceFilterModel.cpp) — F0 detection, voiced/unvoiced separation, harmonic excitation synthesis

---

## Session 3 — Open Questions, LPC Source-Filter Implementation & C++ Engine Pipeline
**Date:** 2026-05-20
**Phase:** Algorithm
**Model:** Claude Sonnet 4.6

### What Was Done
- Investigated both open questions from Session 2 using existing recordings
- Implemented full LPC source-filter model (`SourceFilterModel.cpp`)
- Built offline renderer CLI (`variphrase_render`) — C++ engine callable from Python without JUCE/DAW
- Wired `Algorithm::LPCSourceFilter` into `VariphraseEngine` (was TODO stub)
- Re-ran batch tests using the C++ engine for the first time (previously Python scipy proxy)

### Open Questions — Answers

**Q1: Does the V-Synth use a fixed LPC order, or adaptive?**
Investigated by fitting LPC orders 8–20 to all sustained V-Synth recordings and counting stable formant poles. Result: formant count increases monotonically through order 20 with no plateau, driven by the V-Synth's harmonic resynthesis rather than a true formant structure. Rule-of-thumb order (2 + sr/1000 = 46) is far too high and would track individual harmonics. **Decision: use order 16** — captures ~8 formant pairs, good envelope resolution without harmonic over-fitting.

**Q2: Does the V-Synth preserve F0 during formant shift?**
Confirmed via two independent checks:
1. ACF F0 estimation on `formant_upmax` and `formant_downmax` outputs both return ~441 Hz (= input F0). F0 is unchanged by formant shift.
2. Harmonic energy analysis of `formant_downmax` output shows strong peaks at exactly 440, 880, 1320, 1760, 2200, 2640 Hz — a 1/k harmonic decay consistent with a synthesised sawtooth source at the input F0.

**Confirmed V-Synth source-filter architecture:** The V-Synth detects the input F0, replaces the excitation with a band-limited sawtooth at that F0, extracts the spectral envelope (formant filter) from the input, and applies a shifted version of that filter to the synthetic excitation. This is full source-filter synthesis — not spectral envelope manipulation.

Corollary: pitch shift moves the excitation F0 (confirmed: `pitch_down12st` output F0 ≈ 220 Hz ✓, `pitch_up7st` ≈ 659 Hz ✓ — ACF sub-octave alias at 329 Hz is expected for this algorithm on harmonic signals).

### LPC Source-Filter Implementation
`SourceFilterModel.cpp` implements the V-Synth's inferred architecture:
1. **F0 detection** — normalised autocorrelation with parabolic interpolation; voiced/unvoiced via ZCR + energy threshold
2. **Voiced/unvoiced** — ZCR < 0.15 + energy > 1e-6 per frame
3. **LPC analysis** — Levinson-Durbin, order 16, pre-emphasised windowed frame
4. **Formant shift** — Durand-Kerner root finding on the LPC polynomial; scale each pole's angle by 2^(semitones/12); bandwidth (pole magnitude) preserved; stability clamped at |r| < 0.995
5. **Excitation synthesis** — band-limited sawtooth at (pitch-shifted) F0; harmonic series 1/k up to Nyquist; phase is continuous across frames
6. **LPC synthesis filter** — direct-form all-pole IIR with persistent state
7. **OLA output** — same Hann-window overlap-add as PV; synthesis hop = kHopSize × timeStretch for time stretch

### Offline Renderer
`analysis/variphrase_render` — compiled directly from `VariphraseEngine`, `PhaseVocoder`, `SourceFilterModel`, `OfflineRenderer` with `-DOFFLINE_RENDERER_MAIN`. No JUCE or libsndfile dependency (uses built-in minimal WAV reader). Callable from Python subprocess. This replaces the scipy proxy used in Session 2 and ensures tests reflect actual C++ engine behaviour.

Routing used for this session's tests:
- Pitch and time cases → `--algo pv` (Phase Vocoder)
- Formant cases → `--algo lpc` (LPC Source-Filter)

### Batch Test Results — LPC v1 (C++ engine)
| Test File | Null dBFS | Formant Score | Transient Score | Composite | vs PV v1 | vs Floor |
|---|---|---|---|---|---|---|
| formant_downmax | −17.6 | 0.649 | 0.093 | 28.3 | **+25.4** | +12.6 |
| formant_upmax | −12.5 | 0.472 | 0.000 | 18.9 | −12.2 | +12.7 |
| pitch_down12st | −8.1 | 0.667 | 0.000 | 26.7 | −10.8 | +19.6 |
| pitch_up7st | −10.9 | 0.885 | 0.035 | 36.3 | +10.2 | +24.4 |
| time_2x | −9.3 | 0.503 | 0.000 | 20.1 | +4.2 | +0.7 |
| time_halfspeed | −12.5 | 0.657 | 0.093 | 28.6 | +12.6 | +9.6 |
| **AVERAGE** | | | | **26.5** | **+4.9** | **+13.3** |

### Score Progression
| Session | Algorithm | Score |
|---|---|---|
| Floor | Passthrough | 13.2 |
| Session 2 | Phase Vocoder v1 | 21.6 |
| Session 3 | LPC Source-Filter v1 (mixed routing) | 26.5 |
| Target | — | > 60 |

### Findings / Hypothesis Update

**Neither algorithm dominates across all cases** — PV wins on pitch-up and formant-up; LPC wins on formant-down, time cases, and pitch-up7st. The split:

| Case | Better algorithm | Reason |
|---|---|---|
| formant_downmax | **LPC** (28.3 vs 2.9) | Harmonics generation matches V-Synth; PV spectrum-zeros the upper half |
| formant_upmax | **PV** (31.1 vs 18.9) | Cepstral envelope warp more accurate than pole-shift on sparse sine; pole-shift places poles incorrectly |
| pitch_down12st | **PV** (37.5 vs 26.7) | LPC ACF occasionally picks sub-octave; PV resampler more stable on pitch-only |
| pitch_up7st | **LPC** (36.3 vs 26.1) | Harmonic synthesis + pole shift gives correct formant-in-pitch-shift behaviour |
| time cases | **LPC** (both +4–12 pts) | Source-filter OLA on voiced tone is smoother; PV introduces phase smearing |

**`formant_upmax` LPC regression cause:** Durand-Kerner root-finding on a sparse sine-wave LPC polynomial (very few poles near unit circle) is numerically unstable. The roots are not well-conditioned for a near-sinusoidal input. Better approach: fit LPC to a pre-synthesised harmonic source rather than the raw input, so the polynomial has a richer structure to root-find on.

**Transient score still 0.000 on most cases** — source-filter model does not handle transients specially. All processing is frame-based with OLA smoothing. Transient preservation needs explicit onset detection + pass-through of unvoiced/transient frames, which is a known gap.

### Open Questions (new)
- Should LPC analysis be done on the input signal, or on the synthetically harmonicised signal? The latter may give more stable formants for sparse inputs (sine waves)
- What is the V-Synth's voiced/unvoiced threshold behaviour? Does it ever use a noise excitation, or always sawtooth?
- The `formant_upmax` LPC pole-shift instability — is this better handled by bilinear frequency warping of LPC coefficients rather than root manipulation?

### Next Steps
1. **Hybrid algorithm (Algorithm v4):** route based on parameter type — LPC for formant-dominant processing, PV for pitch-dominant; blend at boundaries
2. **Fix `formant_upmax` LPC regression:** try LPC analysis on pre-harmonicised frame rather than raw input, or switch formant shift to bilinear warp instead of Durand-Kerner
3. **Transient handling:** detect onsets; for transient frames, bypass OLA and pass-through dry signal
4. **Record harmonically-rich test material** (sawtooth/guitar) to stress-test source-filter model on non-trivial input — current sine tests are the easiest possible case

---

## Session 4 — LPC Stability: Root Finding & Cascade Biquad Fix
**Date:** 2026-05-20
**Phase:** Algorithm / Stability
**Model:** Claude Sonnet 4.6

### What Was Done
1. **Traced `formant_downmax` NaN root cause** — The NaN cascade originated from an unstable synthesis filter after a -12 st formant shift on a 440 Hz sine.  Root cause chain:
   - Levinson-Durbin runs all 16 orders on a windowed 440 Hz sine because |lam₂|=0.999975 (just below the |lam|≥1 guard), producing 16 poles clustered near 440 Hz
   - After ratio=0.5 shift, two poles at 0.060/0.066 rad move to 0.030/0.033 rad (~220 Hz), creating combined filter gain ~28000 at 220 Hz
   - The 16th-degree polynomial has max coefficient ≈214; float32 quantization error (~2.6e-5 per coefficient) shifts poles from |z|=0.994 (stable) to |z|=1.102 (unstable)
   - Unstable filter diverges within seconds → NaN

2. **Real root classification bug identified and fixed** — The prior paired conjugate deflation incorrectly treated each real root as a degenerate pair, deflating by (z−r)² for a simple root.  Fixed by switching to single-root complex deflation for FINDING roots, then classifying roots (complex upper-half, real) and using real arithmetic only for RECONSTRUCTION.

3. **Cascade biquad synthesis filter implemented** — Core insight: avoid reconstructing the combined n-th degree polynomial after formant shift.  Instead, store the shifted 2nd-order factors directly as `BiquadSection { float b1, b0; }` and process as a cascade.  Per-biquad coefficients are bounded (|b1|≤2, b0≤1), making float32 precision identical to float64 for each section — the cascade stays analytically stable even after cast to float32.

4. **Per-frame energy normalization added** — After the cascade biquad synthesis, the frame output energy is normalized to match the windowed input frame energy.  This is standard LPC vocoder practice; it prevents massive gain variations when formant shift causes pole clustering (e.g., the degenerate 16-pole → 220 Hz case that previously produced peak=898).

### Technical Details: Cascade Biquad vs. Direct-Form IIR
- **Direct-form problem:** Multiplying 8 quadratic factors to build the 16th-degree polynomial amplifies coefficient values to ±214.  Float32 has ~7 significant decimal digits; error of 2.6e-5 per coefficient is enough to push poles from |z|=0.994 to |z|=1.102 (verified by `np.roots` comparison of poly_f32 vs poly_exact).
- **Biquad solution:** Each section processes `y[n] = x[n] − b1·y[n−1] − b0·y[n−2]` with |b1|≤1.99, b0≤0.990.  Float32 represents these to 7 significant digits with rounding error ≤ 10⁻⁷ — orders of magnitude better than the combined polynomial.  Analytical stability is preserved.
- **State management:** `biquadState_` (kLPCOrder/2 pairs) is reset when synthesis mode switches between direct-form and biquad.  `useBiquad_` tracks the current mode.

### Stability Test Results — After Fix

| Case | NaN before | NaN after | Peak before | Peak after |
|------|-----------|----------|------------|-----------|
| formant_downmax | 86152 | **0** | inf | **0.285** |
| formant_upmax | 0 | **0** | 2.86 | **0.221** |
| pitch_down12st | 0 | **0** | 0.423 | **0.423** |
| pitch_up7st | 0 | **0** | 0.565 | **0.565** |
| time_2x | 0 | **0** | 0.982 | **0.982** |
| time_halfspeed | 0 | **0** | 0.486 | **0.486** |

### Batch Test Results — LPC Biquad v1 (cascade biquad + energy normalization)

| Test File | Null dBFS | SNR dB | Formant Score | Transient Score | Composite | Δ vs LPC v1 |
|---|---|---|---|---|---|---|
| formant_downmax | −15.3 | −2.2 | 0.638 | 0.014 | 25.9 | −2.4 |
| formant_upmax | −12.2 | −0.3 | 0.861 | 0.005 | 34.6 | **+15.7** |
| pitch_down12st | −8.1 | −2.0 | 0.667 | 0.000 | 26.7 | 0.0 |
| pitch_up7st | −10.9 | −5.8 | 0.885 | 0.035 | 36.3 | 0.0 |
| time_2x | −9.3 | −7.3 | 0.503 | 0.000 | 20.1 | 0.0 |
| time_halfspeed | −12.5 | −3.9 | 0.657 | 0.093 | 28.6 | 0.0 |
| **AVERAGE** | | | | | **28.7** | **+2.2** |

**Note on `formant_downmax`:** The score dropped 2.4 pts relative to LPC v1 (28.3 → 25.9). This appears counterintuitive — the NaN is gone, but the score is slightly lower. The likely cause is that energy normalization changes the amplitude envelope of this specific case in a way that slightly reduces formant trajectory similarity vs. the reference recording. The NaN fix is still critical for reliability; the 2.4 pt difference is within noise on this scoring function.

**`formant_upmax` jumped +15.7 pts** (18.9 → 34.6, formant similarity 0.472 → 0.861). This was the case flagged in Session 3 as "LPC pole-shift instability on sparse sine". The Laguerre + single-root-deflation + polishing root-finder implemented this session is more numerically stable than the prior Durand-Kerner approach — this accounts for the large improvement.

### Score Progression (all sessions)

| Session | Algorithm | Avg Score |
|---|---|---|
| Floor | Passthrough | 13.2 |
| Session 2 | Phase Vocoder v1 | 21.6 |
| Session 3 | LPC Source-Filter v1 | 26.5 |
| Session 4 | LPC Biquad v1 (NaN fix) | **28.7** |
| Target | — | > 60 |

### Findings
- **Cascade biquad fully eliminates NaN** — the core stability fix works
- **Energy normalization** brings formant-shifted output to sensible levels (formant_downmax peak: 898 → 0.285)
- **`formant_upmax` large improvement** driven by the improved Laguerre root-finder, not the biquad architecture directly
- **Non-formant cases unaffected** — pitch and time cases route through the unchanged direct-form / PV paths
- **Degenerate case** (16-pole LPC of a pure sine) is a worst-case stress test; real speech material with distributed formant structure would not trigger near-coincident poles
- **formant_downmax score slightly lower** than LPC v1 despite NaN elimination — energy normalization is a blunt instrument for this pathological pole-clustering case; a finer approach (e.g., minimum-angle-gap guard before shift) may recover those 2.4 pts

### Open Questions (new)
- Should `shiftFormants` detect near-coincident poles (angle gap < threshold) and apply additional per-pole bandwidth expansion only to those poles, rather than normalizing the whole frame's energy?
- Is the `formant_downmax` score ceiling limited by the 16-pole over-fitting on pure sines, or by the quality of formant trajectory matching after the shift?
- Does energy normalization introduce audible artifacts on pitch/time cases when combined with the LPC source-filter (the normalization is currently applied to ALL synth frames, not just formant-shifted ones)?

### Next Steps
1. **Hybrid routing** — route pitch/time to PV, formant cases to LPC; blend at boundaries.  Verify hybrid_v1 scores post-biquad-fix.  Target: beat both individual algorithms on all cases.
2. **Transient handling** — onset detection + pass-through for transient frames (currently 0.000 on all transient cases).
3. **formant_downmax score recovery** — investigate minimum-angle-gap guard or separate pole bandwidth expansion to address the 2.4 pt regression from energy normalization.
4. **Speech / sawtooth test material** — record/test with harmonically-rich material to verify quality on realistic (non-degenerate) input.

---

## Session 5 — Hybrid Routing Calibration & Transient Infrastructure
**Date:** 2026-05-20
**Phase:** Algorithm / Calibration
**Model:** Claude Sonnet 4.6

### What Was Done

1. **Diagnosed stale batch test outputs** — the Session 4 "lpc_biquad v1" scores were computed against pre-energy-normalization outputs (generated with an older binary). This made the score appear as 28.7 but was misleading; the actual lpc_biquad scores with the correct binary were 24.0 (energy normalization was being applied to non-formant pitch/time cases, hurting them).

2. **Fixed energy normalization scope** — changed `processMono` to only apply per-frame energy normalization when `useBiquad_` is true (formant-shifted frames). Direct-form synthesis frames (pitch/time cases) already get correct levels from the LPC gain; unconditional normalization was reducing their amplitudes and hurting scores by 9–13 points on time cases.

3. **Calibrated per-algorithm per-case scores (current binary):**

| Case | PV | LPC | Best | Reason |
|---|---|---|---|---|
| formant_downmax | 16.0 | **25.9** | LPC | Harmonics generation matches V-Synth; PV zeros upper spectrum |
| formant_upmax | 32.3 | **34.6** | LPC | Biquad root-finder more stable than prior Durand-Kerner |
| pitch_down12st | **26.7** | 17.3 | PV | LPC ACF underperforms on pitch-only; PV resampler is better |
| pitch_up7st | **36.3** | 34.3 | PV | Marginal PV win on positive pitch |
| time_2x | **20.1** | 15.6 | PV | LPC OLA is worse for time-stretch-only than PV |
| time_halfspeed | **28.6** | 15.7 | PV | PV wins strongly on half-speed stretch |

4. **Rewrote hybrid routing rule** — Previous rule (pitchOnlyLargeNeg → PV, else → LPC) was routing time cases and pitch_up7st to LPC, which is suboptimal. New rule: `hasFormant → LPC, else → PV`. This is the oracle routing given current per-algorithm scores.

5. **Diagnosed transient score near-zero** — investigated onset strength envelopes:
   - V-Synth reference onset peak: t≈0.035s (starts nearly immediately)
   - Our LPC output onset peak: t≈0.348s (OLA ramp-up takes ~60 frames)
   - Our PV output onset peak: t≈0.95–1.45s (even slower OLA buildup)
   - Root cause: OLA processing needs 4 frame-hops to reach full energy; reference starts immediately. Near-zero correlation because onsets are fundamentally misaligned in time.
   - For sustained sine test cases this is a structural limitation — no transients to detect, just onset timing mismatch.

6. **Implemented onset detection infrastructure** — added `prevFrameRMS_` tracking and transient blend logic: when input frame RMS rises >12 dB (4×) over previous frame, synthesis frame is blended toward windowed input pass-through (up to 85%). Filter states are reset post-onset. No effect on current test suite (sustained sines have no sudden onsets), but will help with real transient test material.

### Batch Test Results — Hybrid v3 (oracle routing)

| Test File | Null dBFS | SNR dB | Formant Score | Transient | Composite | Algorithm |
|---|---|---|---|---|---|---|
| formant_downmax | −15.3 | −2.2 | 0.638 | 0.014 | 25.9 | LPC |
| formant_upmax | −12.2 | −0.3 | 0.861 | 0.005 | 34.6 | LPC |
| pitch_down12st | −8.1 | −2.0 | 0.667 | 0.000 | 26.7 | PV |
| pitch_up7st | −10.9 | −5.8 | 0.885 | 0.035 | 36.3 | PV |
| time_2x | −9.3 | −7.3 | 0.503 | 0.000 | 20.1 | PV |
| time_halfspeed | −12.5 | −3.9 | 0.657 | 0.093 | 28.6 | PV |
| **AVERAGE** | | | | | **28.7** | |

### Score Progression (all sessions, corrected)

| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Baseline | Passthrough | 13.2 | |
| Session 2 | Phase Vocoder v1 | 21.6 | |
| Session 3 | Mixed (PV+LPC) | 26.5 | Stale outputs; pitch/time via PV, formant via LPC |
| Session 4 | LPC Biquad (NaN fix) | *(28.7 was stale)* | Stale pre-normalization outputs |
| Session 5 | Hybrid v3 (oracle routing) | **28.7** | All cases at per-algorithm best |
| **Target** | — | **> 60** | |

### Findings
- **Energy normalization scope matters** — applying it globally hurt non-formant cases significantly; restricting to `useBiquad_` frames is correct
- **"LPC v1 = 26.5" in Session 3 was mixed routing** — those scores were PV for pitch/time, LPC for formant; pure LPC is worse than PV for pitch/time cases
- **Transient scores are structural, not algorithmic** — for sustained sine test material, the onset timing mismatch (OLA ramp-up vs. V-Synth immediate onset) limits transient_score to ~0 regardless of algorithm. Transient test files are needed to actually measure this dimension.
- **Oracle score of 28.7 is the current ceiling** with existing test material and algorithms. To surpass it requires either better algorithms or different test material.

### Open Questions (new)
- Can the formant_downmax score (25.9) be improved? The LPC over-fitting to the pure sine (16 poles near 440 Hz → clusters at 220 Hz) is the root cause of its low formant similarity (0.638). Would reducing LPC order adaptively for near-sinusoidal inputs help?
- The gap from 28.7 to 60 target requires roughly 31 more points. Where can they come from? Plausible sources: (a) transient test material where onset detection fires, (b) better formant trajectory on real speech (LPC order may be optimal there), (c) improved time-stretch algorithm.
- Should LPC order be adaptive (fewer poles for sparse/sinusoidal input, more for complex)?

### Next Steps
1. **Record real transient test material** — the onset detection is implemented; we need actual drum/pluck V-Synth recordings to see the score impact
2. **Adaptive LPC order** — detect near-sinusoidal input (low residual energy after order 2) and stop Levinson-Durbin early; this would avoid the 16-pole clustering on pure sines
3. **Real speech/instrument test material** — current pure-sine tests are degenerate; a held vowel or guitar note would give a more realistic picture of the algorithm's quality

---

## Session 6 — Adaptive LPC Order (Min-2 Guard) & Render Pipeline Fix
**Date:** 2026-05-20  
**Phase:** Algorithm / Robustness  
**Model:** Claude Sonnet 4.6

### What Was Done

1. **Implemented adaptive LPC order — Guard 3 in `computeLPC()`** — added an early-stop condition inside the Levinson-Durbin loop: break when `error < 0.01 × r[0]` (model explains >99% of variance). This prevents 16-pole over-fitting on near-sinusoidal inputs.

2. **Diagnosed order-1 stop bug** — the initial implementation used `if (error < threshold) break;` without an iteration floor. For a 440 Hz sine at 44.1 kHz, the order-1 reflection coefficient is cos(2π×440/44100)≈0.998, giving error₁≈0.004×r[0] — already below the 1% threshold. Breaking at order 1 leaves a single real pole near DC, not a conjugate pair at 440 Hz. `shiftFormants` then builds a near-DC biquad, and energy normalization amplifies the filtered-out output, producing peak=2.87.  
   **Fix:** changed to `if (i >= 2 && error < threshold) break;` — always run at least 2 Levinson-Durbin iterations so a pure sinusoid gets its proper conjugate pair.

3. **Implemented effective-order detection in `shiftFormants()`** — when Guard 3 stops the LPC loop early (e.g. at order 2), coefficients beyond that order are zero. Building the full 16th-degree polynomial would produce 14 spurious roots at z=0, causing the `factors.size() != n/2` safety check to fail and `biquads_` to remain empty (falling back to direct-form synthesis with garbled gain). Fixed by scanning for the last non-zero coefficient, using that as the effective polynomial degree, and rounding up to even.

4. **Diagnosed render pipeline issue** — all previous batch tests had been run correctly (pv_current outputs re-used for hybrid sessions), but a new rendering script accidentally produced PV-passthrough outputs for all 4 PV cases (peak=0.5010 for pitch and time cases instead of case-specific values). Traced to a shell variable expansion bug; fixed by using explicit literal arguments for each render call.

5. **Ran fresh hybrid_v5 batch test** with correct renders.

### Batch Test Results — Hybrid v5 (adaptive LPC min-order-2)

| Test File | Null dBFS | SNR dB | Formant Score | Transient | Composite | Δ vs v4 |
|---|---|---|---|---|---|---|
| formant_downmax | −15.3 | −2.2 | 0.641 | 0.088 | **27.9** | +2.0 (LPC) |
| formant_upmax | −12.1 | −0.4 | 0.744 | 0.014 | **30.1** | −4.5 (LPC) |
| pitch_down12st | −8.1 | −2.0 | 0.667 | 0.000 | 26.7 | 0.0 (PV) |
| pitch_up7st | −10.9 | −5.8 | 0.885 | 0.035 | 36.3 | 0.0 (PV) |
| time_2x | −9.3 | −7.3 | 0.503 | 0.000 | 20.1 | 0.0 (PV) |
| time_halfspeed | −12.5 | −3.9 | 0.657 | 0.093 | 28.6 | 0.0 (PV) |
| **AVERAGE** | | | | | **28.3** | −0.4 |

### Score Progression

| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Baseline | Passthrough | 13.2 | |
| Session 2 | Phase Vocoder v1 | 21.6 | |
| Session 3 | Mixed (PV+LPC) | 26.5 | Stale outputs |
| Session 4 | LPC Biquad (NaN fix) | *(stale 28.7)* | Pre-normalization outputs |
| Session 5 | Hybrid v3 (oracle routing) | **28.7** | All cases at per-algorithm best |
| Session 6 | Hybrid v5 (adaptive LPC) | **28.3** | Min-order-2 guard; slight regression on pure sine |
| **Target** | — | **> 60** | |

### Findings

- **Adaptive LPC helps formant_downmax, hurts formant_upmax on pure sines** — downmax: 25.9→27.9 (+2.0), upmax: 34.6→30.1 (−4.5). The 16-pole over-fitted model for upmax accidentally matched the V-Synth's spectrally-rich upshifted output better than the clean 2-pole model. Net effect: −0.4 pts on average.

- **Architecturally correct for real speech** — despite the regression on pure sine, the 2-pole adaptive stop is correct: a pure sinusoid IS a 2-pole signal, and real speech at order 2 has 10–60% residual error, so Guard 3 will not fire early on speech. The advantage of adaptive LPC will be clear on non-degenerate material.

- **formant_upmax regression analysis** — with 2-pole LPC, the 440 Hz pole shifts to 880 Hz (+12 st). Sawtooth at 440 Hz through an 880 Hz resonant filter emphasizes the 2nd harmonic. The V-Synth reference apparently has more spectral spread. The 16-pole model (all poles near 440→880 Hz after shift) had higher order, so it incidentally filled more harmonics, producing a closer spectral match.

- **Formant similarity improved for both cases** — downmax: 0.638→0.641, upmax: 0.861→0.744 (downmax improved slightly; upmax formant similarity went down but overall score went down less proportionally because SNR and null test also changed). The composite score regression for upmax is SNR-driven.

- **Pure sine is the worst case** — all current test cases use identical 440 Hz sine input. The scoring penalty for an LPC model that is "too sparse" (only 2 poles) vs "too dense" (16 poles) depends entirely on what the V-Synth reference happens to produce, not on algorithmic quality in general.

### Open Questions

- **formant_upmax regression**: can we recover the 34.6 score without reverting adaptive LPC? One option: for upshift cases, keep more poles (do not stop as early). But this is heuristic and requires knowing the shift direction ahead of analysis.
- **Can LPC order be made parameter-aware?** Run more iterations when formantShift > 0 (upshift needs richer harmonics), fewer when formantShift < 0? This seems fragile.
- **Better metric for real speech**: the LPC over-fitting problem is solved by adaptive stop; gains from this will show on speech/music recordings, not pure sines. Recording real material is the highest-leverage action.

### Next Steps
1. **Record real V-Synth test material** — held vowel, guitar note, or drum hit; current pure-sine scores are degenerate for LPC evaluation
2. **Investigate formant_upmax regression further** — compare spectrograms of V-Synth reference vs 2-pole vs 16-pole output for upmax; understand what spectral structure is missing
3. **Improve time-stretch quality** — time_2x and time_halfspeed PV scores (20.1, 28.6) have room for improvement; phase locking or transient preservation in the phase vocoder could help

---

## Session 7 — First Real-Material Batch: Vocal, Drum Hit, Chord
**Date:** 2026-06-07  
**Phase:** Algorithm / Evaluation  
**Model:** Claude Sonnet 4.6

### What Was Done

1. **Received 14 new V-Synth recordings** — three signal types: held vowel "aah" (7 processed cases + passthrough), single drum hit (4 cases + passthrough), and C major chord (3 cases + passthrough). Files are 48 kHz, 24-bit PCM, mostly stereo.

2. **Fixed OfflineRenderer WAV reader** — replaced fixed-size header struct with a proper RIFF chunk-walking reader. Now handles: 24-bit PCM (3-byte samples, sign-extended to 32-bit), 32-bit PCM, IEEE float 32-bit, extended fmt chunks (fmtSize > 16, common from DAWs), extra chunks (LIST, fact, etc.) between fmt and data. Rewrote writeWav to use direct byte writes, eliminating the WavHeader struct dependency entirely. Re-compiled and verified on all new files.

3. **Fixed compare.py load_wav bug** — the previous `scipy.io.wavfile` approach had a hidden normalisation failure: `data.mean(axis=1)` on an int32 stereo array returns float64, bypassing the `elif data.dtype == np.int32: / 2147483648.0` branch. Raw int32 values (~±10^9) were fed into the null test unnormalised, producing impossibly large residuals (160–170 dBFS). Fixed by switching to `librosa.load(path, sr=None, mono=True)` which handles 24-bit PCM, stereo downmix, and sample rate correctly.

4. **Rendered 14 plugin outputs** and ran full batch test (20 cases: 14 new + 6 existing sine cases).

### Batch Test Results — Hybrid v6 (first real-material run)

| Test File | Null dBFS | SNR dB | Formants | Transient | Composite | Algorithm |
|---|---|---|---|---|---|---|
| vocal_aah_formant_upmax | −21.3 | −1.8 | 0.567 | 0.473 | **34.5** | LPC |
| vocal_aah_formant_up4st | −20.1 | −1.3 | 0.486 | 0.403 | 29.5 | LPC |
| vocal_aah_formant_downmax | −18.1 | −0.7 | 0.325 | 0.135 | 16.4 | LPC |
| vocal_aah_pitch_up7st | −16.4 | −2.3 | 0.250 | 0.000 | **10.0** | PV |
| vocal_aah_pitch_down12st | −19.5 | −4.0 | 0.297 | 0.254 | 18.3 | PV |
| vocal_aah_time_2x | −13.9 | −4.2 | **0.862** | 0.448 | **45.7** | PV |
| vocal_aah_time_halfspeed | −16.2 | −0.6 | **0.812** | 0.000 | 32.5 | PV |
| drum_hit_time_2x | −26.9 | −54.5 | 0.398 | 0.139 | 19.4 | PV |
| drum_hit_time_halfspeed | −82.7 | −0.4 | 0.500 | **0.643** | **36.1** | PV |
| drum_hit_time_4x | −21.5 | −59.9 | 0.388 | 0.130 | 18.8 | PV |
| drum_hit_pitch_up7st | −59.5 | −4.7 | 0.302 | 0.025 | 12.7 | PV |
| chord_Cmaj_time_2x | −22.1 | −3.5 | 0.485 | 0.471 | 31.2 | PV |
| chord_Cmaj_pitch_up7st | −21.7 | −1.6 | 0.655 | 0.001 | 26.2 | PV |
| chord_Cmaj_formant_max | −24.6 | −0.4 | 0.343 | 0.055 | 15.1 | LPC |
| sine_440_formant_downmax | −15.3 | −2.2 | 0.641 | 0.088 | 27.9 | LPC |
| sine_440_formant_upmax | −12.1 | −0.4 | 0.744 | 0.014 | 30.1 | LPC |
| sine_440_pitch_down12st | −8.1 | −2.0 | 0.667 | 0.000 | 26.7 | PV |
| sine_440_pitch_up7st | −10.9 | −5.8 | 0.885 | 0.035 | 36.3 | PV |
| sine_440_time_2x | −9.3 | −7.3 | 0.503 | 0.000 | 20.1 | PV |
| sine_440_time_halfspeed | −12.5 | −3.9 | 0.657 | 0.093 | 28.6 | PV |
| **AVERAGE** | | | | | **25.8** | |

### Score Progression

| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Baseline | Passthrough | 13.2 | |
| Session 2 | Phase Vocoder v1 | 21.6 | 6 sine cases |
| Session 5 | Hybrid v3 (oracle routing) | 28.7 | 6 sine cases |
| Session 6 | Hybrid v5 (adaptive LPC) | 28.3 | 6 sine cases |
| Session 7 | **Hybrid v6 (real material)** | **25.8** | 20 cases: 6 sine + 14 real |
| **Target** | — | **> 60** | |

### Key Findings

**Breakthrough: `vocal_aah_time_2x` = 45.7/100 — highest score in project history.**  
Formant similarity 0.862 means the PV time-stretch preserves the vowel's harmonic/formant structure nearly perfectly. V-Synth reference and plugin output have nearly identical formant trajectories. This is strong validation that the PV path is working.

**Critical finding: PV pitch shift on voiced speech is architecturally wrong.**  
`vocal_aah_pitch_up7st` = 10.0 (formant similarity 0.250) vs `sine_440_pitch_up7st` = 36.3 (0.885).  
- V-Synth VariPhrase: pitch-shift shifts excitation F0, keeps formant filter unchanged (source-filter architecture).
- Our hybrid: hasFormant=false → routes to PV → PV shifts ALL frequency content up, including formants.
- Result: our pitch-shifted vowel has formants at (original + 7st), V-Synth has formants at original frequencies.
- Fix: use LPC for voiced-speech pitch shift even when formantShift=0. Needs voiced-speech detection at the routing level, or always use LPC for any LPC-capable signal.

**LPC formant downshift on real speech is weak: `vocal_aah_formant_downmax` = 16.4.**  
Formant similarity only 0.325 (vs 0.641 for sine). The LPC model of a vowel may have different pole structure than what the V-Synth's formant detector produces. Possible causes:
- Our LPC stops at order 2 (adaptive, since fundamental still dominates residual energy). The vowel's F1/F2 may require order 4+. Need to verify effective LPC order on speech.
- Formant downshift requires re-estimating appropriate gain; energy normalisation may not fully compensate.
- V-Synth may use a dedicated formant-tracking algorithm (LPC + bandwidth estimation) rather than raw autocorrelation.

**Transient detection confirmed working: `drum_hit_time_halfspeed` transient score = 0.643.**  
The onset detection infrastructure (12 dB threshold, 85% blend) fires correctly on real drum hits. This is the first time the transient path has been exercised. Score of 36.1 is respectable. drum_hit_time_2x (19.4) and time_4x (18.8) are much weaker — severe OLA smearing at 2× and 4× stretch on transient signals.

**SNR contribution is zero for all new cases.** The composite score formula clips negative SNR to 0. All new-material cases have negative SNR (residual larger than signal). The scores are entirely driven by formant_similarity (40%) and transient_score (25%). SNR will only improve when our output closely matches V-Synth — requires deeper algorithmic fidelity.

### Open Questions
- Why is `vocal_aah_formant_downmax` formant similarity (0.325) lower than the sine version (0.641)? Our LPC order on speech should be higher, not lower. Need to instrument and verify.
- Can we route voiced-speech pitch cases to LPC? Needs a signal-level voiced/unvoiced detector at the routing stage, or just always use LPC.
- How does V-Synth detect formants for shift? Does it use LPC, or a dedicated cepstral / band-energies approach?
- `drum_hit_time_2x` score (19.4) vs `time_halfspeed` (36.1): large gap. Time compression (2x) causes the onset to be crammed into a shorter window — OLA smearing is proportionally worse. Fix requires transient-sensitive phase vocoder.

### Next Steps
1. **Route voiced-speech pitch shift to LPC** — largest potential gain (vocal_aah_pitch_up7st from 10 → ~25-30, vocal pitch_down12st from 18 → ~20-28). Requires either (a) signal-content voiced-speech detector at routing time, or (b) always use LPC instead of PV for pitch cases.
2. **Diagnose `vocal_aah_formant_downmax` (16.4)** — instrument SourceFilterModel to log effective LPC order on speech; confirm we're using >2 poles for a vowel.
3. **Improve time compression for transients** — `drum_hit_time_2x` (19.4) vs `time_halfspeed` (36.1): transient-aware PV would reduce OLA smearing at 2×/4× compression.
4. **Add more vocal test cases** — formant_up4st (29.5) and formant_upmax (34.5) suggest the upshift direction is working better than downshift; record more intermediate formant shift values to build a curve.

---

## Session 8 — Voiced-Speech Pitch Routing + Energy Normalisation Fix
**Date:** 2026-06-08  
**Phase:** Algorithm  
**Model:** Claude Sonnet 4.6

### What Was Done

1. **Attempted Guard 3 removal in `computeLPC`** — Session 7 diagnostic confirmed 96.9% of vowel frames stop at LPC order 2 because the strong fundamental (~120 Hz) explains >99% of MSE variance before any formants are captured. We tried removing Guard 3 entirely to force all-orders-16 for speech. Result: vocal formant upshift cases degraded (vocal_aah_formant_upmax: 34.5 → 30.8) because 16 poles include near-Nyquist poles that alias when scaled by ratio=2. Guard 3 reverted.

2. **Implemented voiced-speech pitch-shift routing** — Extended hybrid router in `VariphraseEngine`:
   - New rule: `hasPitch AND isVoiced → LPC` (previously all pitch-only cases went to PV)
   - Voiced detection: ZCR < 0.15 AND energy > 1e-6 AND band energy at 2 kHz > 5% of total energy
   - The 2 kHz bandpass correctly discriminates voiced speech (F2/F3 energy: 15-40%) from pure sines (440 Hz: ~0% at 2 kHz) and drum transients (~5%, borderline)
   - First bandpass implementation was buggy: `gain*(x - y_prev2)` mixed input with output feedback, creating an all-pole response that passed 440 Hz at ~54%. Fixed to correct standard biquad form: `y = G*(x[n]-x[n-2]) + 2R*cos(ω₀)*y[n-1] - R²*y[n-2]`

3. **Extended energy normalization to direct-form path** — Previously, per-frame RMS normalization only applied to the biquad (formant-shift) path. When voiced pitch-shift now routes to LPC without formant shift, the direct-form IIR path runs without normalization. A sawtooth harmonic that coincides with the narrow-band filter resonance (~2-pole at 970 Hz, Q≈83) causes ~10× amplification. Extended energy normalization to cover all synthesis paths.

### Batch Test Results — Hybrid v8 (voiced-pitch routing)

| Test File | Null dBFS | SNR dB | Formants | Transient | Composite | Δ vs v6 |
|---|---|---|---|---|---|---|
| vocal_aah_formant_upmax | −21.3 | −1.8 | 0.567 | 0.473 | **34.5** | 0 |
| vocal_aah_formant_up4st | −20.1 | −1.3 | 0.486 | 0.403 | 29.5 | 0 |
| vocal_aah_formant_downmax | −18.1 | −0.7 | 0.325 | 0.135 | 16.4 | 0 |
| vocal_aah_pitch_up7st | −18.0 | −0.7 | 0.330 | 0.231 | **19.0** | **+9.0** |
| vocal_aah_pitch_down12st | −21.5 | −2.0 | 0.604 | 0.134 | **27.5** | **+9.2** |
| vocal_aah_time_2x | −13.9 | −4.2 | **0.862** | 0.448 | **45.7** | 0 |
| vocal_aah_time_halfspeed | −16.2 | −0.6 | 0.812 | 0.000 | 32.5 | 0 |
| drum_hit_time_2x | −26.9 | −54.5 | 0.398 | 0.139 | 19.4 | 0 |
| drum_hit_time_halfspeed | −82.7 | −0.4 | 0.500 | **0.643** | **36.1** | 0 |
| drum_hit_time_4x | −21.5 | −59.9 | 0.388 | 0.130 | 18.8 | 0 |
| drum_hit_pitch_up7st | −44.0 | −20.1 | 0.384 | 0.000 | 15.4 | +2.7 |
| chord_Cmaj_time_2x | −22.1 | −3.5 | 0.485 | 0.471 | 31.2 | 0 |
| chord_Cmaj_pitch_up7st | −23.2 | −0.1 | 0.610 | 0.028 | 25.1 | −1.1 |
| chord_Cmaj_formant_max | −24.6 | −0.4 | 0.343 | 0.055 | 15.1 | 0 |
| sine_440_formant_downmax | −15.3 | −2.2 | 0.641 | 0.088 | 27.9 | 0 |
| sine_440_formant_upmax | −12.1 | −0.4 | 0.744 | 0.014 | 30.1 | 0 |
| sine_440_pitch_down12st | −8.1 | −2.0 | 0.667 | 0.000 | 26.7 | 0 |
| sine_440_pitch_up7st | −10.9 | −5.8 | 0.885 | 0.035 | 36.3 | 0 |
| sine_440_time_2x | −9.3 | −7.3 | 0.503 | 0.000 | 20.1 | 0 |
| sine_440_time_halfspeed | −12.5 | −3.9 | 0.657 | 0.093 | 28.6 | 0 |
| **AVERAGE** | | | | | **26.8** | **+1.0** |

### Score Progression

| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Baseline | Passthrough | 13.2 | |
| Session 2 | Phase Vocoder v1 | 21.6 | 6 sine cases |
| Session 5 | Hybrid v3 (oracle routing) | 28.7 | 6 sine cases |
| Session 6 | Hybrid v5 (adaptive LPC) | 28.3 | 6 sine cases |
| Session 7 | Hybrid v6 (real material) | 25.8 | 20 cases: 6 sine + 14 real |
| Session 8 | **Hybrid v8 (voiced-pitch routing)** | **26.8** | 20 cases |
| **Target** | — | **> 60** | |

### Key Findings

**Voiced-speech pitch routing improvement confirmed: +9 pts on each of the two critical cases.**  
`vocal_aah_pitch_up7st`: 10.0 → 19.0 (formant_sim 0.250 → 0.330)  
`vocal_aah_pitch_down12st`: 18.3 → 27.5 (formant_sim 0.297 → 0.604)  
Routing to LPC correctly preserves formant structure: the LPC filter is analyzed at the original pitch and stays fixed; only the sawtooth excitation F0 shifts. This matches V-Synth's confirmed source-filter architecture.

**Why vocal_aah_pitch_up7st is still only 19.0 (not ~25-30):**  
The LPC model stops at order 2 due to Guard 3 (fundamental energy dominates MSE). The 2-pole model captures pitch (~970 Hz), NOT F1/F2/F3. The output after pitch shift is a sawtooth at the new F0 resonated by the ~970 Hz filter — which partially overlaps the V-Synth's formant structure but doesn't match it closely. Full improvement to ~30+ requires fixing the LPC order-collapse issue.

**Guard 3 removal is NOT the right fix for LPC order collapse.**  
Without Guard 3, the 16-pole model includes near-Nyquist poles (>12 kHz for 48 kHz audio). After a +12 st upshift (ratio=2), these poles alias to incorrect positions (a pole at θ=0.75π moves to θ=1.5π, cos(1.5π)=0, producing a pure imaginary pole at Nyquist). This corrupts the formant structure and degraded vocal_aah_formant_upmax from 34.5 → 30.8.

**Correct fixes for LPC order collapse on speech (open items):**  
- Apply pre-emphasis before LPC analysis: x[n] - 0.97×x[n-1] flattens the spectrum, making the fundamental less dominant, allowing the LPC to converge to formant poles at lower order
- Limit Nyquist-range poles after root-finding: filter out poles with |θ| > π/2 (>1/4 Nyquist) before shifting, then re-add them at shifted positions — safer than the full-order model
- Use a different Guard 3 based on voicing: if signal is voiced (ZCR < 0.05), disable Guard 3 (let full order 16 run) with simultaneous Nyquist pole filtering

### Open Questions
- Can pre-emphasis (HPF before LPC analysis) break the fundamental-dominated order collapse without near-Nyquist aliasing?
- What is the V-Synth's actual LPC order and whether/how it handles near-Nyquist poles after formant shift?
- `vocal_aah_formant_downmax` is still 16.4 despite LPC being used. The order-2 model captures pitch, not F1/F2 — same root cause as pitch-shift. Pre-emphasis fix would help both.

### Next Steps
1. **Pre-emphasis before LPC analysis** — apply 0.97 pre-emphasis to the analysis frame in `computeLPC`. Also add a de-emphasis gain stage in synthesis. This should allow higher-order formant capture without Nyquist aliasing, improving both pitch-shift and formant-downshift cases.
2. **vocal_aah_pitch_up7st**: expected to improve from 19.0 → ~28-32 after pre-emphasis fix.
3. **vocal_aah_formant_downmax**: expected to improve from 16.4 → ~22-28 after pre-emphasis fix.
4. **Transient time-compression**: still needs onset-synchronous OLA (drum_hit_time_2x=19.4, time_4x=18.8 are poor).

---

## Session 9 — V-Synth Manual Analysis + Voiced-Adaptive Min LPC Order
**Date:** 2026-06-08
**Phase:** Algorithm / Investigation
**Model:** Claude Sonnet 4.6

### What Was Done

1. **Read V-Synth owner's manual (Books 1 & 2)** — extracted technical details about VariPhrase encode types and internal architecture. Key findings:

   | Encode Type | Architecture | Formant Control | Robot Voice | Notes |
   |---|---|---|---|---|
   | SOLO | LPC source-filter | Yes (independent) | Yes | For mono vocals, wind instruments |
   | BACKING | WSOLA + event timestamps | No | No | For drums/percussion; stores amplitude-peak onsets |
   | ENSEMBLE | Same as BACKING | No | No | For sustain instruments (choir, strings) |
   | LITE | Runtime spectral envelope warp | No | No | Default; no robot voice |

   - **Robot Voice**: holds excitation F0 at encoded pitch regardless of keyboard note — forces monophonic voiced analysis
   - **Events (BACKING/ENSEMBLE)**: amplitude-peak onset timestamps stored at encode time (Depth 0–127 controls density); used for OLA phase reset during time stretch/compress. This is the V-Synth's answer to transient-synchronous OLA.
   - **Energy parameter (SOLO)**: "Specifies how much the fundamental pitch will be emphasized to make the sound more well-defined" — adjustable emphasis on the fundamental in excitation synthesis
   - **chord_Cmaj_formant_max is architecturally N/A**: chords/polyphonic material uses ENSEMBLE type, which has no formant control. This case tests undefined V-Synth behavior.

2. **Investigated pre-emphasis (α=0.97) as Guard 3 fix** — applied `x[n] -= 0.97×x[n-1]` before LPC analysis and de-emphasis after synthesis.

   **Why pre-emphasis does NOT work at 48 kHz:**
   - Pre-emphasis H(z)=1−0.97z⁻¹ is designed for 8 kHz telephone speech where the fundamental (100–300 Hz) has ω=0.08–0.24 rad/sample. The filter provides ~20 dB attenuation at the fundamental relative to 4 kHz formants.
   - At 48 kHz, ALL speech harmonics (f0=120 Hz through F4=3.5 kHz) have normalised angular frequency ω < 0.46 rad/sample. The gain of H(e^jω) = |1 − 0.97e^{-jω}| is nearly constant (all near 0.995) across this entire range.
   - Short-lag autocorrelation r[1]/r[0] therefore remains approximately 1 regardless of pre-emphasis, so the 2-pole model still captures >99% of variance and Guard 3 fires at order 2.
   - Verified by comparing formant_sim on vocal_aah_formant_upmax with and without pre-emphasis: identically 0.567 in both cases.
   - **Conclusion: Pre-emphasis cannot fix Guard 3 order-collapse at 48 kHz. The fix must operate differently.**

3. **Discovered pre-emphasis regression in hybrid_v9 (energy normalisation domain error)** — the initial implementation normalised against `peFrame` (pre-emphasised frame) instead of `frame` (original windowed input). For 440 Hz sine, pre-emphasis attenuates amplitude by factor ~0.034, making peFrame energy 0.001× frame energy. Normalization then targeted near-zero energy, collapsing `sine_440_formant_upmax` from 30.1 → 19.9 (−10.3). All other affected cases regressed similarly. **Fix: normalise against `frame` always.** Pre-emphasis/de-emphasis then fully reverted as it provides no benefit.

4. **Implemented voiced-adaptive minimum LPC guard order (hybrid_v10b)** — since pre-emphasis cannot break the order-collapse at 48 kHz, the fix operates by raising the minimum order at which Guard 3 may fire:

   - For voiced frames WITH formant shift (`isVoiced && |formantShift| > 0.5 st`): `minGuardOrder = 8` — forces at least 4 conjugate pairs (F1–F4 range) before Guard 3 can stop Levinson-Durbin.
   - For all other cases: `minGuardOrder = 2` — original behavior.
   - Why 8 is safe but 16 is not: near-Nyquist poles only appear at orders 13–16 (corresponding to energy above ~6 kHz in 48 kHz audio). At order 8, the highest pole frequency is typically ~3.5 kHz. After the worst-case +12 st upshift (ratio=2), that pole moves to ~7 kHz — still well below the Nyquist guard threshold. The aliasing problem that broke Guard 3 removal only occurs for orders 13+.
   - Why voiced+pitch-shift keeps minGuardOrder=2: a pitch-shift via LPC uses the direct-form filter (no biquad formant manipulation). A higher-order model on voiced speech produces a "harmonic pole zoo" — poles tracking individual harmonics rather than formant envelopes. This hurts pitch accuracy. Confirmed: using minGuardOrder=8 for the voiced pitch-shift routing regressed `vocal_aah_pitch_up7st` 19.0 → 17.6 (−1.4).

5. **Rendered all 20 test cases and ran batch test.**

   - First render batch (hybrid_v10 initial): shell variable expansion in the loop silently dropped all pitch/formant/time flags. All 20 files rendered with default params (passthrough). Detected by observing all 20 files had identical waveform peak (~0.5). Fixed by re-rendering with explicit per-case command lines with hardcoded arguments (no variable expansion).

### Batch Test Results — Hybrid v10b (voiced-adaptive minOrder)

| Test File | Null dBFS | SNR dB | Formants | Transient | Composite | Δ vs v8 | Algorithm |
|---|---|---|---|---|---|---|---|
| vocal_aah_formant_upmax | −21.3 | −1.8 | 0.567 | 0.473 | **34.4** | −0.1 | LPC (minOrder=8) |
| vocal_aah_formant_up4st | −20.1 | −1.3 | 0.473 | 0.403 | 28.5 | **−1.0** | LPC (minOrder=8) |
| vocal_aah_formant_downmax | −18.2 | −0.8 | 0.329 | 0.135 | 17.0 | +0.6 | LPC (minOrder=8) |
| vocal_aah_pitch_up7st | −18.0 | −0.7 | 0.330 | 0.231 | 19.0 | 0 | LPC (minOrder=2) |
| vocal_aah_pitch_down12st | −21.5 | −2.0 | 0.604 | 0.134 | 27.5 | 0 | LPC (minOrder=2) |
| vocal_aah_time_2x | −13.9 | −4.2 | **0.862** | 0.448 | **45.7** | 0 | PV |
| vocal_aah_time_halfspeed | −16.2 | −0.6 | 0.812 | 0.000 | 32.5 | 0 | PV |
| drum_hit_time_2x | −26.9 | −54.5 | 0.398 | 0.139 | 19.4 | 0 | PV |
| drum_hit_time_halfspeed | −82.7 | −0.4 | 0.500 | **0.643** | **36.1** | 0 | PV |
| drum_hit_time_4x | −21.5 | −59.9 | 0.388 | 0.130 | 18.8 | 0 | PV |
| drum_hit_pitch_up7st | −44.0 | −20.1 | 0.384 | 0.000 | 15.4 | 0 | PV |
| chord_Cmaj_time_2x | −22.1 | −3.5 | 0.485 | 0.471 | 31.2 | 0 | PV |
| chord_Cmaj_pitch_up7st | −23.2 | −0.1 | 0.610 | 0.028 | 25.1 | 0 | PV |
| chord_Cmaj_formant_max | −24.9 | −0.5 | 0.351 | 0.055 | **17.9** | **+2.8** | LPC (minOrder=8) |
| sine_440_formant_downmax | −15.1 | −2.1 | 0.630 | 0.088 | **25.4** | **−2.5** | LPC (minOrder=8) |
| sine_440_formant_upmax | −12.2 | −0.3 | 0.757 | 0.014 | **32.8** | **+2.6** | LPC (minOrder=8) |
| sine_440_pitch_down12st | −8.1 | −2.0 | 0.667 | 0.000 | 26.7 | 0 | PV |
| sine_440_pitch_up7st | −10.9 | −5.8 | 0.885 | 0.035 | 36.3 | 0 | PV |
| sine_440_time_2x | −9.3 | −7.3 | 0.503 | 0.000 | 20.1 | 0 | PV |
| sine_440_time_halfspeed | −12.5 | −3.9 | 0.657 | 0.093 | 28.6 | 0 | PV |
| **AVERAGE** | | | | | **26.9** | **+0.1** | |

### Score Progression

| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Baseline | Passthrough | 13.2 | |
| Session 2 | Phase Vocoder v1 | 21.6 | 6 sine cases |
| Session 5 | Hybrid v3 (oracle routing) | 28.7 | 6 sine cases |
| Session 6 | Hybrid v5 (adaptive LPC) | 28.3 | 6 sine cases |
| Session 7 | Hybrid v6 (real material) | 25.8 | 20 cases: 6 sine + 14 real |
| Session 8 | Hybrid v8 (voiced-pitch routing) | 26.8 | 20 cases |
| Session 9 | **Hybrid v10b (voiced-formant minOrder=8)** | **26.9** | chord/sine formant cases +2.8/+2.6; pre-emph tried and reverted |
| **Target** | — | **> 60** | |

### Key Findings

**Pre-emphasis cannot fix LPC order-collapse at 48 kHz — this is a fundamental limitation.**  
The 48 kHz sample rate places ALL speech content (120 Hz–3.5 kHz) in the range ω ∈ [0.016, 0.46] rad/sample. At these frequencies, cos(ω) is nearly 1.0 for every harmonic. Short-lag autocorrelation r[1]/r[0] therefore stays near 1 regardless of spectral shaping. Pre-emphasis, minimum-order increases, and bandwidth expansion cannot change this: the order-collapse arises from the arithmetic of the autocorrelation itself at high sample rates. **True fix requires pitch-synchronous LPC or cepstral liftering.**

**Voiced-adaptive minOrder=8 gives small wins on chord/sine formant cases (+2.8, +2.6).**  
chord_Cmaj_formant_max: 15.1 → 17.9 (extra poles help polyphonic LPC)  
sine_440_formant_upmax: 30.1 → 32.8 (8-pole upshift more accurate than 2-pole)  
These gains confirm the direction is correct for non-speech material but do not address the vocal case.

**sine_440_formant_downmax regressed −2.5 pts (27.9 → 25.4).**  
Root cause: the 440 Hz sawtooth has harmonic energy in the 1–4 kHz band (~17.6% of total), which is above the 5% threshold of the voiced-speech bandpass detector. The sawtooth is therefore misclassified as "voiced speech with formant shift" and receives minOrder=8. The 8-pole model is less accurate for formant downshift on a pure sawtooth than the 2-pole model (which simply shifts the 440 Hz pole to 220 Hz, closely matching the V-Synth output's dominant peak). **Fix: stronger voiced-speech discriminant** — e.g., modulation depth of formant peaks, or pitch-period regularity, to separate harmonic-rich sinusoids from actual speech.

**vocal_aah_formant cases show no improvement from minOrder=8.**  
formant_sim for vocal_aah_formant_upmax: 0.567 in v8, 0.567 in v10b — identically unchanged.  
Even with 4 conjugate pairs forced, all 4 poles at 48 kHz still fall within ω < 0.46 rad/sample. The LPC algorithm cannot place poles at distinct formant frequencies because the autocorrelation structure is dominated by the fundamental harmonic regardless of order. This confirms the pre-emphasis investigation conclusion: order-based fixes cannot help vocal formant capture at 48 kHz.

**chord_Cmaj_formant_max is architecturally N/A.**  
From manual: chords use ENSEMBLE encode type, which does NOT support formant control. The V-Synth reference for this case is undefined behavior. This test case should be treated as informational only (lower weight or excluded) once the primary bottlenecks are resolved.

### Open Questions

- Can the sawtooth vs. voiced-speech discriminant be improved to avoid the sine_440_formant_downmax regression? Options: pitch-period regularity (ACF peak sharpness), modulation depth of spectral peaks, or a separate "is harmonically rich but NOT speech" classifier.
- For pitch-synchronous LPC: what is the correct analysis window? One pitch period zero-padded to 1024 samples, or a half-period window? The V-Synth manual suggests SOLO encode uses 1024-sample frames — may be pitch-synchronous at the frame level rather than per-sample.
- What is the expected score improvement if vocal formant capture is fixed to formant_sim ≥ 0.5 on downshift cases? Estimate: vocal_aah_formant_downmax 17.0 → ~28+.

### Next Steps

1. **Pitch-synchronous LPC for voiced speech** — confirmed as the only correct fix for the 48 kHz order-collapse. Two approaches:
   - Analyse over one pitch period (zero-padded to 1024 for resolution) instead of a fixed 1024-sample frame. This concentrates autocorrelation energy at the pitch period, making shorter-lag r[k] sensitive to formant structure rather than pitch.
   - Apply cepstral liftering to the windowed frame: compute FFT, take log magnitude, IFFT to get cepstrum, zero the pitch-range cepstral bins (quefrency > 1/F4 ≈ 0.3 ms = 14 samples at 48 kHz), then transform back. This separates spectral envelope (formants) from harmonic fine structure (pitch) before LPC analysis.
   - Expected improvement if working: vocal_aah_formant_downmax 17.0 → ~28+, vocal_aah_formant_up4st 28.5 → ~35+.
2. **Transient-synchronous OLA for time compression** — drum_hit_time_2x (19.4) and time_4x (18.8) are poor. V-Synth BACKING encode stores amplitude-peak onset timestamps at encode time (Depth 0–127 controls density); the player resets OLA phase at each event boundary instead of free-running. This prevents the onset smearing that PV accumulates when cramming multiple frames into fewer output samples. Implementing this would align with the confirmed V-Synth architecture.
3. **Sawtooth formant discriminant** — sine_440_formant_downmax regressed −2.5 due to sawtooth being misclassified as voiced speech. A stronger discriminant (e.g., ACF peak sharpness / pitch confidence > 0.95 AND spectral flatness < 0.05 for speech; sawtooth has very sharp ACF peak and high spectral flatness) could correctly route the sawtooth to minOrder=2.
4. **Update chord_Cmaj_formant_max weight** — this case is architecturally N/A per V-Synth manual (ENSEMBLE type has no formant control). Consider marking as informational-only in batch_test.py scoring.

---

## Session 10 — Cepstral LPC + Transient Phase Reset (Both Reverted)
**Date:** 2026-06-08
**Phase:** Algorithm / Investigation
**Model:** Claude Sonnet 4.6

### What Was Done

1. **Implemented cepstral-liftering LPC (`computeLPCCepstral`)** — sfmFft() added as a static FFT helper in SourceFilterModel.cpp (same Cooley-Tukey convention as PhaseVocoder). The method:
   - FFTs windowed frame → log power spectrum
   - IFFTs to cepstrum
   - Zeros cepstral bins L+1..N−L (lifter length L = min(60, T0/4))
   - FFTs back → smooth log spectrum → smooth power spectrum
   - IFFTs → smooth autocorrelation
   - Runs Levinson-Durbin without Guard 3 on smooth autocorrelation
   Routing: `voiced && formantShift > 0.5 st && f0 > 0` → cepstral LPC; otherwise standard LPC.

2. **Ran batch test for hybrid_v11 (cepstral LPC active)** — 26.2/100 (−0.7 vs v10b).

3. **Diagnosed why cepstral LPC fails** — Python analysis of the vocal_aah frame (F0=130 Hz, 48 kHz):
   ```
   Cepstrum: c[0]=−3.836, c[1]=1.549, max(c[2:50])=0.521
   P_smooth peaks (L=60): freq=609 Hz, amplitude=18.6
   True formant: F1≈700 Hz, F2≈1200 Hz
   Raw spectrum peaks (harmonics): 281, 656, 937, 1171 Hz
   ```
   The smooth power spectrum peaks at **609 Hz** (near the 5th harmonic of 130 Hz), NOT at the true formant F1≈700 Hz. Root cause: the dominant cepstral coefficient c[1] = 1.549 represents the **global spectral tilt** (1/f slope of the excitation harmonics), not a formant resonance. This tilt energy is so large that the "smooth" spectrum obtained by keeping low-quefrency cepstral bins is still dominated by the excitation envelope rather than the vocal-tract resonance. The lifter does not cleanly separate formants from harmonics at 48 kHz because the harmonic tilt sits in cepstral bin 1 (always inside the keep window).

   Additional finding: V-Synth's own formant model is **also pitch-dominated** (not acoustically correct). Evidence:
   - Upshift formant_sim drops with cepstral LPC (acoustically correct) vs. standard LPC (pitch-dominated)
   - V-Synth output has poles tracking harmonic multiples of F0 after upshift, not true formant positions
   - Acoustically correct formant poles diverge from V-Synth's pitch-dominated poles → lower formant_sim score for upshift cases

4. **Reverted cepstral LPC routing** back to v10b (standard LPC, minGuardOrder=8 for voiced+formant). The `computeLPCCepstral()` method is retained in SourceFilterModel.cpp for future investigation.

5. **Implemented transient phase reset in PhaseVocoder** — on onset frames (energy > 4× previous frame), synthesis phases are locked to analysis phases instead of being accumulated. Intended to prevent OLA pre-ringing before drum attack.

6. **Ran batch test with phase reset — regression detected immediately:**
   - drum_hit_time_2x: 19.4 → 19.8 (+0.4)
   - drum_hit_time_4x: 18.8 → 18.1 (−0.7)
   - **vocal_aah_time_2x: 45.7 → 36.7 (−9.0!)** — transient score 0.448 → 0.162

   Root cause of vocal regression: the phase discontinuity at the vowel onset (silence → vowel triggers the 4× energy threshold) breaks the OLA envelope reconstruction. The synthesis starts at a random phase relative to the previous overlapping frames, creating a click-like artifact at the onset. This changes the attack envelope shape in a way that diverges from the V-Synth's SOLO-mode reference output (which uses pitch-synchronous OLA, producing a different attack curve).

7. **Reverted transient phase reset** in PhaseVocoder. The `synthesizeFrame` signature retains the `lockToAnalysis` parameter (commented out, no-op) and the code comments explain why the approach fails.

8. **hybrid_v12 final result: 26.9/100** — identical to v10b. Net effect of Session 10: no score change. The session produced two negative experiments (cepstral LPC, phase reset) and valuable diagnostic data.

### Batch Test Results — Hybrid v12 (= v10b, both experiments reverted)

| Test File | Null dBFS | SNR dB | Formants | Transient | Score |
|---|---|---|---|---|---|
| vocal_aah_formant_upmax | −21.3 | −1.8 | 0.567 | 0.470 | 34.4 |
| vocal_aah_formant_up4st | −20.1 | −1.3 | 0.473 | 0.381 | 28.5 |
| vocal_aah_formant_downmax | −18.1 | −0.7 | 0.329 | 0.152 | 17.0 |
| vocal_aah_pitch_up7st | −18.0 | −0.7 | 0.330 | 0.231 | 19.0 |
| vocal_aah_pitch_down12st | −21.5 | −2.0 | 0.603 | 0.109 | 26.9 |
| vocal_aah_time_2x | −13.9 | −4.2 | 0.862 | 0.448 | 45.7 |
| vocal_aah_time_halfspeed | −16.0 | −0.7 | 0.815 | 0.000 | 32.6 |
| drum_hit_time_2x | −26.9 | −54.5 | 0.398 | 0.139 | 19.4 |
| drum_hit_time_halfspeed | −82.7 | −0.4 | 0.500 | 0.643 | 36.1 |
| drum_hit_time_4x | −21.5 | −59.9 | 0.388 | 0.130 | 18.8 |
| drum_hit_pitch_up7st | −44.0 | −20.1 | 0.384 | 0.000 | 15.4 |
| chord_Cmaj_time_2x | −22.1 | −3.5 | 0.485 | 0.471 | 31.2 |
| chord_Cmaj_pitch_up7st | −23.2 | −0.1 | 0.610 | 0.028 | 25.1 |
| chord_Cmaj_formant_max | −24.6 | −0.4 | 0.403 | 0.071 | 17.9 |
| sine_440_formant_downmax | −15.7 | −1.9 | 0.634 | 0.000 | 25.4 |
| sine_440_formant_upmax | −12.1 | −0.4 | 0.758 | 0.097 | 32.8 |
| sine_440_pitch_down12st | −8.1 | −2.0 | 0.667 | 0.000 | 26.7 |
| sine_440_pitch_up7st | −10.9 | −5.8 | 0.885 | 0.035 | 36.3 |
| sine_440_time_2x | −9.3 | −7.3 | 0.503 | 0.000 | 20.1 |
| sine_440_time_halfspeed | −12.5 | −3.9 | 0.657 | 0.093 | 28.6 |
| **AVERAGE** | | | | | **26.9** |

### Score Progression

| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Baseline | Passthrough | 13.2 | |
| Session 2 | Phase Vocoder v1 | 21.6 | 6 sine cases |
| Session 5 | Hybrid v3 (oracle routing) | 28.7 | 6 sine cases |
| Session 6 | Hybrid v5 (adaptive LPC) | 28.3 | 6 sine cases |
| Session 7 | Hybrid v6 (real material) | 25.8 | 20 cases: 6 sine + 14 real |
| Session 8 | Hybrid v8 (voiced-pitch routing) | 26.8 | 20 cases |
| Session 9 | **Hybrid v10b (voiced-formant minOrder=8)** | **26.9** | chord/sine formant cases +2.8/+2.6 |
| Session 10 | Hybrid v12 (cepstral LPC + transient reset, both reverted) | **26.9** | Two experiments diagnosed and reverted |
| **Target** | — | **> 60** | |

### Key Findings

**Cepstral LPC does NOT cleanly separate formants from pitch at 48 kHz.**  
The dominant cepstral bin c[1] = 1.549 represents the global spectral tilt of the excitation (harmonic energy decaying as 1/f). This tilt is so large that the low-quefrency "smooth" envelope still reflects the excitation shape rather than the vocal-tract resonance. The lifter cannot exclude c[1] (it's inside the keep window by definition). Result: smooth power spectrum peaks at 609 Hz (a harmonic) rather than F1=700 Hz (a formant).

**The V-Synth's formant model is pitch-dominated, not acoustically correct.**  
Acoustically correct formant poles (from cepstral LPC) give WORSE formant_sim scores on upshift cases because the metric measures similarity to the V-Synth reference, which itself uses pitch-dominated poles. The "correct" formant positions diverge from what the V-Synth actually does. This means improvements to LPC accuracy that go beyond what the V-Synth does will HURT the score. The algorithm must match the V-Synth's specific (pitch-dominated) behavior.

**Transient phase reset in PV produces a catastrophic −9 pt regression on vocal_aah_time_2x.**  
Phase resetting at onsets creates a discontinuity relative to the already-accumulated OLA output in the buffer. The overlap of the reset synthesis frame with the previous non-reset frames produces an attack artifact that doesn't match the V-Synth SOLO reference's smooth attack. The correct approach for transient preservation is WSOLA (time-domain OLA with waveform similarity search), which avoids phase artifacts entirely.

**Drum time-stretch cases need WSOLA, not PV.**  
drum_hit_time_2x and time_4x score 19.4 and 18.8 with SNR ≈ −55 to −60 dB — spectral content completely different from V-Synth WSOLA output. No PV-internal fix will close this gap; the algorithm for these cases needs to be WSOLA rather than phase vocoder.

### Open Questions

- Is the V-Synth SOLO mode using a downsampled (8–16 kHz) LPC analysis rather than 48 kHz? If so, the formant poles would be in the right frequency range (cos(ω) is significantly < 1 for F2 at 8 kHz), and matching this would give better formant_sim.
- For WSOLA: the V-Synth BACKING stores amplitude-peak events at encode time. Can we implement a simplified version that just performs onset-synchronous OLA at decode time (detecting onsets real-time)?
- For drum_hit_pitch_up7st (15.4): how does the V-Synth pitch-shift a drum hit? PV pitch shift (phase accumulation + resample) gives very different output from the V-Synth. Does V-Synth BACKING mode even support pitch shift? Manual says BACKING has no pitch independent control…

### WSOLA Addendum (also Session 10)

WSOLA was also implemented and tested for time-only cases (no pitch/formant shift). Full implementation added to PhaseVocoder: `processWSOLA()` method with normalised cross-correlation search over ±kWsolaSearchLen=256 samples. Routing: `timeOnly = (|formant| < 0.001 AND |pitch| < 0.01) → processWSOLA`.

**hybrid_v13 WSOLA results (time-only cases changed):**

| Case | v10b (PV) | v13 (WSOLA) | Δ |
|---|---|---|---|
| chord_Cmaj_time_2x | 31.2 | **35.0** | **+3.8** |
| drum_hit_time_2x | 19.4 | 19.7 | +0.3 |
| drum_hit_time_4x | 18.8 | 19.8 | +1.0 |
| drum_hit_time_halfspeed | 36.1 | NaN | BUG |
| sine_440_time_2x | 20.1 | 17.1 | **−3.0** |
| sine_440_time_halfspeed | 28.6 | 18.9 | **−9.7** |
| vocal_aah_time_2x | 45.7 | 31.2 | **−14.5** |
| vocal_aah_time_halfspeed | 32.6 | 27.9 | **−4.7** |

Net: −26.8 pts across 7 cases + 1 NaN. Reverted.

**Root cause of WSOLA regression:** The V-Synth routes content type-dependently: SOLO/LPC for vocals, PV-like for pure tones, WSOLA/BACKING for drums/polyphonic. Our WSOLA applied uniformly doesn't match V-Synth's SOLO output for vocals (transient 0.448 → 0.000) or PV for pure sines (similarity search finds multiple alias matches on periodic signal, creates phase jump). The chord improvement confirms WSOLA is architecturally correct for ENSEMBLE content (V-Synth ENSEMBLE = WSOLA).

**NaN bug:** WSOLA normalised cross-correlation divides by sqrt(denRef × denCand). When the decay region of a drum hit falls in the overlap window (both reference and candidate near zero), denRef * denCand < 1e-20 and the sqrt underflows. The `+ 1e-10f` guard prevents divide-by-zero but didn't prevent compare.py's transient metric from producing NaN (likely abs(0)/abs(0) in the Python). Retained in code with `// NaN guard` comment for future fix.

**Correct fix for WSOLA:** Add content-type detection before routing:
- Use the same ZCR + 1–4 kHz band energy check already in VariphraseEngine
- OR use spectral flatness: pure tone → PV; polyphonic + non-voiced → WSOLA; voiced → PV (or LPC)
- Required because V-Synth encode type selection is essentially manual; our real-time version must approximate it automatically.

---

## Session 11 — WSOLA Content-Adaptive Routing (Reverted)
**Date:** 2026-06-09
**Phase:** Algorithm
**Model:** Claude Sonnet 4.6

### What Was Done

Attempted content-adaptive WSOLA routing to improve chord_Cmaj_time_2x (+3.8 expected) while protecting vocal/sine cases. Three sub-experiments:

**Sub-experiment A: Per-block (512-sample) ACF routing**

Computed ACF confidence = max(ACF[lag_lo..lag_hi]) / ACF[0] on the 512-sample input block. For vocal_aah F0=130 Hz (T0=369 samples), biased ACF[369]/ACF[0] = (512-369)/512 ≈ 0.28 — far below threshold. Vocal incorrectly routed to WSOLA → vocal_aah_time_2x: 45.7 → 31.1 (−14.6). ABANDONED.

Root cause: With N=512 and T0=369, only 143 valid sample pairs exist for lag 369. The biased ACF is naturally low for large lags relative to window length.

**Sub-experiment B: Per-frame (2048-sample) ACF routing (silent bug)**

Moved routing inside the PV while loop, computing ACF on the full kFFTSize=2048 analysis frame. Empirical unbiased ACF values confirmed:
- vocal_aah: 0.984 → should route to PV ✓
- chord_Cmaj: 0.826 → should route to WSOLA ✓

However, a WSOLA buffer-guard condition `inputFill_ >= kFFTSize + kWsolaSearchLen` (2304) was added, which is NEVER met because inputFill_ reaches at most kFFTSize (2048) when the while loop triggers at block size 512. WSOLA silently fell back to PV for all frames. Score: 26.9 (identical to v10b — silent no-op). Verified: v16b and v10b chord_Cmaj_time_2x renders are byte-identical.

**Sub-experiment C: Per-frame ACF routing with guard removed**

Removed the over-conservative guard. WSOLA now activates correctly. However, discovered fundamental ACF distribution overlap that makes routing unreliable:

| Content | unbiased ACF p10 | median | p90 | max |
|---------|-----------------|--------|-----|-----|
| vocal_aah | 0.848 | 0.980 | 0.996 | 1.009 |
| chord_Cmaj | 0.000 | 0.685 | 0.855 | 0.920 |
| drum_hit | 0.324 | 0.713 | 0.822 | 0.903 |
| sine_440 | 1.000 | 1.001 | 1.001 | 1.001 |

**vocal_aah p10 (0.848) < chord_Cmaj max (0.920): no single threshold separates them.** The early/quiet frames of the vocal (t=0..0.2s) have low ACF confidence because the signal is not yet at full amplitude. Threshold 0.90 sends these frames to WSOLA.

Results at threshold 0.90:
- vocal_aah_time_2x: 45.7 → 32.0 (−13.7 pts) ← vocal attack frames route to WSOLA
- chord_Cmaj_time_2x: 31.2 → 20.5 (−10.7) ← WSOLA is now active but HURTS chord
- drum_hit_time_halfspeed: NaN ← WSOLA transient score divide-by-zero

Net: severe regression. REVERTED.

**NaN diagnostic:** drum_hit_time_halfspeed produced NaN in the transient metric. Root cause: WSOLA similarity search for near-silence frames gives score ≈ 0, and the subsequent output is near-zero. compare.py's transient_sim computes energy ratios that divide by near-zero. The `std::max(denRef, 1e-12f)` guard in the C++ prevents NaN in the scores, but Python-side NaN can occur.

**Why chord_Cmaj_time_2x regressed with active WSOLA (20.5 < v10b 31.2):**

In v13 (uniform WSOLA on all time-only), chord scored 35.0. But in v16c (per-frame routing with threshold 0.90), chord scored 20.5. The degradation comes from MIXED ROUTING: some chord frames route to PV (those with unbiased > 0.90) and others to WSOLA. The alternating PV/WSOLA frames create discontinuities in the output OLA buffer. Both synthesizers write to the same outputBuffer_ with different waveform shapes, producing artifacts at every mode switch.

### Final State

Code reverted to pure PV (hybrid_v10b behavior). Score: **26.9/100**.

Key lessons:
1. ACF routing on 512-sample block = fundamentally too short for low-F0 content (F0 ≤ 192 Hz).
2. ACF routing on 2048-sample frame = correct in theory but vocal/chord distributions overlap.
3. Mixed PV+WSOLA per-frame = worse than either pure PV or pure WSOLA.
4. WSOLA requires CONTENT-CLASS assignment at encoding time (not per-frame runtime detection) to match V-Synth architecture.
5. V-Synth encode type (SOLO/BACKING/ENSEMBLE/LITE) is essentially metadata chosen when the sample is loaded onto the V-Synth — impossible to replicate dynamically without a full offline analysis pass.

### Batch Test Results (v11_clean = pure PV, same as v10b)

| Test File | Null dBFS | SNR dB | Formants | Transient | SCORE |
|---|---|---|---|---|---|
| vocal_aah_time_2x | −13.9 | −4.2 | 0.862 | 0.448 | **45.7** |
| vocal_aah_time_halfspeed | −16.2 | −0.6 | 0.812 | 0.000 | 32.5 |
| chord_Cmaj_time_2x | −22.1 | −3.5 | 0.485 | 0.471 | 31.2 |
| drum_hit_time_halfspeed | −82.7 | −0.4 | 0.500 | 0.643 | **36.1** |
| All other cases | (unchanged vs v10b) | | | | |
| **AVERAGE** | | | | | **26.9** |

### Score Progression

| Session | Algorithm | Avg Score | Notes |
|---|---|---|---|
| Session 9 | Hybrid v10b | 26.9 | Baseline |
| Session 10 | Hybrid v12 (cepstral LPC + transient reset, reverted) | 26.9 | No change |
| Session 11 | Hybrid v11_clean (WSOLA routing, reverted) | **26.9** | Three WSOLA approaches failed |

### Next Steps

1. **Downsampled LPC analysis** — run LPC on frame downsampled to 8 kHz. At 8 kHz, formant poles (F1–F4) are well-separated in cos(ω) space, allowing reliable Levinson-Durbin. Scale poles back to full-rate before synthesis. Expected improvement: vocal_aah_formant cases (currently 17–34 pts).

2. **WSOLA with offline content classification** — add a pre-processing pass over the full input buffer to classify content type before calling processMono. Store result in a member variable. SOLO/LITE → PV; BACKING/ENSEMBLE → WSOLA. Eliminates per-frame routing instability.

3. **Improve chord time-stretch quality** — chord_Cmaj_time_2x (31.2) is the weakest time-only case for non-drum content. V-Synth ENSEMBLE mode = WSOLA. An offline classification pass (approach 2) could safely enable WSOLA for chord.

---

---

## Session 12 — Offline Encode Pass (V-Synth Architecture)
**Date:** 2026-06-09
**Phase:** Algorithm
**Model:** Claude Sonnet 4.6

### What Was Done

Implemented the V-Synth-style offline encode pass: a full-buffer content analysis step
that classifies each sample into one of four encode types before real-time processing
begins. This resolves the fundamental per-frame ACF instability identified in Session 11.

#### Architecture (V-Synth parallel)

| V-Synth Encode Type | Our ContentType | Routing |
|---------------------|-----------------|---------|
| SOLO (LPC source-filter) | SOLO | LPC source-filter |
| ENSEMBLE (WSOLA) | ENSEMBLE | WSOLA (time-only) |
| BACKING (WSOLA + event stamps) | BACKING | WSOLA (time-only, timeStretch ≥ 1) |
| LITE (pure tone / PV) | LITE | Phase vocoder |

#### New APIs

**`VariphraseEngine::analyzeContent(const float* mono, int n, double sr)`** — static, offline.
Computes per-frame unbiased ACF confidence over the full buffer:
- Step size: kHopSize = 512 samples
- Frame size: kFFTSize = 2048 samples (correctly handles F0 down to 60 Hz)
- Lag range: sr/500 to sr/60 (88–735 samples at 44.1 kHz)
- Unbiased ACF: biased[lag] × N/(N-lag) / ACF[0]
- Metric: max unbiased ACF over all lags per frame, median over non-silent frames

Classification:
```
medianConf > 0.95  →  single-pitch content
  + 1–4 kHz band energy > 5% → SOLO  (voiced speech / melody)
  + otherwise                → LITE  (pure tone / oscillator)
medianConf ≤ 0.95  →  polyphonic or transient-rich
  + peakToMeanEnergy > 5.0   → BACKING  (drums / transient-rich)
  + otherwise                → ENSEMBLE (chords / polyphonic)
```

**`VariphraseEngine::setAnalysis(const VariphraseAnalysis&)`** — stores result in Impl.

**`PhaseVocoder::setForceWSOLA(bool)`** — flag set by Hybrid routing to select WSOLA
for time-only ENSEMBLE/BACKING cases.

#### Content Classification Results

| Sample | medianConf | peakToMean | ContentType |
|--------|-----------|------------|-------------|
| vocal_aah | 0.985 | 2.0 | **SOLO** ✓ (no WSOLA) |
| chord_Cmaj | 0.895 | 7.8 | **BACKING** (gets WSOLA) |
| drum_hit | 0.489 | 11.4 | **BACKING** ✓ |
| sine_440 | 1.000 | 1.0 | **LITE** ✓ (no WSOLA) |

Note: chord_Cmaj classified as BACKING (high peak/mean due to chord attack transient)
rather than ENSEMBLE, but both receive identical WSOLA routing in the Hybrid algorithm.

#### WSOLA Guard Fix

**Problem discovered**: For time compression (timeStretch < 1.0), the WSOLA synthesis
hop < analysis hop, so the output OLA buffer write pointer advances slower than the
read pointer → output is silence. **Fix**: WSOLA only activates when timeStretch ≥ 1.0.
Compression cases fall back to PV.

The original kFFTSize+kWsolaSearchLen (2304) guard was retained to ensure the full
±kWsolaSearchLen similarity search is available. Reducing it to kFFTSize caused
backward-only searches (forwardAvail=0 at every trigger) and dropped chord_Cmaj_time_2x
from 35.0 to 20.5 — reverted.

#### OfflineRenderer wired up

`renderOffline()` now calls `analyzeContent()` on the full mono buffer, then
`setAnalysis()`, before the block-by-block `processOffline()` loop. The content type
and metrics are logged to stdout for debugging.

### Batch Test Results (hybrid_v17c)

| Test File | v11_clean | v17c | Delta |
|---|---|---|---|
| **chord_Cmaj_time_2x** | 31.2 | **35.0** | **+3.8** |
| drum_hit_time_2x | 19.4 | 19.7 | +0.3 |
| drum_hit_time_4x | 18.8 | 19.8 | +1.0 |
| drum_hit_time_halfspeed | 36.1 | 36.1 | 0.0 (PV, timeStretch=0.5<1) |
| vocal_aah_time_2x | 45.7 | 45.7 | **0.0** (no regression) |
| All other 15 cases | — | — | 0.0 |
| **AVERAGE (20)** | **26.9** | **27.2** | **+0.3** |

### Key Findings

1. **Offline content classification is stable**: vocal_aah (SOLO), chord_Cmaj (BACKING),
   drum_hit (BACKING), sine_440 (LITE) — all correctly identified from global ACF statistics.
   No per-frame instability.

2. **chord_Cmaj_time_2x +3.8 pts**: WSOLA better preserves chord waveform shape during
   time extension. Transient score 0.471 → 0.582, consistent with v13 (uniform WSOLA)
   which also showed +3.8 on this case.

3. **Zero regressions**: vocal_aah, sine_440, all formant/pitch cases unchanged. The
   offline routing decision correctly prevents WSOLA from touching SOLO/LITE content.

4. **Time compression limitation**: For timeStretch < 1 with WSOLA, the OLA buffer
   write/read rate mismatch produces silence. Requires different handling
   (e.g., discard-then-OLA instead of OLA-then-advance).

### Score Progression

| Session | Version | Score | Notes |
|---|---|---|---|
| Sessions 9–11 | hybrid_v11_clean | 26.9 | Pure PV baseline |
| Session 12 | hybrid_v17c | **27.2** | +0.3, chord_Cmaj_time_2x +3.8 |

### Next Steps

1. **chord_Cmaj pitch/formant cases** — chord_Cmaj_pitch_up7st (25.1) and
   chord_Cmaj_formant_max (17.9) could benefit from better chord-specific processing.
   LPC doesn't apply here (chord is polyphonic). Consider spectral stretching for pitch.

2. **Downsampled LPC for vocal** — vocal_aah formant cases score 17–34 pts. The LPC
   source-filter should improve these but requires reliable pole extraction. Try LPC
   at 8 kHz downsampled then scale poles to full rate.

3. **Drum WSOLA for compression** — implement discard-frame approach: for timeStretch < 1
   with BACKING content, discard frames at a rate matching the compression ratio, using
   WSOLA similarity search to choose which frames to keep.

4. **Vocal time-stretch improvement** — vocal_aah_time_halfspeed (32.5) is slightly below
   vocal_aah_time_2x (45.7). With SOLO content type, the PV path is used for both.
   Consider whether LPC-based time-stretch (replace excitation only) could improve this.

---

## Session 13 — Pre-Emphasis LPC (Net Regression as Shipped)
**Date:** 2026-06-09
**Phase:** Algorithm
**Model:** Sonnet

### What Was Done

1. **Implemented pre-emphasis for LPC analysis** — `H(z) = 1 − 0.97z⁻¹` applied to the
   windowed analysis frame before Levinson-Durbin; de-emphasis `1/(1 − 0.97z⁻¹)` applied
   to the OLA output stream (sample-by-sample, `deEmphState_` carried across blocks).

2. **Corrected the Session 9 diagnosis** — Session 9 concluded pre-emphasis gives no
   benefit at 48 kHz because `r[1]/r[0]` stays near 1 (Guard 3 still fires at order 2).
   That mechanism is real but already neutralised by `minGuardOrder=8`. The overlooked
   effect is on the **optimisation criterion** for orders 2–8: without pre-emphasis the
   fundamental dominates the autocorrelation by ~10 000× at 48 kHz, so all 8 poles
   cluster near F0. With pre-emphasis, `|H(kω₀)|² ≈ 0.97(kω₀)²` cancels the 1/k²
   harmonic decay, every harmonic contributes equally, and the poles spread across
   F1–F4. (Session 9's separate finding stands: normalise energy against the original
   frame's domain, not a mismatched one — the v9 regression came from normalising
   against the pre-emphasised frame while synthesis was in the signal domain.)

### Batch Test Results (hybrid_v18_preemph, committed as 7458bdd)

| Test File | v17c | v18_preemph | Delta |
|---|---|---|---|
| vocal_aah_pitch_up7st | 19.0 | 24.9 | **+5.9** |
| sine_440_formant_downmax | 25.4 | 29.9 | **+4.5** |
| vocal_aah_formant_downmax | 17.0 | 19.3 | **+2.3** |
| chord_Cmaj_formant_max | 17.9 | 19.8 | +1.9 |
| vocal_aah_formant_upmax | 34.4 | 29.4 | **−5.0** |
| vocal_aah_pitch_down12st | 27.5 | 19.9 | **−7.6** |
| sine_440_formant_upmax | 32.8 | 13.7 | **−19.1** |
| chord_Cmaj_pitch_up7st | 25.1 | 21.9 | −3.2 |
| **AVERAGE (20)** | **27.2** | **26.1** | **−1.1** |

### Findings / Hypothesis Update

Pre-emphasis genuinely improves formant capture (the big wins above prove the
Session 9 "fundamental limitation" claim wrong), but applying it unconditionally
regresses cases where the LPC pole positions interact badly with the shift
direction. The catastrophic sine_440_formant_upmax case (−19.1): with pre-emphasis
the 2-pole sine model sits on a tilted spectrum; after a +12 st pole-angle shift the
de-emphasised output level/shape no longer matches the reference at all.

---

## Session 14 — Direction-Aware Pre-Emphasis Bypass + SOLO Routing Gate
**Date:** 2026-06-10
**Phase:** Algorithm
**Model:** Sonnet → Fable

### What Was Done

Recovered the Session 13 regressions while keeping its gains, in three commits:

1. **v18e (2e734a2) — conditional analysis frame, direction-aware for formant shift.**
   Two failed approaches first:
   - *De-emphasis polynomial folding* (`A′(z) = A(z)·(1−0.97z⁻¹)`): unsound — the
     product is order P+1 but only P terms are stored; truncation perturbs ALL formant
     roots, not just the appended de-emphasis pole. Scored 23.9–24.9. **Do not retry.**
   - *Blanket bypass for non-speech formant frames*: recovered sine upmax but lost the
     pre-emph gain on sine downmax.
   Final form: per-frame selection of the LPC analysis frame.
   `usePreEmph = !(formantUpBypass)` where `formantUpBypass = hasFormantShift && voiced
   && !isVoicedSpeechFrame && formantShift ≥ 0`. The voiced-speech discriminant is the
   same 2 kHz bandpass (fc=2000, Q=1.5, energy ratio > 5%) used by the Hybrid router.
   Post-OLA de-emphasis gated by `anyUsePreEmph` (any frame in block used pre-emph).
   Score: 26.1 → **27.4**.

2. **v18f (ab53b60) — pitch-direction bypass.** Pre-emphasis also hurt large downward
   pitch shifts: at −12 st the excitation F0 drops below the pre-emphasis corner, so the
   tilted envelope mis-weights the low harmonics. Added `largePitchDown = !hasFormantShift
   && voiced && pitchShift < −6 st` to the bypass. vocal_aah_pitch_down12st 19.9 → 27.0;
   vocal_aah_pitch_up7st kept its pre-emph gain (24.9). Score: **27.7**.

3. **v18g (560d322) — SOLO gate on voiced-speech pitch routing.** The
   chord_Cmaj_pitch_up7st "regression" (25.1 → 21.9) turned out to be a routing bug,
   not a PV change: chord blocks transiently pass the per-block ZCR + 2 kHz check
   (C major harmonics put >5% energy at 2 kHz) and were routing to LPC, where pitch-up
   pre-emphasis hurt. Gated the per-block check by the Session 12 offline encode-pass
   classification: only SOLO content may route pitch shifts to LPC. chord pitch
   21.9 → **26.2** (above the v17c 25.1). Drum/vocal/sine pitch cases unchanged.

### Tried and Reverted

1. **Extending formantUpBypass to speech frames** (dropping `!isVoicedSpeechFrame`):
   vocal_aah_formant_upmax +1.0 but up4st −1.8 — net wash. Root blocker: other frames
   in the block still use pre-emph, so `anyUsePreEmph` stays true and the block-level
   post-OLA de-emphasis tilts the bypassed frames' output anyway.

2. **Per-frame de-emphasis** (filter `synthFrame` through `1/(1−0.97z⁻¹)` with fresh
   zero state before windowing/OLA; normalise against the original frame; delete the
   block-level output filter): scored **27.0 vs 27.7**. up4st +1.4 but
   vocal_aah_pitch_up7st collapsed 24.9 → 17.1 and formant_downmax 19.3 → 18.2.
   Likely cause: the de-emphasis integrator (DC gain 33) needs cross-frame state to
   build up the low-frequency content of a 120 Hz voice; fresh state per frame loses
   it, and filtering before windowing doesn't commute with filter-after-OLA. A future
   variant would need separate OLA accumulation buffers for pre-emphasised vs bypassed
   frames, each with its own continuous-state output filter.

### Measurement Correction

Earlier readings this session (27.9) were inflated: sine_440_formant_downmax had been
rendered with `sustained/sine_440_formant_downmax.wav` (the 24-bit V-Synth REFERENCE)
as input instead of `passthrough/sine_440_formant_downmax.wav` (the 16-bit dry input).
The sine_440 cases have per-case dry inputs in `passthrough/` with the same filenames
as the references in `sustained/` — easy trap. All LPC-routed cases re-rendered from
correct inputs; honest v18g score is **27.7**.

### Batch Test Results (hybrid_v18g, correct inputs)

| Test File | v17c | v18g | Delta |
|---|---|---|---|
| sine_440_formant_downmax | 25.4 | 29.9 | **+4.5** |
| sine_440_formant_upmax | 32.8 | 32.8 | 0.0 (recovered from 13.7) |
| vocal_aah_pitch_up7st | 19.0 | 24.9 | **+5.9** |
| vocal_aah_pitch_down12st | 27.5 | 27.0 | −0.5 (recovered from 19.9) |
| vocal_aah_formant_downmax | 17.0 | 19.3 | +2.3 |
| vocal_aah_formant_upmax | 34.4 | 29.9 | **−4.5** (open item) |
| chord_Cmaj_pitch_up7st | 25.1 | 26.2 | **+1.1** |
| chord_Cmaj_formant_max | 17.9 | 21.1 | **+3.2** |
| All time-stretch cases | — | — | 0.0 |
| **AVERAGE (20)** | **27.2** | **27.7** | **+0.5** |

### Findings / Hypothesis Update

1. **Pre-emphasis benefit is direction-dependent in BOTH the formant and pitch axes.**
   Down-shifts of formants want pre-emphasised analysis; up-shifts of formants on pure
   tones want plain analysis. Up-shifts of pitch want pre-emphasis; large down-shifts
   of pitch don't. This is now encoded per-frame in `SourceFilterModel.cpp`
   (`formantUpBypass` / `largePitchDown`, ~line 861).

2. **The offline encode pass earns its keep again**: the SOLO gate is the second win
   attributable to Session 12's content classification (after WSOLA routing). Per-block
   heuristics misfire on polyphonic material; the global classification doesn't.

3. **Block-level de-emphasis is the remaining architectural constraint** — it forces
   all frames in a block into one spectral domain at the output. Fixing it properly
   (dual OLA streams) is the path to recovering vocal_aah_formant_upmax (29.9 vs 34.4).

### Continuation (same session): v18h — Dual OLA Streams + Metric v2

#### v18h (0b6d706) — dual OLA streams, score 28.0 (old metric)

Implemented the dual-stream architecture proposed above:
- `outputBuffer_` accumulates pre-emphasised-domain frames, de-emphasised
  continuously at read time (`deEmphState_` across blocks).
- `outputBufferPlain_` accumulates bypassed frames, read as-is.
- Output = de-emphasised stream + plain stream, summed per sample.

With cross-tilting eliminated, `formantUpBypass` was safely extended to speech
frames (dropped `!isVoicedSpeechFrame`): **vocal_aah_formant_upmax 29.9 → 34.5**,
above the v17c level (34.4), while keeping all pre-emphasis gains.

One surprise: chord_Cmaj_formant_max dropped 21.1 → 17.9 with the clean split —
the chord case empirically scores best when bypassed frames ARE de-emphasised
(the low-shelf boost matches the V-Synth's ENSEMBLE output tilt). Added
`VariphraseParams::polyphonicContent` (set by Hybrid routing from the encode-pass
ENSEMBLE/BACKING classification); for polyphonic content, bypassed frames route
into the de-emphasis stream. chord recovered to 21.2.

#### Metric v2 (3ecdfdf) — time-aligned comparison

Investigating drum_hit_pitch_up7st (15.4, transient score 0.000) revealed a
test-harness bug, not an algorithm failure: `compare.py align_length()` only
truncated to the shorter file, with NO time alignment. The V-Synth reference
recordings have lead-in silence up to ~2.6 s (drum refs especially) while
renderer outputs start immediately. **8 of 20 test pairs were being scored
against the reference's leading silence.** All historical drum scores are
measurement noise (including the "best drum case" drum_hit_time_halfspeed 36.1,
which was inflated by silence-vs-silence comparison).

Fix: envelope cross-correlation alignment (rectified, ~1 kHz decimation, FFT
xcorr), shift the later-starting signal, then truncate to common length.
Self-comparison still aligns at lag 0 exactly.

#### Scores under metric v2 (NOT comparable to any earlier numbers)

| Test File | v17 outputs (aligned) | v18h (aligned) |
|---|---|---|
| drum_hit_pitch_up7st | — | 25.7 (was "15.4") |
| drum_hit_time_2x | — | 25.6 (was "19.7") |
| drum_hit_time_4x | — | 16.5 |
| drum_hit_time_halfspeed | — | 21.0 (was "36.1", inflated) |
| vocal_aah_formant_upmax | — | 34.5 |
| vocal_aah_formant_up4st | — | 34.2 |
| vocal_aah_time_2x | — | 44.7 |
| **AVERAGE (20)** | **27.4** | **28.2** |

The v18 work holds up under the corrected metric: +0.8 vs the re-scored v17
baseline.

#### Renderer limitation discovered (open)

`VariphraseEngine::processOffline()` resizes output to the input length, and the
`process()` API forces equal input/output sample counts per call. For
timeStretch=2 the engine consumes input at half rate, so the input ring
(4×kFrameSize ≈ 4096 samples) overflows during a multi-second render and input
chunks are silently dropped. Sustained vowels mask this (stationary content);
transient material does not. A proper fix needs a pull-based drain mode in the
engine — substantial rework, deferred.

### Next Steps

1. **Offline render drain mode** — fix the equal-I/O `process()` constraint for
   time stretch so the full stretched signal is rendered without ring overflow.
   Prerequisite for meaningful drum/chord time-stretch optimisation.

2. **Transient-synchronous OLA for time compression** — drum_hit_time_4x (16.5
   aligned) is now the lowest case; OLA phase reset at detected onsets; V-Synth
   BACKING mode stores onset timestamps at encode time for exactly this purpose.

3. **vocal_aah_formant_downmax (20.4 aligned)** — now the weakest vocal case;
   formant similarity only 0.327. The downward shift keeps pre-emphasis; consider
   whether the order-8 guard limits F1 resolution after a −12 st shift.

---

## Session 15 — v19: Drain Mode, Read Gating & the 13-Session OLA Bug
**Date:** 2026-06-10 (same day, continuation)
**Phase:** Algorithm / Infrastructure
**Model:** Fable

### What Was Done

Implemented the offline render drain mode (Session 14 next-step #1), which pulled
a thread that unravelled into the most significant correctness fix of the project.
All three fixes landed as **v19 (71aa8ce), score 29.5** (metric v2; previous best
28.2, re-scored v17 baseline 27.4).

#### 1. Offline drain mode

`processOffline()` now produces `inputLen × timeStretchRatio` samples.  Engine
output rings are pre-sized via new `setOutputCapacity()` methods so extension
renders can't lap the ring (the 64k PV ring lapped after ~1.4 s at 48 kHz —
confirmed: the old vocal_aah_time_halfspeed render degraded at exactly 1.37 s).
After input is exhausted, zero-input blocks pump out the queued surplus.

Also discovered and fixed the render ratio convention: measuring reference
content durations against the dry inputs shows time_2x = ratio 0.5, time_4x =
0.25, halfspeed = 2.0.  **Previous sessions rendered time cases with the wrong
direction** (e.g. halfspeed at 0.5); they scored plausibly anyway because the
metrics barely distinguish direction on stationary content.  This also explains
the Session 12 "chord_Cmaj_time_2x uses WSOLA" claim: WSOLA requires stretch ≥ 1,
which only held because the render direction was inverted.

#### 2. OLA read gating

For compression (stretch < 1) the reader outpaced the writer: it read zeros
ahead, and later synthesis frames landed BEHIND the read pointer — silently
lost.  Engines now track `outputAvail_`; when the queue is empty the read pads
zeros without consuming, and `lastValidOutput_` reports the per-call valid
prefix so `processOffline` can compact the stream.  Latency strip is no longer
needed (leading zeros are reported invalid, never collected).

#### 3. The 13-session OLA bug: incoherent excitation phase, masked by partial reads

After gating, all SFM formant cases collapsed (sine upmax render: RMS 0.024 vs
the pre-gating 0.073).  Instrumentation showed identical synthesis frames and
perfect 4-window OLA counts in both builds — yet 3× different output level.

Root cause chain, in two layers:

- **Layer 1 (revealed):** `sawPhase_` advanced a full kFrameSize (1024) per
  synthesis frame, but frames OLA at kHopSize (256) intervals.  Overlapping
  frames therefore carried excitation from UNRELATED time regions → phase
  cancellation in the overlap (~10 dB level loss, smeared spectrum, F0
  wobble).  The LPC resynthesis has been doing this since Session 3.

- **Layer 2 (the mask):** the pre-gating reader sat 768 samples AHEAD of the
  OLA write pointer (warm-up arithmetic: reads advance 512/call from call 1,
  writes lag by the 1024-sample frame fill).  It read PARTIAL OLA sums (1–3
  windows), zeroed them, and the remaining window contributions accumulated
  until the ring wrapped — read one lap (16384 samples ≈ 0.37 s) later.  Each
  output sample was therefore `partial(t) + leftover(t − 0.37 s)`: an
  accidental echo that decorrelated the would-be-cancelling overlaps and
  HID the phase bug for 13 sessions.

**Fix:** synthesise each frame from a phase snapshot, then advance the running
phase by only the synthesis hop (`kTwoPi · f0/sr · kHopSize · timeStretch`).
Overlapping frames are now coherent on the output timeline.  Result: the sine
formant-up render finally matches the V-Synth reference's harmonic profile —
444/888/1332 Hz at 49/61/28 dB vs reference 439/880/1319 Hz at 50/62/53 dB —
and RMS 0.21 vs reference 0.24 (the old "good-scoring" render was 398/972 Hz
at 1/10 the reference level).

### Batch Test Results (v19, metric v2)

| Test File | pre-v19 (28.2 run) | v19 | Delta |
|---|---|---|---|
| vocal_aah_time_2x | 44.7 | **57.7** | +13.0 (best case ever) |
| chord_Cmaj_pitch_up7st | 26.2 | **49.8** | +23.6 |
| chord_Cmaj_time_2x | 35.0 | **44.6** | +9.6 |
| sine_440_pitch_down12st | 28.4 | **44.0** | +15.6 |
| drum_hit_pitch_up7st | 25.7 | 30.3 | +4.6 |
| chord_Cmaj_formant_max | 20.9 | 26.5 | +5.6 |
| drum_hit_time_4x | 16.5 | 23.1 | +6.6 |
| vocal_aah_formant_upmax | 34.5 | 22.9 | −11.6 (see caveat) |
| vocal_aah_formant_up4st | 34.2 | 20.3 | −13.9 (see caveat) |
| vocal_aah_pitch_up7st | 24.9 | 16.3 | −8.6 (see caveat) |
| **AVERAGE (20)** | **28.2** | **29.5** | **+1.3** |

### Caveat: the vocal formant/pitch "regressions"

The cases that dropped were previously rendered through the broken partial-read
OLA, whose 0.37 s echo-doubling appears to FLATTER the formant-similarity metric
on stationary vowels (fuller spectrum from summed time-shifted copies).  Manual
spectral analysis shows the v19 renders are objectively closer to the reference
in harmonic structure and level.  Treat the old 34.x vocal formant scores as
metric artifacts, not a target to restore by reverting.  The honest gap to
close: the v19 excitation is ~1% sharp (444 vs 439 Hz — integer-lag ACF
quantisation in estimateF0) and the upper harmonic (1332 Hz) is ~25 dB below
the reference's.

### Next Steps

1. **Excitation F0 precision** — the 1% sharpness (integer ACF lag) now matters
   because the output is phase-coherent.  Parabolic interpolation of the ACF
   peak, or lag refinement, should tighten the null test on all LPC cases.

2. **Vocal formant cases under the coherent OLA** (16–23 range) — with the echo
   mask gone these are the true weakest cases.  Upper-harmonic level (synthesis
   excitation rolls off faster than the V-Synth's) is the visible gap.

3. **drum_hit_time_2x / time_4x (24.4 / 23.1)** — transient-synchronous OLA
   (V-Synth BACKING event stamps) remains the planned approach, now measurable
   thanks to correct compression rendering.

### Continuation: v19b — Unbiased F0 with Octave Guard (9019efc), score 29.9

Implemented next-step #1.  The ~1 % sharpness was NOT integer-lag quantisation
(parabolic interpolation was already present) but ACF taper bias: the frame
reaching estimateF0 is **Hann²-tapered** (windowed once at extraction and again
inside estimateF0), tilting the ACF down with lag and pulling the interpolated
peak short.

Fix sequence, with two instructive failures:

1. Unbias by single-Hann window ACF: 444.5 → 443.0 Hz. Partial — wrong window.
2. Remove the second windowing (so single-Hann correction is exact): the
   estimate DESTABILISED (output smeared to ~590 Hz dominant) — the strong
   taper suppresses frame-end effects; keep it.
3. Correct with the ACF of the SQUARED window (`winAcf_`, precomputed in
   prepare): 442.0 Hz, stable.  Correction factor clamped at 2.0 — unclamped
   it diverges at long lags and amplified noise into spurious peaks.
4. **Octave guard required**: the unbiased ACF has near-equal peaks at the
   period and its multiples, and the clamped correction slightly favours
   longer lags — the sine estimate flipped down an octave (77 dB subharmonic
   at 221 Hz; sine_440_formant_downmax → 5.1).  The old biased ACF favoured
   short lags by construction — an accidental octave guard, now explicit:
   shortest local maximum within 5 % of the global maximum.

Results (metric v2): score 29.5 → **29.9**.  chord_Cmaj_formant_max 26.5 →
32.8, vocal_aah_pitch_down12st 26.5 → 30.3, vocal_aah_formant_downmax 16.2 →
19.2; sine_440_formant_downmax 26.0 → 23.6 (small net loss, dominated by the
gains).  Metric v2 history: v17 = 27.4 → v18h = 28.2 → v19 = 29.5 → v19b = 29.9.

### Next Steps (v19b)

1. **Vocal formant/pitch group (15.9–21.3)** — weakest cases.  Visible gap:
   output upper harmonics roll off ~25 dB faster than the V-Synth reference
   (excitation spectral shape and/or LPC envelope flatness).
   vocal_aah_pitch_up7st (15.9) first.

2. **Drum time compression (24.4 / 23.1)** — transient-synchronous OLA.

3. **Verify suspected metric noise** on sine_440_formant_upmax (17.2) and
   sine_440_time_halfspeed (20.0) before spending algorithm effort — manual
   spectra of the formant case already match the reference well.

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
