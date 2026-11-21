// xt_fx <rom.4> vc2's control changes (v402; HARDWARE_FIRMWARE.md, "rom.4's MFX sends by CC 91 and 93"):
// - By hand: XtFxControl::controller against rom.4's own receiver (0x89360b90) given a message's three bytes on a channel
//   drawn, then its event's routine, from the check's own addresses; MFX sends edited and patches loaded between; the
//   chip's words and the driver's object after each.  Some control change must move a word, or this shows nothing.
// - The sends' law: the words hang on send × CC / 127 alone, and the start's CC is 127, so a send with no CC is the same
//   send at 127.
// XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckVc2.h"

namespace xtfxcheck {
namespace {
// [FW] written apart from XtFxImage.cpp: rom.4's receiver, the object it's a method of (the driver's +0x54), its event's
// routine and the kernel's post it jumps to; the MFX's two sends and the controllers that scale them
constexpr uint32_t kReceiver = 0x89360b90;
constexpr uint32_t kReceiverObject = 0x89980f78;
constexpr uint32_t kSendsEvent = 0x89360930;
constexpr uint32_t kKernelPost = 0x89301830;
constexpr uint32_t kMessageAt = 0x8C0F0100;   // the message's three bytes, in the rig's RAM
constexpr int kChorusSend = 0x1b9;
constexpr int kReverbSend = 0x1ba;
constexpr int kReverbCc = 91;
constexpr int kChorusCc = 93;
constexpr int kMfxSwitch = 0x13d;
constexpr int kLawPairs = 96;

// The chip's words and the driver's object, from its address to its message's flags, apart between the two
int apart(const XtFxControl& c, HandDriver& hand) {
    int n = wordsApart(c, hand.chip);
    for (uint32_t a = hand.im.self; a < hand.im.message; ++a) n += c.driverByte(a) != hand.c.rd8(a);
    return n;
}

// A control change on the hand's side: the message's bytes to the receiver, then the event it posts
void handController(HandDriver& hand, int channel, int number, int value) {
    hand.c.wr8(kMessageAt, (uint8_t)(0xb0 | channel));
    hand.c.wr8(kMessageAt + 1, (uint8_t)number);
    hand.c.wr8(kMessageAt + 2, (uint8_t)value);
    hand.call(kReceiver, { kReceiverObject, kMessageAt, 0 });
    hand.call(kSendsEvent, { hand.im.self });
}

// One parameter on both sides, live or as a patch
void editBoth(XtFxControl& c, HandDriver& hand, int id, int value, bool patch) {
    const int v = std::clamp(value, c.lowest(id), c.highest(id));
    hand.store[(size_t)id] = (int16_t)v;
    editOne(c, id, v, patch);
    hand.message(0, patch ? kPatch : kMfx);
}

// The chip's words after an MFX send and, when cc >= 0, a control change scaling it, on a fresh start: through the
// queue, so that what the driver sent and never flushed doesn't count
std::vector<uint32_t> wordsAfter(const char* rom, int send, int sendId, int number, int cc) {
    constexpr long kMostFrames = 44100;
    XtFxControl c(rom, nullptr, vc2Fx());
    editOne(c, kMfxSwitch, 1);
    editOne(c, sendId, send);
    if (cc >= 0) c.controller(number, cc);
    const auto chip = std::make_unique<XtFxChip>();
    playQueue(c, *chip, kMostFrames);
    std::vector<uint32_t> words(2048);
    for (unsigned i = 0; i < 2048; ++i) words[i] = chip->word(i) & 0x0fffffff;
    return words;
}
} // namespace

void handControllers(const char* rom, int events, uint32_t seed) {
    auto hand = std::make_unique<HandDriver>(rom, handRom4());
    hand->c.hook(kKernelPost, [](Sh4& m) { m.ret(0); });
    XtFxControl c(rom, nullptr, vc2Fx());
    Rng r(seed + 600);
    int tried = 1;
    int different = apart(c, *hand) ? 1 : 0;
    int moves = 0;
    std::string first = different ? "the start" : "";
    editBoth(c, *hand, kMfxSwitch, 1, true);
    for (int i = 0; i < events; i++) {
        const auto before = std::make_unique<XtFxChip>(hand->chip);
        const int kind = r.below(8);
        if (kind == 0) {
            const int id = r.below(2) ? kChorusSend : kReverbSend;
            editBoth(c, *hand, id, r.below(128), false);
        } else if (kind == 1) {
            editBoth(c, *hand, vc2Ids().mfxType, r.below(vc2Ids().image.mfxTypes), true);
        } else {
            const int number = r.below(4) ? (r.below(2) ? kReverbCc : kChorusCc) : r.below(128);
            const int value = r.below(128);
            c.controller(number, value);
            handController(*hand, r.below(16), number, value);
            moves += wordsApart(*before, hand->chip) != 0;
        }
        tried++;
        if (c.ok() && apart(c, *hand) == 0) continue;
        if (first.empty()) first = "event " + std::to_string(i) + (c.ok() ? "" : ": " + c.error());
        different++;
    }
    tried++;
    different += moves ? 0 : 1;
    if (different && xtSwitch("XT_FX_FIRST"))
        std::printf("  control changes: %s; %d moved a word\n", first.empty() ? "none moved" : first.c_str(), moves);
    std::printf("vc2 control's control changes against the driver by hand: %d samples, %d different\n", tried,
                different);
}

void sendsLaw(const char* rom, uint32_t seed) {
    Rng r(seed + 650);
    int tried = 0, different = 0;
    const std::pair<int, int> sends[] = { { kChorusSend, kChorusCc }, { kReverbSend, kReverbCc } };
    for (const auto& [sendId, number] : sends) {
        std::map<int, std::vector<uint32_t>> byLevel;   // send × CC / 127, the firmware's own division
        for (int k = 0; k < kLawPairs; ++k) {
            const int send = r.below(128);
            const int cc = k % 4 == 0 ? -1 : r.below(128);   // -1: no control change, the start's 127
            const int level = send * (cc < 0 ? 127 : cc) / 127;
            const std::vector<uint32_t> words = wordsAfter(rom, send, sendId, number, cc);
            const auto [at, fresh] = byLevel.try_emplace(level, words);
            tried++;
            different += fresh || at->second == words ? 0 : 1;
        }
        tried++;
        different += byLevel.size() > 1 && byLevel.begin()->second != byLevel.rbegin()->second ? 0 : 1;
    }
    if (different && xtSwitch("XT_FX_FIRST")) std::printf("  the sends' law: %d pairs off it\n", different);
    std::printf("vc2 control's sends by CC 91 and 93: %d samples, %d different\n", tried, different);
}
} // namespace xtfxcheck
#endif
