// What each kind of block has (UiBlockKinds.h; UI step 3)
#include "UiBlockKinds.h"
#include "PluginParamIds.h"
#include "UiDetailTables.h"

namespace Blocks {

namespace {

juce::String oscParam(int index, const juce::String& name) {
    return Params::oscId(index, name);
}
juce::String cosmParam(int index, const juce::String& name) {
    return Params::cosmId(index, name.toRawUTF8());
}
juce::String plainParam(int, const juce::String& name) {
    return name;
}

constexpr EnvName kNoEnv { nullptr, nullptr };
constexpr Controls::KnobRow kNoDepth { nullptr, nullptr };
constexpr std::array<EnvName, kMaxEnvs> kNoEnvs { kNoEnv, kNoEnv, kNoEnv, kNoEnv };
constexpr std::array<const char*, kMaxDests> kNoDests { nullptr, nullptr, nullptr, nullptr };
constexpr std::array<Controls::KnobRow, kMaxDepths> kNoDepths { kNoDepth, kNoDepth, kNoDepth, kNoDepth };

// The kinds in Blocks::Kind's order (UI_DESIGN.md, "The blocks and the lower panel"): an OSC's LFO n reaches its pitch,
// time (or PW), formant (or Fat) and level; COSM n's LFO is LFO 2+n, its depths its type's own (on its panel and in
// DETAILS); the patch TVA's LFO 5 its level and pan; MOD has no envelope, LFO or route
constexpr BlockKind kKinds[] = {
    { "OSC", true, oscParam,
      { { { "Pitch", "pitchEnv" }, { "Time", "timeEnv" }, { "Formant", "formantEnv" }, { "Amp", "oscEnv" } } },
      "lfo", 1, { { "OSC", nullptr, nullptr, nullptr } },
      { { { "lfoPitchDepth", "Pitch" }, { "lfoTimeDepth", "Time / PW" }, { "lfoFormantDepth", "Formant / Fat" },
          { "oscLevelLfoDepth", "Level" } } },
      "oscType", kOscDetails, kOscFaces },
    { "MOD", false, plainParam, kNoEnvs, nullptr, 0, kNoDests, kNoDepths, nullptr, kModDetails, 1 },
    { "COSM", true, cosmParam, { { { "Env A", "EnvA" }, { "Env B", "EnvB" }, kNoEnv, kNoEnv } }, "Lfo", 3,
      { { "CSM", nullptr, nullptr, nullptr } }, kNoDepths, "Type", kCosmDetails, kCosmFaces },
    { "AMP", false, plainParam, { { { "TVA", "amp" }, kNoEnv, kNoEnv, kNoEnv } }, "tvaLfo", 5,
      { { "TVA-", "MFX-SEND", "CHO-SEND", "REV-SEND" } },
      { { { "tvaLevelLfo", "Level" }, { "tvaPanLfo", "Pan" }, kNoDepth, kNoDepth } }, nullptr, kAmpDetails, 1 },
};

}

const BlockKind& kindOf(const Block& b) {
    return kKinds[(size_t)b.kind];
}

juce::String blockName(const Block& b) {
    const BlockKind& k = kindOf(b);
    return k.numbered ? juce::String(k.name) + " " + juce::String(b.index + 1) : juce::String(k.name);
}

juce::String blockParam(const Block& b, const juce::String& name) {
    return kindOf(b).param(b.index, name);
}

juce::String typeIdOf(const Block& b) {
    const BlockKind& k = kindOf(b);
    return k.type != nullptr ? blockParam(b, k.type) : juce::String();
}

int envelopesOf(const Block& b) {
    int n = 0;
    for (const EnvName& e : kindOf(b).envs) n += e.name != nullptr ? 1 : 0;
    return n;
}

juce::String lfoNameOf(const Block& b) {
    const BlockKind& k = kindOf(b);
    return k.lfo != nullptr ? "LFO " + juce::String(k.lfoFirst + b.index) : juce::String();
}

bool reaches(const Block& b, const juce::String& dest) {
    const BlockKind& k = kindOf(b);
    for (const char* stem : k.dests) {
        if (stem == nullptr) continue;
        const juce::String number = k.numbered ? juce::String(b.index + 1) + "-" : juce::String();
        if (dest.startsWith(juce::String(stem) + number)) return true;
    }
    return false;
}

}
