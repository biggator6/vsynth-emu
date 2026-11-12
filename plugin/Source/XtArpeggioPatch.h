#pragma once
// A patch's arpeggio as XtArpeggio takes it (v189; the reads were xt_arp's in v188): the settings 0x8930c190 reads (IDs
// 0x42e…0x437) and the pattern 0x8930ca30 lays out from IDs 0x449…0x48c, 16 lines of them, each read by its line as a zone
// is (xtPatchValue; xt_files holds those reads to the firmware's).  XtPatch's controls and xt_arp's presets read them;
// xt_tone's patchOf gives them back (xtArpPatchIds).  No JUCE.
#include "XtArpeggio.h"
#include "XtSynthControls.h"
#include <cstdint>
#include <unordered_map>
#include <vector>

inline constexpr int kXtArpSwitchId = 0x42e;
inline constexpr int kXtArpCtrlMost = 95;      // a CTRL line's controller [FW 0x8930ca30]

// A setting's ID and its field
struct XtArpSettingId {
    int id;
    int XtArpSettings::*field;
};
inline constexpr XtArpSettingId kXtArpSettingIds[] = {
    { 0x42f, &XtArpSettings::hold },
    { 0x430, &XtArpSettings::motif },
    { 0x431, &XtArpSettings::duration },
    { 0x432, &XtArpSettings::shuffleRate },
    { 0x433, &XtArpSettings::shuffleRes },
    { 0x434, &XtArpSettings::keyVelocity },
    { 0x435, &XtArpSettings::octave },
    { 0x436, &XtArpSettings::grid },
    { 0x437, &XtArpSettings::endPoint },
};

XtArpSettings xtArpSettingsOf(const std::vector<uint8_t>& store);
XtArpPattern xtArpPatternOf(const std::vector<uint8_t>& store);

// The other way round, for patchOf: `p` with the arpeggio's values by ID, a pattern's ID with its line in the top 16 bits
using XtPatchIds = std::unordered_map<int, int>;
XtPatchIds xtArpPatchIds(XtPatchIds p, const XtArpSettings& s, const XtArpPattern& pat);
// A pattern's IDs, read by line
bool xtArpByLine(int id);

// The controls with the patch's arpeggio, its settings and pattern: a patch's tone is xtPatchControls (XtPatch.h), which
// the plugin's parameters hold; its arpeggio goes in beside it (loadPatch, v190; xt_tone's round trip)
XtControls xtWithPatchArp(const std::vector<uint8_t>& store, XtControls c);
// The controls' 4-byte words before the arpeggio's, the tone's own: what xt_tone's one-word edits pick from
int xtToneWords(const XtControls& c);
// Whether a field in the word at byte `at` differs between a and b.  The word's padding never counts: its bytes are whatever
// was there, so a compare of them followed the stack, and xt_tone's cases with it, until v189
bool xtToneWordMoved(const XtControls& a, const XtControls& b, size_t at);
