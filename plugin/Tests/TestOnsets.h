#pragma once
// A render's onsets, by its frames' level: VSynthEmuArpTest's (v189), shared with VSynthEmuArpPluginTest (v190).
#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

// One frame's RMS over both channels
inline float frameRms(const float* l, const float* r, int n) {
    double sum = 0.0;
    for (int i = 0; i < n; ++i) sum += (double)l[i] * l[i] + (double)r[i] * r[i];
    return (float)std::sqrt(sum / (2.0 * n));
}

// Where the level first rises past a third of the render's loudest frame from below a tenth of it, in ms
inline std::vector<double> onsetsMs(const std::vector<float>& rms, double frameMs) {
    const float top = *std::max_element(rms.begin(), rms.end());
    std::vector<double> at;
    bool quiet = true;
    for (size_t f = 0; f < rms.size(); ++f) {
        if (quiet && rms[f] > top / 3.0f) at.push_back((double)f * frameMs);
        if (rms[f] > top / 3.0f) quiet = false;
        if (rms[f] < top / 10.0f) quiet = true;
    }
    return at;
}

inline std::string said(const std::vector<double>& at) {
    std::string s;
    for (const double t : at) s += " " + std::to_string((int)std::lround(t));
    return s;
}
