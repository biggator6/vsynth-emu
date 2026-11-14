#pragma once
// What each kind of block on the path has (research/UI_DESIGN.md, "The blocks and the lower panel"; UI step 3): its name,
// how an instance's parameter IDs are made, its envelopes, its LFO, the Matrix destinations that land on it and the
// depths its LFO reaches it by.  One row a kind, in Blocks::Kind's order: a block's lower panel is made from its kind's
// row and its index, so each OSC and each COSM is made the same way.
#include "UiBlocks.h"
#include "UiControls.h"
#include <array>

namespace Blocks {

inline constexpr int kMaxEnvs = 4, kMaxDests = 4, kMaxDepths = 4, kMaxDetails = 16;

// An envelope of a block: its tab's name and the prefix its parameters' names take ("pitchEnv" + "Attack")
struct EnvName {
    const char* name;
    const char* prefix;
};

// A face of a block's DETAILS, for one value of its Type: its rows in their order, a row of no ID after the last
struct DetailFace {
    std::array<Controls::KnobRow, kMaxDetails> rows;
};

// A kind: its name, whether its instances are numbered, the instance's ID of a parameter's name (as the first
// instance's name, which the instance makes its own), its envelopes, its LFO's prefix ("lfo" + "Rate") and the number
// of the first instance's LFO, the Matrix destinations' names that land on it (a numbered kind's take the instance's
// number and "-" after them), the depths by which its LFO reaches it with no route, and its DETAILS: the Type its faces
// follow (none: one face) and a face for each of Type's values (UiDetailTables.cpp)
struct BlockKind {
    const char* name;
    bool numbered;
    juce::String (*param)(int index, const juce::String& name);
    std::array<EnvName, kMaxEnvs> envs;
    const char* lfo;
    int lfoFirst;
    std::array<const char*, kMaxDests> dests;
    std::array<Controls::KnobRow, kMaxDepths> depths;
    const char* type;
    const DetailFace* details;
    int faces;
};

const BlockKind& kindOf(const Block& b);
juce::String blockName(const Block& b);                        // "OSC 1", "MOD"
juce::String blockParam(const Block& b, const juce::String& name);   // the instance's own parameter of that name
juce::String typeIdOf(const Block& b);                         // its Type's ID, which DETAILS follows; empty: none
int envelopesOf(const Block& b);                               // how many envelopes it has
juce::String lfoNameOf(const Block& b);                        // "LFO 3"; empty if it has no LFO
bool reaches(const Block& b, const juce::String& dest);        // whether a Matrix destination's name lands on it

}
