// rom.4's effect driver on an XtFxChip (vc2fxdriver.h).  Every address is rom.4's; rom.2's are XtFxControl.cpp's, paired
// by shape (codepairs.py), and through their callers the coefficient write, the sleep and the reads.
#include "vc2fxdriver.h"
#include "vc2patches.h"

namespace vc2 {

namespace {

// [FW] the driver's object and its message's flags (rom.2's 0x89ced1b4, 0x89ced200, by literal)
constexpr uint32_t kThis = 0x89980f24, kMessage = 0x89980f84;
constexpr uint32_t kStatic = 0x89368280, kStart = 0x8935f9a0, kStartTask = 0x8935fa80, kTakeMessage = 0x8935fc40;
constexpr uint32_t kQuick[] = {0x89362430, 0x89362450, 0x89365e50,   // [FW] the spins and the sleep
                               0x893951b0, 0x89395040, 0x89394fa0,   // the tone generator's lists, not set up here
                               0x89301610};   // the system reader's end: ID 0x79 to the kernel (rom.2's 0x89301680)
constexpr uint32_t kPutWord = 0x89362490, kGetWord = 0x89362610, kPutCoef = 0x893626d0, kPutOffset = 0x893627f0;
constexpr uint32_t kReads[] = {0x89324350, 0x89324370};   // [FW] (section or 0, id) → its value
constexpr uint8_t kSystem = 0x20, kPatch = 0x80;           // [FW] the message's second and fourth bytes
constexpr uint32_t kLowPage = 0x1000;                      // two MFX types read through null (XtFxControl)

int valueOf(Machine& m, const FxSettings& s, int id) {
    if (const auto it = s.find(id); it != s.end()) return it->second;
    if (id >= (int)kFirstTyped) return typedDescriptorOf(m, (uint32_t)id).def;
    const Descriptor d = descriptorOf(m, (uint32_t)id);
    return d.def - d.centre;
}

// [FW] the bytes the coefficient write sends (0x893626d0, rom.2's 0x894af610): (value, shift)
uint32_t coefficientWord(uint32_t value, uint32_t shift) {
    const uint32_t v = value & 0xffff, s = shift & 0xff;
    return (v >> 8 & 0xff) | ((v << 1 & 0xfc) | (s & 3)) << 8 | ((v >> 7 & 1) | (s << 2 & 12)) << 16;
}

// [FW] the bytes the memory offset write sends (0x893627f0, rom.2's 0x894af6a0): the offset, with what it reads back of
// words i and i + 4
uint32_t offsetWord(const XtFxChip& chip, uint32_t i, uint32_t v) {
    const uint32_t a = chip.word(i) & 0x0fffffff, b = chip.word(i + 4) & 0x0fffffff;
    return (a >> 20 & 0xf8) | (v & 0xff) << 8 | (v >> 8 & 0xff) << 16 | ((v >> 16 & 3) | (b >> 24 & 0xc0)) << 24;
}

} // namespace

bool fxMessage(Machine& m, uint32_t bytes) {
    for (uint32_t k = 0; k < 4; ++k) m.cpu.wr8(kMessage + k, (uint8_t)(bytes >> (24 - 8 * k)));
    return m.run(kTakeMessage, {kThis});
}

void hookFxDriver(Machine& m, XtFxChip& chip, const FxSettings& s, std::set<int>& heard) {
    m.cpu.mapRam(0, kLowPage);
    answerKernel(m);
    for (const uint32_t a : kQuick) m.cpu.hook(a, [](Sh4& c) { c.ret(0); });
    m.cpu.hook(kPutWord, [&chip](Sh4& c) {
        chip.hostWord(0x34, c.r[4] & 0xffff, c.r[5]);
        c.ret(0);
    });
    m.cpu.hook(kGetWord, [&chip](Sh4& c) { c.ret(chip.word(c.r[4] & 0xffff) & 0x0fffffff); });
    m.cpu.hook(kPutCoef, [&chip](Sh4& c) {
        chip.hostWord(0x35, c.r[4] & 0xffff, coefficientWord(c.r[5], c.r[6]));
        c.ret(0);
    });
    m.cpu.hook(kPutOffset, [&chip](Sh4& c) {
        chip.hostWord(0x36, c.r[4] & 0xffff, offsetWord(chip, c.r[4] & 0xffff, c.r[5]));
        c.ret(0);
    });
    for (const uint32_t a : kReads)
        m.cpu.hook(a, [&m, &s, &heard](Sh4& c) {
            heard.insert((int16_t)c.r[5]);
            c.ret((uint32_t)valueOf(m, s, (int16_t)c.r[5]));
        });
}

bool startFxDriver(Machine& m) {
    if (!m.run(kStatic, {}) || !m.run(kStart, {kThis})) return false;
    return m.run(kStartTask, {kThis}) && reloadFx(m);
}

bool reloadFx(Machine& m) {
    return fxMessage(m, (uint32_t)kSystem << 16 | kPatch);
}

} // namespace vc2
