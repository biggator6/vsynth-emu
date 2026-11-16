// HIRESO's analysis: see Vc2HiresoAnalysis.h (v215).  Each routine's arithmetic in its own order, in XtFloat (FMAC a
// product then a sum, each rounded); every address is rom.4's.  The groups' sample counts (8, 16, 32: rom.4's
// 0x8993fe60) and strides into the gate (4, 2, 1: 0x8993fe6c) are the block at each rate.
#include "Vc2HiresoAnalysis.h"
#include "XtFloat.h"
#include <algorithm>
#include <cstddef>
#include <cstring>

namespace {
using xtf::add;
using xtf::mul;
using xtf::sub;
using Words = Vc2HiresoAnalysisWords;

constexpr int kStates = 4;   // a band's: its two resonators' two each
constexpr float kNone = -1.0f;   // [FW] a window's search starts below any energy (0x893edd10's fldi1, fneg)

int samples(int group) { return Words::kBlock >> (Words::kGroups - 1 - group); }
int stride(int group) { return 1 << (Words::kGroups - 1 - group); }
} // namespace

float vc2HiresoResonate(const float* c, float* st, float d) {
    const float y = add(mul(c[0], d), st[0]);
    st[0] = add(mul(c[1], y), st[1]);
    st[1] = mul(c[2], y);
    return y;
}

void Vc2HiresoAnalysis::clear() {
    std::ranges::fill(s.bank, 0.0f);
    std::memset(&s.ring, 0, sizeof s.ring);
    std::memset(&s.max, 0, offsetof(Words, gate) - offsetof(Words, max));   // the envelopes' maxima, roots and ages
}

void Vc2HiresoAnalysis::reset() {
    // 0x893edee0: the clear's, then the frame and the gate's, all 0, and the groups' words
    clear();
    s.frame = 0;
    std::ranges::fill(s.gate, 0.0f);
    s.gateLevel = 0.0f;
    std::ranges::copy(kVc2HiresoEdges, s.edge);
    const Vc2HiresoTables& t = vc2HiresoTables();
    for (int g = 0; g < Words::kGroups; ++g)
        s.table[g] = t.resonatorAt + 4 * (uint32_t)(Vc2HiresoTables::kResonatorWords * s.edge[g]);
    // then 0x893ed8f0's own: the decimators' table (the envelopes' 0s again, as it writes them)
    toHalf.c = toQuarter.c = t.analysisRate;
}

void Vc2HiresoAnalysis::block(const float* in, bool voiced) {
    s.frame = s.frame + 1 >= Words::kFrames ? 0 : s.frame + 1;
    vc2Decimate(toHalf, in, half, kHalf);
    vc2Decimate(toQuarter, half, quarter, kQuarter);
    const float* const at[Words::kGroups] = { quarter, half, in };
    for (int g = Words::kGroups - 1; g >= 0; --g) bank(at[g], g);
    gateTo(voiced);
    for (int g = Words::kGroups - 1; g >= 0; --g) energies(g);
}

void Vc2HiresoAnalysis::bank(const float* in, int group) {
    const int first = s.edge[group], end = s.edge[group + 1];
    float* last = &s.bank[Words::kBankStride * first];
    for (int n = 0; n < samples(group); ++n) {
        const float d = sub(in[n], *last);   // the first difference
        *last = in[n];
        float* st = last + 1;
        const float* c = &vc2HiresoTables().resonator[Vc2HiresoTables::kResonatorWords * first];
        for (int b = first; b < end; ++b, st += kStates, c += Vc2HiresoTables::kResonatorWords) {
            const float y = vc2HiresoResonate(c, st, d);
            const float z = vc2HiresoResonate(c + 3, st + 2, d);
            s.rows[b][n] = add(y, z);
        }
    }
}

void Vc2HiresoAnalysis::gateTo(bool voiced) {
    const float flag = voiced ? 1.0f : 0.0f;   // FLOAT of 0x893ed3e0's 0 or 1
    const float rate = vc2HiresoTables().gateRate[flag > s.gateLevel ? 0 : 1];   // chosen once a block
    float g = s.gateLevel;
    for (float& v : s.gate) {
        g = add(mul(sub(flag, g), rate), g);
        v = g;
    }
    s.gateLevel = g;
}

void Vc2HiresoAnalysis::energies(int group) {
    const float scale = vc2HiresoTables().energyScale[group];
    for (int b = s.edge[group]; b < s.edge[group + 1]; ++b) {
        float e = 0.0f;
        for (int n = 0; n < samples(group); ++n) {
            const float v = mul(s.rows[b][n], s.gate[n * stride(group)]);
            e = add(mul(v, v), e);
        }
        s.ring[b][s.frame] = mul(e, scale);
    }
}

void Vc2HiresoAnalysis::envelopes() {
    for (int b = 0; b < kBands; ++b) {
        const float e = s.ring[b][s.frame];
        if (e > s.max[b]) {
            s.max[b] = e;   // the age stands as it was
            s.envelope[b] = xtf::fsqrt(e);
        } else if (s.age[b]-- == 0) {
            rescan(b);
        }
    }
}

void Vc2HiresoAnalysis::rescan(int band) {
    const int window = vc2HiresoTables().window[band];
    int i = s.frame - window + 1;
    if (i < 0) i += Words::kFrames;
    float m = kNone;
    int at = 0;
    for (int k = 0; k < window; ++k) {
        const float v = s.ring[band][i];
        i = i + 1 >= Words::kFrames ? 0 : i + 1;
        if (v > m) {
            m = v;
            at = k;
        }
    }
    s.max[band] = m;
    s.age[band] = at;
    s.envelope[band] = xtf::fsqrt(m);
}
