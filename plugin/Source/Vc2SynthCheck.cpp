// xt_vc2synth <rom.4> (v314): Vc2Synth's start against rom.4's.  On a fresh image (C startup's .data and .bss), rom.4's
// own static initializers run in the interpreter in its table's order: the whole table's two OS MIDI rows (0x893179d0,
// 0x89317db0), then the part's rows from 0x898c5cc0 to the one before 0x893f5070 (a task's that isn't the part's).
// Then Vc2Synth built and started, and every member compared with rom.4's memory at its address, byte for byte: the
// whole objects, the tables a field at a time, the tone pool's tones by the words a tone holds (its head and tail),
// the vocoder by v219's shared compares (its engines and stages) with its engine and hook words.  The start is one,
// so the run is one case whatever [cases] says.  XT_VC2SYNTH_FIRST=1 says what differs.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2CheckRig.h"
#include "Vc2CheckWords.h"
#include "Vc2Synth.h"
#include "Vc2SynthMemory.h"   // v342: the part's regions
#include "Vc2AutoNoteTables.h"
#include "Vc2CarrierTables.h"
#include "Vc2CosmTables.h"
#include "Vc2HiresoTables.h"
#include "Vc2Kind6Tables.h"
#include "Vc2KeysTables.h"
#include "Vc2LegacyTables.h"
#include "Vc2LibMathTables.h"
#include "Vc2MatrixTables.h"
#include "Vc2OsMidiTables.h"
#include "Vc2OscTables.h"
#include "Vc2PartMixerTables.h"
#include "Vc2PartSettingsTables.h"
#include "Vc2PartVoicesTables.h"
#include "Vc2RampTables.h"
#include "Vc2SetEnvelopeTables.h"
#include "Vc2ToneMixTables.h"
#include "Vc2TrackerTables.h"
#include "Vc2TvaTables.h"
#include "Vc2Unvoiced.h"
#include "Vc2VocoderTables.h"
#include <cstring>
#include <memory>
#include <vector>

namespace {
constexpr uint32_t kRowsAt = 0x898c5cc0, kRowBytes = 12, kRowRoutine = 4;   // [FW] the part's rows: {0, routine, 0}
constexpr uint32_t kFirstOutside = 0x893f5070;                              // [FW] row 95's routine, not the part's
constexpr uint32_t kMidiRows[] = { 0x893179d0, 0x89317db0 };                 // [FW] the OS MIDI's rows, earlier
constexpr uint32_t kVocoderAt = kVc2SynthVocoderAt, kHiresoAt = kVocoderAt + 0x24, kLegacyAt = kVocoderAt + 0x2d94;
constexpr uint32_t kEngineWord = kVocoderAt + 0x20, kHookWord = 0x899d9d8c, kLegacyHook = 0x893f0480;   // [FW]

// v342: the part's regions are the plugin's (Vc2SynthMemory.h, moved from here)
using SynthRegion = Vc2SynthRegion;
std::vector<SynthRegion> synthRegions(Vc2Synth& s) { return vc2SynthRegions(s); }
template <class T>
SynthRegion region(uint32_t at, T& mine, const char* what) { return vc2SynthRegion(at, mine, what); }

std::string bytesDifference(Vc2Fw& fw, const SynthRegion& r) {
    std::vector<uint8_t> theirs(r.bytes);
    fw.cpu.read(r.at, theirs.data(), r.bytes);
    const auto* ours = static_cast<const uint8_t*>(r.mine);
    for (size_t k = 0; k < r.bytes; ++k) {
        if (theirs[k] == ours[k]) continue;
        char t[160];
        std::snprintf(t, sizeof t, "%s, byte 0x%08x: %02x, against %02x", r.what, r.at + (uint32_t)k, ours[k],
                      theirs[k]);
        return t;
    }
    return "";
}

// What differs: the regions in order, then the vocoder's engines, stages, engine word and hook word
std::string synthDifference(Vc2Fw& fw, Vc2Synth& s) {
    for (const SynthRegion& r : synthRegions(s))
        if (std::string d = bytesDifference(fw, r); !d.empty()) return d;
    std::string d = vc2HiresoDifference(fw, kHiresoAt, s.vocoder.hireso);
    if (d.empty()) d = vc2LegacyDifference(fw, kLegacyAt, s.vocoder.legacy);
    if (d.empty()) d = vc2UnvoicedDifference(fw, s.vocoder.stage);
    if (d.empty() && fw.cpu.rd32(kEngineWord) != 0) d = "the vocoder's engine word";
    if (d.empty() && fw.cpu.rd32(kHookWord) != kLegacyHook) d = "the vocoder's hook word";
    return d;
}

// rom.4's start: the OS MIDI's rows, then the part's, each called in turn; how many, or what faulted
std::string runRows(Vc2Fw& fw, int& rows) {
    std::vector<uint32_t> routines(std::begin(kMidiRows), std::end(kMidiRows));
    for (uint32_t at = kRowsAt; fw.cpu.rd32(at + kRowRoutine) != kFirstOutside; at += kRowBytes)
        routines.push_back(fw.cpu.rd32(at + kRowRoutine));
    for (const uint32_t routine : routines) {
        if (!fw.call(routine, {})) return "the initializer " + std::to_string(routine) + " faults: " + fw.cpu.error();
        ++rows;
    }
    return "";
}

// v340: a part's words with the index the tone keeps for it laid over its word at `indexAt` (an envelope's +4, an
// LFO's +0x34), as rom.4's memory holds the two in one
template <class Words>
Words withIndex(const Words& w, int32_t index, size_t indexAt) {
    Words out = w;
    std::memcpy(reinterpret_cast<uint8_t*>(&out) + indexAt, &index, sizeof index);
    return out;
}

// v340: a made tone's words, each region by its place in the tone: its head, tail and link, its OSC voices, TVA, mix,
// and its envelopes' and LFOs' words (its COSM objects hold rom.4's own pointers, which xt_vc2tone masks: there)
std::string madeDifference(Vc2Fw& fw, Vc2Synth& s, int tone) {
    constexpr uint32_t kOsc2At = 0xba4, kTvaAt = 0x19f0, kMixAt = 0x1ab8, kLinkAt = 0x2618;     // [FW] the tone's
    constexpr uint32_t kOsc1At = 0x20, kEnvelopesAt = 0x1b38, kLfosAt = 0x2158;                  // [FW]
    const uint32_t at = s.tones.addressOf(tone);
    Vc2Tone& t = s.tones.tones[(size_t)tone];
    Vc2SynthTone& h = s.hosts[(size_t)tone];
    std::vector<SynthRegion> regions = {
        region(at, t.head, "its head"), region(at + kVc2ToneTailAt, t.tail, "its tail"),
        region(at + kLinkAt, t.link, "its link"), region(at + kOsc1At, h.oscs[0], "OSC 1's voice"),
        region(at + kOsc2At, h.oscs[1], "OSC 2's voice"), region(at + kTvaAt, h.tva.w, "its TVA"),
        region(at + kMixAt, h.mix.w, "its mix"),
    };
    constexpr size_t kEnvelopeIndexAt = 4, kLfoIndexAt = 0x34;   // [FW]
    std::vector<Vc2EnvelopeWords> envelopes;
    std::vector<Vc2LfoWords> lfos;
    for (size_t k = 0; k < h.mods.envelopes.size(); ++k) {
        envelopes.push_back(withIndex(h.mods.envelopes[k].w, t.envelopeIndex[k], kEnvelopeIndexAt));
    }
    for (size_t k = 0; k < h.mods.lfos.size(); ++k) {
        lfos.push_back(withIndex(h.mods.lfos[k].w, t.lfoIndex[k], kLfoIndexAt));
    }
    for (size_t k = 0; k < envelopes.size(); ++k) {
        regions.push_back(region(at + kEnvelopesAt + (uint32_t)(k * sizeof(Vc2EnvelopeWords)), envelopes[k],
                                 "an envelope"));
    }
    for (size_t k = 0; k < lfos.size(); ++k) {
        regions.push_back(region(at + kLfosAt + (uint32_t)(k * sizeof(Vc2LfoWords)), lfos[k], "an LFO"));
    }
    for (const SynthRegion& r : regions) {
        if (std::string d = bytesDifference(fw, r); !d.empty()) return "tone " + std::to_string(tone) + ", " + d;
    }
    return "";
}

// v340: the 24 tones made in turn on both sides, each id drawn: rom.4's 0x893b2db0(0, id), and the pool's make through
// the tone's host, the part's; after each the pool's words, the chaos generator and the tone's words
std::string makeTones(Vc2Fw& fw, Vc2Synth& s, std::mt19937& rng, int& made) {
    constexpr uint32_t kMakeAt = 0x893b2db0, kIds = 64;   // [FW]
    // [FW] the OS's lock and unlock the make takes (into the kernel at 0x88008000): nothing on one thread
    for (uint32_t at : { 0x893018b0u, 0x893018c0u }) {
        fw.cpu.hook(at, [](Sh4& cpu) { cpu.ret(0); });
    }
    for (int k = 0; k < Vc2TonePool::kTones; ++k) {
        const uint32_t id = rng() % kIds;
        const int next = s.tones.head == 0 ? 0 : s.tones.indexOf(s.tones.head);
        if (!fw.call(kMakeAt, { 0, id })) return "the make faults: " + fw.cpu.error();
        const Vc2Tone* mine = s.tones.make(id, s.hosts[(size_t)next]);
        if (mine == nullptr || fw.cpu.r[0] != s.tones.addressOf(next)) return "the make's tone";
        ++made;
        std::string d = bytesDifference(fw, region(0x8993fb48, s.tones.head, "the tone pool's head"));
        if (d.empty()) d = bytesDifference(fw, region(0x89999668, s.chaos, "the chaos generator"));
        if (d.empty()) d = madeDifference(fw, s, next);
        if (!d.empty()) return d;
    }
    return "";
}

bool synthLoadTables(const char* path) {
    return vc2LoadAll<vc2LoadPartVoices, vc2LoadKeys, vc2LoadOsMidi, vc2LoadPartSettings, vc2LoadUnvoiced,
                      vc2LoadLegacy, vc2LoadHireso, vc2LoadVocoder, vc2LoadCarrier, vc2LoadTracker, vc2LoadAutoNote,
                      vc2LoadKind6, vc2LoadMixer, vc2LoadCosm, vc2LoadToneMix, vc2LoadSetEnvelopes, vc2LoadMatrix,
                      vc2LoadLibMath, vc2LoadOsc, vc2LoadRamp, vc2LoadTva>(path);   // v340: the tones' parts' too
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2synth", synthLoadTables);
    if (!run) return 1;
    int rows = 0;
    std::string what = runRows(fw, rows);
    const auto synth = std::make_unique<Vc2Synth>();
    synth->start();
    if (what.empty()) what = synthDifference(fw, *synth);
    if (!what.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the start: %s\n", what.c_str());
    std::printf("synth: the start, %d of rom.4's initializers, %zu regions and the vocoder compared\n", rows,
                synthRegions(*synth).size());
    std::mt19937 rng((uint32_t)run->seed);   // v340: the tones made through the part's hosts
    int made = 0;
    std::string tones = what.empty() ? makeTones(fw, *synth, rng, made) : "";
    if (!tones.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the tones made: %s\n", tones.c_str());
    std::printf("synth: %d tones made through the part's hosts, their words compared\n", made);
    std::printf("xt_vc2synth: %d different\n", (what.empty() ? 0 : 1) + (tones.empty() ? 0 : 1));
    return 0;
}
#endif
