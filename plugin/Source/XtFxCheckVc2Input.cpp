// xt_fx <rom.4> vc2's outside input (v400; HARDWARE_FIRMWARE.md, "rom.4's outside input"):
// - The routing by hand: External In Type through XtFxControl against rom.4's request and event run by hand, the
//   routing's bytes and every word, at the start and at each value; the Type moves no chip word, the firmware's own way.
// - The routing's law on the chip: every External In Type and USB External In Type, noise into the analog pair and into
//   the USB pair, each slot the chip hands back against the routing's law as this check knows it.
// - The input through the unit: the plugin's one input reaches rom.4's analog pair alone, under every USB External In Type.
// XT_FX_MAIN only.
#ifdef XT_FX_MAIN
#include "XtFxCheckUnit.h"
#include "XtFxCheckVc2.h"

namespace xtfxcheck {
namespace {
// [FW] the routing's bytes, written apart from XtFxInput.cpp: what it wants (from 0x89980fa9) and what it last applied
// (from 0x89980fb0), seven each, the Type the seventh
constexpr uint32_t kWanted = 0x89980fa9;
constexpr uint32_t kApplied = 0x89980fb0;
constexpr uint32_t kRoutingBytes = 7;
constexpr uint32_t kTypeByte = 6;

// [FW] rom.4's routing as runs of its driver found it, written apart from the port: what each slot the chip hands back
// takes from a pair, in halves of each side.  The left slots take the left half-pair, the right slot the right; 0x92 and
// 0x96 take nothing from either pair.  The analog pair goes the same way whatever either Type says.
struct InputWay { int leftL, leftR, rightL, rightR; };
constexpr InputWay kAnalog { 2, 0, 0, 2 };
constexpr InputWay kUsbWays[] = {   // by USB External In Type: OFF, STEREO, MONO-L+R, MONO-L, MONO-R
    { 0, 0, 0, 0 }, { 2, 0, 0, 2 }, { 1, 1, 1, 1 }, { 1, 1, 0, 0 }, { 0, 0, 1, 1 } };
constexpr unsigned kLeftSlots[] = { 0x90, 0x94, 0x98 };
constexpr unsigned kRightSlot = 0x9a;
constexpr unsigned kDeadSlots[] = { 0x92, 0x96 };
constexpr unsigned kAnalogPair[] = { 0x10, 0x12 };
constexpr unsigned kUsbPair[] = { 0x18, 0x1a };
constexpr unsigned kOtherPair[] = { 0x14, 0x16 };   // the third pair program A reads, which reaches nothing here
constexpr int kFrames = 1024;                       // frames measured
constexpr int kLatest = 4;                          // the latest delay tried
constexpr int kFades = 8820;                        // 200 ms in silence after a change, for the routing's fades

int bytesApart(const XtFxControl& c, HandDriver& hand) {
    int n = 0;
    for (uint32_t k = 0; k < kRoutingBytes; ++k) {
        n += c.driverByte(kWanted + k) != hand.c.rd8(kWanted + k);
        n += c.driverByte(kApplied + k) != hand.c.rd8(kApplied + k);
    }
    return n;
}

// Every input slot silent, then `pair` given a frame of noise (null: none)
void inputs(XtFxChip& chip, const unsigned* pair, const int32_t* lr) {
    for (const unsigned* p : { kAnalogPair, kUsbPair, kOtherPair })
        for (unsigned k = 0; k < 2; ++k) chip.setGram(p[k], 0);
    if (!pair) return;
    for (unsigned k = 0; k < 2; ++k) chip.setGram(pair[k], lr[k]);
}

// The queue played until the chip has it all, then the fades the routing's waits stand for, in silence
void settleQuiet(XtFxControl& c, XtFxChip& chip) {
    for (long k = 0; k < (1L << 22) && !c.drained(); ++k) {
        c.pump();
        c.service(chip);
        inputs(chip, nullptr, nullptr);
        chip.runFrame();
    }
    for (int k = 0; k < kFades; ++k) {
        c.service(chip);
        inputs(chip, nullptr, nullptr);
        chip.runFrame();
    }
}

// A slot's frames against a law over the noise, at the delay that fits best: the frames off it, 0 when one fits them all.
// Within a word's last bit: the chip halves each side on its own.
int offLaw(const std::vector<int32_t>& got, const std::vector<int32_t>& in, int fromL, int fromR) {
    int fewest = kFrames;
    for (int late = 0; late <= kLatest; ++late) {
        int off = 0;
        for (int i = kLatest; i < kFrames; ++i) {
            const size_t at = 2 * (size_t)(i - late);
            const double law = (fromL * (double)in[at] + fromR * (double)in[at + 1]) / 2;
            off += std::fabs(got[(size_t)i] - law) > 1.0;
        }
        fewest = std::min(fewest, off);
    }
    return fewest;
}

// Noise into one pair: each handed-back slot against its side of the way; the dead slots must stay silent
int offWay(XtFxControl& c, XtFxChip& chip, const unsigned* pair, const InputWay& way, Rng& r) {
    std::vector<int32_t> in(2 * kFrames);
    std::vector<std::vector<int32_t>> got(256, std::vector<int32_t>(kFrames));
    for (int32_t& x : in) x = (int32_t)r.next() >> 10;   // a quarter of the chip's full scale
    for (int i = 0; i < kFrames; ++i) {
        c.service(chip);
        inputs(chip, pair, &in[2 * (size_t)i]);
        chip.runFrame();
        for (unsigned s = 0x90; s < 0xa0; s += 2) got[s][(size_t)i] = chip.gram(s + 1);   // written at s, read one on
    }
    int off = offLaw(got[kRightSlot], in, way.rightL, way.rightR);
    for (const unsigned s : kLeftSlots) off += offLaw(got[s], in, way.leftL, way.leftR);
    for (const unsigned s : kDeadSlots) off += (int)std::ranges::count_if(got[s], [](int32_t v) { return v != 0; });
    return off;
}
} // namespace

void handInput(const char* rom) {
    auto hand = std::make_unique<HandDriver>(rom, handRom4());
    XtFxControl c(rom, nullptr, vc2Fx());
    std::set<int> heard;
    c.observeReads([](void* to, int id) { static_cast<std::set<int>*>(to)->insert(id); }, &heard);
    int tried = 1;
    int different = bytesApart(c, *hand) + wordsApart(c, hand->chip) ? 1 : 0;
    std::string first = different ? "the start" : "";
    const int def = c.get(kExtInType);
    int last = def;
    for (int v = c.lowest(kExtInType); v <= c.highest(kExtInType) + 1; v++) {
        const int value = v > c.highest(kExtInType) ? def : v;
        const auto before = std::make_unique<XtFxChip>(hand->chip);
        heard.clear();
        editOne(c, kExtInType, value);
        hand->store[(size_t)kExtInType] = (int16_t)value;
        hand->route();
        tried++;
        // the firmware's own way: the Type lands among what the routing applied and moves no chip word
        const bool read = value == last || heard.count(kExtInType);
        last = value;
        if (c.ok() && read && bytesApart(c, *hand) == 0 && wordsApart(c, hand->chip) == 0 &&
            wordsApart(*before, hand->chip) == 0 && hand->c.rd8(kApplied + kTypeByte) == value)
            continue;
        if (first.empty()) first = "External In Type " + std::to_string(value) + (c.ok() ? "" : ": " + c.error());
        different++;
    }
    if (different && xtSwitch("XT_FX_FIRST")) std::printf("  the input's routing: %s\n", first.c_str());
    std::printf("vc2 control's input routing against the driver by hand: %d samples, %d different\n", tried, different);
}

void vc2RoutingLaw(const char* rom, uint32_t seed) {
    XtFxControl c(rom, nullptr, vc2Fx());
    const auto chip = std::make_unique<XtFxChip>();
    Rng r(seed + 400);
    int tried = 0, different = 0;
    for (int type = c.lowest(kExtInType); type <= c.highest(kExtInType); ++type)
        for (int usb = c.lowest(kUsbExtInType); usb <= c.highest(kUsbExtInType); ++usb) {
            c.set(kExtInType, type);
            c.set(kUsbExtInType, usb);
            c.update();
            settleQuiet(c, *chip);
            const int analog = offWay(c, *chip, kAnalogPair, kAnalog, r);
            const int usbOff = offWay(c, *chip, kUsbPair, kUsbWays[usb], r);
            tried += 2;
            different += (analog ? 1 : 0) + (usbOff ? 1 : 0) + (c.ok() ? 0 : 1);
            if ((analog || usbOff) && xtSwitch("XT_FX_FIRST"))
                std::printf("  the routing: External In Type %d, USB %d: analog %d, USB %d frames off\n", type, usb,
                            analog, usbOff);
        }
    std::printf("vc2 control's input routing on the chip: %d samples, %d different\n", tried, different);
}

void unitInputVc2(const char* rom, uint32_t seed) {
    int tried = 0, different = 0;
    Rng r(seed + 450);
    for (int usb = 0; usb < (int)std::size(kUsbWays); ++usb) {
        const auto u = settledUnit(rom, vc2Fx(), { { kUsbExtInType, usb } });
        std::vector<int32_t> chipIn(2 * kFrames), in(2 * kFrames);
        for (size_t i = 0; i < chipIn.size(); ++i) {
            chipIn[i] = (int32_t)r.next() >> 10;   // a quarter of the chip's full scale
            in[i] = chipIn[i] * 256;               // the converter's int32, the chip's word on top
        }
        const std::vector<int32_t> back = handedBack(*u, in);
        std::vector<int32_t> left(kFrames), right(kFrames);
        for (size_t i = 0; i < (size_t)kFrames; ++i) {
            left[i] = back[2 * i] / 256;
            right[i] = back[2 * i + 1] / 256;
        }
        // words 4 and 5, rom.4's left and right slots: the analog pair's way alone, the USB pair silent
        const int off = offLaw(left, chipIn, kAnalog.leftL, kAnalog.leftR) +
                        offLaw(right, chipIn, kAnalog.rightL, kAnalog.rightR);
        tried++;
        different += off || !u->ok() ? 1 : 0;
        if (off && xtSwitch("XT_FX_FIRST")) std::printf("  unit: USB External In Type %d: %d frames off\n", usb, off);
    }
    std::printf("vc2 control's input through the unit: %d samples, %d different\n", tried, different);
}
} // namespace xtfxcheck
#endif
