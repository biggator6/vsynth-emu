// vc2run: rom.4's vocoder run in the SH-4 interpreter (research/VC2_VOCAL_DESIGNER.md, "Running rom.4").
//   vc2run <rom.4> boot            the vocoder object built by its own routine, each engine's vtable and slots
//   vc2run <rom.4> warp ...        HIRESO's slot 4, the formant map (vc2warp.cpp)
//   vc2run <rom.4> ratio           the Formant Shift ratio's library call against powf (vc2ratio.cpp)
//   vc2run <rom.4> hold [engine]   which controller reaches the hold (vc2hold.cpp)
//   vc2run <rom.4> growl [blocks]  the VOCODER OSC's growl branch against its model (vc2growl.cpp)
//   vc2run <rom.4> kind6           voice kind 6 on a wave of known period, its output's period (vc2kind6.cpp)
//   vc2run <rom.4> lfo <rom.2>     the LFO on both images' routines and the port, side by side (vc2twinlfo.cpp)
//   vc2run <rom.4> env <rom.2>     the envelope and its converters the same way (vc2twinenv.cpp)
//   vc2run <rom.4> cosm <rom.2>    COSM's converters, note-on, update and TVFs, both images (vc2twincosm.cpp)
//   vc2run <rom.4> osc <rom.2>     the OSC block's shared routines, the pitch ramp, the key state (vc2twinosc.cpp)
//   vc2run <rom.4> analog <rom.2>  the analog OSC's waveform routines on a voice rom.2 makes (vc2twinanalog.cpp)
//   vc2run <rom.4> band <rom.2>    BACKING's and ENSEMBLE's players on waves rom.2 encodes (vc2twinband.cpp)
//   vc2run <rom.4> solo <rom.2>    SOLO's player on records rom.2's encoder makes (vc2twinsolo.cpp)
//   vc2run <rom.4> lite <rom.2>    LITE's player on a state set as XtLitePlayer's note-on leaves it (vc2twinlite.cpp)
//   vc2run <rom.4> tva <rom.2>     the patch TVA's render and end test on a state set as its note-on leaves it
//                                  (vc2twintva.cpp)
//   vc2run <rom.4> patches <file.p02> …   the VC-2's patches read by rom.4's own routines (vc2patches.cpp)
//   vc2run <rom.4> track           the pitch tracker on tones of known pitch, noise and silence (vc2track.cpp)
//   vc2run <rom.4> vocosc          the VOCODER OSC's IDs from the store through the builder and converter (vc2vocosc.cpp)
//   vc2run <rom.4> autonote        kind 6's mode and Auto Note's notes and ratio on sung tones (vc2autonote.cpp)
//   vc2run <rom.4> mixer           the part's levels and block mixer on known input frames (vc2mixer.cpp)
//   vc2run <rom.4> typed <file.p02> …   MFX, Chorus, Reverb and COSMs read through the second bank (vc2typed.cpp)
//   vc2run <rom.4> fxprog <rom.2>  the reverb types' chip programs, against rom.2's (vc2fxprog.cpp)
//   vc2run <rom.4> fxroute <rom.2> where program A takes each input, both images on XtFxChip (vc2fxroute.cpp)
//   vc2run <rom.4> fxreverb        the Reverb Types on XtFxChip, measured as reverbs (vc2fxreverb.cpp)
//   vc2run <rom.4> splits <index>  a VC-2 wave's split at every velocity and a map's at every key (vc2split.cpp)
//   vc2run <rom.4> splitbytes <index>  what reads the bytes a split passes on (vc2splitbytes.cpp)
//   vc2run <rom.4> bank <index> <bank>  how the store comes to hold each VC-2 sample (vc2bank.cpp)
//   vc2run <rom.4> wavein <rom.2> <index> <bank>  what the wave reader stores, against xtWaveRead (vc2wavein.cpp)
//   vc2run <rom.4> cosmgrowl [notes] [plant]  COSM type 8, the Growl, against its model (vc2cosmgrowl.cpp)
//   vc2run <rom.4> fxchorus <rom.2> [lfo]  the Chorus on both images, an impulse at each side, and its LFO's phase
//                                          (vc2fxchorus.cpp, vc2fxchorusphase.cpp)
// Prints to stdout, for build/ only.  Every address is rom.4's CPU address unless it says rom.2.
// Build: clang++ -std=c++20 -O2 -Iplugin/Source -Iresearch/tools research/tools/vc2*.cpp plugin/Source/Sh4Interp.cpp
//        plugin/Source/Sh4Setup.cpp plugin/Source/XtLfo.cpp plugin/Source/XtEnvelope.cpp plugin/Source/XtFirmware.cpp
//        plugin/Source/XtFwMachine.cpp plugin/Source/XtMultiBandPlayer.cpp plugin/Source/XtFxChip.cpp
//        plugin/Source/XtFxControl.cpp plugin/Source/XtFxValueText.cpp plugin/Source/XtOsc.cpp
//        plugin/Source/XtFxInput.cpp plugin/Source/XtExtIn.cpp
//        plugin/Source/XtWaveFile.cpp -o build/tools/vc2/vc2run
#include "vc2runs.h"
#include <algorithm>
#include <array>
#include <charconv>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace vc2 {

uint32_t slotOf(Machine& m, uint32_t engine, int slot) {
    return m.cpu.rd32(m.cpu.rd32(engine) + 12 + 12 * (uint32_t)slot + 8);
}

bool runF(Machine& m, uint32_t addr, std::initializer_list<uint32_t> args, float fr4, float fr5) {
    std::memcpy(&m.cpu.fb[0][4], &fr4, 4);   // call() leaves FPSCR.FR at 0: bank 0 holds FR4, FR5
    std::memcpy(&m.cpu.fb[0][5], &fr5, 4);
    return m.run(addr, args);
}

std::vector<uint8_t> fileContents(const char* path) {
    std::vector<uint8_t> f;
    if (FILE* fp = std::fopen(path, "rb")) {
        uint8_t buf[4096];
        for (size_t n; (n = std::fread(buf, 1, sizeof buf, fp)) > 0;) f.insert(f.end(), buf, buf + n);
        std::fclose(fp);
    }
    return f;
}

int answerKernel(Machine& m) {
    // [FW] the stubs into the kernel (0x88000000…, not mapped here): mov.l @(disp, pc), rn; jmp @rn, in this stretch
    constexpr uint32_t kStubsFrom = 0x89301000, kStubsTo = 0x89302000, kKernel = 0x88000000, kKernelEnd = 0x89000000;
    int n = 0;
    for (uint32_t pc = kStubsFrom; pc < kStubsTo; pc += 2) {
        const uint32_t op = m.cpu.rd16(pc), reg = op >> 8 & 15;
        if (op >> 12 != 0xd || m.cpu.rd16(pc + 2) != (0x402bu | reg << 8)) continue;
        const uint32_t literal = m.cpu.rd32(((pc + 4) & ~3u) + 4 * (op & 0xff));
        if (literal < kKernel || literal >= kKernelEnd) continue;
        m.cpu.hook(pc, [](Sh4& c) { c.ret(0); });
        ++n;
    }
    return n;
}

bool initVocoderModule(Machine& m) {
    constexpr uint32_t kFirst = 0x898c609c, kLast = 0x898c6108, kStride = 12;   // [FW] the init table's entries
    for (uint32_t e = kFirst; e <= kLast; e += kStride)
        if (!m.run(m.cpu.rd32(e), {})) return false;
    return true;
}

int boot(Machine& m, int, char**) {
    if (!m.run(kBuild, {kVocoder})) return 1;
    const Sh4::ImageSections& sec = m.cpu.image();
    std::printf("sections: .data %08x..%08x from %08x, .bss to %08x\n", sec.data, sec.bss, sec.dataFrom, sec.bssEnd);
    std::printf("built in %llu steps; +0 %08x +0x1c %08x +0x20 %08x\n", (unsigned long long)m.cpu.steps(),
                m.cpu.rd32(kVocoder), m.cpu.rd32(kVocoder + 0x1c), m.cpu.rd32(kVocoder + 0x20));
    for (const uint32_t engine : {kHireso, kLegacy}) {
        std::printf("engine at %08x: vtable %08x, slots", engine, m.cpu.rd32(engine));
        for (int k = 0; k < 8; ++k) std::printf(" %08x", slotOf(m, engine, k));
        std::printf("\n");
    }
    return 0;
}

} // namespace vc2

namespace {

struct Run {
    std::string_view name;
    int (*fn)(vc2::Machine&, int, char**);
};
constexpr std::array kRuns{Run{"boot", vc2::boot}, Run{"warp", vc2::warp}, Run{"hold", vc2::hold},
                           Run{"growl", vc2::growl}, Run{"kind6", vc2::kind6}, Run{"ratio", vc2::ratio},
                           Run{"lfo", vc2::lfo}, Run{"env", vc2::env}, Run{"cosm", vc2::cosm},
                           Run{"osc", vc2::osc}, Run{"analog", vc2::analog}, Run{"band", vc2::band},
                           Run{"solo", vc2::solo}, Run{"lite", vc2::lite}, Run{"tva", vc2::tva},
                           Run{"patches", vc2::patches}, Run{"track", vc2::track}, Run{"vocosc", vc2::vocosc},
                           Run{"autonote", vc2::autonote}, Run{"mixer", vc2::mixer}, Run{"typed", vc2::typed},
                           Run{"fxprog", vc2::fxprog}, Run{"fxroute", vc2::fxroute}, Run{"fxreverb", vc2::fxreverb},
                           Run{"splits", vc2::splits}, Run{"splitbytes", vc2::splitbytes},
                           Run{"bank", vc2::bank}, Run{"wavein", vc2::wavein},
                           Run{"cosmgrowl", vc2::cosmgrowl}, Run{"fxchorus", vc2::fxchorus}};

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: vc2run <rom.4> <run> [args]: boot, warp, ratio, hold, growl, kind6, patches,\n"
                             "       track, vocosc, autonote, mixer, typed, fxreverb, splits, splitbytes, bank, cosmgrowl;\n"
                             "       vc2run <rom.4> <block> <rom.2>: lfo, env, cosm, osc, analog, band, solo, lite,\n"
                             "       tva, fxprog, fxroute, wavein, fxchorus\n");
        return 2;
    }
    vc2::Machine m(argv[1]);
    if (!m.ok()) {
        std::fprintf(stderr, "vc2run: %s\n", m.error().c_str());
        return 1;
    }
    const auto* run = std::ranges::find(kRuns, std::string_view(argv[2]), &Run::name);
    if (run == kRuns.end()) {
        std::fprintf(stderr, "vc2run: no run %s\n", argv[2]);
        return 2;
    }
    return run->fn(m, argc - 2, argv + 2);
}
