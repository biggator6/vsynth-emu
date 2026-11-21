// LITE's heads' renders on the OSC voice (v326): rom.4's 0x893c6e80 (mono) and 0x893c6e8c (stereo) are rom.2's
// 0x8950a7f0 and 0x8950a7fc, which XtLitePlayer ports, but for two things: their words lie four bytes up in rom.4's
// object from +0x94, and their backward reads take each sample as it is, where rom.2's negate it (XtLitePlayer's
// upright switch).  The beat's block first, rom.4's 0x893c8990 (Vc2OscBeat.cpp), which leaves the walk step's word;
// then XtLitePlayer on the object's words (XtLitePlayer::words), the wave's halfwords as rom.4 reads them through the
// voice's parts, and its words back.  Every address is rom.4's; every offset the voice's.  HARDWARE_FIRMWARE.md, "The
// VC-2's LITE heads".  Audio thread.
#include "Vc2OscPlayers.h"
#include "XtLitePlayer.h"

namespace {
// [FW] a word's place in rom.2's object in the voice: rom.4's object at +0x15c, four bytes up from +0x94 (rom.4's
// word inserted there)
constexpr uint32_t kInsertAt = 0x94, kInsert = 4;
uint32_t inRom4(uint32_t at) { return Vc2OscVoice::kObject + at + (at >= kInsertAt ? kInsert : 0); }
// [FW] the samples' address (the object's +0xf4, rom.2's +0xf0); a frame's bytes, mono and stereo; the right channel's
// halfword in a stereo frame
constexpr uint32_t kSamplesAt = 0x250, kMonoFrame = 2, kStereoFrame = 4, kRightAt = 2;

XtLiteSamples channel(const Vc2OscSamples& wave, uint32_t first, uint32_t frame) {
    return { .memory = wave.samples, .base = wave.address, .count = wave.count, .first = first, .frame = frame };
}
} // namespace

void vc2OscHeads(Vc2OscVoice& v, float* out, Vc2OscParts& parts, bool stereo) {
    const int32_t word = v.beatBlock(parts);
    XtLitePlayer player;
    player.setUpright(true);
    vc2OscWordsFrom(v, &player, XtLitePlayer::words(), inRom4);
    const Vc2OscSamples wave = parts.waveSamples();
    const uint32_t first = v.u32(kSamplesAt);
    const uint32_t frame = stereo ? kStereoFrame : kMonoFrame;
    player.setSamples(channel(wave, first, frame), stereo ? channel(wave, first + kRightAt, frame) : XtLiteSamples {});
    player.walkBlock(out, word);
    vc2OscWordsTo(v, &player, XtLitePlayer::words(), inRom4);
}
