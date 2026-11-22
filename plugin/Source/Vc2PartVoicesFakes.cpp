// xt_vc2voices' two sides' fakes: see Vc2PartVoicesCheck.h (v272).  rom.4's: the routines its two voices' vtables
// destroy by, the tone's end test its vtable reaches and the tone's cost, hooked to log and answer; the port's: its
// parts reading what rom.4's routines read, and answering the same.  XT_VC2VOICES_MAIN only.
#ifdef XT_VC2VOICES_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2PartVoicesCheck.h"
#include <bit>

namespace {
constexpr uint32_t kOrderAt = 0x20, kOrderBytes = 4;   // [FW] a voice's two ages, +0x20 and +0x24
constexpr uint32_t kEntryRoutine = 8;                  // [FW] a vtable slot's routine, after its adjustment's word

// A tone's cost: its usual size most often (a block's share, 10⁻⁵…10⁻³), its edges now and then; never a NaN or an
// infinity, whose sums the interpreter's host floats and xtf's doubles may give otherwise
float drawCost(std::mt19937& rng) {
    constexpr float kEdges[6] = { 0.0f, -0.0f, -1e-4f, 3e38f, 1.0f, 1e-30f };
    if (rng() % 5 == 0) return kEdges[rng() % 6];
    return 1e-5f * std::pow(100.0f, vc2Unit(rng));
}

VoicesAnswer drawAnswer(std::mt19937& rng) {
    VoicesAnswer a;
    a.ended = rng() % 3 == 0 ? 1 : rng() % 8 == 0 ? (uint32_t)rng() : 0;
    a.cost = drawCost(rng);
    a.note = rng() % 2 ? 0 : kVoicesNotesAt + kVoicesNoteBytes * (uint32_t)(rng() % kVoicesNotes);
    a.leave = rng() % 2 ? Vc2PartVoices::kStayOnLeave : Vc2PartVoices::kDestroyOnLeave;
    a.state = rng() % 4 == 0 ? (uint8_t)rng() : (uint8_t)(rng() % 4);
    a.updated = rng() % 2 ? 0 : (uint16_t)rng();
    a.age = (int)(rng() % 3);
    return a;
}

const char* kindName(int32_t kind) {
    constexpr const char* kNames[kVoicesKinds] = { "destroy", "end test", "cost", "prepare", "retune", "note-on",
                                                   "note's start", "truncation once", "queue's wait", "key-off",
                                                   "parameters' message", "clock", "note's routine",
                                                   "conversion set's routine", "OS's routine", "change test",
                                                   "update task's call" };
    return kNames[kind];
}
} // namespace

VoicesCall voicesSlotCall(Vc2Fw& fw, int32_t kind, uint32_t object, uint32_t vtable, uint32_t slot) {
    return { kind, fw.cpu.rd32(vtable + slot + kEntryRoutine), object + fw.cpu.rd32(vtable + slot) };
}

const VoicesAnswer& VoicesDraws::next(size_t& at) {
    while (drawn.size() <= at) drawn.push_back(drawAnswer(rng));
    return drawn[at++];
}

void VoicesDraws::restart(uint32_t seed) {
    rng.seed(seed);
    drawn.clear();
    rom = port = 0;
}

void voicesApplyDestroy(Vc2VoiceNode& v, uint32_t self, const VoicesAnswer& a) {
    v.note = a.note;
    v.leave = a.leave;
    v.state = a.state;
    v.updated = a.updated;
    v.age = a.age == 0 ? Vc2PartVoices::kZeroAge : self + kOrderAt + kOrderBytes * (uint32_t)(a.age - 1);
}

void voicesLog(std::vector<VoicesCall>& log, const VoicesCall& call) {
    if (log.size() >= kVoicesLogBound) throw VoicesRunaway {};
    log.push_back(call);
}

void VoicesPortParts::destroy(uint32_t voice) {
    voicesLog(log_, voicesSlotCall(fw_, kVoicesDestroy, voice, port_.at(voice).vtable, kVoicesDestroyEntry));
    voicesApplyDestroy(port_.at(voice), voice, draws_.next(draws_.port));
}

int8_t VoicesPortParts::kindOf(uint32_t note) {
    uint8_t kind = 0;
    fw_.cpu.read(note + kVoicesNoteKind, &kind, 1);
    return (int8_t)kind;
}

bool VoicesPortParts::ended(uint32_t tone) {
    const uint32_t vtable = fw_.cpu.rd32(tone + kVoicesToneVtable);
    voicesLog(log_, voicesSlotCall(fw_, kVoicesEnded, tone, vtable, kVoicesEndedEntry));
    return draws_.next(draws_.port).ended != 0;
}

float VoicesPortParts::cost(uint32_t tone) {
    voicesLog(log_, { kVoicesCost, kVoicesCostAt, tone });
    return draws_.next(draws_.port).cost;
}

void voicesInstallRom(Vc2Fw& fw, VoicesDraws& draws, std::vector<VoicesCall>& log) {
    for (const uint32_t vtable : { Vc2PartVoices::kVoiceVtable, kVoicesNoteClass }) {
        const uint32_t routine = fw.cpu.rd32(vtable + kVoicesDestroyEntry + kEntryRoutine);
        fw.cpu.hook(routine, [routine, &draws, &log](Sh4& c) {
            log.push_back({ kVoicesDestroy, routine, c.r[4] });
            Vc2VoiceNode v;
            c.read(c.r[4], &v, sizeof v);
            voicesApplyDestroy(v, c.r[4], draws.next(draws.rom));
            c.write(c.r[4], &v, sizeof v);
            c.ret(0);
        });
    }
    const uint32_t ended = fw.cpu.rd32(kVoicesToneClass + kVoicesEndedEntry + kEntryRoutine);
    fw.cpu.hook(ended, [ended, &draws, &log](Sh4& c) {
        log.push_back({ kVoicesEnded, ended, c.r[4] });
        c.ret(draws.next(draws.rom).ended);
    });
    fw.cpu.hook(kVoicesCostAt, [&draws, &log](Sh4& c) {
        log.push_back({ kVoicesCost, kVoicesCostAt, c.r[4] });
        c.fb[0][0] = std::bit_cast<uint32_t>(draws.next(draws.rom).cost);
        c.ret(0);
    });
}

std::string voicesCallsDifference(const std::vector<VoicesCall>& rom, const std::vector<VoicesCall>& port) {
    return vc2CallsDifference(rom, port, [](const VoicesCall& c) {
        char t[160];
        const auto& m = c.more;
        std::snprintf(t, sizeof t, "the %s %08x (%08x %08x %08x %08x %08x %08x %08x %08x)", kindName(c.kind), c.routine,
                      c.object, m[0], m[1], m[2], m[3], m[4], m[5], m[6]);
        return std::string(t);
    });
}
#endif
