// xt_vc2tone's part for the tone's remaining methods and the part's lists (v273): see Vc2ToneSides.h.  rom.4's own methods
// run on its pool with what they call hooked (Vc2ToneFakes.cpp), and its own list routines on the lists' words at
// 0x89999678, 0x899996d0 and 0x899996ec, against Vc2Tone's methods and Vc2ToneLists.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2CheckCalls.h"
#include <bit>
#include <cstddef>

namespace {
using In = Vc2ToneInput;
using Lists = Vc2ToneLists;
// [FW] the lists' routines, the start, and the tone's key-off, truncations and cost
constexpr uint32_t kUnlistSoundingAt = 0x893ad9a0, kUnlistNewAt = 0x893ada30, kEnlistAt = 0x893ada10;
constexpr uint32_t kJoinAt = 0x893ada90, kStartAt = 0x893b1cd0, kKeyOffAt = 0x893b3ad0;
constexpr uint32_t kTruncateWaitAt = 0x893b3bc0, kTruncateNowAt = 0x893b3c20, kCostAt = 0x893b4020;
constexpr uint32_t kListHeadBytes = 0x1c;   // [FW] 0x893b1fcc: each head an object of 0x1c bytes
constexpr int kMostWaits = 3;
static_assert(offsetof(Lists, joined) == offsetof(Lists, newest) + sizeof(uint32_t), "the marks' words");

enum class ToneOnList { none, sounding, fresh };

bool listHolds(ToneSides& s, uint32_t first, uint32_t tone) {
    for (uint32_t at = first; at != 0; at = s.pool.at(at).head.next) {
        if (at == tone) return true;
    }
    return false;
}

ToneOnList listOf(ToneSides& s, int i) {
    const uint32_t tone = s.pool.addressOf(i);
    if (listHolds(s, s.shared.lists.sounding, tone)) return ToneOnList::sounding;
    return listHolds(s, s.shared.lists.fresh, tone) ? ToneOnList::fresh : ToneOnList::none;
}

// A list's routine that takes a tone off it, rom.4's and the port's, each answering the tone or 0
struct ToneListMove {
    uint32_t at;
    uint32_t (Lists::*move)(uint32_t, Vc2TonePool&);
};
constexpr ToneListMove kUnlistSounding = { kUnlistSoundingAt, &Lists::unlistSounding };
constexpr ToneListMove kUnlistNew = { kUnlistNewAt, &Lists::unlistNew };

std::string listMoveOn(ToneSides& s, int i, const ToneListMove& m) {
    const uint32_t tone = s.pool.addressOf(i);
    uint32_t mine = 0;
    const std::string what = vc2BothOn(
        s.fw, m.at, { 0, tone }, "a list's routine", [&] { mine = (s.shared.lists.*m.move)(tone, s.pool); },
        [&] { return toneDifference(s, -1); });
    return what.empty() && s.fw.cpu.r[0] != mine ? "a list's routine's answer" : what;
}

std::string bothListsOn(ToneSides& s, uint32_t at, const char* name, void (Lists::*mine)(Vc2TonePool&)) {
    return vc2BothOn(
        s.fw, at, {}, name, [&] { (s.shared.lists.*mine)(s.pool); }, [&] { return toneDifference(s, -1); });
}

// [FW] the block setters: the routine, how many numbers it takes (0 none) and whether one past 1 is its to pass over,
// the method, one taking a number or one not, and the input's block it is given: the mix's, or from v315, when the
// settings are real, the LFO's or envelope's own, the first at `block` and the others `stride` apart
struct ToneSetter {
    uint32_t at;
    uint32_t numbers;
    bool bounded;
    void (Vc2Tone::*numbered)(uint32_t, const uint32_t*, Vc2ToneParts&);
    void (Vc2Tone::*plain)(const uint32_t*, Vc2ToneParts&);
    uint32_t block = In::kMix, stride = 0;
};
constexpr uint32_t kSetOscAt = 0x893b3110;   // [FW] the OSC block's setter
constexpr ToneSetter kSetters[] = {
    { kSetOscAt, 2, true, &Vc2Tone::setOscBlock, nullptr },
    { 0x893b31a0, Vc2Tone::kLfos, false, &Vc2Tone::setLfoBlock, nullptr, In::kLfos, In::kLfoStride },
    { 0x893b31f0, Vc2Tone::kEnvelopes, false, &Vc2Tone::setEnvelopeBlock, nullptr, In::kEnvelopes,
      In::kEnvelopeStride },
    { 0x893b3240, 2, true, &Vc2Tone::setCosmBlock, nullptr, In::kCosms[0], In::kCosms[1] - In::kCosms[0] },
    { 0x893b32d0, 0, false, nullptr, &Vc2Tone::setMixBlock },
    { 0x893b3310, 0, false, nullptr, &Vc2Tone::setTvaBlock, In::kTva },
    { 0x893b3350, 0, false, nullptr, &Vc2Tone::setLegato, In::kOscSet },
};

// [FW] the position getters, each by an OSC's number
struct ToneGetter {
    uint32_t at;
    uint32_t (Vc2Tone::*mine)(uint32_t, Vc2ToneParts&) const;
};
constexpr ToneGetter kGetters[] = { { 0x893b3f80, &Vc2Tone::oscWord168 }, { 0x893b3fd0, &Vc2Tone::oscPosition } };

// An OSC's or a COSM's number: theirs, past theirs now and then, any word now and then
uint32_t drawPartNumber(std::mt19937& rng, uint32_t numbers) {
    const uint32_t pick = rng() % 8;
    return pick == 0 ? (uint32_t)rng() : (pick == 1 ? numbers : rng() % numbers);
}
} // namespace

std::string toneListsDifference(ToneSides& s) {
    const Lists& l = s.shared.lists;
    return vc2RegionsDifference(s.fw, { { Lists::kNewestAt, &l.newest, 2 * sizeof(uint32_t), "the lists' marks" },
                                        { Lists::kSoundingAt, &l.sounding, sizeof l.sounding, "the sounding list" },
                                        { Lists::kNewAt, &l.fresh, sizeof l.fresh, "the new list" } });
}

void toneEmptyLists(ToneSides& s) {
    const std::array<uint8_t, kListHeadBytes> zeros {};
    for (uint32_t at : { Lists::kSoundingAt, Lists::kNewAt }) s.fw.cpu.write(at, zeros.data(), zeros.size());
    s.fw.cpu.write(Lists::kNewestAt, zeros.data(), 2 * sizeof(uint32_t));
    s.shared.lists = {};
}

std::string toneLeaveLists(ToneSides& s, int i) {
    const ToneOnList on = listOf(s, i);
    if (on == ToneOnList::none) return "";
    ++s.n.leaves;
    return listMoveOn(s, i, on == ToneOnList::sounding ? kUnlistSounding : kUnlistNew);
}

// Half the time, links as the tone's last use might have left them: nothing reads a tone's links off its lists
void toneDrawLinks(ToneSides& s, int i, std::mt19937& rng) {
    if (rng() % 2 == 0) return;
    Vc2ToneHead& h = s.pool.tones[(size_t)i].head;
    toneBothWord(s, i, kToneNextAt, h.next, (uint32_t)rng());
    toneBothWord(s, i, kTonePrevAt, h.prev, (uint32_t)rng());
}

// A block from the input, drawn as its converter writes it, as the settings it reaches are real: the mix's, or (v315) the
// LFOs' and envelopes', (v317) the TVA's, (v318) a COSM's; a COSM's number past 1 reaches none, its block COSM 1's.  From
// v319 an OSC voice's settings, xt_vc2osc's settings block (its calls name a settings word by its offset there), and the
// legato's six words as a ramp's
std::string toneSetterOn(ToneSides& s, std::mt19937& rng) {
    const ToneSetter& set = kSetters[rng() % std::size(kSetters)];
    const int i = toneDrawLive(s, rng);
    In& in = s.shared.input;
    toneDrawMixBlock(in, rng);
    const uint32_t n = set.numbers == 0 ? 0 : (set.bounded ? drawPartNumber(rng, set.numbers) : rng() % set.numbers);
    if (set.numbered == &Vc2Tone::setCosmBlock) toneDrawCosmNewBlock(s, i, n, rng);
    else if (set.stride != 0) toneDrawModulatorBlocks(in, rng);
    if (set.block == In::kTva) toneDrawTvaBlock(in, rng);
    if (set.block == In::kOscSet) toneDrawOscRamp(in, rng);
    s.fw.cpu.write(kToneInputAt, in.words.data(), sizeof in.words);
    const uint32_t offset = set.block + set.stride * (n < set.numbers ? n : 0);
    const uint32_t tone = s.pool.addressOf(i), block = kToneInputAt + offset;
    Vc2Tone& t = s.pool.tones[(size_t)i];
    Vc2ToneParts& parts = s.parts[(size_t)i];
    ++s.n.setters;
    if (set.numbered == &Vc2Tone::setEnvelopeBlock) toneSteerSustain(s, i, n, rng);
    // v319: the sync's flag as a blend's note-on leaves it; from v331 half of those 0, as another type's settings leave
    // it, where a blend's settings set it (mix-settle-blend-sync had been caught by one case's chance)
    if (set.plain == &Vc2Tone::setMixBlock && rng() % 2) {
        constexpr uint32_t kSyncOnAt = 0x1afc;   // [FW] the tail's +4
        toneBothWord(s, i, kSyncOnAt, t.tail.syncOn, rng() % 2 ? 1u : 0u);
    }
    if (set.plain) return toneOn(s, i, set.at, { tone, block }, [&] { (t.*set.plain)(in.block(offset), parts); });
    if (set.numbered == &Vc2Tone::setOscBlock) return toneOscSettingsOf(s, i, n, rng, false);
    const std::string what =
        toneOn(s, i, set.at, { tone, n, block }, [&] { (t.*set.numbered)(n, in.block(offset), parts); });
    // v321: half the time a COSM's new block goes on into a render of the tone, before an update converts again: only
    // there does the sound's own new block show, which 40 cases reached about twice
    if (what.empty() && set.numbered == &Vc2Tone::setCosmBlock && rng() % 2) return toneRenderOf(s, i, rng);
    return what;
}

std::string toneOscSettingsOf(ToneSides& s, int i, uint32_t n, std::mt19937& rng, bool readsLoop) {
    toneDrawOscSettings(s, i, n, rng);
    if (readsLoop) {   // v328: a type whose way reads the loop record, Vari off's (0) or the heads' (2)
        constexpr uint32_t kTypeWord = 0xb0 / 4, kVariOff = 0, kHeads = 2;   // [FW]
        s.shared.osc.settings[kTypeWord] = rng() % 2 != 0 ? kVariOff : kHeads;
        s.fw.cpu.write(kOscSettingsAt, s.shared.osc.settings.data(), sizeof s.shared.osc.settings);
    }
    const uint32_t* settings = s.shared.osc.settings.data();
    Vc2Tone& t = s.pool.tones[(size_t)i];
    Vc2ToneParts& parts = s.parts[(size_t)i];
    const std::initializer_list<uint32_t> args = { s.pool.addressOf(i), n, kOscSettingsAt };
    return toneOn(s, i, kSetOscAt, args, [&] { t.setOscBlock(n, settings, parts); });
}

std::string toneKeyOffOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawSounding(s, rng, 4);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    ++s.n.keyOffs;
    toneCountReleases(s, i);
    return toneOn(s, i, kKeyOffAt, { s.pool.addressOf(i) }, [&] { tone.keyOff(s.parts[(size_t)i]); });
}

// The wait's truncation one time in three; else the one at once
std::string toneTruncateOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    Vc2Tone& tone = s.pool.tones[(size_t)i];
    Vc2ToneParts& parts = s.parts[(size_t)i];
    const uint32_t at = s.pool.addressOf(i);
    ++s.n.truncations;
    if (rng() % 3 != 0) return toneOn(s, i, kTruncateNowAt, { at }, [&] { tone.truncateAtOnce(parts); });
    const int waits = 1 + (int)(rng() % kMostWaits);
    s.shared.fade = { at, waits, waits };
    const std::string what = toneOn(s, i, kTruncateWaitAt, { at }, [&] { tone.truncateAndWait(parts); });
    s.n.waits += vc2CountCalls(s.rom, kWaitFade);
    return what;
}

std::string toneCostOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    float mine = 0.0f;
    ++s.n.costs;
    const std::string what = toneOn(s, i, kCostAt, { s.pool.addressOf(i) },
                                    [&] { mine = s.pool.tones[(size_t)i].cost(s.parts[(size_t)i]); });
    return what.empty() && s.fw.cpu.fb[0][0] != std::bit_cast<uint32_t>(mine) ? "the cost" : what;
}

std::string toneGetterOn(ToneSides& s, std::mt19937& rng) {
    const ToneGetter& get = kGetters[rng() % std::size(kGetters)];
    const int i = toneDrawLive(s, rng);
    toneDrawOscReads(s, i, rng);   // v319, v321
    const uint32_t n = drawPartNumber(rng, 2), tone = s.pool.addressOf(i);
    const Vc2Tone& t = s.pool.tones[(size_t)i];
    Vc2ToneParts& parts = s.parts[(size_t)i];
    uint32_t mine = 0;
    ++s.n.getters;
    const std::string what = toneOn(s, i, get.at, { tone, n }, [&] { mine = (t.*get.mine)(n, parts); });
    return what.empty() && s.fw.cpu.r[0] != mine ? "a getter's answer" : what;
}

// The tones it starts: those still built on either list, as the join brings the new ones onto the sounding list
std::string toneStartOn(ToneSides& s, std::mt19937& rng) {
    toneDrawLive(s, rng);
    ++s.n.starts;
    for (uint32_t first : { s.shared.lists.sounding, s.shared.lists.fresh }) {
        for (uint32_t at = first; at != 0; at = s.pool.at(at).head.next) {
            s.n.started += s.pool.at(at).head.state == Vc2Tone::kFromOne;
        }
    }
    return bothListsOn(s, kStartAt, "the start", &Lists::start);
}

// One of the lists' routines alone: the join now and then; a tone not listed onto the new list half the time; else a
// tone taken off the list that holds it two times in three, off the other one, where it isn't found, the third
std::string toneListOn(ToneSides& s, std::mt19937& rng) {
    const int i = toneDrawLive(s, rng);
    ++s.n.listMoves;
    if (rng() % 6 == 0) return bothListsOn(s, kJoinAt, "the join", &Lists::join);
    const ToneOnList on = listOf(s, i);
    if (on == ToneOnList::none && rng() % 2 == 0) {
        const uint32_t tone = s.pool.addressOf(i);
        return vc2BothOn(
            s.fw, kEnlistAt, { 0, tone }, "the enlist", [&] { s.shared.lists.enlist(tone, s.pool); },
            [&] { return toneDifference(s, -1); });
    }
    const bool own = rng() % 3 != 0, either = rng() % 2 == 0;
    const bool sounding = on == ToneOnList::sounding ? own : (on == ToneOnList::fresh ? !own : either);
    return listMoveOn(s, i, sounding ? kUnlistSounding : kUnlistNew);
}
#endif
