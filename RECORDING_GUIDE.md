# V-Synth Test Recording Guide

This directory holds WAV recordings captured from the real V-Synth hardware.
These are the "oracle" files that all plugin development is measured against.

## Recording Setup
- Interface: any clean audio interface, 44100 Hz / 24-bit minimum
- V-Synth patch: use a CLEAN patch with NO effects, NO COSM, NO reverb/chorus
  - This isolates pure VariPhrase behavior
  - Document exact patch settings in the log below
- Input signal: feed from a DAW via the V-Synth's audio input
- Capture V-Synth output to DAW track

## V-Synth Patch Settings for Recording
Document here once you've established a clean test patch:
- Patch name: [EMU_TEST]
- Oscillator 1: PCM / EAS / External: [PCM]
- VariPhrase mode: [SOLO]
- Effects: ALL OFF
- COSM: OFF

## Test File Naming Convention
Format: `{signal_type}_{variphrase_setting}_{value}.wav`

Examples:
- `sine_440_pitch_up7st.wav` — 440 Hz sine, pitch shifted +7 semitones
- `vocal_aah_time_2x.wav` — held "aah" vowel, time stretched 2x
- `drum_hit_stretch_4x.wav` — single drum hit, 4x time stretch
- `piano_chord_formant_up4st.wav` — piano chord, formant shifted +4 semitones

## Priority Order for First Recording Session
Record these first — they test the most diagnostic aspects of VariPhrase:

### Priority 1: Sustained tones (clean, analyzable)
- [x ] sine_440_pitch_up7st.wav
- [x ] sine_440_pitch_down12st.wav  
- [x ] sine_440_time_2x.wav
- [x ] sine_440_time_halfspeed.wav
- [x ] sine_440_formant_upmax.wav
- [x ] sine_440_formant_downmax.wav

### Priority 2: Held vowels (formant analysis gold standard)
- [ ] vocal_aah_pitch_up7st.wav
- [ ] vocal_aah_time_2x.wav
- [ ] vocal_aah_formant_up4st.wav  ← most diagnostic
- [ ] vocal_aah_formant_down4st.wav
- [ ] vocal_aah_pitch_up_formant_neutral.wav  ← pitch up without formant change

### Priority 3: Transients (reveal time-stretching artifacts)
- [ ] drum_hit_stretch_2x.wav
- [ ] drum_hit_stretch_4x.wav
- [ ] drum_hit_stretch_halfspeed.wav
- [ ] pluck_stretch_2x.wav

### Priority 4: Polyphonic (hardest for phase vocoders)
- [ ] piano_chord_stretch_2x.wav
- [ ] two_sines_350_440_pitch_up7st.wav
- [ ] piano_chord_pitch_up7st.wav

### Priority 5: Edge cases
- [ ] whisper_pitch_up7st.wav  (unvoiced — tests noise handling)
- [ ] noise_stretch_2x.wav
- [ ] extreme_pitch_up24st.wav
- [ ] extreme_time_4x.wav

## Also Record: Passthrough Reference
For every test file, also record a PASSTHROUGH version (VariPhrase bypassed).
This is the input signal and serves as the reference for the null test baseline.
Store these in `test_files/passthrough/` with the same filenames.
