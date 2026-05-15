"""
lpc.py — Formant extraction via Linear Predictive Coding (LPC).

VariPhrase's core claim is independent formant control. This module
extracts formant trajectories from audio so we can compare whether our
plugin preserves them the same way the real V-Synth does.

Used by compare.py and batch_test.py.
"""

import numpy as np
from scipy.signal import lfilter, hamming
import librosa
import warnings
warnings.filterwarnings('ignore')


# ---------------------------------------------------------------------------
# LPC Analysis
# ---------------------------------------------------------------------------

def lpc_coefficients(frame: np.ndarray, order: int) -> np.ndarray:
    """
    Compute LPC coefficients for a single audio frame using the Levinson-Durbin algorithm.
    
    Args:
        frame: audio samples (windowed)
        order: LPC order (typically 2 + sr/1000, so 10-14 for voice)
    
    Returns:
        LPC coefficients array of length (order + 1)
    """
    # Autocorrelation
    n = len(frame)
    r = np.correlate(frame, frame, mode='full')[n-1:n+order+1]

    if r[0] < 1e-12:
        return np.zeros(order + 1)

    # Levinson-Durbin recursion
    a = np.zeros(order + 1)
    a[0] = 1.0
    e = r[0]

    for i in range(1, order + 1):
        lam = -np.dot(a[1:i], r[i-1:0:-1]) - r[i]
        lam /= e
        a[1:i+1] += lam * a[i-1::-1]
        a[i+1:] = 0
        e *= (1 - lam ** 2)
        if e <= 0:
            break

    return a


def formants_from_lpc(lpc_coeffs: np.ndarray, sr: int, max_formants: int = 5) -> np.ndarray:
    """
    Extract formant frequencies from LPC coefficients by finding roots of the LPC polynomial.
    
    Returns array of formant frequencies in Hz (ascending order).
    """
    # Find roots of the LPC polynomial
    roots = np.roots(lpc_coeffs)

    # Keep roots with positive imaginary part (each conjugate pair = one resonance)
    roots = roots[np.imag(roots) > 0]

    # Convert to frequencies
    angles = np.angle(roots)
    freqs = angles * (sr / (2 * np.pi))

    # Filter to speech/musical range (80 Hz - Nyquist/2)
    freqs = freqs[(freqs > 80) & (freqs < sr / 2)]

    # Sort ascending
    freqs = np.sort(freqs)

    # Pad or trim to max_formants
    if len(freqs) < max_formants:
        freqs = np.pad(freqs, (0, max_formants - len(freqs)), constant_values=np.nan)
    else:
        freqs = freqs[:max_formants]

    return freqs


# ---------------------------------------------------------------------------
# Trajectory Extraction
# ---------------------------------------------------------------------------

def extract_formants(audio: np.ndarray, sr: int,
                     frame_length: int = 1024,
                     hop_length: int = 256,
                     lpc_order: int = None,
                     max_formants: int = 4) -> np.ndarray:
    """
    Extract formant trajectories across the whole audio signal.
    
    Returns:
        formants: shape (n_frames, max_formants), frequencies in Hz
                  NaN where formant not detected
    """
    if lpc_order is None:
        lpc_order = int(2 + sr / 1000)  # standard rule of thumb

    n_frames = 1 + (len(audio) - frame_length) // hop_length
    formants = np.full((n_frames, max_formants), np.nan)

    for i in range(n_frames):
        start = i * hop_length
        frame = audio[start:start + frame_length]

        if len(frame) < frame_length:
            break

        # Pre-emphasis (emphasize high frequencies for better LPC accuracy)
        frame = np.append(frame[0], frame[1:] - 0.97 * frame[:-1])

        # Window
        frame = frame * hamming(len(frame))

        # LPC + formants
        try:
            coeffs = lpc_coefficients(frame, lpc_order)
            f = formants_from_lpc(coeffs, sr, max_formants)
            formants[i] = f
        except Exception:
            pass  # leave as NaN

    return formants


# ---------------------------------------------------------------------------
# Trajectory Similarity
# ---------------------------------------------------------------------------

def formant_trajectory_similarity(ref: np.ndarray, out: np.ndarray,
                                   sr: int,
                                   max_formants: int = 4) -> float:
    """
    Compare formant trajectories between reference and plugin output.
    
    Returns a similarity score 0-1 (1 = identical trajectories).
    
    Strategy:
    - Extract formant trajectories for both signals
    - For each formant (F1, F2, ...), compute normalized RMS difference
    - Weight lower formants more heavily (F1, F2 most perceptually important)
    - Return weighted average similarity
    """
    ref_formants = extract_formants(ref, sr, max_formants=max_formants)
    out_formants = extract_formants(out, sr, max_formants=max_formants)

    # Align frame counts
    n = min(len(ref_formants), len(out_formants))
    ref_f = ref_formants[:n]
    out_f = out_formants[:n]

    weights = np.array([0.40, 0.35, 0.15, 0.10])[:max_formants]
    weights /= weights.sum()

    formant_scores = []

    for fi in range(max_formants):
        ref_traj = ref_f[:, fi]
        out_traj = out_f[:, fi]

        # Only use frames where both have valid formant data
        valid = ~(np.isnan(ref_traj) | np.isnan(out_traj))
        if valid.sum() < 5:
            formant_scores.append(0.5)  # not enough data, neutral score
            continue

        ref_v = ref_traj[valid]
        out_v = out_traj[valid]

        # Normalize by reference frequency — makes score independent of absolute frequency
        diff = np.abs(ref_v - out_v) / (ref_v + 1e-8)
        rms_diff = np.sqrt(np.mean(diff ** 2))

        # Map to 0-1 score: 0% error → 1.0, 50%+ error → 0.0
        score = max(0.0, 1.0 - rms_diff * 2)
        formant_scores.append(score)

    formant_scores = np.array(formant_scores)
    return float(np.dot(weights, formant_scores))


# ---------------------------------------------------------------------------
# Visualization helpers
# ---------------------------------------------------------------------------

def plot_formant_trajectories(ref: np.ndarray, out: np.ndarray, sr: int,
                               ax, max_formants: int = 3, hop_length: int = 256):
    """
    Plot formant trajectories for both reference and plugin output on a given matplotlib axis.
    Called by compare.py for the detailed report.
    """
    ref_f = extract_formants(ref, sr, hop_length=hop_length, max_formants=max_formants)
    out_f = extract_formants(out, sr, hop_length=hop_length, max_formants=max_formants)

    n = min(len(ref_f), len(out_f))
    times = np.arange(n) * hop_length / sr

    colors = ['#ff6b6b', '#4ecdc4', '#45b7d1']
    formant_names = ['F1', 'F2', 'F3', 'F4']

    for fi in range(max_formants):
        c = colors[fi % len(colors)]
        ax.plot(times, ref_f[:n, fi], color=c, lw=1.5, alpha=0.9,
                label=f'{formant_names[fi]} (ref)')
        ax.plot(times, out_f[:n, fi], color=c, lw=1.0, alpha=0.5,
                linestyle='--', label=f'{formant_names[fi]} (plugin)')

    ax.set_xlabel('Time (s)', color='#aaaaaa')
    ax.set_ylabel('Frequency (Hz)', color='#aaaaaa')
    ax.set_title('Formant Trajectories (solid=reference, dashed=plugin)', color='white')
    ax.legend(fontsize=8, ncol=max_formants)
    ax.set_facecolor('#0d0d1a')
    ax.tick_params(colors='#aaaaaa')


if __name__ == '__main__':
    # Quick smoke test
    import sys
    sr = 44100
    t = np.linspace(0, 0.5, sr // 2, dtype=np.float32)

    # Synthetic vowel-like signal: buzz + formant filter approximation
    buzz = 0.3 * np.sign(np.sin(2 * np.pi * 120 * t))
    audio = lfilter([1.0], [1.0, -0.8], buzz).astype(np.float32)

    formants = extract_formants(audio, sr)
    print(f"Extracted formants shape: {formants.shape}")
    print(f"Sample frame formants (Hz): {formants[10]}")
    print("lpc.py smoke test passed ✓")
