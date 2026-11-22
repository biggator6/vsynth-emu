// The VC-2 mode's blocks (UiVc2Kinds.h; v453)
#include "UiVc2Kinds.h"
#include "PluginVc2Params.h"
#include "UiVc2Details.h"
#include <algorithm>
#include <array>
#include <span>

namespace Vc2Blocks {

namespace {

using Blocks::EnvName;
using Blocks::Kind;
using Controls::KnobRow;

// [FW] OSC 2's IDs against OSC 1's, by run (research/UI_DESIGN_VC2.md, "What it rests on"): the OSC's own, its Detune
// and sub oscillator, the VOCODER OSC's, Harm Formant, the vibrato and Wave start offset, the second Wave Number, and its
// four envelopes and its LFO; COSM 2's envelopes and LFO against COSM 1's.  A run: its first and last ID, the distance
struct Run {
    int first, last, step;
};
constexpr Run kOscRuns[] = { { 0x2e4, 0x317, 0x34 }, { 0x34d, 0x34f, 3 }, { 0x354, 0x361, 0xe }, { 0x371, 0x375, 5 },
                             { 0x37c, 0x37f, 4 },    { 0x389, 0x389, 4 }, { 0x390, 0x3c3, 0x34 }, { 0x439, 0x440, 8 } };
constexpr Run kCosmRuns[] = { { 0x3f8, 0x411, 0x1a }, { 0x449, 0x450, 8 } };
constexpr int kCosmFirst = 0x462, kCosmType = 0x463, kCosmTyped = 0x927;   // [FW] a COSM's own IDs, the host's COSM n

// [FW] An envelope's and an LFO's parameters by role, after their first ID (Attack, Waveform): the roles D's envelope
// bars and LFO take (UiEnvelope.cpp, UiLfo.cpp)
struct Role {
    const char* name;
    int at;
};
constexpr Role kEnvRoles[] = { { "Attack", 0 },  { "Decay", 2 },       { "Sustain", 3 },     { "Release", 5 },
                               { "VelCurve", 6 }, { "VelSens", 7 },    { "AttackSens", 8 },  { "DecaySens", 9 },
                               { "ReleaseSens", 10 }, { "TimeKF", 11 } };
constexpr Role kLfoRoles[] = { { "Wave", 0 }, { "Rate", 2 },     { "Offset", 3 }, { "Delay", 4 },
                               { "FadeMode", 5 }, { "Fade", 6 }, { "KeySync", 7 } };
constexpr int kEnvFirst = 0x390, kEnvLast = 0x438, kLfoFirst = 0x439;
// [FW] An OSC's envelopes' Depth, the block's: Pitch's, Time's and Formant's (the Amp envelope has none), by envelope
constexpr std::array<std::pair<int, int>, 3> kEnvDepths { { { 0x390, 0x2ea }, { 0x39d, 0x30a }, { 0x3aa, 0x30f } } };
// [FW] d-6: rom.4 holds them, nothing found reads them, the first instance's
constexpr int kUnread[] = { 0x967, 0x968, 0x969, 0x96a, 0x96b, 0x373, 0x374, 0x37f, 0x389 };
constexpr int kMatrixFirst = 0x16a, kMatrixStride = 5;

// Each kind's runs, in Kind's order: MOD's and AMP's IDs are their zone's alone
constexpr std::array<std::span<const Run>, 4> kRunsOf { std::span<const Run>(kOscRuns), std::span<const Run>(),
                                                        std::span<const Run>(kCosmRuns), std::span<const Run>() };

int stepOf(std::span<const Run> runs, int id) {
    for (const Run& r : runs)
        if (id >= r.first && id <= r.last) return r.step;
    return 0;
}

bool cosmHolds(int id) {
    return id == kCosmFirst || id == kCosmType || id >= kCosmTyped;
}

template <size_t N>
int roleIn(const Role (&roles)[N], const juce::String& name) {
    for (const Role& r : roles)
        if (name == r.name) return r.at;
    return -1;
}

// A kind's row for an instance, as the kinds' table takes it
template <Kind K>
juce::String kindParam(int zone, int index, const juce::String& row) {
    return param(zone, K, index, row);
}

constexpr EnvName kNoEnv { nullptr, nullptr };
constexpr KnobRow kNoDepth { nullptr, nullptr };
constexpr std::array<EnvName, Blocks::kMaxEnvs> kNoEnvs { kNoEnv, kNoEnv, kNoEnv, kNoEnv };
constexpr std::array<const char*, Blocks::kMaxDests> kNoDests { nullptr, nullptr, nullptr, nullptr };
constexpr std::array<KnobRow, Blocks::kMaxDepths> kNoDepths { kNoDepth, kNoDepth, kNoDepth, kNoDepth };

// The kinds in Kind's order (UI_DESIGN_VC2.md, "SYNTH"): OSC n's envelopes 4n…4n+3 (Pitch, Time, Formant, Amp) and its
// LFO n; COSM n's envelopes 8+2n and 9+2n and its LFO 2+n; AMP's envelope 12 and LFO 5; the Matrix's destinations by
// rom.4's names, which the V-Synth's stems take; the depths an LFO reaches a block by
constexpr Blocks::BlockKind kKinds[] = {
    { "OSC", true, kindParam<Kind::osc>,
      { { { "Pitch", "390+" }, { "Time", "39d+" }, { "Formant", "3aa+" }, { "Amp", "3b7+" } } },
      "439+", 1, { { "OSC", nullptr, nullptr, nullptr } },
      { { { "2e9", "Pitch" }, { "309", "Time" }, { "30e", "Formant" }, { "2f0", "Level" } } },
      "2e5", kVc2OscDetails, kOscFaces },
    { "MOD", false, kindParam<Kind::mod>, kNoEnvs, nullptr, 0, kNoDests, kNoDepths, nullptr, kVc2ModDetails, 1 },
    { "COSM", true, kindParam<Kind::cosm>, { { { "Env A", "3f8+" }, { "Env B", "405+" }, kNoEnv, kNoEnv } }, "449+", 3,
      { { "CSM", nullptr, nullptr, nullptr } }, kNoDepths, "463", kVc2CosmDetails, kCosmFaces },
    { "AMP", false, kindParam<Kind::amp>, { { { "TVA", "42c+" }, kNoEnv, kNoEnv, kNoEnv } }, "459+", 5,
      { { "TVA-", "MFX-SEND", "CHO-SEND", "REV-SEND" } },
      { { { "2dc", "Level" }, { "2df", "Pan" }, kNoDepth, kNoDepth } }, nullptr, kVc2AmpDetails, 1 },
};

constexpr Blocks::Model kModels[kZones] = { { kKinds, 0, matrixId }, { kKinds, 1, matrixId } };

}

const Blocks::Model& model(int zone) {
    return kModels[(size_t)juce::jlimit(0, kZones - 1, zone)];
}

Blocks::Path path(int zone) {
    return Blocks::makePath(&model(zone));
}

int instanceId(Kind kind, int index, int id) {
    return id + index * stepOf(kRunsOf[(size_t)kind], id);
}

int whichInstance(Kind kind, int id, int cosm) {
    if (kind == Kind::cosm && cosmHolds(id)) return cosm;
    for (const Run& r : kRunsOf[(size_t)kind])
        for (int index = 0; index < 2; ++index)
            if (id >= r.first + index * r.step && id <= r.last + index * r.step) return index;
    return -1;
}

int rowId(const juce::String& row) {
    if (row.isEmpty()) return -1;
    const int id = row.upToFirstOccurrenceOf("+", false, false).getHexValue32();
    if (!row.containsChar('+')) return id;
    const juce::String role = row.fromFirstOccurrenceOf("+", false, false);
    if (id >= kLfoFirst) return roleIn(kLfoRoles, role) < 0 ? -1 : id + roleIn(kLfoRoles, role);
    if (role == "Depth") {
        const auto depth = std::ranges::find(kEnvDepths, id, &std::pair<int, int>::first);
        return depth != kEnvDepths.end() ? depth->second : -1;
    }
    return id <= kEnvLast && id >= kEnvFirst && roleIn(kEnvRoles, role) >= 0 ? id + roleIn(kEnvRoles, role) : -1;
}

juce::String param(int zone, Kind kind, int index, const juce::String& row) {
    const int id = rowId(row);
    if (id < 0) return {};
    const int cosm = kind == Kind::cosm && cosmHolds(id) ? index : 0;
    return vc2HostParamId(instanceId(kind, index, id), zone, cosm);
}

juce::String matrixId(int control, int field) {
    return vc2HostParamId(kMatrixFirst + kMatrixStride * control + field, 0, 0);
}

bool unread(int id) {
    for (const int first : kUnread)
        for (int index = 0; index < 2; ++index)
            if (id == instanceId(Kind::osc, index, first)) return true;
    return false;
}

}
