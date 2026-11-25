// VariPhrase's own routines on the OSC voice (v330): kind 2's update 0x893c88b0, its players' start 0x893c8670 (the
// routine after the update in its note-on, 0x893af420, Vc2OscPlayers.cpp) and its settings 0x893b0280, kind 2's row's
// ways (Vc2OscVoiceKinds.cpp).  The players' set-ups the start runs went out by their addresses on the object until
// each was the voice's own (v333…v335, the period-jump player's the last); its seeks go by the voice's
// (Vc2OscVoice::seekBy), from v335 all its own.  Every address is rom.4's; every offset the voice's.
// HARDWARE_FIRMWARE.md, "The VC-2's VariPhrase update, start and settings".  Audio thread.
#include "Vc2OscPlayers.h"
#include "Vc2OscTables.h"
#include "Vc2OscTargets.h"
#include "XtFloat.h"
#include <array>

namespace {
// [FW] the words the update steps toward their targets, in rom.4's order, each by the pool word 0.7 through FMAC (the
// product, then the sum): the pitch (0x893afd10), the formant (0x893afeb0), the level toward the block's +0x18 (the
// voice's +0x6c, with no target routine), the walk (0x893afdf0), the gain (0x893aff30)
struct Stepped {
    uint32_t at;
    float (Vc2OscTargets::*target)() const;   // none: toward the voice's word `from`
    uint32_t from;
};
constexpr std::array<Stepped, 5> kStepped { { { 0x1cc, &Vc2OscTargets::tunedRatio, 0 },
                                             { 0x1d0, &Vc2OscTargets::formant, 0 },
                                             { 0x1d4, nullptr, 0x6c },
                                             { 0x1d8, &Vc2OscTargets::walk, 0 },
                                             { 0x1dc, &Vc2OscTargets::gain, 0 } } };

// [FW] the start's targets: the gain (0x893aff30) and the level (0x893aff60)
constexpr std::array<Vc2OscTargets::Word, 2> kStartTargets { { { 0x1dc, &Vc2OscTargets::gain },
                                                              { 0x1e0, &Vc2OscTargets::level } } };
// [FW] the players by the wave's type 1…8 (the switches at 0x893c86b0, 0x893c8764 and 0x893c8800): the set-up the
// start runs on the object, the voice's own (v333 the band players', Vc2OscBandSetUp.cpp, BACKING's 0x893cd1d0 and
// ENSEMBLE's 0x893cc8f0; v334 SOLO's, Vc2OscSoloSetUp.cpp, 0x893cc2e0 and 0x893cc310; v335 the period-jump player's,
// Vc2OscJumpRecords.cpp, 0x893ccff0), and the seek to the block's seek point; none for types 2, 4 and 7
using OwnSetUp = void (*)(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& shared);
void setUpBacking(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s) { vc2OscBandSetUp(v, p, s, false); }
void setUpEnsemble(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s) { vc2OscBandSetUp(v, p, s, true); }
void setUpSolo(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s) { vc2OscSoloSetUp(v, p, s, false); }
void setUpSoloSix(Vc2OscVoice& v, Vc2OscParts& p, const Vc2ToneShared& s) { vc2OscSoloSetUp(v, p, s, true); }
struct VariPlayer {
    OwnSetUp own;
    uint32_t seek;
};
constexpr std::array<VariPlayer, 8> kPlayers { { { setUpSolo, 0x893cc630 },
                                                { nullptr, 0 },
                                                { setUpBacking, 0x893cd440 },
                                                { nullptr, 0 },
                                                { setUpEnsemble, 0x893cd440 },
                                                { setUpSoloSix, 0x893cc630 },
                                                { nullptr, 0 },
                                                { vc2OscJumpSetUp, 0x893cd090 } } };
// [FW] the block's flags (+0x138; 0x80 the range in samples), the range's start and the seek point (+0x13c, +0x144:
// words in samples, half words in events)
constexpr uint32_t kFlags = 0x138, kFrom = 0x13c, kSeekTo = 0x144;
constexpr uint8_t kInSamples = 0x80;
constexpr int kSampleShift = 4;   // [FW] in samples, the seek's word the point × 16

// [FW] the settings' bytes, from their +0xb0: the new block's flags (+0x34…+0x36, the block's +0xe4…, which the voice
// keeps at +0x138); in the second, 0x80 asks a seek.  The object's byte 0: 0x02 counts, 0x04 holds, 0x20 Vari off
// (kept through the settings).  The count's and the matched count's bytes; the gain's target
constexpr uint32_t kFlagsFrom = 0x34, kFlagBytes = 3, kCount = 0x19c, kMatched = 0x19d, kGainTarget = 0x1dc;
constexpr uint8_t kAsksSeek = 0x80, kCounts = 0x02, kHolds = 0x04, kVariOff = 0x20;
} // namespace

// [FW] 0x893c88b0: each of the five words 0.7 of the way to its target
void Vc2OscVoice::updateVari(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    const Vc2OscTargets targets(*this, parts, shared);
    const float step = vc2OscTables().variStep;
    for (const auto& [at, target, from] : kStepped) {
        const float was = f32(at);
        const float to = target != nullptr ? (targets.*target)() : f32(from);
        setF32(at, xtf::add(xtf::mul(xtf::sub(to, was), step), was));
    }
}

// [FW] 0x893c8670: the gain's and the level's targets; by the wave's type, its player's set-up on the object, then,
// when the block's seek point isn't its range's start, its seek there (in samples the point × 16, in events its
// number); a type of none, or 0 or past 8, nothing more
void Vc2OscVoice::startVari(Vc2OscParts& parts, const Vc2ToneShared& shared) {
    Vc2OscTargets(*this, parts, shared).writeInto(*this, kStartTargets);
    const uint32_t type = u32(kWave) - 1;   // unsigned: type 0 and past 8 run neither switch's way
    if (type >= kPlayers.size()) return;
    const VariPlayer& player = kPlayers[type];
    if (player.own != nullptr) player.own(*this, parts, shared);
    const bool inSamples = (u8(kFlags) & kInSamples) != 0;
    const bool moved = inSamples ? u32(kSeekTo) != u32(kFrom) : u16(kSeekTo) != u16(kFrom);
    if (!moved || player.seek == 0) return;
    seekBy(player.seek, inSamples ? (int32_t)(u32(kSeekTo) << kSampleShift) : (int32_t)u16(kSeekTo), parts);
}

// [FW] 0x893b0280 (given the settings' +0xb0): with their 0x80 asking, the object's 0x04 held and the new flags' 0x04
// clear, a seek pending and the gain's target 0; the new flags into the voice's three; with the object's 0x02 clear and
// the new flags', the count's byte 0; with its 0x04 clear and theirs, the matched count's byte 1; then the object's
// bytes 0…2 the flags, byte 0's 0x20 kept.  Under the OS's interrupt mask from the flags on, which the port doesn't need
void Vc2OscVoice::settingsVari(const uint32_t* from) {
    const auto* given = reinterpret_cast<const uint8_t*>(from) + kFlagsFrom;
    const uint8_t was = u8(kObject);
    if ((given[1] & kAsksSeek) != 0 && (was & kHolds) != 0 && (given[0] & kHolds) == 0) {
        tail.seekPending = 1;
        setF32(kGainTarget, 0.0f);
    }
    for (uint32_t k = 0; k < kFlagBytes; ++k) setU8(kFlags + k, given[k]);
    if ((was & kCounts) == 0 && (u8(kFlags) & kCounts) != 0) setU8(kCount, 0);
    if ((was & kHolds) == 0 && (u8(kFlags) & kHolds) != 0) setU8(kMatched, 1);
    setU8(kObject, (uint8_t)((u8(kFlags) & ~kVariOff) | (was & kVariOff)));
    for (uint32_t k = 1; k < kFlagBytes; ++k) setU8(kObject + k, u8(kFlags + k));
}
