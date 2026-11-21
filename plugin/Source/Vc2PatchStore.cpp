// rom.4's patch store: see Vc2PatchStore.h (v450).  Every number is rom.4's; its check is xt_vc2patch's
// (Vc2PatchStoreCheck.cpp), against the pointing routines and the reads themselves.
#include "Vc2PatchStore.h"
#include "Vc2Params.h"
#include <algorithm>
#include <iterator>

namespace {

// [FW] the sections, each where its pointing routine aims a read object in the part's store (0x8950d8e0's entry +8; the
// routine beside each takes +4): the common 0x893207b0, MFX 0x893208d0, Chorus 0x89320950, Reverb 0x893209d0, the tone
// 0x89320a50 (the block << 7), the envelopes 0x89320ad0, the LFOs 0x89320b70, the COSMs 0x89320c10 (r7 the COSM), Beam
// Type to the arpeggio's End Point 0x89320d10, and the arpeggio pattern's lines 0x89320da0.  A section holds the IDs
// whose bits its block holds, by the descriptors; the conversion copies each block whole (0x89324b20).
constexpr XtPatchSectionDef kSections[] = {
    { 0x120, 0x1b5, 0x000, 0 },        // the common: name, tune, keys, the zones' splits, the Matrix, the vocoder, EQ
    { 0x1b6, 0x23b, 0x06c, 0 },        // MFX: its Type, levels and slots
    { 0x23c, 0x26f, 0x0b0, 0 },        // Chorus
    { 0x270, 0x2d2, 0x0cc, 0 },        // Reverb
    { 0x2d3, 0x38f, 0x0fe, 0x80 },     // the tone's 8 blocks: Structure, MOD, TVA, both OSCs with the VOCODER OSC's
    { 0x390, 0x438, 0x4fe, 0x70 },     // the 13 envelopes, a zone's (16)
    { 0x439, 0x461, 0xbfe, 0x14 },     // the 5 LFOs, a zone's (16)
    { 0x462, 0x4a5, 0xd3e, 0x22 },     // a COSM's Switch, Type and slots, a zone's (16); COSM 2 kCosmStride on
    { 0x4a6, 0x4b3, 0x117e, 0 },       // Beam Type, Pad Mode and Hold, the arpeggio
    { 0x4b4, 0x4f6, 0x1184, 0x22 },    // the arpeggio pattern's 16 lines: Control Switch and Number, Steps 1-32
};
constexpr int kCosmFirst = 0x462, kCosmLast = 0x4a5;
constexpr int kCosmStride = 0x220;   // [FW] COSM 2's blocks after COSM 1's (0x89320c10's 0xf5e against 0xd3e)

// [FW] the typed IDs' blocks, each its first typed ID and its slot base, the block's Type ID + 4, 3, 2, 1 (rom.4's getter
// 0x89324370 passes it to 0x8932b460; VC2_VOCAL_DESIGNER.md, "The second descriptor bank, read").  The Reverb's runs on
// past its 15 types' IDs to the COSM's: two types no patch can choose and 14 IDs in no table, read as its slots.
struct TypedBlock {
    int first, slotBase;
};
constexpr TypedBlock kTyped[] = { { 0x56a, 0x1bb }, { 0x7ce, 0x23f }, { 0x819, 0x272 }, { 0x926, 0x464 } };

} // namespace

const XtStoreLayout& vc2Store() {
    static constexpr XtStoreLayout kVc2 { kSections, kCosmFirst, kCosmLast, kCosmStride, vc2ParamDesc, vc2TypedDesc };
    return kVc2;
}

int vc2StoreSlotBase(int id) {
    if (id < kVc2FirstTyped || id >= kVc2Ids) return -1;
    const auto b = std::ranges::find_if(std::rbegin(kTyped), std::rend(kTyped),
                                        [id](const TypedBlock& t) { return id >= t.first; });
    return b->slotBase;
}

int vc2StoreTyped(const std::vector<uint8_t>& store, int id, int zone, int cosm) {
    const int base = vc2StoreSlotBase(id);
    return base < 0 ? 0 : xtStoreTyped(vc2Store(), store, base, id, zone, cosm);
}
