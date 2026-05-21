# V-Synth Test Recording Guide (in-tree copy)

See the full guide at the project root: `../../RECORDING_GUIDE.md`

This directory holds V-Synth reference WAV recordings organised as:

```
passthrough/      ← unprocessed inputs (source signals, V-Synth bypassed)
sustained/        ← sustained processed outputs (held vowels, sine tones)
transients/       ← drum hits, plucks, clicks
polyphonic/       ← chords, multi-pitch material
edge_cases/       ← whisper, noise, extreme settings
```

## Quick Checklist — Priority 2/3 (next recording session)

### Vowel "aah" (sustained, ~4 s, F0 ≈ 200–250 Hz)
- [ ] `passthrough/vocal_aah_passthrough.wav`
- [ ] `sustained/vocal_aah_formant_up12st.wav`   (Formant +12 st)
- [ ] `sustained/vocal_aah_formant_down12st.wav`  (Formant −12 st)
- [ ] `sustained/vocal_aah_formant_up4st.wav`     (Formant +4 st)
- [ ] `sustained/vocal_aah_pitch_up7st.wav`       (Pitch +7 st)
- [ ] `sustained/vocal_aah_pitch_down12st.wav`    (Pitch −12 st)
- [ ] `sustained/vocal_aah_time_2x.wav`           (Time 2.0×)
- [ ] `sustained/vocal_aah_time_halfspeed.wav`    (Time 0.5×)

### Drum hit (single kick or snare, silence before onset)
- [ ] `passthrough/drum_hit_passthrough.wav`
- [ ] `transients/drum_hit_time_2x.wav`           (Time 2.0×)
- [ ] `transients/drum_hit_time_halfspeed.wav`    (Time 0.5×)
- [ ] `transients/drum_hit_time_4x.wav`           (Time 4.0×)
- [ ] `transients/drum_hit_pitch_up7st.wav`       (Pitch +7 st)

### Chord — C major piano or guitar (held 2–3 s)
- [ ] `passthrough/chord_Cmaj_passthrough.wav`
- [ ] `polyphonic/chord_Cmaj_pitch_up7st.wav`     (Pitch +7 st)
- [ ] `polyphonic/chord_Cmaj_time_2x.wav`         (Time 2.0×)
- [ ] `polyphonic/chord_Cmaj_formant_up4st.wav`   (Formant +4 st)

## Completed
- [x] All 6 `sine_440_*.wav` processed files (sustained/) + matching passthrough files
