// xt_vc2settings' fifth part (v290): the vocoder reader 0x893a72f0 against rom.4's, on the seed four after the run's,
// every table noise; rom.4's powf its own, its callees hooked to log their values among the reads (the part mixer's
// setters' floats and its targets, Auto Note's two setters, the vocoder's mode, the settings struct word for word but
// the pivots, and the stereo flag), the port's calls out the same through SettingsPortOuts; robot human depth's word
// and Unvoice Level's compared as memory.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsCheck.h"

namespace {
constexpr uint32_t kVocoderReader = 0x893a72f0, kSettingsWrapper = 0x893b1be0;   // [FW]
constexpr int16_t kAutoNoteId = 0x19c;                                        // [FW] read with an EXTIN OSC alone
constexpr int kFirstOffset = 2;                                               // [FW] the settings' first offset

// A callee's value: a float's bits in fr4, a word in r4, or none
uint32_t floatArgument(Sh4& c) { return c.fb[(c.fpscr >> 21) & 1][4]; }
uint32_t wordArgument(Sh4& c) { return c.r[4]; }
uint32_t noArgument(Sh4&) { return 0; }

// [FW] a callee: its address, its call's code in the log, where its value is
struct VocoderCallee {
    uint32_t at;
    int32_t what;
    uint32_t (*value)(Sh4&);
};
constexpr VocoderCallee kCallees[] = {
    { 0x893b1eb0, kOutDirect, floatArgument }, { 0x893b1ec0, kOutReverb, floatArgument },
    { 0x893b1ed0, kOutMfx, floatArgument }, { 0x893b1ee0, kOutTargets, noArgument },
    { 0x893ebc20, kOutAutoNote, wordArgument }, { 0x893ebc60, kOutLegato, wordArgument },
    { 0x893b1c00, kOutMode, wordArgument },
};

void hookCallees(SettingsSides& s) {
    for (const VocoderCallee& k : kCallees)
        s.fw.cpu.hook(k.at, [&s, k](Sh4& c) {
            logOut(s.rom.log, k.what, k.value(c));
            c.ret(0);
        });
    s.fw.cpu.hook(kSettingsWrapper, [&s](Sh4& c) {
        uint32_t words[kSettingsWords];
        for (int k = 0; k < kSettingsWords; ++k) words[k] = c.rd32(c.r[4] + 4 * (uint32_t)k);
        logSettingsWords(s.rom.log, words, (int32_t)c.r[5]);
        c.ret(0);
    });
}

// The reader on a drawn part, counting the events with an EXTIN OSC and those with a template's offsets
std::string vocoderEvent(SettingsSides& s, std::mt19937& rng) {
    const std::string what = partEventWith<kVocoderReader, kRobotAt, 26>(s, rng, [&](int part) {
        return s.settings.vocoder(s.rows, part, s.outs);
    });
    for (const SettingsCall& c : s.mine.log) {
        s.n.extin += c.id == kAutoNoteId && c.block == (int32_t)Vc2ParamBlock::kPart;
        s.n.offsets += c.block == kOutSettings && c.kind == kFirstOffset && c.id != 0;
    }
    return what;
}

const SettingsEvent kVocoderEvents[] = { vocoderEvent };

void sayVocoderPart(const SettingsCounts& n) {
    std::printf("vocoder: %ld events, %ld with an EXTIN OSC, %ld with a template's offsets (%ld calls)\n",
                n.vocoderEvents, n.extin, n.offsets, n.vocoderCalls);
}
} // namespace

const SettingsPartRow kVocoderPartRow { 4, &SettingsCounts::vocoderEvents, &SettingsCounts::vocoderCalls,
                                        settingsCaseOf<kVocoderEvents, sizeof(Vc2PartRows), 5>, sayVocoderPart,
                                        hookCallees };
#endif
