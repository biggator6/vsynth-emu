// xt_vc2voices' assigner part's events (v277): each of the assigner's routines on both sides, rom.4's then the port's,
// and what differs after: the voices' span, the calls and the answer.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2AssignCheck.h"
#include "Vc2CheckMain.h"

namespace {
using V = Vc2PartVoices;
using OnCall = void (Vc2Assigner::*)(const Vc2AssignCall&, Vc2PartVoices&, Vc2AssignParts&);

// rom.4's routine on `args`, then the port's `mine()` giving its answer; what differs, then the answers
template <class Mine>
std::string assignOnBoth(AssignSides& a, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args, Mine mine) {
    return vc2BothAnswer(a.s.fw, r, args, mine, [&] { return voicesDifference(a.s); });
}

// The port's member on a call, answering nothing
auto assignOnCall(AssignSides& a, OnCall member, const Vc2AssignCall& c) {
    return [&a, member, &c] {
        (a.assigner.*member)(c, a.s.port, a.parts);
        return 0u;
    };
}

uint32_t assignVoice(AssignSides& a, std::mt19937& rng) { return a.s.port.addressOf((int)(rng() % V::kVoices)); }

// The arguments past r7, on the stack as a caller leaves them (Sh4::call sets r4…r7 alone)
void assignStack(AssignSides& a, std::initializer_list<uint32_t> words) {
    uint32_t at = kVc2Sp;
    for (const uint32_t w : words) {
        a.s.fw.cpu.wr32(at, w);
        at += 4;
    }
}

std::string assignStartEvent(AssignSides& a, std::mt19937& rng) {
    const Vc2AssignCall c = assignDrawCall(a, rng);
    ++a.n.byEvent[0];
    assignStack(a, { c.kind });
    return assignOnBoth(a, { 0x89390860, "the start" }, { c.message, c.part, c.onByte, c.passed },
                        assignOnCall(a, &Vc2Assigner::start, c));
}

// The class builder: its first register unread, the word and the kind on the stack
std::string assignBuildEvent(AssignSides& a, std::mt19937& rng) {
    const Vc2AssignCall c = assignDrawCall(a, rng);
    ++a.n.byEvent[1];
    assignStack(a, { c.passed, c.kind });
    return assignOnBoth(a, { 0x893969e0, "the class builder" }, { 0, c.message, c.part, c.onByte },
                        assignOnCall(a, &Vc2Assigner::build, c));
}

std::string assignTruncateSameEvent(AssignSides& a, std::mt19937& rng) {
    const Vc2AssignCall c = assignDrawCall(a, rng);
    ++a.n.byEvent[6];
    return assignOnBoth(a, { 0x893907d0, "the truncation of a part's voices" }, { c.message, c.part, c.kind },
                        assignOnCall(a, &Vc2Assigner::truncateSame, c));
}

std::string assignPartnerEvent(AssignSides& a, std::mt19937& rng) {
    const Vc2AssignCall c = assignDrawCall(a, rng);
    ++a.n.byEvent[2];
    Vc2Partner p;
    const std::string what = assignOnBoth(a, { 0x89390720, "the partner", vc2R0 },
                                          { c.message, c.part, c.kind, kVoicesOutAt }, [&] {
                                              p = a.assigner.partner(c, a.s.port, a.parts);
                                              return p.voice;
                                          });
    const uint32_t theirs = a.s.fw.cpu.rd32(kVoicesOutAt);
    if (!what.empty() || theirs == p.marked) return what;
    return "the partner's mark: " + std::to_string(p.marked) + ", against " + std::to_string(theirs);
}

std::string assignMatchingEvent(AssignSides& a, std::mt19937& rng) {
    const Vc2AssignCall c = assignDrawCall(a, rng);
    ++a.n.byEvent[3];
    return assignOnBoth(a, { 0x893913a0, "the matching test", vc2R0 }, { c.message, c.part, c.kind },
                        [&] { return (uint32_t)a.assigner.matching(c, a.s.port, a.parts); });
}

// [FW] mono's two retunes: of the notes not of kind 1, of kind 1
template <bool KindOne>
std::string assignRetuneEvent(AssignSides& a, std::mt19937& rng) {
    const Vc2AssignCall c = assignDrawCall(a, rng);
    const uint32_t value = (uint32_t)rng();
    ++a.n.byEvent[KindOne ? 5 : 4];
    const Vc2CheckRoutine r { KindOne ? 0x89390b90u : 0x89390b20u, "a retune" };
    return assignOnBoth(a, r, { c.message, c.part, value }, [&] {
        a.assigner.retune(c, value, KindOne, a.s.port, a.parts);
        return 0u;
    });
}

std::string assignTruncateVoiceEvent(AssignSides& a, std::mt19937& rng) {
    const uint32_t voice = assignVoice(a, rng);
    ++a.n.byEvent[7];
    return assignOnBoth(a, { 0x89391a50, "a voice's truncation" }, { voice }, [&] {
        a.assigner.truncateVoice(voice, a.s.port, a.parts);
        return 0u;
    });
}

std::string assignClearOrdersEvent(AssignSides& a, std::mt19937& rng) {
    const uint32_t voice = assignVoice(a, rng);
    ++a.n.byEvent[8];
    return assignOnBoth(a, { 0x89391aa0, "the orders' clear" }, { voice }, [&] {
        a.assigner.clearOrders(voice, a.s.port);
        return 0u;
    });
}

std::string assignSetStateEvent(AssignSides& a, std::mt19937& rng) {
    const uint32_t voice = assignVoice(a, rng), state = (uint32_t)rng();
    ++a.n.byEvent[9];
    return assignOnBoth(a, { 0x89391a90, "the state's setter" }, { voice, state }, [&] {
        a.assigner.setState(voice, state, a.s.port);
        return 0u;
    });
}

const AssignEvent kEvents[] = {
    assignStartEvent, assignStartEvent, assignBuildEvent, assignBuildEvent, assignTruncateSameEvent,
    assignPartnerEvent, assignMatchingEvent, assignRetuneEvent<false>, assignRetuneEvent<true>,
    assignTruncateVoiceEvent, assignClearOrdersEvent, assignSetStateEvent,
};
} // namespace

std::span<const AssignEvent> assignEvents() { return kEvents; }
#endif
