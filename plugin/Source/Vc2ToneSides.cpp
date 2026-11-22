// xt_vc2tone's sides: their compare and what each event draws on both, see Vc2ToneSides.h (v228, v229).
// XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2OscDraws.h"
#include <algorithm>
#include <cstring>
#include <ranges>

namespace {
using In = Vc2ToneInput;
// [FW] the pool's words
constexpr uint32_t kHeadAt = 0x8993fb48;
constexpr uint32_t kSerialAt = 0x8993fb4c;
// [FW] the LFOs' constructors' generator; a tone's own words by offset: the tail, the pool's link
constexpr uint32_t kChaosAt = 0x89999668;
constexpr uint32_t kTailAt = 0x1af8;
constexpr uint32_t kLinkAt = 0x2618;
constexpr int kListed = 3;                     // the most tones the lists hold
constexpr uint32_t kNoSerial = 0x7ffffff0;      // a serial no tone of a case reaches
constexpr uint32_t kFixMix = 4, kFixKind = 3, kFixWave = 10, kFixedWave = 2;   // [FW] the note-on's fix-up's case

bool drawFlag(std::mt19937& rng) { return rng() % 2 == 0; }

// Tone i's own words and parts' words against rom.4's
std::string toneWordsDifference(ToneSides& s, int i) {
    const Vc2Tone& t = s.pool.tones[(size_t)i];
    const uint32_t at = s.pool.addressOf(i);
    std::string what = vc2RegionsDifference(s.fw, { { at, &t.head, sizeof t.head, "its head" },
                                                    { at + kTailAt, &t.tail, sizeof t.tail, "its tail" },
                                                    { at + kLinkAt, &t.link, sizeof t.link, "its link" } });
    if (what.empty()) what = toneModulatorsDifference(s, i);
    if (what.empty()) what = toneMixDifference(s, i);
    if (what.empty()) what = toneTvaDifference(s, i);
    if (what.empty()) what = toneCosmDifference(s, i);
    if (what.empty()) what = toneOscDifference(s, i);
    return what.empty() ? what : "tone " + std::to_string(i) + ", " + what;
}

// The input: every word small (each a usable envelope's or LFO's number), then the words the note-on decides by; the
// OSC blocks (v319) before the modulators', whose LFOs' tempo is OSC 1's block's +0x60
void drawToneInput(ToneSides& s, std::mt19937& rng) {
    In& in = s.shared.input;
    std::ranges::generate(in.words, [&] { return rng() % 8; });
    in.at(In::kStructure) = rng() % 5 == 0 ? (uint32_t)rng() : rng() % 4;
    toneDrawOscBlocks(s, rng);
    toneDrawModulatorBlocks(in, rng);
    for (uint32_t l = 0; l < Vc2Tone::kLfos; ++l) in.at(In::kLfos + In::kLfoStride * l + In::kKeySync) = rng() % 2;
    for (uint32_t block : In::kOscs) in.at(block + In::kLegato) = rng() % 2;
    toneDrawCosmBlocks(in, rng);
    toneDrawMixBlock(in, rng);
    in.at(In::kTva) = rng() % Vc2Tone::kEnvelopes;
    toneDrawTvaBlock(in, rng);
    if (rng() % 6 == 0) {   // the fix-up's case: its kind half the time, else one whose type 10 the note-on must leave
        // [FW] kinds whose install takes no wave's type but kinds 4's and 7's flag at type 1 (Vc2OscVoiceKinds.cpp)
        constexpr std::array<uint32_t, 3> kOtherKinds { 0, 4, 7 };
        in.at(In::kMix) = kFixMix;
        const uint32_t kind = drawFlag(rng) ? kFixKind : kOtherKinds[rng() % kOtherKinds.size()];
        toneOscBlockOfKind(s, 1, kind, kFixWave, rng);
    }
}

// The fix-up's case against a legato: the earlier tone's OSC 2 of the fix-up's kind, at the wave before it or after it,
// so that whether the note-on fixes the input before its OSCs' note-ons decides whether the legato takes the player;
// from v319 that voice made an analog one on both sides, as its note would have left it
void drawFixedLegato(ToneSides& s, int from, std::mt19937& rng) {
    In& in = s.shared.input;
    toneMakeOscAnalog(s, from, 1, drawFlag(rng) ? kFixWave : kFixedWave, rng);
    in.at(In::kMix) = kFixMix;
    toneOscBlockOfKind(s, 1, kFixKind, kFixWave, rng);
    in.at(In::kOscs[1] + In::kLegato) = 1;
}

// v328: tone i's OSC made a mode 1 voice (LITE's kind, type 1) on both sides, as its note and install pass leave it:
// its records, head and tempo words, then the install on both, which runs the set-up.  Only a voice bound to its
// tone's own parts, as a note's binding leaves it: one no note bound reads through address 0 once a player
void makeModeOne(ToneSides& s, int i, int osc) {
    constexpr uint32_t kInstallAt = 0x893ae0f0, kLite = 1, kModeOne = 1;   // [FW] the voice's install
    Vc2OscVoice& v = s.parts[(size_t)i].oscs[(size_t)osc];
    const uint32_t tone = s.pool.addressOf(i);
    if (!std::ranges::all_of(v.tail.binds, [&](uint32_t b) { return b - tone < Vc2Tone::kSize; })) return;
    v.setU32(Vc2OscVoice::kKind, kLite);
    v.setU32(Vc2OscVoice::kWave, kModeOne);
    toneSteerModeOne(s, i, osc);
    if (s.fw.call(kInstallAt, { toneOscAt(s.pool.addressOf(i), osc) })) s.parts[(size_t)i].installOsc(osc);
}

// The legato's tone: none, a tone made (its OSCs' kind and +0xb0 matched now and then), or a serial no tone has (with
// the legato's switches off, or rom.4 would read from address 0)
void drawPrevious(ToneSides& s, std::mt19937& rng) {
    In& in = s.shared.input;
    const uint32_t pick = rng() % 3;
    in.at(In::kPrevious) = pick == 0 ? (uint32_t)-1 : kNoSerial;
    if (pick == 1) {
        const int from = s.live[rng() % s.live.size()];
        in.at(In::kPrevious) = (uint32_t)s.pool.tones[(size_t)from].head.serial;
        for (size_t o = 0; o < In::kOscs.size(); ++o) {
            if (rng() % 2 != 0) continue;
            // v328: a take brings the voice's object and head, through which the set-up reads its records: a mode 1
            // voice made and set up one time in six, else a mode 1 voice's words as its note-on left them
            if (rng() % 6 == 0) makeModeOne(s, from, (int)o);
            else if (toneOscModeOne(s, from, (int)o)) toneSteerModeOne(s, from, (int)o);
            const Vc2OscVoice& was = s.parts[(size_t)from].oscs[o];   // v319: the real voice, its kind's words drawn
            toneOscBlockOfKind(s, (int)o, was.kind(), was.u32(Vc2OscVoice::kWave), rng);
        }
        if (rng() % 4 == 0) drawFixedLegato(s, from, rng);
    }
    if (pick == 2) {
        for (uint32_t block : In::kOscs) in.at(block + In::kLegato) = 0;
    }
}

// The tones the note-on's walk back reaches, up to three (the part's real lists from v273): now and then one of them
// with tone i's id, and each node's +0x14 itself, now and then 0 (passed over) or another tone
void drawListedWords(ToneSides& s, int i, std::mt19937& rng) {
    const std::array<uint32_t, 2> marks = s.shared.lists.marks();
    std::vector<int> listed;
    uint32_t node = marks[0] != 0 ? marks[0] : marks[1];
    for (; node != 0 && s.pool.holds(node) && listed.size() < kListed; node = s.pool.at(node).head.prev) {
        listed.push_back(s.pool.indexOf(node));
    }
    if (!listed.empty() && drawFlag(rng)) {
        const int j = listed[rng() % listed.size()];
        toneBothWord(s, j, kToneIdAt, s.pool.tones[(size_t)j].head.id, s.pool.tones[(size_t)i].head.id);
    }
    for (int t : listed) {
        const uint32_t pick = rng() % 6;
        const int other = s.live[rng() % s.live.size()];
        const uint32_t self = pick == 0 ? 0 : s.pool.addressOf(pick == 1 ? other : t);
        toneBothWord(s, t, kToneSelfAt, s.pool.tones[(size_t)t].head.self, self);
    }
}
} // namespace

std::string toneDifference(ToneSides& s, int only) {
    const Vc2TonePool& p = s.pool;
    const float& chaos = s.shared.chaos;
    std::string what = vc2RegionsDifference(s.fw, { { kHeadAt, &p.head, sizeof p.head, "the pool's head" },
                                                    { kSerialAt, &p.serial, sizeof p.serial, "the serial" },
                                                    { kChaosAt, &chaos, sizeof chaos, "the LFOs' generator" } });
    for (int i = 0; i < Vc2TonePool::kTones && what.empty(); ++i) {
        if (only < 0 || i == only) what = toneWordsDifference(s, i);
    }
    const auto& in = s.shared.input.words;
    if (what.empty()) what = vc2FirstDifference(s.fw, kToneInputAt, in.data(), sizeof in, "the input");
    if (what.empty()) what = toneListsDifference(s);
    if (what.empty()) what = tonePoolsDifference(s);
    if (what.empty() && only < 0) what = toneFreeLfosDifference(s);   // the events that can reach the manager
    if (what.empty()) what = toneCallsDifference(s.rom, s.shared.calls);
    return what.empty() ? toneOscCallsDifference(s) : what;
}

bool toneOscModeOne(ToneSides& s, int i, int osc) { return oscIsModeOne(s.parts[(size_t)i].oscs[(size_t)osc]); }

void toneSteerModeOne(ToneSides& s, int i, int osc) {
    Vc2OscVoice& v = s.parts[(size_t)i].oscs[(size_t)osc];
    const uint32_t at = toneOscAt(s.pool.addressOf(i), osc);
    oscSteerModeOneSetUp(v, s.shared.osc, at);
    s.fw.cpu.write(at, &v, sizeof v);
}

void toneSteerOscs(ToneSides& s, int i, bool noteOn) {
    constexpr uint32_t kVariPhrase = 2, kLevelAt = 0x1e0;   // [FW] the kind; the object's level, the re-seek's wait
    constexpr uint32_t kFlagsWord = 0xe4 / 4, kWaveWord = 0xb0 / 4;   // [FW] v331: a block's flags and wave type
    for (int osc = 0; osc < 2; ++osc) {
        Vc2OscVoice& v = s.parts[(size_t)i].oscs[(size_t)osc];
        const uint32_t* block = &s.shared.input.at(In::kOscs[(size_t)osc]);
        const bool seeks = noteOn ? oscBlockIsModeOne(block) : oscIsModeOne(v) && v.tail.seekPending != 0;
        const bool band = !noteOn && v.kind() == kVariPhrase && s.shared.osc.renders() % 4 == 0;
        if (band) {
            v.tail.seekPending = 1;
            v.setF32(kLevelAt, 0.0f);
            ++s.n.bandReseeks;
        }
        s.n.modeOneSeeks += oscSteerModeOneSeek(v, s.shared.osc, seeks);
        const bool vari = (noteOn ? block[0] : v.kind()) == kVariPhrase;   // v330: its update steps its words
        const long drawn = vari ? oscSteerVariWords(v, s.shared.osc.renders) : 0;
        s.n.variDrawn += drawn;
        // v331: its note-on's range by events reads the object's events' table and the block's three events
        const bool events = noteOn && vari && oscVariByEvents((uint8_t)block[kFlagsWord], block[kWaveWord]);
        if (events) {
            oscSteerVariTable(v, s.shared.osc.records.events);
            oscHoldVariBlockEvents({ &s.shared.input.at(In::kOscs[(size_t)osc]), kOscBlockWords },
                                   s.shared.osc.records.events);
            ++s.n.variByEvents;
        }
        if (seeks || band || drawn != 0 || events) s.fw.cpu.write(toneOscAt(s.pool.addressOf(i), osc), &v, sizeof v);
    }
}

namespace {
// v328: the voice a LITE voice's head names when that is another pool tone's LITE voice holding a loop record's address,
// as a take leaves it; else its own
uint32_t takenHead(ToneSides& s, int i, int osc) {
    constexpr uint32_t kHeadAt = 0x1a0, kHeadOf = 0x108, kLoopAt = 0x130, kLite = 1;   // [FW]
    const uint32_t own = toneOscAt(s.pool.addressOf(i), osc);
    const Vc2OscVoice& self = s.parts[(size_t)i].oscs[(size_t)osc];
    const uint32_t named = self.u32(kHeadAt) - kHeadOf;
    const int t = s.pool.indexOf(named);
    for (int o = 0; self.kind() == kLite && s.pool.holds(named) && named != own && o < 2; ++o) {
        const Vc2OscVoice& v = s.parts[(size_t)t].oscs[(size_t)o];
        const auto isLoop = [&](int k) { return oscLoopAt(k) == v.u32(kLoopAt); };
        if (toneOscAt(s.pool.addressOf(t), o) != named || v.kind() != kLite) continue;
        if (std::ranges::any_of(std::views::iota(0, kOscRecords), isLoop)) return named;
    }
    return own;
}
} // namespace

uint32_t toneSettingsVoice(ToneSides& s, int i, int osc) {
    const uint32_t at = takenHead(s, i, osc);
    s.n.settingsTaken += at != toneOscAt(s.pool.addressOf(i), osc);
    return at;
}

int toneTakenOsc(ToneSides& s, int i) {
    for (int o = 0; o < 2; ++o) {
        if (takenHead(s, i, o) != toneOscAt(s.pool.addressOf(i), o)) return o;
    }
    return -1;
}

void tonePrepare(ToneSides& s, int i, std::mt19937& rng) {
    toneDrawOscs(s, rng);
    toneDrawModulatorWords(s, i, rng);
    toneDrawTva(s, i, rng);
    toneDrawCosms(s, i, rng);
    s.rom.clear();
    s.shared.calls.clear();
    ++s.n.events;
}

void tonePrepareNoteOn(ToneSides& s, int i, std::mt19937& rng) {
    tonePrepare(s, i, rng);
    drawToneInput(s, rng);
    drawPrevious(s, rng);
    // v328: a LITE wave of type 1 (the legato's kinds too) the records its set-up reads, from the renders' generator
    for (uint32_t block : In::kOscs) oscModeOneBlock(&s.shared.input.at(block), s.shared.osc.renders);
    toneSteerOscs(s, i, true);   // v329, v330; from v331 before the input's write, as it holds a block's events
    s.fw.cpu.write(kToneInputAt, s.shared.input.words.data(), sizeof s.shared.input.words);
    drawListedWords(s, i, rng);
}
#endif
