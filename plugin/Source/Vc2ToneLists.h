#pragma once
// The VC-2 part's two lists of tones (v273): those sounding and those new, each headed by an object of 0x1c bytes at
// 0x899996d0 and 0x899996ec that rom.4's static initializer builds, the tones linked through their +0 and +4.  Two words
// beside mark an end of each: 0x89999678 the last tone listed new, 0x8999967c the last one joined to the sounding list.
// A note-on lists its tone new (0x893ada10); the start (0x893b1cd0) joins the new list onto the sounding one and starts
// every tone there still waiting; the destructor takes a tone off its list (0x893ada30, 0x893ad9a0).  The heads' +4 and
// +0x14 are never written, so the note-on's walk back from a mark ends at a head.  Bit for bit: xt_vc2tone against
// rom.4's own routines.  HARDWARE_FIRMWARE.md, "The VC-2's tone: its remaining methods".  The notes' task.
#include <array>
#include <cstdint>

class Vc2TonePool;

class Vc2ToneLists {
public:
    static constexpr uint32_t kNewestAt = 0x89999678;      // [FW] the marks
    static constexpr uint32_t kSoundingAt = 0x899996d0;    // [FW] the heads
    static constexpr uint32_t kNewAt = 0x899996ec;

    std::array<uint32_t, 2> marks() const { return { newest, joined }; }   // what the note-on walks back from
    void enlist(uint32_t tone, Vc2TonePool& pool);              // 0x893ada10
    uint32_t unlistNew(uint32_t tone, Vc2TonePool& pool);       // 0x893ada30: the tone, 0 when not on the list
    uint32_t unlistSounding(uint32_t tone, Vc2TonePool& pool);  // 0x893ad9a0: the same
    void join(Vc2TonePool& pool);                               // 0x893ada90
    void start(Vc2TonePool& pool);                              // 0x893b1cd0: the join, then each tone still built

    uint32_t newest = 0;     // 0x89999678
    uint32_t joined = 0;     // 0x8999967c
    uint32_t sounding = 0;   // 0x899996d0: the sounding list's first
    uint32_t fresh = 0;      // 0x899996ec: the new list's first

private:
    uint32_t& nextOf(uint32_t at, Vc2TonePool& pool);
    uint32_t tailOf(uint32_t head, Vc2TonePool& pool);
    uint32_t unlist(uint32_t head, uint32_t& mark, uint32_t tone, Vc2TonePool& pool);
};
