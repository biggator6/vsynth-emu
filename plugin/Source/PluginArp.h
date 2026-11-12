#pragma once
// The arpeggio's pattern as the processor keeps it (v190): a patch's data, not knobs, so no host parameter; set on the
// message thread when a patch or a session comes in, read by the audio thread each block into XtSynth's controls.
#include "XtArpeggio.h"
#include <array>
#include <atomic>
#include <cstdint>

// The XT's own pattern before any patch: every line NOTE C4 and every cell REST, the defaults of IDs 0x44a, 0x44b and the
// steps [FW 0x897a167c: 0, 60, 0]; with it the arpeggio plays nothing until a patch brings one
XtArpPattern xtArpDefaultPattern();
// Whether no step of it starts a note or a controller: every cell REST (0) or TIE (1), so the arpeggio is silent
bool xtArpPatternSilent(const XtArpPattern& p);

// The pattern a word at a time, each word whole and no lock: a reader between two stores can see words of both, as it can
// a patch's parameters, which its load moves one by one, and the next block reads the pattern whole.
class ArpPatternWords {
public:
    ArpPatternWords();
    XtArpPattern load() const;
    void store(const XtArpPattern& p);

private:
    static_assert(sizeof(XtArpPattern) % sizeof(uint32_t) == 0, "the pattern goes in whole words");
    static constexpr size_t kWords = sizeof(XtArpPattern) / sizeof(uint32_t);
    std::array<std::atomic<uint32_t>, kWords> words_;
};
