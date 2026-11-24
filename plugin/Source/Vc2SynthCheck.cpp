// xt_vc2synth <rom.4> (v314): Vc2Synth's start against rom.4's.  On a fresh image (C startup's .data and .bss), rom.4's
// own static initializers run in the interpreter in its table's order: the whole table's three OS MIDI rows (0x893179d0,
// 0x89317db0, v347's 0x89318140), then the part's rows from 0x898c5cc0 to the one before 0x893f5070 (a task's that
// isn't the part's); from v347 the system's start after them (the store object, the record's and D's defaults).
// Then Vc2Synth built and started, and every member compared with rom.4's memory at its address, byte for byte: the
// whole objects, the tables a field at a time, the tone pool's tones by the words a tone holds (its head and tail),
// the vocoder by v219's shared compares (its engines and stages) with its engine and hook words.  The start is one,
// so the run is one case whatever [cases] says.  XT_VC2SYNTH_FIRST=1 says what differs.  XT_VC2SYNTH_MAIN only.
#ifdef XT_VC2SYNTH_MAIN
#include "Vc2CheckRig.h"
#include "Vc2CheckVocoder.h"   // v345
#include "Vc2CheckWords.h"
#include "Vc2Synth.h"
#include "Vc2SynthCheck.h"    // v343
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
#include "Vc2CheckRom2.h"          // v347
#include "Vc2OscConvertTables.h"   // v347
#include "XtAnalog.h"              // v347: rom.2's tables
#include "XtCosm.h"
#include "XtEnvelope.h"
#include "XtLfo.h"
#include "XtMultiBandPlayer.h"
#include "XtOsc.h"
#include "Vc2OscTables.h"
#include "Vc2PartMixerTables.h"
#include "Vc2PartSettingsTables.h"
#include "Vc2PartVoicesTables.h"
#include "Vc2Params.h"   // v345
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
// [FW] the OS MIDI's rows, earlier: its ports, its states, (v347) its part objects
constexpr uint32_t kMidiRows[] = { 0x893179d0, 0x89317db0, 0x89318140 };

// v342: the part's regions are the plugin's (Vc2SynthMemory.h, moved from here)
using SynthRegion = Vc2SynthRegion;
std::vector<SynthRegion> synthRegions(Vc2Synth& s) { return vc2SynthRegions(s); }
template <class T>
SynthRegion region(uint32_t at, T& mine, const char* what) { return vc2SynthRegion(at, mine, what); }

std::string bytesDifference(Vc2Fw& fw, const SynthRegion& r) { return vc2SynthBytesDifference(fw, r); }

std::string synthDifference(Vc2Fw& fw, Vc2Synth& s) { return vc2SynthDifference(fw, s); }

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
        if (!fw.call(kMakeAt, { 0, id })) return "the make faults: " + fw.cpu.error();
        const uint32_t mine = s.makeTone(id);   // v345: the part's own pick of the tone
        if (mine == 0 || fw.cpu.r[0] != mine) return "the make's tone";
        const int next = s.tones.indexOf(mine);
        ++made;
        std::string d = bytesDifference(fw, region(0x8993fb48, s.tones.head, "the tone pool's head"));
        if (d.empty()) d = bytesDifference(fw, region(0x89999668, s.chaos, "the chaos generator"));
        if (d.empty()) d = madeDifference(fw, s, next);
        if (!d.empty()) return d;
    }
    return "";
}

// v346: the tones given back in turn, rom.4's 0x893b2fc0(tone, 3) and the pool's unmake through the tone's host, so a
// note finds the pool free; every region after each
std::string unmakeTones(Vc2Fw& fw, Vc2Synth& s) {
    constexpr uint32_t kUnmakeAt = 0x893b2fc0, kBack = 3;   // [FW]
    for (int k = 0; k < Vc2TonePool::kTones; ++k) {
        if (!fw.call(kUnmakeAt, { s.tones.addressOf(k), kBack })) return "the unmake faults: " + fw.cpu.error();
        s.tones.unmake(s.tones.tones[(size_t)k], true, s.hosts[(size_t)k]);
        const std::string d = vc2SynthRegionsDifference(fw, s);
        if (!d.empty()) return "tone " + std::to_string(k) + "'s unmake: " + d;
    }
    return "";
}

// v347: rom.2's tables the part's ports read (its math's velocity curve and exp2, the envelopes', LFOs', COSMs', band
// players' and analog OSC's), from the rom.2 beside rom.4, as the plugin loads them beside rom.4's
bool synthRom2Tables(const std::string& rom2) {
    const char* p = rom2.c_str();
    return xtOscLoadTables(p) && xtEnvLoadTables(p) && xtLfoLoadTables(p) && xtCosmLoadTables(p) && xtMbLoadTables(p)
           && xtAnalogLoadTables(p);
}

bool synthLoadTables(const char* path) {
    return vc2LoadAll<vc2LoadPartVoices, vc2LoadKeys, vc2LoadOsMidi, vc2LoadPartSettings, vc2LoadUnvoiced,
                      vc2LoadLegacy, vc2LoadHireso, vc2LoadVocoder, vc2LoadCarrier, vc2LoadTracker, vc2LoadAutoNote,
                      vc2LoadKind6, vc2LoadMixer, vc2LoadCosm, vc2LoadToneMix, vc2LoadSetEnvelopes, vc2LoadMatrix,
                      vc2LoadLibMath, vc2LoadOsc, vc2LoadRamp, vc2LoadTva, vc2LoadParams, vc2LoadSystem,
                      vc2LoadOscConvert>(path)   // v347: the system, the OSC converters' (a note reaches C)
           && synthRom2Tables(vc2Rom2Of(path));
}

// v347: the system's start on rom.4's side, as an XT starts with no system file: the store object built (its read
// objects aimed, C's defaults), then the record's defaults and D's (its caller given no flag)
std::string systemStart(Vc2Fw& fw) {
    constexpr uint32_t kStoreObject = 0x89961250, kBuild = 0x8931a640;     // [FW] the OS row 0x8931a840's first call
    constexpr uint32_t kRecordDefaults = 0x89319e10, kDDefaults = 0x89319a40;   // [FW]
    const bool built = fw.call(kBuild, { kStoreObject });
    const bool done = built && fw.call(kRecordDefaults, {}) && fw.call(kDDefaults, { 0, 0 });
    return done ? "" : "the system's start faults: " + fw.cpu.error();
}
} // namespace

std::string vc2SynthBytesDifference(Vc2Fw& fw, const Vc2SynthRegion& r) {
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

std::string vc2SynthRegionsDifference(Vc2Fw& fw, Vc2Synth& s) {
    for (const SynthRegion& r : synthRegions(s))   // v347: not the part's own words
        if (std::string d = r.own ? "" : vc2SynthBytesDifference(fw, r); !d.empty()) return d;
    return "";
}

std::string vc2SynthToneDifference(Vc2Fw& fw, Vc2Synth& s, int tone) { return madeDifference(fw, s, tone); }

// v345: the vocoder by the checks' shared compare (its engine's and hook's words by the port's engine)
std::string vc2SynthDifference(Vc2Fw& fw, Vc2Synth& s) {
    const std::string d = vc2SynthRegionsDifference(fw, s);
    return d.empty() ? vc2VocoderDifference(fw, s.vocoder) : d;
}

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2synth", synthLoadTables);
    if (!run) return 1;
    int rows = 0;
    std::string what = runRows(fw, rows);
    if (what.empty()) what = systemStart(fw);   // v347
    const auto synth = std::make_unique<Vc2Synth>();
    synth->start();
    if (vc2SystemDefaults() != nullptr) synth->giveSystem(*vc2SystemDefaults());
    if (what.empty()) what = synthDifference(fw, *synth);
    if (!what.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the start: %s\n", what.c_str());
    std::printf("synth: the start, %d of rom.4's initializers, %zu regions and the vocoder compared\n", rows,
                synthRegions(*synth).size());
    std::mt19937 rng((uint32_t)run->seed);   // v340: the tones made through the part's hosts
    int made = 0;
    std::string tones = what.empty() ? makeTones(fw, *synth, rng, made) : "";
    if (tones.empty() && what.empty()) tones = unmakeTones(fw, *synth);   // v346: the pool free again
    if (!tones.empty() && xtSwitch("XT_VC2SYNTH_FIRST")) std::printf("  the tones made: %s\n", tones.c_str());
    std::printf("synth: %d tones made through the part's hosts, their words compared, then given back\n", made);
    const bool sound = what.empty() && tones.empty();
    Vc2SynthPosts posts;   // v346: the OS's hooks, the parts' one rig
    vc2SynthInstallOs(fw, posts);
    const long load = sound ? vc2SynthLoadPart(fw, *synth, *run, posts) : 0;     // v345
    const long notes = sound ? vc2SynthNotesPart(fw, *synth, *run, posts) : 0;   // v346
    const long reads = sound ? vc2SynthReadsPart(fw, *synth, *run) : 0;   // v343: after the load, its noise last
    const long tally = (what.empty() ? 0 : 1) + (tones.empty() ? 0 : 1) + load + notes + reads;
    std::printf("xt_vc2synth: %ld different\n", tally);
    return 0;
}
#endif
