"""
batch_test.py — Automated regression test runner for VariPhrase emulator.

Runs compare.py across the entire test_files/ battery and produces:
  - Console summary table
  - JSON results file (for tracking progress over sessions)
  - HTML report with all spectrograms embedded

Usage:
    python batch_test.py --ref-dir test_files/ --plugin-dir plugin_outputs/ [--output results/]
    python batch_test.py --report results/batch_TIMESTAMP.json   # view previous results
    python batch_test.py --baseline                              # run passthrough baseline

Directory structure expected:
    test_files/
        sustained/
            sine_440_pitchup.wav
            ...
        transients/
            drum_hit_stretch_2x.wav
            ...
    plugin_outputs/        ← your plugin renders go here, same filenames
        sustained/
            sine_440_pitchup.wav
            ...
"""

import argparse
import json
import os
import glob
import sys
import datetime
import numpy as np

# Add analysis dir to path
sys.path.insert(0, os.path.dirname(__file__))
from compare import analyze, load_wav


# ---------------------------------------------------------------------------
# Batch Runner
# ---------------------------------------------------------------------------

def find_test_pairs(ref_dir: str, plugin_dir: str) -> list[tuple[str, str]]:
    """
    Walk ref_dir, find matching files in plugin_dir.
    Returns list of (ref_path, plugin_path) tuples.
    """
    pairs = []
    for ref_path in sorted(glob.glob(os.path.join(ref_dir, '**', '*.wav'), recursive=True)):
        rel = os.path.relpath(ref_path, ref_dir)
        plugin_path = os.path.join(plugin_dir, rel)
        if os.path.exists(plugin_path):
            pairs.append((ref_path, plugin_path))
        else:
            print(f"  WARNING: No matching plugin output for {rel}")
    return pairs


def run_batch(ref_dir: str, plugin_dir: str, output_dir: str) -> dict:
    """Run analysis on all test pairs, return aggregated results."""
    os.makedirs(output_dir, exist_ok=True)

    pairs = find_test_pairs(ref_dir, plugin_dir)
    if not pairs:
        print(f"ERROR: No matching test pairs found in {ref_dir} / {plugin_dir}")
        sys.exit(1)

    print(f"\n{'='*70}")
    print(f"  Batch test: {len(pairs)} file pairs")
    print(f"  Reference: {ref_dir}")
    print(f"  Plugin output: {plugin_dir}")
    print(f"{'='*70}")

    results = []
    for ref_path, plugin_path in pairs:
        spec_dir = os.path.join(output_dir, 'spectrograms')
        result = analyze(ref_path, plugin_path, output_dir=spec_dir)
        result['ref_path'] = ref_path
        result['plugin_path'] = plugin_path
        results.append(result)

    return results


# ---------------------------------------------------------------------------
# Reporting
# ---------------------------------------------------------------------------

def print_summary_table(results: list[dict]):
    """Print a formatted summary table to console."""
    if not results:
        return

    col_w = [38, 10, 8, 8, 8, 8, 8]
    headers = ['Test File', 'Null dBFS', 'SNR dB', 'Spectral', 'Formant', 'Transnt', 'SCORE']
    sep = '+' + '+'.join('-' * (w + 2) for w in col_w) + '+'
    row_fmt = '| ' + ' | '.join(f'{{:<{w}}}' for w in col_w) + ' |'

    print(f"\n{'='*80}")
    print("  BATCH TEST RESULTS")
    print(f"{'='*80}")
    print(sep)
    print(row_fmt.format(*headers))
    print(sep)

    scores = []
    for r in results:
        name = os.path.basename(r['test_file'])[:38]
        null_db = f"{r['null_rms_dbfs']:.1f}"
        snr = f"{r['snr_db']:.1f}"
        spectral = f"{r.get('spectral_similarity', float('nan')):.3f}"
        formant = f"{r['formant_similarity']:.3f}"
        transient = f"{r['transient_score']:.3f}"
        score = f"{r['composite_score']:.1f}"
        scores.append(r['composite_score'])
        print(row_fmt.format(name, null_db, snr, spectral, formant, transient, score))

    print(sep)
    avg = np.mean(scores)
    print(row_fmt.format('AVERAGE', '', '', '', '', '', f"{avg:.1f}"))
    print(sep)
    print(f"\n  Overall composite score: {avg:.1f}/100")

    # Grade
    if avg >= 80:
        grade = "🟢 Excellent — perceptually very close"
    elif avg >= 60:
        grade = "🟡 Good — close but audible differences on some material"
    elif avg >= 40:
        grade = "🟠 Fair — significant work still needed"
    else:
        grade = "🔴 Early stage — algorithm needs major development"
    print(f"  {grade}\n")


def save_json_results(results: list[dict], output_dir: str) -> str:
    """Save results to timestamped JSON file."""
    timestamp = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    path = os.path.join(output_dir, f'batch_{timestamp}.json')

    avg_score = np.mean([r['composite_score'] for r in results])
    output = {
        'timestamp': timestamp,
        'average_composite_score': avg_score,
        'n_tests': len(results),
        'results': results,
    }

    # Remove numpy types for JSON serialization
    def clean(obj):
        if isinstance(obj, (np.float32, np.float64)):
            return float(obj)
        if isinstance(obj, np.ndarray):
            return obj.tolist()
        return obj

    output_clean = json.loads(json.dumps(output, default=clean))
    with open(path, 'w') as f:
        json.dump(output_clean, f, indent=2)

    print(f"  Results saved: {path}")
    return path


def save_html_report(results: list[dict], json_path: str, output_dir: str):
    """Generate an HTML report with embedded spectrograms."""
    import base64

    timestamp = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    avg_score = np.mean([r['composite_score'] for r in results])

    rows = ''
    for r in results:
        name = os.path.basename(r['test_file'])
        score_color = ('#4ecdc4' if r['composite_score'] >= 70
                       else '#ffd93d' if r['composite_score'] >= 40
                       else '#ff6b6b')

        # Embed spectrogram inline if it exists
        img_tag = ''
        if 'spectrogram_path' in r and os.path.exists(r['spectrogram_path']):
            with open(r['spectrogram_path'], 'rb') as f:
                img_b64 = base64.b64encode(f.read()).decode()
            img_tag = f'<img src="data:image/png;base64,{img_b64}" style="width:100%;margin-top:10px">'

        # A/B audio players (metric v4): numbers can't hear — every batch run
        # should be one click away from comparing reference vs render by ear.
        # Relative paths so the report works wherever the repo is checked out.
        audio_tag = ''
        if r.get('ref_path') and r.get('plugin_path'):
            ref_rel = os.path.relpath(os.path.abspath(r['ref_path']),
                                      os.path.abspath(output_dir))
            out_rel = os.path.relpath(os.path.abspath(r['plugin_path']),
                                      os.path.abspath(output_dir))
            audio_tag = (
                f'<div style="display:flex;gap:24px;align-items:center;margin-top:6px">'
                f'<span style="color:#4ecdc4">V-Synth ref</span>'
                f'<audio controls preload="none" src="{ref_rel}"></audio>'
                f'<span style="color:#ffd93d">our render</span>'
                f'<audio controls preload="none" src="{out_rel}"></audio>'
                f'</div>'
            )

        spec_sim = r.get('spectral_similarity', float('nan'))
        cclass   = r.get('content_class', '?')
        rows += f"""
        <tr>
          <td>{name}<br><span style="color:#666">{cclass}</span></td>
          <td>{r['null_rms_dbfs']:.1f}</td>
          <td>{r['snr_db']:.1f}</td>
          <td>{spec_sim:.3f}</td>
          <td>{r['formant_similarity']:.3f}</td>
          <td>{r['transient_score']:.3f}</td>
          <td style="color:{score_color};font-weight:bold">{r['composite_score']:.1f}</td>
        </tr>
        <tr><td colspan="7">{audio_tag}{img_tag}</td></tr>
        """

    html = f"""<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<title>VariPhrase Emulator — Batch Test Report</title>
<style>
  body {{ background: #1a1a2e; color: #eee; font-family: monospace; margin: 40px; }}
  h1 {{ color: #00ff88; }}
  h2 {{ color: #4ecdc4; border-bottom: 1px solid #333; padding-bottom: 8px; }}
  .score {{ font-size: 2em; color: #00ff88; }}
  table {{ border-collapse: collapse; width: 100%; margin-top: 20px; }}
  th {{ background: #0d0d1a; padding: 10px; text-align: left; color: #4ecdc4; }}
  td {{ padding: 8px 10px; border-bottom: 1px solid #222; }}
  tr:hover {{ background: #1e1e3a; }}
</style>
</head>
<body>
  <h1>VariPhrase Emulator — Batch Test Report</h1>
  <p>Generated: {timestamp}</p>
  <p>Tests: {len(results)}</p>
  <div class="score">Score: {avg_score:.1f}/100</div>

  <h2>Results</h2>
  <table>
    <tr>
      <th>Test File</th>
      <th>Null RMS (dBFS)</th>
      <th>SNR (dB)</th>
      <th>Spectral Sim</th>
      <th>Formant Similarity</th>
      <th>Transient Score</th>
      <th>Composite Score</th>
    </tr>
    {rows}
  </table>

  <p style="margin-top:40px;color:#666">Raw data: {json_path}</p>
</body>
</html>"""

    ts = datetime.datetime.now().strftime('%Y%m%d_%H%M%S')
    html_path = os.path.join(output_dir, f'report_{ts}.html')
    with open(html_path, 'w') as f:
        f.write(html)
    print(f"  HTML report: {html_path}")
    return html_path


def compare_to_previous(current_avg: float, output_dir: str):
    """Find the most recent previous JSON result and show progress."""
    jsons = sorted(glob.glob(os.path.join(output_dir, 'batch_*.json')))
    if len(jsons) < 2:
        return
    prev_path = jsons[-2]
    with open(prev_path) as f:
        prev = json.load(f)
    prev_avg = prev.get('average_composite_score', 0)
    delta = current_avg - prev_avg
    arrow = '▲' if delta > 0 else '▼'
    print(f"  Progress vs previous run: {arrow} {abs(delta):.1f} points "
          f"({prev_avg:.1f} → {current_avg:.1f})")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='VariPhrase batch regression test runner')
    parser.add_argument('--ref-dir', default='test_files',
                        help='Directory containing V-Synth reference WAVs')
    parser.add_argument('--plugin-dir', default='plugin_outputs',
                        help='Directory containing plugin output WAVs (same structure)')
    parser.add_argument('--output', '-o', default='results',
                        help='Output directory for reports')
    parser.add_argument('--report', help='View a previous JSON results file')
    parser.add_argument('--baseline', action='store_true',
                        help='Run baseline test (plugin_outputs = test_files, passthrough)')
    args = parser.parse_args()

    if args.report:
        with open(args.report) as f:
            data = json.load(f)
        print_summary_table(data['results'])
        sys.exit(0)

    ref_dir = args.ref_dir
    plugin_dir = args.ref_dir if args.baseline else args.plugin_dir

    if not os.path.exists(ref_dir):
        print(f"ERROR: Reference directory not found: {ref_dir}")
        print("Put your V-Synth WAV recordings in test_files/ (see ARCHITECTURE.md)")
        sys.exit(1)

    results = run_batch(ref_dir, plugin_dir, args.output)
    print_summary_table(results)
    json_path = save_json_results(results, args.output)
    save_html_report(results, json_path, args.output)

    avg = np.mean([r['composite_score'] for r in results])
    compare_to_previous(avg, args.output)
