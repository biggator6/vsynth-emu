# Plugin Outputs Directory

This directory holds WAV files rendered by the offline renderer (`variphrase_render`)
for comparison against V-Synth reference recordings.

## Directory Structure

```
plugin_outputs/
├── hybrid_v17c/       ← current best version (Session 12)
│   └── sustained/     ← matches test_files/sustained/ file names
├── hybrid_v11_clean/  ← pure PV baseline (Sessions 9–11), score 26.9
│   └── sustained/
└── ...                ← older versions retained for regression reference
```

## How to Generate Outputs

See the full guide at `../../RECORDING_GUIDE.md` for the rebuild + render workflow.

Quick render example:

```bash
cd ../../   # project root
BASE=analysis
R="$BASE/variphrase_render"
VER=hybrid_v17c

./plugin/Source/g++_build.sh   # or see ARCHITECTURE.md rebuild command

for case in vocal_aah_time_2x chord_Cmaj_time_2x drum_hit_time_2x; do
  # determine --time/--pitch/--formant from case name and render with --algo hybrid
  echo "render $case ..."
done
```

## Current Scores (hybrid_v17c)

Average composite: **27.2 / 100**
Best case: vocal_aah_time_2x = 45.7
Key improvement vs baseline: chord_Cmaj_time_2x 31.2 → 35.0 (+3.8, WSOLA routing)

See `../../results/hybrid_v17c.json/` for full per-file breakdown.
