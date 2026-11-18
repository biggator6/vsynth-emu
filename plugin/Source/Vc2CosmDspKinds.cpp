// xt_vc2cosm's seventh part's kinds: see Vc2CosmDspCheck.h (v264; the first twelve moved from Vc2CosmDspCheck.cpp as
// they stood).  Every offset is D's, rom.4's.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CosmDspCheck.h"
#include <bit>

namespace {
constexpr int kRingFloats = 3266;   // [FW] the Compressor's, Limiter's and Frequency Shifter's rings

void wordsFrom(Vc2CosmDspWords& w, uint32_t at, const float* v, int n) {
    for (int k = 0; k < n; ++k) w.push_back({ at + 4 * (uint32_t)k, v[k] });
}

// A type's two values and their targets (D+0xbc, +0xc0, D+0xa4, +0xa8) by its getters, and `Words` of state from
// D+0x1c, R's from D+0x54
template <auto Member, auto Value0, auto Value1, auto Target0, auto Target1, int Words>
void twoState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    const auto& c = s.*Member;
    w.insert(w.end(), { { 0xbc, (c.*Value0)() }, { 0xc0, (c.*Value1)() }, { 0xa4, (c.*Target0)() },
                        { 0xa8, (c.*Target1)() } });
    wordsFrom(w, 0x1c, c.state(), Words);
    if (stereo) wordsFrom(w, 0x54, c.state() + 14, Words);
}

// The same, with the gain at D+0x8c
template <auto Member, auto Value0, auto Value1, auto Target0, auto Target1, int Words>
void gainedState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    twoState<Member, Value0, Value1, Target0, Target1, Words>(s, stereo, w);
    w.push_back({ 0x8c, (s.*Member).gain() });
}

template <int Words>
constexpr auto odState = gainedState<&XtCosmSection::od, &XtCosmOd::drive, &XtCosmOd::tone, &XtCosmOd::driveTarget,
                                     &XtCosmOd::toneTarget, Words>;
constexpr auto wsState = gainedState<&XtCosmSection::ws, &XtCosmWs::drive, &XtCosmWs::tone, &XtCosmWs::driveTarget,
                                     &XtCosmWs::toneTarget, 6>;
constexpr auto resState = twoState<&XtCosmSection::res, &XtCosmRes::size, &XtCosmRes::balance, &XtCosmRes::sizeTarget,
                                   &XtCosmRes::balanceTarget, 12>;
constexpr auto dualState = twoState<&XtCosmSection::dual, &XtCosmDual::c1, &XtCosmDual::c2, &XtCosmDual::target1,
                                    &XtCosmDual::target2, 8>;
constexpr auto tvfState = twoState<&XtCosmSection::tvf, &XtCosmTvf::c1, &XtCosmTvf::c2, &XtCosmTvf::target1,
                                   &XtCosmTvf::target2, 4>;

void dynamicState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    tvfState(s, stereo, w);
    w.insert(w.end(), { { 0x90, s.tvf.qOwnSlewed() }, { 0x94, s.tvf.cutoffOwnSlewed() }, { 0x98, s.tvf.level() } });
}
void lofiState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    twoState<&XtCosmSection::lofi, &XtCosmLofi::filter, &XtCosmLofi::balance, &XtCosmLofi::filterTarget,
             &XtCosmLofi::balanceTarget, 6>(s, stereo, w);
    w.insert(w.end(), { { 0x94, s.lofi.rateCoef() }, { 0x98, s.lofi.upScale() }, { 0x9c, s.lofi.downScale() } });
}
void ampState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    w.insert(w.end(), { { 0xbc, s.amp.gain() }, { 0xa4, s.amp.gainTarget() } });
    wordsFrom(w, 0x8c, s.amp.slews(), 4);
    wordsFrom(w, 0x1c, s.amp.state(), 10);
    if (stereo) wordsFrom(w, 0x54, s.amp.state() + 14, 10);
}
void spkState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    wordsFrom(w, 0x1c, s.spk.state(), 14);
    if (stereo) wordsFrom(w, 0x54, s.spk.state() + 14, 14);
}
void compState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    const XtCosmComp& c = s.comp;
    wordsFrom(w, 0x8c, c.slews(), 3);
    w.insert(w.end(), { { 0x98, c.gainTarget(0) }, { 0x9c, c.gain(0) }, { 0xa0, c.envelope(0) } });
    if (stereo) w.insert(w.end(), { { 0xcc, c.gainTarget(1) }, { 0xd0, c.gain(1) }, { 0xd4, c.envelope(1) } });
}
void limState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    const XtCosmLim& l = s.lim;
    w.insert(w.end(), { { 0x8c, l.slope() }, { 0x90, l.gain(0) }, { 0x94, l.smooth(0) }, { 0xa0, l.slews()[3] } });
    wordsFrom(w, 0xcc, l.slews(), 3);
    if (stereo) w.insert(w.end(), { { 0x98, l.gain(1) }, { 0x9c, l.smooth(1) } });
}
void fsState(const XtCosmSection& s, bool, Vc2CosmDspWords& w) {
    const XtCosmFs& f = s.fs;
    w.insert(w.end(), { { 0xbc, f.depth() }, { 0xc0, f.balance() }, { 0xa4, f.depthTarget() },
                        { 0xa8, f.balanceTarget() }, { 0x94, f.phase() } });
}
// The Side Band Filters: the second has no Sub line, and keeps its level where the first keeps the Sub's slew
template <bool Second>
void sbfState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    const XtCosmSbf& f = s.sbf;
    w.insert(w.end(), { { 0xbc, f.delay() }, { 0xd8, f.newDelay() }, { 0xc0, f.width() }, { 0x8c, f.gain() },
                        { 0xe4, f.progress() }, { 0xa4, f.detuneTarget() }, { 0xa8, f.widthTarget() },
                        { Second ? 0x98u : 0xa0u, f.level() }, { 0x34, f.dc(0) }, { 0x90, f.slews()[0] },
                        { 0x94, f.slews()[1] }, { 0xcc, f.slews()[3] }, { 0xd0, f.slews()[4] },
                        { 0xdc, f.fractions()[0] } });
    if (!Second) w.insert(w.end(), { { 0x98, f.slews()[2] }, { 0xe0, f.fractions()[1] } });
    const int words = Second ? 4 : 6;
    wordsFrom(w, 0x1c, f.state(), words);
    if (!stereo) return;
    wordsFrom(w, 0x54, f.state() + 14, words);
    w.push_back({ 0x6c, f.dc(1) });
}
void tbState(const XtCosmSection& s, bool stereo, Vc2CosmDspWords& w) {
    const XtCosmTb& t = s.tb;
    wordsFrom(w, 0x1c, t.coef(), 4);
    wordsFrom(w, 0x5c, t.coef() + 4, 3);
    wordsFrom(w, 0x3c, t.target(), 4);
    wordsFrom(w, 0x78, t.target() + 4, 3);
    wordsFrom(w, 0x2c, t.lpState(), 4);
    wordsFrom(w, 0x68, t.hpState(), 4);
    if (!stereo) return;
    wordsFrom(w, 0x4c, t.lpState() + 4, 4);
    wordsFrom(w, 0x84, t.hpState() + 4, 4);
}
// Through holds nothing: the frames alone say it leaves them as they came
void throughState(const XtCosmSection&, bool, Vc2CosmDspWords&) {}

// A kind whose state is its XtCosm object's
template <auto State>
void ofSection(const Vc2CosmDsp& d, const Vc2CosmObjectWords&, bool stereo, Vc2CosmDspWords& w) {
    State(d.section(), stereo, w);
}
// The Growl's state is the object's own words: its position, phase and step
void growlState(const Vc2CosmDsp&, const Vc2CosmObjectWords& o, bool, Vc2CosmDspWords& w) {
    w.insert(w.end(), { { 4, std::bit_cast<float>(o.position) }, { 0x1c, o.cleared[0] }, { 0x20, o.cleared[1] } });
}

template <auto Member>
Vc2CosmDspRing ringOf(const Vc2CosmDsp& d, const Vc2CosmObjectWords&) {
    const auto& c = d.section().*Member;
    return { c.writeIndex(), { c.buffer(0), nullptr, c.buffer(1), nullptr }, kRingFloats };
}
Vc2CosmDspRing sbfRing(const Vc2CosmDsp& d, const Vc2CosmObjectWords&) {
    const XtCosmSbf& f = d.section().sbf;
    return { f.writeIndex(), { f.buffer(0), f.buffer(1), f.buffer(2), f.buffer(3) }, Vc2CosmPools::kBufferFloats };
}
Vc2CosmDspRing growlRing(const Vc2CosmDsp& d, const Vc2CosmObjectWords& o) {
    const Vc2CosmGrowl& g = d.growl();
    return { o.position, { g.left.data(), nullptr, g.right.data(), nullptr }, Vc2CosmGrowl::kLine };
}

constexpr Vc2CosmDspKind kKinds[] { { 0, ofSection<throughState>, nullptr },
                                    { 1, ofSection<odState<9>>, nullptr },
                                    { 2, ofSection<odState<5>>, nullptr },
                                    { 3, ofSection<wsState>, nullptr },
                                    { 4, ofSection<ampState>, nullptr },
                                    { 5, ofSection<spkState>, nullptr },
                                    { 6, ofSection<resState>, nullptr },
                                    { 7, ofSection<sbfState<false>>, sbfRing },
                                    { 8, ofSection<sbfState<true>>, sbfRing },
                                    { Vc2CosmGrowl::kCode, growlState, growlRing },
                                    { 12, ofSection<dualState>, nullptr },
                                    { 13, ofSection<tvfState>, nullptr },
                                    { 14, ofSection<dynamicState>, nullptr },
                                    { 15, ofSection<compState>, ringOf<&XtCosmSection::comp> },
                                    { 16, ofSection<limState>, ringOf<&XtCosmSection::lim> },
                                    { 18, ofSection<fsState>, ringOf<&XtCosmSection::fs> },
                                    { 19, ofSection<lofiState>, nullptr },
                                    { 20, ofSection<tbState>, nullptr } };
} // namespace

std::span<const Vc2CosmDspKind> vc2CosmDspKinds() { return kKinds; }

const Vc2CosmDspKind& vc2DspKindOf(int32_t code) {
    for (const Vc2CosmDspKind& k : kKinds)
        if (k.code == code) return k;
    return kKinds[0];
}

const Vc2CosmDspKind& vc2DspDrawKind(const Vc2CosmDspKind* last, std::mt19937& rng) {
    constexpr std::array<int32_t, 6> kWithRings { 7, 8, Vc2CosmGrowl::kCode, 15, 16, 18 };
    const uint32_t pick = rng() % 10;
    if (pick < 3) return vc2DspKindOf(pick < 2 ? 14 : 20);   // the Dynamic TVF and TB Filter read the last note's words
    if (pick < 5 && last != nullptr) return *last;
    if (pick == 5) return vc2DspKindOf(kWithRings[rng() % kWithRings.size()]);
    return kKinds[rng() % std::size(kKinds)];
}
#endif
