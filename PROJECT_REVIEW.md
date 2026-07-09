# Project Review — Session 15 (2026-06-11)

A step-back assessment of the V-Synth VariPhrase reverse-engineering project:
what is working, what is not, and how to proceed.  Written after v25
(score 36.5/100 under metric v3; re-baselined v17 = 27.5).

---

## 1. What is working well

**Process discipline.**  The session-by-session RESEARCH_LOG with explicit
hypotheses, evidence, per-case score tables, and — critically — *do-not-retry
lists with the reasons failures happened* is best-practice empirical
engineering.  Multiple times this session a failed idea was correctly not
re-attempted (or re-attempted only when the underlying engine had changed
enough to invalidate the original failure, e.g. the onset phase-reset retry).

**Isolated engine architecture.**  The standalone C++ engine callable from
Python via `variphrase_render` (no JUCE, no DAW) is the single best structural
decision in the project.  The entire Session 15 iterate-measure loop —
26 commits, ~15 experiments — was only possible because a render-and-score
cycle takes seconds.

**The black-box methodology found real things.**  Confirmed source-filter
resynthesis (Session 3), encode-pass content classification (Session 12), the
resynthesize-everything discovery, the formant-knob saturation (~0.75×
upward), and event stamps (Session 15).  These are genuine reverse-engineering
results, not curve fits.

**Honest bookkeeping under metric changes.**  When the metric was fixed
(alignment v2, gain-matching v3), old baselines were re-scored rather than
compared across incompatible metrics.  Score history is annotated by metric
version.

## 2. The core problem: the metric no longer matches the goal

The stated goal is "perceptually indistinguishable."  The composite score
cannot measure that, and the 60/100 target is likely **unreachable by
construction**:

- **35 % phase-sensitive null test.**  Session 15 proved the V-Synth
  RESYNTHESIZES (its time-stretched sine reference carries sawtooth harmonics
  the input lacks).  Output phase therefore never matches the reference, and
  the SNR term (full marks at 60 dB; best cases reach ~20 dB) is mostly
  unreachable regardless of perceptual quality.
- **40 % formant-trajectory similarity applied to all content**, including
  sines, drums and chords where formants are meaningless.  Demonstrated
  insensitivity: the formant-downmax render matches the reference envelope
  almost exactly (492/1395/2139 Hz vs 463/1441/2227) and four independent
  interventions could not move its score.
- **25 % transient preservation**, ≈ 0 on sustained content by nature.

Per-content arithmetic puts the realistic per-case ceiling around 50–65.
vocal_aah_time_2x (57.7) is probably near the achievable maximum.  Late
Session 15 iterations were increasingly optimizing against metric noise.

**No listening notes exist anywhere in the log.**  For an audio project, ears
are ground truth; the entire record is numbers.  At least one reverted
"regression" (the level-correct normalization under metric v2) was almost
certainly an audible improvement rejected by a gain-sensitive metric.

## 3. Recommendations (priority order)

1. **Fix the measurement before more algorithm work.**
   a. Add phase-blind spectral metrics (multi-resolution STFT distance,
      log-mel distance) — the standard tools for evaluating resynthesis.
   b. Weight the composite per content class (formant term for speech only,
      transient term for percussive only).
   c. Embed audio players in the HTML batch report for one-click A/B
      listening; start recording listening impressions in the log.
   d. Measure the metric ceiling: score two hardware captures of the SAME
      setting against each other (Round-2 wishlist item ⑥).  Recalibrate the
      target from that number.
   → Items a–c implemented as **metric v4** in this session; d needs hardware.

2. **Search the patent literature.**  VariPhrase shipped in 2000 (VP-9000);
   Roland filed patents covering time-stretch with event markers and
   formant-preserving pitch shift.  Fifteen sessions of black-box inference
   may be verifiable — or correctable — against the filed descriptions in an
   afternoon.  The single most leveraged untried action.

3. **Record the Round-2 wishlist** (RECORDING_GUIDE.md), calibration sweep and
   duplicate takes first — both feed recommendation 1.

4. **Engine consolidation pass.**  Dead WSOLA code, sediment-layered routing
   conditions, and hard-won invariants (OLA coherence, ring gating, drain
   exactness) protected only by the slow 20-case batch.  Add fast C++ unit
   tests for the invariants (synthetic sine in → assert F0/level/duration
   out).  Tag git releases per version; stop reusing the `hybrid_v18d` output
   directory for later versions.

5. **Reframe the target** once the ceiling is known: "X % of measured ceiling
   per content class" plus a blind-listening criterion, not a raw 60/100.

## 4. One-line summary

The algorithm work has caught up with the measurement apparatus.  The next
points come from better ground truth (recordings, patents, ears) and a metric
that can see improvements already being made — not from more iterations
against the current score.

---

## Status addendum (end of Session 15)

Every recommendation except the hardware-dependent ones was executed the same
session:

1. **Metric fixed** — metric v4 (spectral similarity, content-aware weights,
   A/B audio players).  ✅
2. **Patent search** — paid off beyond expectation: all three Roland
   VariPhrase patents found and IMPLEMENTED (granular SOLO engine, subband
   ENSEMBLE stretch, event-stamp BACKING path).  See research/PATENTS.md.  ✅
3. **Round-2 recordings** — guide updated; awaiting hardware.  ⏳
4. **Consolidation + unit tests** — EngineTests.cpp (10 invariant checks,
   all passing); version tags; ARCHITECTURE.md refreshed.  ✅
5. **Target reframing** — pending the duplicate-take ceiling measurement.  ⏳

Score at review time: 36.5 (v3) ≈ 47.8 (v4).  Score at session close:
**51.8 (v4)** — the review's central claim (points were waiting in ground
truth and measurement, not algorithm iteration) was confirmed within hours:
the patents alone were worth ~4 points and rewrote the architecture.
