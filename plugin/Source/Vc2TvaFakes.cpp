// xt_vc2tva's two sides' fakes: see Vc2TvaCheck.h (v233).  rom.4's: the OS's call hooked; the port's: its parts
// reading what rom.4's routines read.  XT_VC2TVA_MAIN only.
#ifdef XT_VC2TVA_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2TvaCheck.h"

namespace {
// [FW] the OS's routine a told truncation's end calls (it jumps into the OS at 0x88008070, which Vc2Fw doesn't map)
constexpr uint32_t kSetEvent = 0x89301830;
} // namespace

Vc2TvaEnvelope TvaPortParts::envelope(uint32_t at) {
    Vc2TvaEnvelope e { (int32_t)fw_.cpu.rd32(at + kTvaState) };
    for (const TvaEnvelopeFloat& f : kTvaEnvelopeFloats) e.*f.field = f32(at + f.at);
    return e;
}

void tvaInstallRom(Vc2Fw& fw, std::vector<TvaCall>& log) {
    fw.cpu.hook(kSetEvent, [&log](Sh4& cpu) {
        log.push_back({ cpu.r[4], cpu.r[5] });
        cpu.ret(0);
    });
}

std::string tvaCallsDifference(const std::vector<TvaCall>& rom, const std::vector<TvaCall>& port) {
    return vc2CallsDifference(rom, port, [](const TvaCall& c) {
        char t[64];
        std::snprintf(t, sizeof t, "the OS's (%08x, %08x)", c.event, c.flags);
        return std::string(t);
    });
}
#endif
