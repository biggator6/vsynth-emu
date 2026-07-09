# Roland VariPhrase Patent Findings (Session 15, 2026-06-11)

Patent-literature search per PROJECT_REVIEW.md recommendation #2.  Three
Roland Corporation patents filed February–May 2000 (the VP-9000 launch
window) describe the VariPhrase system directly.  **The core playback patent
rewrites our understanding of the SOLO algorithm.**

---

## US6421642B1 — THE VariPhrase playback patent

**"Device and method for reproduction of sounds with independently variable
duration and pitch"** — Roland Corp, filed 2000-05-02, granted 2002-07-16.
https://patents.google.com/patent/US6421642B1/en

### The actual algorithm (pitch-synchronous granular, PSOLA-family)

**Encode pass** stores, per sample phrase:
- *Cut waveforms*: the phrase segmented into ~one-pitch-period grains, each
  with a cut start address and its measured pitch (`cwp`, cut waveform pitch).
- *Syllable marks*: indices of cut boundaries that start a syllable
  (vowel transition) — our "event stamps," confirmed.

**Playback** — three fully orthogonal controls:

| Axis | Mechanism |
|---|---|
| **Time** (`tcv`) | Playing position `pp` advances by `tcv` per output sample; `pp` selects WHICH cut waveform is current.  Time stretch = how fast you walk the grain list.  Nothing else changes. |
| **Pitch** (`pr`) | Grains are RE-TRIGGERED at the target period `ppw = cwp × pr`.  Two processing channels trigger half a cycle apart with triangular windows (sum = constant), each reading the current cut waveform. |
| **Formant** (`fsv`) | Grain READ VELOCITY.  Each grain's content is resampled by `fsv` (read address = counter × fsv), scaling the spectral envelope without changing the repetition rate.  Window length `wl = cwp / fsv`, **clamped to ≤ ppw**. |

### Why this explains every black-box finding

1. **Resynthesis harmonics on time-stretched sine** (Session 15): grain
   re-triggering at F0 = an impulse train shaped by the grain spectrum →
   harmonic series even from a pure-sine source.  Confirmed mechanism.
2. **Independent axes**: trivial in this architecture — each axis touches a
   different variable.
3. **Formant-knob saturation (~0.75× upward, measured)**: the window-length
   clamp (`wl ≤ ppw`) plus the window's spectral convolution bound the
   effective envelope shift at extreme settings.  Mechanistic explanation for
   what we calibrated empirically.
4. **Syllable marks = event stamps**: BACKING-mode onset behaviour and the
   stop/jump-at-syllable features are stored cut-list annotations.
5. **Our LPC source-filter is an approximation**: the V-Synth does NOT do
   LPC.  Grain-resampling formant shift and grain-retrigger pitch shift
   produce source-filter-LIKE behaviour (envelope and excitation decoupled)
   by time-domain means.

## US6564187B1 — multiband time stretch (polyphonic path)

**"Waveform signal compression and expansion along time axis having
different sampling rates for different main-frequency bands"** — Roland,
filed 2000-03-28.  https://patents.google.com/patent/US6564187

Octave-cascade filter bank (successive half-band lowpass + decimate), each
band split into 4 sub-bands; per-sub-band amplitude + instantaneous
frequency extraction; time modification by segment culling/repetition;
resynthesis by successive upsampling.  This is the plausible
**ENSEMBLE/BACKING time-stretch path** — pitch-synchronous cutting is
impossible on polyphony, so Roland used a subband vocoder there.

## US6201175B1 — band-wise sinusoidal modelling

**"Waveform reproduction apparatus"** — Roland, filed 2000-02-22.
https://patents.google.com/patent/US6201175B1/en

Bands of adjacent harmonics → per-band amplitude/phase trajectories →
time-scale by duplicating/omitting amplitude segments at zero-crossing
marks with crossfades → cosine-bank resynthesis.  A sinusoidal-modelling
variant; possibly an alternative or earlier approach to the same problem.

---

## Implications for the project

1. **The SOLO/LITE emulation should be re-architected as pitch-synchronous
   granular** per US6421642.  We already have every prerequisite from the
   existing engine: per-frame F0 (estimateF0), voiced detection, onset
   stamps, and an offline encode pass.  The encode step (cut the phrase into
   pitch-period grains, store per-grain pitch) is straightforward offline
   work.  Expected wins: the vocal group (spectral sim ~0.3, our weakest
   cluster) — grain resynthesis reproduces the reference's actual synthesis
   mechanism instead of approximating it with LPC.
2. **Keep PV for ENSEMBLE** (or eventually implement the US6564187 subband
   method); the chord/polyphonic references may match the subband character
   better than our PV.
3. **The formant clamp** (`wl ≤ ppw`) should replace the empirical 0.75×
   calibration once the granular path exists.
4. Fifteen sessions of black-box inference were **directionally right**
   (resynthesis, encode pass, event stamps, independent axes, envelope/
   excitation decoupling) but wrong about the mechanism (LPC vs granular).
   The black-box scores were nonetheless what located the correct patents.
