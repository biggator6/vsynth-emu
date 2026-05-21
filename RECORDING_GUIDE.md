# V-Synth Test Recording Guide

This file is the master recording reference.  After each session add new files to the
checklist and update `RESEARCH_LOG.md` with results.

---

## Recording Chain

```
DAW (playback) → V-Synth audio input (rear panel) → V-Synth audio output → DAW (capture)
```

- Sample rate: **44100 Hz**
- Bit depth: **16-bit PCM** (matches existing test files; 24-bit also fine — batch_test handles both)
- Channels: **mono** (V-Synth VariPhrase is mono in / mono out when Oscillator is set to External)
- Trim: set so the passthrough signal peaks around **−3 to −6 dBFS** — not too quiet, not clipping

---

## V-Synth Patch: [EMU_TEST]

Document current settings here; update if anything changes between sessions.

| Parameter | Setting | Note |
|---|---|---|
| Oscillator 1 type | **External (EXT)** | Routes audio input through VariPhrase |
| Oscillator 2 | Off | Avoids contamination from onboard PCM |
| VariPhrase mode | **SOLO** | Single-voice; deterministic behavior |
| Effects (FX) | **ALL OFF** | Isolates VariPhrase algorithm |
| COSM | **OFF** | |
| Reverb / Chorus | **OFF** | |
| TVF (filter) | Open / flat | Set cutoff to max, resonance 0 |
| TVA (amp) | Unity gain | No envelope modulation |
| VariPhrase Pitch | As needed | Set per test case |
| VariPhrase Time | As needed | Set per test case |
| VariPhrase Formant | As needed | Set per test case |

**Critical:** zero all VariPhrase parameters first, confirm passthrough is clean (null test
should show > 40 dB null when comparing passthrough to input), THEN set the test parameter.

---

## File Naming Convention

```
{signal}_{descriptor}_{parameter}_{value}.wav
```

Examples:
- `vocal_aah_formant_up12st.wav`
- `drum_hit_time_2x.wav`
- `chord_Cmaj_pitch_up7st.wav`

For the matching passthrough (V-Synth bypassed or all params at zero), use the same name
but replace the parameter section with `passthrough`:
- `vocal_aah_passthrough.wav`
- `drum_hit_passthrough.wav`

---

## Directory Layout

```
analysis/test_files/
├── passthrough/      ← all unprocessed reference inputs (per signal type)
├── sustained/        ← held vowels + any other sustained processed outputs
├── transients/       ← drum hits, plucks, clicks
├── polyphonic/       ← chords, multi-pitch material
└── edge_cases/       ← whisper, noise, extreme settings
```

Every processed file in `sustained/`, `transients/`, or `polyphonic/` must have a matching
passthrough file in `passthrough/` with the same base name.  The offline renderer takes the
passthrough as its `--input`.

---

## Completed Recordings

### Priority 1: Sustained sine (done)

| File | Location | Status |
|---|---|---|
| sine_440_pitch_up7st.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_pitch_down12st.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_time_2x.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_time_halfspeed.wav | sustained/ + passthrough/ | ✅ done |
| sine_440_formant_upmax.wav (+12 st) | sustained/ + passthrough/ | ✅ done |
| sine_440_formant_downmax.wav (−12 st) | sustained/ + passthrough/ | ✅ done |

---

## Next Recording Session — Priority 2 & 3

These are the highest-leverage recordings given current algorithm state.

### Signal A — Held vowel "aah" (sustained, ~4 seconds)

**Source:** sing or speak a steady "aah" (or feed a synthesized vowel) into the V-Synth.
Aim for F0 ≈ 200–250 Hz (comfortable male pitch), steady throughout.

Passthrough first (parameters all at zero, VariPhrase bypassed or zeroed):
- [ ] `passthrough/vocal_aah_passthrough.wav`

Then record processed versions — **change only one VariPhrase parameter at a time**:

| File | VariPhrase Pitch | VariPhrase Time | VariPhrase Formant | Dir |
|---|---|---|---|---|
| `vocal_aah_formant_up12st.wav` | 0 | 1.0× | **+12 st** | sustained/ |
| `vocal_aah_formant_down12st.wav` | 0 | 1.0× | **−12 st** | sustained/ |
| `vocal_aah_formant_up4st.wav` | 0 | 1.0× | **+4 st** | sustained/ |
| `vocal_aah_pitch_up7st.wav` | **+7 st** | 1.0× | 0 | sustained/ |
| `vocal_aah_pitch_down12st.wav` | **−12 st** | 1.0× | 0 | sustained/ |
| `vocal_aah_time_2x.wav` | 0 | **2.0×** | 0 | sustained/ |
| `vocal_aah_time_halfspeed.wav` | 0 | **0.5×** | 0 | sustained/ |

**Why these?**
- `formant_up12st` / `formant_down12st` directly replicate the existing formant_upmax /
  formant_downmax test cases but on real speech — expected to expose the adaptive-LPC
  improvement (and explain the formant_upmax regression on pure sine).
- `pitch_up7st` / `pitch_down12st` replicate existing pitch cases on real speech.
- `time_2x` / `time_halfspeed` replicate existing time cases on real speech.


### Signal B — Single drum hit (transient)

**Source:** a single kick drum or snare hit.  Feed a clean, isolated hit with silence
before and after.  The transient should start at t ≈ 0.5 s (leave 0.5 s of silence at
the start so our onset detector has a baseline).

Passthrough first:
- [ ] `passthrough/drum_hit_passthrough.wav`

Processed:

| File | VariPhrase Pitch | VariPhrase Time | VariPhrase Formant | Dir |
|---|---|---|---|---|
| `drum_hit_time_2x.wav` | 0 | **2.0×** | 0 | transients/ |
| `drum_hit_time_halfspeed.wav` | 0 | **0.5×** | 0 | transients/ |
| `drum_hit_pitch_up7st.wav` | **+7 st** | 1.0× | 0 | transients/ |
| `drum_hit_time_4x.wav` | 0 | **4.0×** | 0 | transients/ |

**Why these?**
- Transient time-stretch directly exercises the onset detection path (first time it
  will actually fire in the test suite).
- time_4x is an extreme stretch that will reveal OLA smearing artifacts.


### Signal C — Chord (polyphonic)

**Source:** a short piano or guitar chord (C major or similar), held 2–3 seconds.
Feed via audio input into External VariPhrase mode.

Passthrough first:
- [ ] `passthrough/chord_Cmaj_passthrough.wav`

Processed:

| File | VariPhrase Pitch | VariPhrase Time | VariPhrase Formant | Dir |
|---|---|---|---|---|
| `chord_Cmaj_pitch_up7st.wav` | **+7 st** | 1.0× | 0 | polyphonic/ |
| `chord_Cmaj_time_2x.wav` | 0 | **2.0×** | 0 | polyphonic/ |
| `chord_Cmaj_formant_up4st.wav` | 0 | 1.0× | **+4 st** | polyphonic/ |

**Why these?**
- Polyphonic pitch shift is a known weakness of LPC (designed for monophonic voiced
  speech).  This will reveal whether the PV path handles chords better.
- formant_up4st on a chord tests whether the biquad formant shift makes musical
  sense on harmonic-rich polyphonic content.

---

## Rendering Plugin Outputs After Recording

Once you have the new files, run the renderer for each one:

```bash
BASE=/path/to/vsynth-emu
R="$BASE/analysis/variphrase_render"

# Example: vocal_aah_formant_up12st — use LPC (hasFormant = true)
"$R" --input  "$BASE/analysis/test_files/passthrough/vocal_aah_passthrough.wav" \
     --output "$BASE/analysis/plugin_outputs/hybrid_v6/sustained/vocal_aah_formant_up12st.wav" \
     --algo lpc --formant 12

# Example: vocal_aah_pitch_up7st — use PV (no formant shift)
"$R" --input  "$BASE/analysis/test_files/passthrough/vocal_aah_passthrough.wav" \
     --output "$BASE/analysis/plugin_outputs/hybrid_v6/sustained/vocal_aah_pitch_up7st.wav" \
     --algo pv --pitch 7

# Example: drum_hit_time_2x — use PV
"$R" --input  "$BASE/analysis/test_files/passthrough/drum_hit_passthrough.wav" \
     --output "$BASE/analysis/plugin_outputs/hybrid_v6/transients/drum_hit_time_2x.wav" \
     --algo pv --time 2.0
```

**Hybrid routing reminder:**
- `|formantShift| > 0.5 st` → `--algo lpc`
- All other cases → `--algo pv`

Then run the full batch test:

```bash
python3 analysis/batch_test.py \
    --ref-dir    analysis/test_files \
    --plugin-dir analysis/plugin_outputs/hybrid_v6 \
    --output     analysis/results/hybrid_v6
```

The batch runner walks `test_files/` recursively and matches any file that also exists in
`plugin_outputs/hybrid_v6/` at the same relative path.  Passthrough files produce warnings
(no matching plugin output) — that is expected.

---

## Tips for Clean Recordings

1. **Zero all VariPhrase parameters before each take.** Confirm with a passthrough null
   test before setting the test parameter.
2. **Record a few seconds of silence before playing the signal.** This lets the batch
   test measure the noise floor.
3. **Use the same audio interface and cable for all takes in a session** so the output
   chain is consistent.
4. **Label takes immediately** — do not rely on memory.  Record the V-Synth panel display
   settings in a photo alongside the audio file.
5. **Check the recording with compare.py before moving on** — a quick null test against
   the passthrough reveals miswired settings immediately.
