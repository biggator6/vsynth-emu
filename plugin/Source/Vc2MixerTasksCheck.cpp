// xt_vc2mixer's second part (v311): the voices' update and schedule, Vc2PartMixer's updateTones and scheduleTones,
// against rom.4's own 0x893b16a0 and 0x893b1710 on its own mixer words (0x8993eac4), on the seed after the run's.  Up
// to three tones linked in the part's list as the first part links its voices, their vtable's +0x0c, +0x30 and +0x3c
// each at a hooked address of its own logging the tone and its argument; the free LFOs' clock 0x893ad870 and the
// vocoder's glue 0x893f07a0 hooked to log their float.  Each case 400 events, either routine, before each the tones'
// count, the blocks mixed and the two routines' last blocks a few blocks apart (about the update's tick, 0, any, behind)
// and the carried word 0, 1 or any.  After each the mixer's words and the calls.  XT_VC2MIXER_MAIN only.
#ifdef XT_VC2MIXER_MAIN
#include "Vc2CheckMain.h"
#include "Vc2PartMixerCheck.h"
#include <bit>

namespace {
constexpr uint32_t kWordsAt = 0x8993eac4;                              // [FW] the mixer's words
constexpr uint32_t kUpdateAt = 0x893b16a0, kScheduleAt = 0x893b1710;   // [FW]
constexpr uint32_t kFreeLfosClockAt = 0x893ad870, kGlueAt = 0x893f07a0;   // [FW]
// [FW] the tone's three methods by their slot in its table, each at a hooked address after the first part's four
constexpr uint32_t kToneSlots[] = { 0x0c, 0x30, 0x3c };
constexpr MixKind kToneKinds[] = { kToneUpdate, kToneClock, kToneCount };
constexpr bool kFloatArg[] = { false, true, false };
constexpr uint32_t kFirstToneMethod = 4, kMethodStride = 0x10;
constexpr int kEvents = 400;

uint32_t toneMethodAt(size_t k) { return kMixMethodsAt + kMethodStride * (uint32_t)(kFirstToneMethod + k); }
int32_t floatArg(float f) { return std::bit_cast<int32_t>(f); }

// A port's tone: its calls logged as its hooks log them
class TasksPortTone final : public Vc2MixerTone {
public:
    TasksPortTone(int index, std::vector<MixCall>& log) : index_(index), log_(&log) {}
    void update(int32_t n) override { log_->push_back({ kToneUpdate, index_, n }); }
    void clock(float seconds) override { log_->push_back({ kToneClock, index_, floatArg(seconds) }); }
    void count(uint32_t x) override { log_->push_back({ kToneCount, index_, (int32_t)x }); }

private:
    int index_;
    std::vector<MixCall>* log_;
};

class TasksPortParts final : public Vc2MixerTaskParts {
public:
    explicit TasksPortParts(std::vector<MixCall>& log) : log_(log) {}
    void freeLfosClock(float seconds) override { log_.push_back({ kFreeLfosClock, -1, floatArg(seconds) }); }
    void glue(float carried) override { log_.push_back({ kGlue, -1, floatArg(carried) }); }

private:
    std::vector<MixCall>& log_;
};

// rom.4's side: the tones' three methods in their table, and the two calls out
void tasksInstallRom(Vc2Fw& fw, std::vector<MixCall>& rom) {
    for (size_t k = 0; k < std::size(kToneSlots); ++k) {   // an entry: the this-delta, a word, the method
        const uint32_t entry[3] = { 0, 0, toneMethodAt(k) };
        fw.cpu.write(kMixVtableAt + kToneSlots[k], entry, sizeof entry);
        fw.cpu.hook(toneMethodAt(k), [&rom, k](Sh4& c) {
            const int32_t arg = kFloatArg[k] ? (int32_t)c.fb[0][4] : (int32_t)c.r[5];
            rom.push_back({ kToneKinds[k], (int32_t)((c.r[4] - kMixVoicesAt) / kMixVoiceStride), arg });
            c.ret(0);
        });
    }
    for (const auto& [at, kind] : { std::pair { kFreeLfosClockAt, kFreeLfosClock }, std::pair { kGlueAt, kGlue } })
        fw.cpu.hook(at, [&rom, kind](Sh4& c) {
            rom.push_back({ kind, -1, (int32_t)c.fb[0][4] });
            c.ret(0);
        });
}

// The blocks a routine last ran at, from the blocks mixed: a few before, about the update's tick, the same, any, after
int32_t lastBlock(int32_t blocks, std::mt19937& rng) {
    const int32_t picks[5] = { blocks - (int32_t)(rng() % 4), blocks - 13 - (int32_t)(rng() % 3), blocks,
                               (int32_t)rng(), blocks + 1 + (int32_t)(rng() % 3) };
    return picks[rng() % 5];
}

struct TasksSides {
    Vc2Fw& fw;
    Vc2PartMixer& mixer;
    std::vector<MixCall>& rom;
    std::vector<MixCall> mine {};
    long events = 0;
};

std::string tasksEvent(TasksSides& t, std::mt19937& rng) {
    Vc2MixerWords& w = t.mixer.w;
    w.blocks = rng() % 4 ? (int32_t)(rng() % 100000) : (int32_t)rng();
    w.updatedAt = lastBlock(w.blocks, rng);
    w.scheduledAt = lastBlock(w.blocks, rng);
    const float carried[3] = { 0.0f, 1.0f, std::bit_cast<float>((uint32_t)rng() & 0x7f7fffff) };
    w.carried = carried[rng() % 3];
    const int count = (int)(rng() % (kMixVoices + 1));
    mixLinkVoices(t.fw, count);
    t.fw.cpu.write(kWordsAt, &w, sizeof w);
    std::vector<TasksPortTone> tones;
    for (int k = 0; k < count; ++k) tones.emplace_back(k, t.mine);
    std::vector<Vc2MixerTone*> span;
    for (TasksPortTone& tone : tones) span.push_back(&tone);
    t.rom.clear();
    t.mine.clear();
    TasksPortParts parts { t.mine };
    const bool update = rng() % 2 != 0;
    ++t.events;
    if (!t.fw.call(update ? kUpdateAt : kScheduleAt, {})) return "the routine faults: " + t.fw.cpu.error();
    (t.mixer.*(update ? &Vc2PartMixer::updateTones : &Vc2PartMixer::scheduleTones))(span, parts);
    const std::string words = vc2RegionsDifference(t.fw, { { kWordsAt, &w, sizeof w, "the mixer's words" } });
    return words.empty() ? mixCallsDifference(t.rom, t.mine) : words;
}

std::string tasksCase(TasksSides& t, std::mt19937& rng) {
    std::string what;
    for (int k = 0; k < kEvents && what.empty(); ++k) what = tasksEvent(t, rng);
    return what;
}
} // namespace

long vc2MixerTasksPart(Vc2Fw& fw, Vc2PartMixer& mixer, std::vector<MixCall>& rom, const Vc2Run& run) {
    tasksInstallRom(fw, rom);
    TasksSides t { fw, mixer, rom };
    const long different = vc2CheckPart(run, 1, { "XT_VC2MIXER_FIRST", "XT_VC2MIXER_STOP" }, t.events,
                                        [&](std::mt19937& rng) { return tasksCase(t, rng); });
    std::printf("voices' update and schedule: %ld events\n", t.events);
    return different;
}
#endif
