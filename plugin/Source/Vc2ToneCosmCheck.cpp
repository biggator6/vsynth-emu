// xt_vc2tone's COSMs (v318): see Vc2ToneSides.h.  rom.4's own COSM objects in its tone against the host's, Vc2ToneHost:
// their frame's words, the pools' words, and their sound through the render's buffer c.  XT_VC2TONE_MAIN only.
#ifdef XT_VC2TONE_MAIN
#include "Vc2ToneSides.h"
#include "Vc2CosmGrowl.h"
#include "Vc2CosmNoteOnDraws.h"
#include "Vc2ToneMixTables.h"
#include <cstring>

namespace {
using In = Vc2ToneInput;
constexpr uint32_t kPoolsAt = 0x89999290, kPoolsInit = 0x893ab550;   // [FW] the pools and their static initializer
constexpr uint32_t kSlotsAt = 0x89999294, kSlotBytes = 0x14;         // [FW] each pool's 24 slots, in use first
constexpr uint32_t kPastAt = 0x89998e20;   // [FW] after buffer c: the part mixer's frames
constexpr uint32_t kBindingsAt = 0x150;    // [FW] the object's four bound parts' addresses
constexpr uint32_t kSlotAt = 0x144;        // [FW] the object's slot, −1 none
constexpr int32_t kFirstSideBand = 7, kSecondSideBand = 8;   // [FW] the Side Band Filters' codes
constexpr uint32_t kStereoWave = 1, kMonoWave = 0;   // [FW] an EXT IN's flag by the wave's type (kind 4's row, v319)
constexpr float kPastLevel = 2.0f;   // the words past buffer c, against the mix's full scale (xt_vc2cosm's are the frames')

// [FW] what of the object the sound keeps, rom.4's DSP's state (D+4…), which the port keeps in XtCosm's objects (v263)
// and xt_vc2cosm's seventh part holds to rom.4's: the lines' write position, D+0x18…+0xab, the two values and
// D+0xcc…+0xe7; and the three bytes after the set-up's byte, rom.4's stack, which nothing reads
struct Masked {
    uint32_t from, to;
};
constexpr Masked kMasked[] = { { 0x0d, 0x10 }, { 0x5c, 0x60 }, { 0x70, 0x104 }, { 0x114, 0x11c }, { 0x124, 0x140 } };

// [FW] 0x893b3870…: the note-on starts the COSMs for the structures 0…2 alone
constexpr uint32_t kLastStarting = 2;
bool startsCosms(ToneSides& s) { return s.shared.input.at(In::kStructure) <= kLastStarting; }

uint32_t slotAt(uint32_t pool, int32_t slot) {
    return kSlotsAt + (pool * Vc2CosmPoolWords::kSlots + (uint32_t)slot) * kSlotBytes;
}

// A block's code and its type's words as its converter writes them, the other words as they stand
void drawTypeOf(Vc2ToneInput& in, uint32_t at, int32_t code, std::mt19937& rng) {
    Vc2CosmBlockWords b;
    std::memcpy(&b, &in.at(at), sizeof b);
    b.code = code;
    vc2DrawTypeWords(b, rng);
    std::memcpy(&in.at(at), &b, sizeof b);
}
} // namespace

std::string toneCosmDifference(ToneSides& s, int i) {
    std::string what;
    for (int k = 0; k < 2 && what.empty(); ++k) {
        const uint32_t at = toneCosmAt(s.pool.addressOf(i), k);
        Vc2CosmObjectWords mine = s.parts[(size_t)i].cosms[(size_t)k];
        for (const Masked& m : kMasked) {
            s.fw.cpu.read(at + m.from, reinterpret_cast<char*>(&mine) + m.from, m.to - m.from);
        }
        const std::string name = "its COSM " + std::to_string(k + 1) + " (code " + std::to_string(mine.code) + ")";
        what = vc2FirstDifference(s.fw, at, &mine, sizeof mine, name.c_str());
    }
    return what;
}

std::string tonePoolsDifference(ToneSides& s) {
    const Vc2CosmPoolWords& w = s.shared.pools.w;
    return vc2FirstDifference(s.fw, kPoolsAt, &w, sizeof w, "the COSMs' pools");
}

// A COSM never bound bound to two envelopes and two LFOs of the tone's own, as its note-on binds them (an update holds
// their levels, and rom.4 would read near address 0); one bound keeps its note-on's, as rom.4's tone binds a COSM at its
// note-on alone and its sound may read through them; the words past buffer c
void toneDrawCosms(ToneSides& s, int i, std::mt19937& rng) {
    const uint32_t tone = s.pool.addressOf(i);
    for (int k = 0; k < 2; ++k) {
        std::array<uint32_t, 4>& b = s.parts[(size_t)i].cosms[(size_t)k].bindings;
        if (b != std::array<uint32_t, 4> {}) continue;
        b = { toneEnvelopeAt(tone, (int)(rng() % Vc2Tone::kEnvelopes)),
              toneEnvelopeAt(tone, (int)(rng() % Vc2Tone::kEnvelopes)), toneLfoAt(tone, (int)(rng() % Vc2Tone::kLfos)),
              toneLfoAt(tone, (int)(rng() % Vc2Tone::kLfos)) };
        s.fw.cpu.write(toneCosmAt(tone, k) + kBindingsAt, b.data(), sizeof b);
    }
    const float level = kPastLevel * vc2ToneMixTables().fullScale;   // about the frames', now and then past them
    for (float& p : s.shared.past) p = level * (2.0f * vc2Unit(rng) - 1.0f);
    s.fw.cpu.write(kPastAt, s.shared.past.data(), sizeof s.shared.past);
}

// A code a converter writes (not 10, 11 or 17, which none does) and its type's words as it writes them; the words it
// doesn't write, among them the envelopes' and LFOs' numbers the tone's note-on binds by, as the input holds them.
// COSM 1's a Side Band Filter one time in three, the one type whose stereo follower reads past buffer c
void toneDrawCosmBlocks(Vc2ToneInput& in, std::mt19937& rng) {
    const int32_t first = rng() % 3 == 0 ? (rng() % 2 == 0 ? kFirstSideBand : kSecondSideBand) : vc2DrawPortedCode(rng);
    drawTypeOf(in, In::kCosms[0], first, rng);
    drawTypeOf(in, In::kCosms[1], vc2DrawPortedCode(rng), rng);
}

// The setter's block for COSM `cosm`: the object's code three times in four, so its type's new block runs
// v330: a number past 1 reaches no COSM, its block COSM 1's, which is drawn as for COSM 1 itself: a setter that reached a
// COSM it shouldn't would then run that type's new block (methods-cosm-bound's catch had been one event's luck)
void toneDrawCosmNewBlock(ToneSides& s, int i, uint32_t cosm, std::mt19937& rng) {
    toneDrawCosmBlocks(s.shared.input, rng);
    const uint32_t to = cosm > 1 ? 0 : cosm;
    if (rng() % 4 == 0) return;
    drawTypeOf(s.shared.input, In::kCosms[to], s.parts[(size_t)i].cosms[to].code, rng);
}

// A note-on drawn that starts the COSMs again: each slot the tone's last one took in use no longer on both sides, as the
// part's destructor would have left it.  One that starts none (a structure past 2, which the part's notes never have):
// COSM 1's mode kept, as its install would otherwise switch a sounding note's
void toneCosmsBeforeNoteOn(ToneSides& s, int i, std::mt19937& rng) {
    if (!startsCosms(s)) {
        toneKeepCosmMode(s, i, rng);
        return;
    }
    for (size_t k = 0; k < 2; ++k) {
        int32_t& slot = s.cosm.slots[(size_t)i][k];
        if (slot < 0) continue;
        s.shared.pools.release((uint32_t)k, slot);
        s.fw.cpu.write(slotAt((uint32_t)k, slot), &s.shared.pools.w.pools[k][(size_t)slot].inUse, sizeof(uint32_t));
        slot = -1;
        ++s.n.cosmFreed;
    }
    // v319: the Side Band Filters (their delay) and the Growl (its lines) take OSC 1's ratio and root, which their
    // converters keep within 0.25…4 and 55…1760 Hz (xt_vc2cosm's draws); a real voice's +0x1cc is its kind's (kinds 5
    // and 6 leave it, their pitch in P), so OSC 1 a steady EXT IN's, stereo or mono
    const auto first = static_cast<int32_t>(s.shared.input.at(In::kCosms[0]));
    if (first != kFirstSideBand && first != kSecondSideBand && first != Vc2CosmGrowl::kCode) return;
    toneOscSteady(s, 0, rng() % 2 ? kStereoWave : kMonoWave, rng);
    ++s.n.oscSteadied;
}

void toneNoted(ToneSides& s, int i) {
    if (!startsCosms(s)) return;
    s.cosm.noted[(size_t)i] = true;
    for (size_t k = 0; k < 2; ++k) {
        const Vc2CosmObjectWords& o = s.parts[(size_t)i].cosms[k];
        s.cosm.slots[(size_t)i][k] = o.pool == k ? o.slot : -1;
        s.n.cosmSlots += o.slot >= 0;
    }
}

// A COSM that holds no slot by the check's count names none, on both sides: its destructor frees the slot its object
// names, which a tone the check built and never gave a note names from its memory's last use (0 at a case's start) and
// another tone may hold; the part gives every tone it builds a note, so its destructor frees what that note took
void toneHoldNoStaleSlots(ToneSides& s, int i) {
    for (size_t k = 0; k < 2; ++k) {
        int32_t& slot = s.parts[(size_t)i].cosms[k].slot;
        if (s.cosm.slots[(size_t)i][k] >= 0 || slot < 0) continue;
        slot = -1;
        s.fw.cpu.write(toneCosmAt(s.pool.addressOf(i), (int)k) + kSlotAt, &slot, sizeof slot);
        ++s.n.cosmStale;
    }
}

// OSC 1's flag, which COSM 1's install takes as mono (0) or stereo, as its note chose: rom.4 reaches its install pass from
// the note-on alone (nothing names 0x893b4060: its note-on holds the pass inline), so a sound's mode never changes during
// a note, and rom.4's mono routines keep a stereo state the port's mono sound doesn't.  From v319 the voices are real: a
// voice's kind and flag stay as its note-on left them (nothing else writes them), so the install pass alone keeps the
// mode by itself, and a note-on that starts no COSMs (a structure past 2, which the part never gives) is what could
// change it: OSC 1's block is made an EXT IN's (kind 4), whose note-on's flag is 1 for the wave's type 1 alone, at a
// steady pitch, which a sounding Side Band Filter takes at COSM 1's updates
void toneKeepCosmMode(ToneSides& s, int i, std::mt19937& rng) {
    if (!s.cosm.noted[(size_t)i]) return;
    toneOscSteady(s, 0, s.parts[(size_t)i].sounds[0].section().stereo ? kStereoWave : kMonoWave, rng);
    ++s.n.cosmModesKept;
}

bool toneCosmFollows(ToneSides& s, int i) {
    const int32_t code = s.parts[(size_t)i].cosms[0].code;
    return (code == kFirstSideBand || code == kSecondSideBand) && s.parts[(size_t)i].sounds[0].section().stereo;
}

void toneUnnoted(ToneSides& s, int i, bool freed) {
    s.cosm.noted[(size_t)i] = false;
    if (freed) s.cosm.slots[(size_t)i] = { -1, -1 };
}

std::string toneFreshCosms(ToneSides& s) {
    s.cosm.forget();
    return vc2BothOn(s.fw, kPoolsInit, {}, "the pools' set-up", [&] { s.shared.pools.init(); },
                     [&] { return tonePoolsDifference(s); });
}
#endif
