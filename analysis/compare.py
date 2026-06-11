"""
compare.py — Core analysis tool for VariPhrase reverse engineering.

Given two WAV files (V-Synth reference and plugin output), produces:
  - Null test residual level (dBFS)
  - Side-by-side spectrogram comparison image
  - LPC formant trajectory comparison
  - Transient smearing metric
  - Composite similarity score

Usage:
    python compare.py <reference.wav> <plugin_output.wav> [--output results/]
    python compare.py --validate  # sanity check: compare file with itself
"""

import argparse
import os
import sys
import numpy as np
import librosa
import librosa.display
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from scipy.io import wavfile
from scipy.signal import find_peaks
import warnings
warnings.filterwarnings('ignore')

from lpc import extract_formants, formant_trajectory_similarity


# ---------------------------------------------------------------------------
# Audio I/O
# ---------------------------------------------------------------------------

def load_wav(path: str) -> tuple[np.ndarray, int]:
    """Load a WAV file, return (mono float32 array, sample_rate).

    Uses librosa.load (backed by soundfile) so that 24-bit PCM, 32-bit PCM,
    and 48 kHz files are all handled correctly.  librosa returns float32 data
    already normalised to [-1, 1] and downmixed to mono by default.

    The previous scipy.io.wavfile approach had a latent bug: for stereo integer
    files, data.mean(axis=1) returns float64, bypassing the integer-normalisation
    branch and leaving raw ±10^9 values in the array — causing impossibly large
    null-test residuals (160+ dBFS) for 24-bit stereo recordings.
    """
    data, sr = librosa.load(path, sr=None, mono=True)
    return data.astype(np.float32), sr


def align_length(ref: np.ndarray, out: np.ndarray,
                 sr: int = 48000) -> tuple[np.ndarray, np.ndarray]:
    """Time-align via envelope cross-correlation, then trim to equal length.

    Session 14 fix: the original implementation only truncated to the shorter
    file.  The V-Synth reference recordings have lead-in silence of up to
    ~2.6 s (drum refs especially), while renderer outputs start immediately —
    so 8 of 20 test pairs were being scored against the reference's leading
    SILENCE.  All drum-case scores prior to this fix are measurement noise.

    Alignment: cross-correlate the amplitude envelopes (rectified, smoothed,
    decimated to ~1 kHz for speed/robustness), shift the later-starting signal
    forward, then truncate both to the common length.
    """
    def envelope(x: np.ndarray, dec: int) -> np.ndarray:
        e = np.abs(x).astype(np.float64)
        n = (len(e) // dec) * dec
        return e[:n].reshape(-1, dec).mean(axis=1)

    dec = max(1, sr // 1000)   # ~1 kHz envelope rate
    er, eo = envelope(ref, dec), envelope(out, dec)
    if len(er) > 1 and len(eo) > 1 and er.max() > 1e-9 and eo.max() > 1e-9:
        m = len(er) + len(eo) - 1
        nfft = 1 << int(np.ceil(np.log2(m)))
        xc = np.fft.irfft(np.fft.rfft(er, nfft) *
                          np.conj(np.fft.rfft(eo, nfft)), nfft)[:m]
        # circular xcorr → lag in [-(len(eo)-1), len(er)-1]
        lags = np.concatenate([np.arange(len(er)), -np.arange(len(eo) - 1, 0, -1)])
        lag = int(lags[np.argmax(np.concatenate([xc[:len(er)], xc[-(len(eo) - 1):]]))]) * dec
        if lag > 0:
            ref = ref[lag:]      # reference starts later → drop its lead-in
        elif lag < 0:
            out = out[-lag:]
    n = min(len(ref), len(out))
    return ref[:n], out[:n]


# ---------------------------------------------------------------------------
# Null Test
# ---------------------------------------------------------------------------

def null_test(ref: np.ndarray, out: np.ndarray) -> dict:
    """
    Phase-invert output, sum with reference. The residual reveals differences.
    Returns dict with rms_dbfs, peak_dbfs, and the residual array.

    Metric v3 (Session 15): the output is scaled by the least-squares optimal
    gain g = <ref,out>/<out,out> before the subtraction.  The V-Synth
    references are HARDWARE RECORDINGS whose absolute level depends on the
    recording chain's gain staging (the sine references sit ~7 dB below the
    dry input; vocal/chord references at unity) — a constant gain offset is a
    capture artifact, not an algorithm error, and was dominating the null
    residual on otherwise well-matched renders.  g is clamped to [0.25, 4]
    so a wildly wrong level still penalises the score.
    """
    ref_a, out_a = align_length(ref, out)

    denom = float(np.dot(out_a, out_a))
    if denom > 1e-12:
        g = float(np.dot(ref_a, out_a)) / denom
        g = float(np.clip(g, 0.25, 4.0))
    else:
        g = 1.0
    out_a = out_a * g

    residual = ref_a + (-1.0 * out_a)  # phase invert output then sum

    rms = np.sqrt(np.mean(residual ** 2))
    peak = np.max(np.abs(residual))

    rms_dbfs = 20 * np.log10(rms + 1e-12)
    peak_dbfs = 20 * np.log10(peak + 1e-12)

    # Reference level for context
    ref_rms = np.sqrt(np.mean(ref_a ** 2))
    ref_dbfs = 20 * np.log10(ref_rms + 1e-12)

    return {
        'rms_dbfs': rms_dbfs,
        'peak_dbfs': peak_dbfs,
        'ref_level_dbfs': ref_dbfs,
        'residual': residual,
        'snr_db': ref_dbfs - rms_dbfs,  # higher = more similar
    }


# ---------------------------------------------------------------------------
# Spectrogram
# ---------------------------------------------------------------------------

def compute_spectrogram(audio: np.ndarray, sr: int, n_fft: int = 2048, hop: int = 512):
    """Compute log-magnitude spectrogram (dB)."""
    S = np.abs(librosa.stft(audio, n_fft=n_fft, hop_length=hop))
    return librosa.amplitude_to_db(S, ref=np.max)


def plot_spectrograms(ref: np.ndarray, out: np.ndarray, sr: int,
                      ref_label: str, out_label: str,
                      null_result: dict, output_path: str):
    """Save side-by-side spectrogram comparison + null test residual."""
    fig = plt.figure(figsize=(18, 10))
    fig.patch.set_facecolor('#1a1a2e')

    gs = gridspec.GridSpec(2, 3, figure=fig, hspace=0.4, wspace=0.35)

    ref_a, out_a = align_length(ref, out)

    S_ref = compute_spectrogram(ref_a, sr)
    S_out = compute_spectrogram(out_a, sr)
    S_res = compute_spectrogram(null_result['residual'], sr)

    vmin, vmax = -80, 0

    def spec_plot(ax, S, title, sr, hop=512):
        img = librosa.display.specshow(S, sr=sr, hop_length=hop, x_axis='time',
                                       y_axis='log', ax=ax, vmin=vmin, vmax=vmax,
                                       cmap='magma')
        ax.set_title(title, color='white', fontsize=11, pad=8)
        ax.set_facecolor('#0d0d1a')
        ax.tick_params(colors='#aaaaaa')
        ax.xaxis.label.set_color('#aaaaaa')
        ax.yaxis.label.set_color('#aaaaaa')
        return img

    ax1 = fig.add_subplot(gs[0, 0])
    ax2 = fig.add_subplot(gs[0, 1])
    ax3 = fig.add_subplot(gs[0, 2])

    spec_plot(ax1, S_ref, f'Reference (V-Synth)\n{ref_label}', sr)
    spec_plot(ax2, S_out, f'Plugin Output\n{out_label}', sr)
    img = spec_plot(ax3, S_res, f'Null Test Residual\n(lower = more similar)', sr)

    # Colorbar
    cbar = fig.colorbar(img, ax=[ax1, ax2, ax3], orientation='horizontal',
                        pad=0.08, fraction=0.03, aspect=60)
    cbar.set_label('dBFS', color='white')
    cbar.ax.xaxis.set_tick_params(color='white')
    plt.setp(cbar.ax.xaxis.get_ticklabels(), color='white')

    # Difference spectrogram (bottom row)
    ax4 = fig.add_subplot(gs[1, 0])
    S_diff = S_ref - S_out
    librosa.display.specshow(S_diff, sr=sr, hop_length=512, x_axis='time',
                              y_axis='log', ax=ax4, cmap='RdBu_r',
                              vmin=-20, vmax=20)
    ax4.set_title('Spectral Difference (ref − plugin)', color='white', fontsize=11)
    ax4.set_facecolor('#0d0d1a')
    ax4.tick_params(colors='#aaaaaa')

    # Stats panel
    ax5 = fig.add_subplot(gs[1, 1:])
    ax5.set_facecolor('#0d0d1a')
    ax5.axis('off')

    stats_text = (
        f"NULL TEST RESULTS\n"
        f"{'─' * 40}\n"
        f"Reference level:    {null_result['ref_level_dbfs']:+.1f} dBFS\n"
        f"Residual RMS:       {null_result['rms_dbfs']:+.1f} dBFS\n"
        f"Residual Peak:      {null_result['peak_dbfs']:+.1f} dBFS\n"
        f"SNR (higher=better): {null_result['snr_db']:.1f} dB\n\n"
        f"INTERPRETATION\n"
        f"{'─' * 40}\n"
        f"> −60 dB residual = very close match\n"
        f"−40 to −60 dB    = good match\n"
        f"−20 to −40 dB    = audible differences\n"
        f"< −20 dB         = significant divergence"
    )
    ax5.text(0.05, 0.95, stats_text, transform=ax5.transAxes,
             fontsize=11, verticalalignment='top', fontfamily='monospace',
             color='#00ff88', bbox=dict(boxstyle='round', facecolor='#0a0a1a',
                                        edgecolor='#00ff88', alpha=0.8))

    fig.suptitle('VariPhrase Emulator — Analysis Report', color='white',
                 fontsize=14, fontweight='bold', y=0.98)

    plt.savefig(output_path, dpi=150, bbox_inches='tight',
                facecolor=fig.get_facecolor())
    plt.close()
    print(f"  Spectrogram saved: {output_path}")


# ---------------------------------------------------------------------------
# Transient Smearing Metric
# ---------------------------------------------------------------------------

def transient_smearing_score(ref: np.ndarray, out: np.ndarray, sr: int) -> float:
    """
    Measures how well the plugin preserves transient sharpness.
    Uses onset strength envelope comparison.
    Returns a score 0-1 (higher = less smearing = better).
    """
    ref_a, out_a = align_length(ref, out)

    ref_onset = librosa.onset.onset_strength(y=ref_a, sr=sr)
    out_onset = librosa.onset.onset_strength(y=out_a, sr=sr)

    n = min(len(ref_onset), len(out_onset))
    ref_onset, out_onset = ref_onset[:n], out_onset[:n]

    # Normalize
    ref_onset = ref_onset / (np.max(ref_onset) + 1e-8)
    out_onset = out_onset / (np.max(out_onset) + 1e-8)

    # Correlation as similarity measure
    correlation = np.corrcoef(ref_onset, out_onset)[0, 1]
    return float(np.clip(correlation, 0, 1))


# ---------------------------------------------------------------------------
# Composite Score
# ---------------------------------------------------------------------------

def composite_score(null_result: dict, formant_sim: float, transient_score: float) -> float:
    """
    Weighted composite similarity score, 0-100 (higher = more similar to V-Synth).
    
    Weights reflect importance to VariPhrase's character:
    - Formant accuracy: 40% (this IS the soul of VariPhrase)
    - SNR / null test: 35%
    - Transient preservation: 25%
    """
    # Normalize SNR: 0 dB → 0 pts, 60 dB → 100 pts
    snr_score = np.clip(null_result['snr_db'] / 60.0, 0, 1) * 100

    score = (
        0.40 * formant_sim * 100 +
        0.35 * snr_score +
        0.25 * transient_score * 100
    )
    return float(score)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def analyze(ref_path: str, out_path: str, output_dir: str = '.') -> dict:
    """Run full analysis, return results dict."""
    os.makedirs(output_dir, exist_ok=True)

    ref_label = os.path.basename(ref_path)
    out_label = os.path.basename(out_path)
    base_name = os.path.splitext(ref_label)[0]

    print(f"\n{'='*60}")
    print(f"  Analyzing: {ref_label}")
    print(f"  vs:        {out_label}")
    print(f"{'='*60}")

    print("  Loading audio...")
    ref, sr_ref = load_wav(ref_path)
    out, sr_out = load_wav(out_path)

    if sr_ref != sr_out:
        print(f"  WARNING: Sample rate mismatch ({sr_ref} vs {sr_out}). Resampling output.")
        out = librosa.resample(out, orig_sr=sr_out, target_sr=sr_ref)

    print("  Running null test...")
    null_result = null_test(ref, out)
    print(f"    Residual RMS: {null_result['rms_dbfs']:.1f} dBFS  |  SNR: {null_result['snr_db']:.1f} dB")

    print("  Analyzing formants...")
    formant_sim = formant_trajectory_similarity(ref, out, sr_ref)
    print(f"    Formant similarity: {formant_sim:.3f}")

    print("  Measuring transient preservation...")
    t_score = transient_smearing_score(ref, out, sr_ref)
    print(f"    Transient score: {t_score:.3f}")

    score = composite_score(null_result, formant_sim, t_score)
    print(f"\n  ► COMPOSITE SCORE: {score:.1f}/100")

    print("  Generating spectrogram...")
    spec_path = os.path.join(output_dir, f"{base_name}_comparison.png")
    plot_spectrograms(ref, out, sr_ref, ref_label, out_label, null_result, spec_path)

    return {
        'test_file': ref_label,
        'null_rms_dbfs': null_result['rms_dbfs'],
        'null_peak_dbfs': null_result['peak_dbfs'],
        'snr_db': null_result['snr_db'],
        'formant_similarity': formant_sim,
        'transient_score': t_score,
        'composite_score': score,
        'spectrogram_path': spec_path,
    }


def validate_self():
    """Sanity check: compare a file with itself. Should give near-perfect scores."""
    import tempfile
    from scipy.io import wavfile
    print("Running self-validation (compare identical files — should score ~100)...")
    sr = 44100
    t = np.linspace(0, 1.0, sr, dtype=np.float32)
    audio = 0.5 * np.sin(2 * np.pi * 440 * t)
    with tempfile.NamedTemporaryFile(suffix='.wav', delete=False) as f:
        wavfile.write(f.name, sr, audio)
        tmp = f.name
    result = analyze(tmp, tmp, output_dir='/tmp/vsynth_validate')
    os.unlink(tmp)
    print(f"\nValidation composite score: {result['composite_score']:.1f}/100 (expect ~100)")
    assert result['composite_score'] > 90, "Self-validation failed!"
    print("✓ Validation passed.")


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='VariPhrase analysis tool')
    parser.add_argument('reference', nargs='?', help='Reference WAV (V-Synth output)')
    parser.add_argument('plugin_output', nargs='?', help='Plugin output WAV')
    parser.add_argument('--output', '-o', default='results', help='Output directory')
    parser.add_argument('--validate', action='store_true', help='Run self-validation test')
    args = parser.parse_args()

    if args.validate:
        validate_self()
    elif args.reference and args.plugin_output:
        result = analyze(args.reference, args.plugin_output, output_dir=args.output)
        print(f"\nDone. Results saved to {args.output}/")
    else:
        parser.print_help()
