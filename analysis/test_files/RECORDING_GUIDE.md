# V-Synth Test Files Directory

See the full recording guide at the project root: `../../RECORDING_GUIDE.md`

This directory holds V-Synth reference WAV recordings:

```
passthrough/      ← unprocessed inputs (raw signals, V-Synth parameters all zero)
sustained/        ← V-Synth processed outputs (all 20 current test cases)
```

## Current Test Battery — 20 Files (all in sustained/)

### Vocal "aah" — 7 files (current scores, hybrid_v17c)
| File | Params | Score |
|---|---|---|
| vocal_aah_time_2x.wav | Time 2× | **45.7** ★ |
| vocal_aah_time_halfspeed.wav | Time 0.5× | 32.5 |
| vocal_aah_formant_upmax.wav | Formant +12 st | 34.4 |
| vocal_aah_formant_up4st.wav | Formant +4 st | 28.5 |
| vocal_aah_formant_downmax.wav | Formant −12 st | 17.0 |
| vocal_aah_pitch_up7st.wav | Pitch +7 st | 19.0 |
| vocal_aah_pitch_down12st.wav | Pitch −12 st | 27.5 |

### Drum hit — 4 files
| File | Params | Score |
|---|---|---|
| drum_hit_time_2x.wav | Time 2× | 19.7 |
| drum_hit_time_4x.wav | Time 4× | 19.8 |
| drum_hit_time_halfspeed.wav | Time 0.5× | 36.1 |
| drum_hit_pitch_up7st.wav | Pitch +7 st | 15.4 |

### Chord C major — 3 files
| File | Params | Score |
|---|---|---|
| chord_Cmaj_time_2x.wav | Time 2× | **35.0** (WSOLA) |
| chord_Cmaj_pitch_up7st.wav | Pitch +7 st | 25.1 |
| chord_Cmaj_formant_max.wav | Formant +12 st | 17.9 |

### Sine 440 Hz — 6 files
| File | Params | Score |
|---|---|---|
| sine_440_pitch_up7st.wav | Pitch +7 st | 36.3 |
| sine_440_pitch_down12st.wav | Pitch −12 st | 26.7 |
| sine_440_time_2x.wav | Time 2× | 20.1 |
| sine_440_time_halfspeed.wav | Time 0.5× | 28.6 |
| sine_440_formant_upmax.wav | Formant +12 st | 32.8 |
| sine_440_formant_downmax.wav | Formant −12 st | 25.4 |

**Overall average: 27.2 / 100** (hybrid_v17c, Session 12)

## Passthrough Files (in passthrough/)

| File | Used for |
|---|---|
| vocal_aah_passthrough.wav | All 7 vocal_aah test cases |
| drum_hit_passthrough.wav | All 4 drum_hit test cases |
| chord_Cmaj_passthrough.wav | All 3 chord_Cmaj test cases |
| sine_440_*.wav | Corresponding sine test case (all byte-identical) |

---

**Round 2 wishlist** (updated after v27, score 51.5): see the root
`RECORDING_GUIDE.md`.  Priorities: ① formant sweep (vocal at −9/−6/−3/+2/+3/
+6/+9 st — now directly validates the US6421642 window clamp), ② "ee" vowel +
higher-F0 voice, ③ multi-onset drum loop, ④ sustained pad chord (validates the
v27 subband engine), ⑤ moving-pitch phrase, ⑥ duplicate takes (metric-v4
ceiling).  Minimum useful set: ① + ⑥.  Passthrough per source, `_dry` suffix,
~0.5 s lead-ins, ≥4 s steady content.
