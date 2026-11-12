// XtSynth's Multi Step Modulator (v193): part 0's XtMsm on the tempo clock's ticks, the tone updates it asks for, the
// allocator's head that starts it again, its settings, and the terms a conversion pass reads.  Where each converter adds
// them is XtSynthKnobs.cpp's and XtSynthEnvKnobs.cpp's.  HARDWARE_FIRMWARE.md, "The Multi Step Modulator in XtSynth".
#include "XtSynth.h"
#include <cstring>

namespace {
// A note of the part on the allocator's list: not let go (0x894dc270 hasn't run) and not truncated (+0x2c)
bool listed(const XtVoice& v) {
    return v.active && !v.releasing && !v.damping;
}

// 0x894d5550(0): one whose key is down, its state word's bit 29
bool keyHeld(const XtVoice* voices, int n) {
    for (int i = 0; i < n; ++i)
        if (listed(voices[i]) && (voices[i].keyFlags & 1)) return true;
    return false;
}

// 0x894d55a0(0): any
bool sounding(const XtVoice* voices, int n) {
    for (int i = 0; i < n; ++i)
        if (listed(voices[i])) return true;
    return false;
}

// The envelopes' Releases, whose terms a key-off reads (0x894def60): COSM 1's two, COSM 2's two, the TVA's
constexpr int kReleases[] = {
    xtmsm::Cosm1 + xtmsm::CosmEnv1 + xtmsm::Release, xtmsm::Cosm1 + xtmsm::CosmEnv2 + xtmsm::Release,
    xtmsm::Cosm2 + xtmsm::CosmEnv1 + xtmsm::Release, xtmsm::Cosm2 + xtmsm::CosmEnv2 + xtmsm::Release,
    xtmsm::Tva + xtmsm::TvaEnv + xtmsm::Release,
};
}  // namespace

// Its tone update (0x894d96b0 → 0x894d72f0 → 0x894d50d0): every note of the part, as a controller's update
bool XtSynth::msmHook() {
    msm.onUpdate = [](void* self) {
        XtSynth& s = *static_cast<XtSynth*>(self);
        for (Voice& v : s.voice)
            if (v.active) s.newBlocks(v);
    };
    msm.updateTo = this;
    return true;
}

// The section's settings, part 0's: a patch (msmPatch) stops it and reads them (0x894d9950); an edit reads them without
// stopping it (0x894d9b20), unless the Master Switch went off
void XtSynth::msmSync() {
    if (msmPatchDue.exchange(false, std::memory_order_relaxed)) {
        msm.load(ctl.msm, 0);
        msmGiven = ctl.msm;
        return;
    }
    if (ctl.msm == msmGiven) return;
    msm.edit(ctl.msm, 0);
    msmGiven = ctl.msm;
}

// A ms of the tempo clock's ticks (arpMs): its timer, which the OS's tempo timer fires (0x8930eb00); then, when it fired,
// the tone task's flag 0x20 that it posts (0x894d4240), which the task takes first, ahead of the arpeggio's notes
// (0x894d4048).  Only a firing posts it: a clock left counted by a stop (no note sounding) waits for the next one.
void XtSynth::msmMs(int32_t ticks) {
    const int32_t counted = msm.pending;
    if (ticks != 0) msm.tick(ticks);
    if (msm.pending != counted) msm.clocks(sounding(voice, kVoices));
}

// The allocator's head (0x894d4b08), before it takes the note: by Key Sync, started, or stopped and started
void XtSynth::msmNoteOn() {
    msm.noteOn(0, keyHeld(voice, kVoices));
}

// The terms a conversion pass reads, each as its converter asks: by the note's zone (0x894d96d0), PORTA-TIME of any zone
// (0x894d9750), each as a word.  A key-off's pass reads only the envelopes' Releases.  True if one moved.
bool XtSynth::msmTerms(Voice& v, bool release) const {
    if (release) {
        for (const int d : kReleases) v.msm[d] = (int16_t)msm.term(d, 0, v.zone);
        return false;
    }
    int16_t next[kXtMsmDests];
    for (int d = 0; d < kXtMsmDests; ++d) next[d] = (int16_t)msm.term(d, 0, v.zone);
    next[xtmsm::PortaTime] = (int16_t)msm.termAnyZone(xtmsm::PortaTime, 0);
    const bool moved = std::memcmp(next, v.msm, sizeof next) != 0;
    std::memcpy(v.msm, next, sizeof next);
    return moved;
}
