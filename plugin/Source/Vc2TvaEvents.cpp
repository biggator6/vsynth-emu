// xt_vc2tva's events (v233): each of the TVA's routines on both sides, rom.4's then the port's, and what differs after:
// the TVA's words, the output, the calls and the answer.  XT_VC2TVA_MAIN only.
#ifdef XT_VC2TVA_MAIN
#include "Vc2TvaCheck.h"
#include "Vc2TvaTables.h"
#include <bit>

namespace {
// [FW] the TVA's routines
constexpr uint32_t kConstruct = 0x893b20b0, kNoteOn = 0x893b2100, kNewBlock = 0x893b23b0, kUpdate = 0x893b23d0;
constexpr uint32_t kOver = 0x893b2560, kTruncateTold = 0x893b25a0, kTruncate = 0x893b25b0, kSilent = 0x893b25c0;
constexpr uint32_t kStopped = 0x893b2610, kRender = 0x893b2640, kLevel = 0x893b2780;

enum class Answer : uint8_t { none, r0, fr0 };

// rom.4's `routine` on `args`, then the port's `mine()` giving its answer's bits; then what differs, the answers last,
// compared as `how` says
template <class Mine>
std::string onTva(TvaSides& s, uint32_t routine, std::initializer_list<uint32_t> args, const char* name, Answer how,
                  Mine mine) {
    uint32_t ours = 0;
    return vc2BothOn(s.fw, routine, args, name, [&] { ours = mine(); }, [&] {
        const std::string what = tvaDifference(s);
        const uint32_t theirs = how == Answer::fr0 ? s.fw.cpu.fb[0][0] : s.fw.cpu.r[0];
        if (!what.empty() || how == Answer::none || theirs == ours) return what;
        char t[96];
        std::snprintf(t, sizeof t, "%s answers %08x, against %08x", name, ours, theirs);
        return std::string(t);
    });
}

// The port's side of a routine given the TVA alone, its answer's bits: one with no answer, a test answering 0 or 1
// (its 1s counted), and the others
template <void (Vc2Tva::*Set)()>
uint32_t setting(TvaSides& s) {
    (s.tva.*Set)();
    return 0;
}
template <bool (Vc2Tva::*Test)(Vc2TvaParts&) const, long TvaRunCounts::*Yes>
uint32_t tested(TvaSides& s) {
    const bool yes = (s.tva.*Test)(s.parts);
    s.n.*Yes += yes;
    return yes;
}
uint32_t constructed(TvaSides& s) {
    s.tva.construct();
    return kTvaAt;   // the constructor answers its TVA
}
uint32_t updated(TvaSides& s) {
    ++s.n.updates;
    s.tva.update(s.parts);
    return 0;
}
uint32_t stoppedAnswer(TvaSides& s) { return s.tva.stoppedWord(); }
uint32_t levelAnswer(TvaSides& s) { return std::bit_cast<uint32_t>(s.tva.envelopeLevel(s.parts)); }

// A routine given the TVA alone: rom.4's, its name, how its answer compares, and the port's
struct Alone {
    uint32_t routine;
    const char* name;
    Answer how;
    uint32_t (*mine)(TvaSides&);
};
// The update twice, as the tone runs it the most
constexpr Alone kAlone[] = {
    { kConstruct, "the constructor", Answer::r0, constructed },
    { kUpdate, "the update", Answer::none, updated },
    { kUpdate, "the update", Answer::none, updated },
    { kTruncateTold, "the told truncation", Answer::none, setting<&Vc2Tva::truncateTold> },
    { kTruncate, "the truncation", Answer::none, setting<&Vc2Tva::truncate> },
    { kOver, "the end test", Answer::r0, tested<&Vc2Tva::over, &TvaRunCounts::over> },
    { kSilent, "the silence test", Answer::r0, tested<&Vc2Tva::silent, &TvaRunCounts::silent> },
    { kStopped, "the stopped test", Answer::r0, stoppedAnswer },
    { kLevel, "the envelope's level", Answer::fr0, levelAnswer },
};

std::string onAlone(TvaSides& s, std::mt19937& rng) {
    const Alone& a = kAlone[rng() % std::size(kAlone)];
    return onTva(s, a.routine, { kTvaAt }, a.name, a.how, [&] { return a.mine(s); });
}

// A note-on or a new block on a drawn block
std::string onBlock(TvaSides& s, std::mt19937& rng) {
    const std::array<uint32_t, kTvaBlockWords> block = tvaDrawBlock(rng);
    s.fw.cpu.write(kTvaBlockAt, block.data(), sizeof block);
    if (rng() % 3 == 0) {
        return onTva(s, kNewBlock, { kTvaAt, kTvaBlockAt }, "the new block", Answer::none, [&] {
            s.tva.newBlock(block.data());
            return 0u;
        });
    }
    const uint32_t flag = rng() % 2 ? rng() % 2 : (uint32_t)rng();
    ++s.n.noteOns;
    return onTva(s, kNoteOn, { kTvaAt, kTvaBlockAt, flag }, "the note-on", Answer::none, [&] {
        s.tva.noteOn(block.data(), flag, s.parts);
        return 0u;
    });
}

// A render of a drawn input into a drawn output, one stereo pair, at a level of a few kinds
std::string onRenderPair(TvaSides& s, std::mt19937& rng) {
    std::array<float, Vc2Tva::kOut> in {};
    for (float* buffer : { in.data(), s.out.data() }) {
        const float scale = rng() % 2 ? 1.0f : rng() % 2 ? 0.001f : tvaDrawFloat(rng);
        for (int i = 0; i < Vc2Tva::kOut; ++i) buffer[i] = scale * (2.0f * vc2Unit(rng) - 1.0f);
    }
    s.fw.cpu.write(kTvaInAt, in.data(), sizeof in);
    s.fw.cpu.write(kTvaOutAt, s.out.data(), sizeof s.out);
    // counted: the renders that fade (truncated, the fade above the floor), and those whose fade reaches it
    const bool fading = (s.tva.w.truncatedTold != 0 || s.tva.w.truncated != 0)
                        && s.tva.w.fade > vc2TvaTables().renderFloor;
    ++s.n.renders;
    s.n.fades += fading;
    std::string what = onTva(s, kRender, { kTvaAt, kTvaInAt, kTvaOutAt }, "the render", Answer::none, [&] {
        s.tva.render(in.data(), s.out.data(), s.parts);
        return 0u;
    });
    s.n.fadesEnded += fading && s.tva.w.fade == 0.0f;
    return what;
}

// Each kind's share of the events: the renders the most, as the tone runs them
const TvaEvent kEvents[] = { onAlone, onAlone, onAlone, onAlone, onBlock, onBlock, onRenderPair, onRenderPair,
                             onRenderPair };
} // namespace

std::span<const TvaEvent> tvaEvents() { return kEvents; }
#endif
