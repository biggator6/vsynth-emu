// xt_vc2tva <rom.4> [cases] [seed] (v233): the VC-2's TVA, Vc2Tva, against rom.4's own routines on a TVA in the
// interpreter's RAM; its envelopes, LFOs and the fade's step drawn in the interpreter's memory, which rom.4's routines
// read and the port's fake parts read too (Vc2TvaFakes.cpp); the OS's call a told truncation's end makes hooked on
// rom.4's side and logged on the port's.  Each case fills the TVA with the same noise on both sides, then 400 events
// (Vc2TvaEvents.cpp): constructed, a note-on or a new block on a block drawn in the interpreter's RAM, updated, a
// render of a drawn input into a drawn output, either truncation, or a question (over, stopped, silent, the envelope's
// level).  Before each, the records drawn and, each one time in four, the words each routine decides by (the
// truncations, the fade, the gains, targets and steps about each other and the floor, the elapsed count, the
// bindings, the block's level, depths, switch and factors; drawn by Vc2TvaDraws from v317, which xt_vc2tone shares).
// After each, the TVA's words, the output, the calls and the answer.  XT_VC2TVA_FIRST=1 says where the first case to
// differ does, XT_VC2TVA_STOP=1 stops there.  XT_VC2TVA_MAIN only.
#ifdef XT_VC2TVA_MAIN
#include "Vc2TvaCheck.h"
#include "Vc2TvaTables.h"
#include <bit>
#include <cmath>

namespace {
constexpr int kSteps = 400;

using std::bit_cast;

// A word the TVA reads beyond itself, drawn knowing the TVA's words: its bits
using WordDraw = uint32_t (*)(std::mt19937&, const Vc2TvaWords&);

uint32_t drawState(std::mt19937& rng, const Vc2TvaWords&) {   // done half the time
    return rng() % 2 ? (uint32_t)Vc2Tva::kDone : rng() % 3 == 0 ? (uint32_t)rng() : rng() % 7;
}
uint32_t drawAttack(std::mt19937& rng, const Vc2TvaWords&) {   // 0, about the 10 ms bound, under or over it
    const float picks[5] = { 0.0f, -0.0f, tvaAboutUnit(rng, 10.0f), 10.0f * vc2Unit(rng), 100.0f * vc2Unit(rng) };
    return bit_cast<uint32_t>(rng() % 6 == 0 ? tvaDrawFloat(rng) : picks[rng() % 5]);
}
uint32_t drawNext(std::mt19937& rng, const Vc2TvaWords&) {
    const float picks[3] = { 0.0f, -0.0f, 50.0f * vc2Unit(rng) };
    return bit_cast<uint32_t>(rng() % 4 == 0 ? tvaDrawFloat(rng) : picks[rng() % 3]);
}
// An envelope's output or onset: now and then at 1 or just under, so a gain or target lands at the clamp's edge
uint32_t drawEnvOut(std::mt19937& rng, const Vc2TvaWords&) {
    const float picks[4] = { 1.0f, std::nextafter(1.0f, 0.0f), 0.9995f, tvaDrawFloat(rng) };
    return bit_cast<uint32_t>(rng() % 2 ? vc2Unit(rng) : picks[rng() % 4]);
}
// The stage's time: in sustain, or what is left of it after the elapsed samples about 0 and 10 ms
uint32_t drawStageTime(std::mt19937& rng, const Vc2TvaWords& w) {
    const float past = xtf::mul(w.elapsed, vc2TvaTables().perSample);
    const float picks[5] = { -1.0f, tvaAboutUnit(rng, past), tvaAboutUnit(rng, xtf::add(past, 10.0f)),
                             xtf::add(past, 20.0f * vc2Unit(rng)), tvaDrawFloat(rng) };
    return bit_cast<uint32_t>(picks[rng() % 5]);
}
// How each of an envelope's floats is drawn, in kTvaEnvelopeFloats' order: the attack, the next stage, the output, the
// onset, the stage's time
constexpr std::array<WordDraw, kTvaEnvelopeFloats.size()> kEnvelopeDraws { drawAttack, drawNext, drawEnvOut,
                                                                           drawEnvOut, drawStageTime };

// The records the TVA reads, written where its words point (both sides read them there), and the fade's step
void drawReads(Vc2Fw& fw, std::mt19937& rng, const Vc2TvaWords& w) {
    for (int k = 0; k < kTvaRecords; ++k) {
        fw.cpu.wr32(tvaEnvelopeAt(k) + kTvaState, drawState(rng, w));
        for (size_t f = 0; f < kEnvelopeDraws.size(); ++f)
            fw.cpu.wr32(tvaEnvelopeAt(k) + kTvaEnvelopeFloats[f].at, kEnvelopeDraws[f](rng, w));
        const float lfo = rng() % 2 ? 2.0f * vc2Unit(rng) - 1.0f : tvaDrawFloat(rng);
        fw.cpu.wr32(tvaLfoAt(k) + kTvaLfoOut, bit_cast<uint32_t>(lfo));
    }
    fw.cpu.wr32(kTvaFadeStep, tvaDrawFadeStep(rng, w));
}

// The bindings, every time, to the check's records: the tone binds the TVA before its note-on (the constructor clears
// them, and rom.4 would read address 0 on)
void bindRecords(Vc2TvaWords& w, std::mt19937& rng) {
    w.envelope = tvaEnvelopeAt((int)(rng() % kTvaRecords));
    w.lfo = tvaLfoAt((int)(rng() % kTvaRecords));
    w.panLfo = tvaLfoAt((int)(rng() % kTvaRecords));
}

// A case of the check: the TVA the same noise on both sides, then its events
std::string tvaCheckCase(TvaSides& s, std::mt19937& rng) {
    auto* words = reinterpret_cast<uint32_t*>(&s.tva.w);
    for (size_t k = 0; k < sizeof s.tva.w / 4; ++k) words[k] = (uint32_t)rng();
    const std::span<const TvaEvent> events = tvaEvents();
    std::string what;
    for (int k = 0; k < kSteps && what.empty(); ++k, ++s.n.events) {
        tvaSteer(s.tva.w, rng, k == 0, bindRecords);
        s.fw.cpu.write(kTvaAt, &s.tva.w, sizeof s.tva.w);
        drawReads(s.fw, rng, s.tva.w);
        s.rom.clear();
        s.port.clear();
        what = events[rng() % events.size()](s, rng);
        s.n.told += (long)s.port.size();
    }
    return what;
}

// What the run did, and how many cases differ
void sayCounts(const Vc2Run& run, const TvaRunCounts& n, long different) {
    std::printf("tva: %d cases, %ld events, %ld note-ons, %ld updates, %ld renders (%ld fading, %ld fades ended, %ld "
                "told), %ld over, %ld silent\n", run.cases, n.events, n.noteOns, n.updates, n.renders, n.fades,
                n.fadesEnded, n.told, n.over, n.silent);
    std::printf("xt_vc2tva: %ld different\n", different);
}
} // namespace

std::string tvaDifference(TvaSides& s) {
    const std::string what = vc2RegionsDifference(
        s.fw, { { kTvaAt, &s.tva.w, sizeof s.tva.w, "the TVA's words" },
                { kTvaOutAt, s.out.data(), sizeof s.out, "the output" } });
    return what.empty() ? tvaCallsDifference(s.rom, s.port) : what;
}

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2tva", vc2LoadTva);
    if (!run) return 1;
    Vc2Tva tva;
    std::vector<TvaCall> rom, port;
    TvaPortParts parts(fw, port);
    std::array<float, Vc2Tva::kOut> out {};
    TvaSides s { fw, tva, parts, rom, port, out };
    tvaInstallRom(fw, rom);
    Vc2Tally tally { "XT_VC2TVA_FIRST", "XT_VC2TVA_STOP" };
    vc2RunCases(*run, tally, "", s.n.events, [&](std::mt19937& rng, bool) { return tvaCheckCase(s, rng); });
    sayCounts(*run, s.n, tally.different);
    return 0;
}
#endif
