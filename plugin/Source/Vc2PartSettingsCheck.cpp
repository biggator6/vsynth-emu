// xt_vc2settings (v286): Vc2PartSettings against rom.4's own readers on its own tables (0x89997090, 0x89e2765c,
// 0x89e2f85c, 0x89e32a60, 0x89e4987c, the byte 0x89997fcc), the seven read objects' set-ups hooked to leave a handle
// (the block, part and kind) and the getter 0x89324350 hooked to answer from a store both sides draw alike and log;
// the held flags (0x8993d0c0…c4) drawn, and now and then one set after a drawn read, on both sides.  Each case noise in
// the tables, then 400 events: each reader, its rows, the systems' and the two laws alone; after each the tables word
// for word, the calls and the answer.  XT_VC2SETTINGS_MAIN only.
#ifdef XT_VC2SETTINGS_MAIN
#include "Vc2PartSettingsEvents.h"

namespace {
using Settings = Vc2PartSettings;
using Block = Vc2ParamBlock;
constexpr uint32_t kHeldAt = 0x8993d0c0, kGetter = 0x89324350, kTypedGetter = 0x89324370;    // [FW]
// [FW] IDs drawn within a few values: a COSM input's type always, 0…16 (the run table's); half the time the OSCs'
// Types 0…3 (v289), Formant Template 0…17 (v290), either side of the templates' 16
constexpr SettingsNarrow kNarrowIds[] = { { 0x463, Vc2PartSettingsTables::kCosmTypes, kAlways },
                                          { 0x2e5, 4, kHalfTheTime }, { 0x319, 4, kHalfTheTime },
                                          { 0x1a0, 18, kHalfTheTime } };
constexpr uint32_t kLfoWords = 0x3200, kTvaWords = 0x1804;   // the tables' words; the bytes past them by hand

// [FW] the read objects' set-ups, by block
struct SetUp {
    uint32_t routine;
    Block block;
};
constexpr SetUp kSetUps[] = { { 0x893207b0, Block::kPart }, { 0x89320790, Block::kSystem },
                              { 0x89320760, Block::kSystemB }, { 0x89320f60, Block::kSystemC },
                              { 0x89320a50, Block::kKindA }, { 0x89320ad0, Block::kKindB },
                              { 0x89320b70, Block::kKindC }, { 0x89320c10, Block::kCosm } };

// Values at the laws' edges and the stores' widths
constexpr int32_t kEdges[] = { 0, 1, -1, 2, 7, 8, 31, 32, 33, -32, -33, 63, 64, -63, -64, 127, 128, 255, 256, 287,
                               -287, 32767, -32768 };

std::string saidRead(const SettingsCall& c) {
    char t[80];
    std::snprintf(t, sizeof t, "block %d, part %d, kind %d, COSM %d, ID 0x%x", c.block, c.part, c.kind, c.sub,
                  c.id & 0xffff);
    return t;
}

// The bytes past the tables' words, and the controls' flag
std::string settingsTails(SettingsSides& s) {
    const Vc2PartRows& t = s.rows;
    const std::pair<uint32_t, uint8_t> bytes[] = { { kLfosAt + kLfoWords, t.lfos[kLfoWords] },
                                                   { kLfosAt + kLfoWords + 1, t.lfos[kLfoWords + 1] },
                                                   { kTvasAt + kTvaWords, t.tvas[kTvaWords] },
                                                   { kTvasAt + kTvaWords + 1, t.tvas[kTvaWords + 1] },
                                                   { kControlsAt, t.controls },
                                                   { kTripAssignsAt, (uint8_t)t.tripAssigns[0] },
                                                   { kTripAssignsAt + 1, (uint8_t)t.tripAssigns[1] },
                                                   { kPatchRemainAt, t.patchRemain } };
    for (const auto& [at, mine] : bytes) {
        if (s.fw.cpu.rd8(at) == mine) continue;
        char text[96];
        std::snprintf(text, sizeof text, "byte 0x%08x: %02x, against %02x", at, mine, s.fw.cpu.rd8(at));
        return text;
    }
    return "";
}

std::string settingsDifference(SettingsSides& s) {
    const Vc2PartRows& t = s.rows;
    const std::string words = vc2RegionsDifference(s.fw, { { kCommonsAt, &t.commons, sizeof t.commons, "commons" },
        { kEnvelopesAt, &t.envelopes, sizeof t.envelopes, "envelopes" }, { kLfosAt, &t.lfos, kLfoWords, "LFOs" },
        { kMixesAt, &t.mixes, sizeof t.mixes, "mix" }, { kTvasAt, &t.tvas, kTvaWords, "TVAs" },
        { kBytesAt, &t.bytes, sizeof t.bytes, "bytes" }, { kCosmsAt, &t.cosms, sizeof t.cosms, "COSM inputs" },
        { kOscsAt, &t.oscs, sizeof t.oscs, "OSCs" }, { kStepsAt, &t.steps, sizeof t.steps, "step counts" },
        { kRobotAt, &t.robot, sizeof t.robot, "robot human depth" },
        { kUnvoiceLevelAt, &s.outs.unvoiceLevel, sizeof s.outs.unvoiceLevel, "Unvoice Level" },
        { kModesAt, &t.modes, sizeof t.modes, "mode rows" },
        { kNodesAt, s.nodes.nodes.data(), sizeof s.nodes.nodes, "nodes" }, { kMatrixAt, &s.g, sizeof s.g, "G" },
        { kHeldNoteAt, &t.heldNote, sizeof t.heldNote, "the held note" },
        { kVoiceCountsAt, t.voiceCounts.data(), sizeof t.voiceCounts, "the voices' counts" } });
    if (!words.empty()) return words;
    const std::string tails = settingsTails(s);
    return tails.empty() ? vc2CallsDifference(s.rom.log, s.mine.log, saidRead) : tails;
}

template <auto Law, uint32_t Routine, size_t Counted>
std::string lawEvent(SettingsSides& s, std::mt19937& rng) {
    const int32_t x = rng() % 2 ? kEdges[rng() % std::size(kEdges)] : (int32_t)(rng() % 2 ? rng() % 80 - 40 : rng());
    ++s.n.byEvent[Counted];
    return settingsOnBoth(s, { Routine, "a law", vc2R0 }, { (uint32_t)x }, [&] { return (uint32_t)Law(x); });
}

// [FW] the events by routine
const SettingsEvent kSettingsEvents[] = {
    partEvent<&Settings::common, 0x893a1be0, kCommonsAt, 0>,
    partEvent<&Settings::envelopes, 0x8939ada0, kEnvelopesAt, 1>,
    rowEvent<&Settings::envelopeRow, 0x8939ab90, kEnvelopesAt, 13, 2>,
    partEvent<&Settings::lfos, 0x8939b9a0, kLfosAt, 3>,
    rowEvent<&Settings::lfoRow, 0x8939b860, kLfosAt, 5, 4>,
    systemEvent<&Settings::lfoSystem, 0x8939b970, kLfosAt, 5>,
    partEvent<&Settings::mixes, 0x8939bff0, kMixesAt, 6>,
    kindEvent<&Settings::mixRow, 0x8939bf30, kMixesAt, 7>,
    partEvent<&Settings::tvas, 0x893a71f0, kTvasAt, 8>,
    kindEvent<&Settings::tvaRow, 0x893a6f80, kTvasAt, 9>,
    kindEvent<&Settings::tvaVelocityRow, 0x893a70c0, kTvasAt, 10>,
    systemEvent<&Settings::tvaTrailer, 0x893a7160, kTvasAt, 11>,
    systemEvent<&Settings::tvaRoute, 0x893a71a0, kTvasAt, 12>,
    systemEvent<&Settings::tvaControls, 0x893a71d0, 0, 13>,
    lawEvent<vc2CurveOf, 0x893a8120, 14>,
    lawEvent<vc2DepthOf, 0x893a8070, 15>,
};

// [FW] the second part's (v287), counted after the first's
const SettingsEvent kCosmEvents[] = {
    partEvent<&Settings::bytes, 0x893a8250, kBytesAt, 16>,
    kindEvent<&Settings::byteRow, 0x893a81d0, kBytesAt, 17>,
    partEvent<&Settings::cosms, 0x89399ed0, kCosmsAt, 18>,
    rowEvent<&Settings::cosmRow, 0x89399c90, kCosmsAt, 2, 19>,
    rowEvent<&Settings::cosmVelocityRow, 0x89399d60, kCosmsAt, 3, 20>,
};

std::string settingsCase(SettingsSides& s, std::mt19937& rng) {
    return settingsCaseOf<kSettingsEvents, offsetof(Vc2PartRows, bytes), 4>(s, rng, s.n.events, s.n.calls);
}

// The second part (v287), on the seed after the run's: the part's bytes and the COSM inputs', v287's tables noise
void sayCosmPart(const SettingsCounts& n) {
    const auto& e = n.byEvent;
    std::printf("COSM inputs: %ld events: bytes %ld, %ld rows; COSM inputs %ld, %ld rows, %ld velocity rows (%ld "
                "reads)\n", n.moreEvents, e[16], e[17], e[18], e[19], e[20], n.moreCalls);
}

const SettingsPartRow kCosmPart { 1, &SettingsCounts::moreEvents, &SettingsCounts::moreCalls,
                                  settingsCaseOf<kCosmEvents, offsetof(Vc2PartRows, oscs), 5>, sayCosmPart };

// The further parts, in turn on the seeds after the run's
long settingsMoreParts(SettingsSides& s, const Vc2Run& run) {
    long different = settingsRunPart(s, run, kCosmPart);
    for (const SettingsPartRow& p : kOscPartRows) different += settingsRunPart(s, run, p);
    for (const SettingsPartRow* p : { &kVocoderPartRow, &kModePartRow, &kDispatchPartRow, &kCategoryPartRow,
                                      &kStaticPartRow, &kSystemPartRow })
        different += settingsRunPart(s, run, *p);
    return different + settingsRunPart(s, run, kMessagePartRow);
}

void saySettingsCounts(const Vc2Run& run, const SettingsCounts& n, long different) {
    const auto& e = n.byEvent;
    std::printf("settings: %d cases, %ld events: %ld commons; envelopes %ld, %ld rows; LFOs %ld, %ld rows, %ld "
                "systems; mix %ld, %ld rows; TVAs %ld, %ld rows, %ld velocity rows, %ld trailers, %ld routes, %ld "
                "controls; laws %ld curves, %ld depths (%ld reads)\n", run.cases, n.events, e[0], e[1], e[2], e[3],
                e[4], e[5], e[6], e[7], e[8], e[9], e[10], e[11], e[12], e[13], e[14], e[15], n.calls);
    std::printf("xt_vc2settings: %ld different\n", different);
}

} // namespace

void settingsDraw(SettingsSides& s, std::mt19937& rng, uint32_t flipped) {
    s.rom.store.seed = (uint32_t)rng();
    s.rom.flags = {};
    if (rng() % 4 == 0) s.rom.flags[rng() % s.rom.flags.size()] = rng() % 2 ? 1 : (uint8_t)(1 + rng() % 255);
    const auto spans = std::to_array<uint32_t>({ 16, 48, 2400 });
    s.rom.flipAt = rng() % 4 == 0 ? (int)(1 + rng() % spans[rng() % spans.size()]) : 0;
    s.rom.flipFlag = (int)(rng() % flipped);
}

int32_t SettingsStore::value(const SettingsCall& c) const {
    uint32_t h = seed ^ (uint32_t)c.block * 0x9e3779b9u ^ (uint32_t)c.part * 0x85ebca6bu;
    h ^= (uint32_t)c.kind * 0xc2b2ae35u ^ (uint32_t)(uint16_t)c.id * 0x27d4eb2fu;
    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    const uint32_t pick = h & 7, v = h >> 3;
    if (c.block == kOscBlockCall) return v % 64 == 0 ? 0 : pick < 6 ? 1 : (int32_t)(h | 0x100);
    for (const std::span<const SettingsNarrow> ids : { std::span<const SettingsNarrow>(kNarrowIds), narrow })
        for (const SettingsNarrow& n : ids)
            if (c.id == n.id && pick < n.picks) return n.from + (int32_t)(v % n.values);
    if (pick < 4) return (int32_t)(v % 128);
    if (pick < 6) return (int32_t)(v % 129) - 64;
    return pick == 6 ? kEdges[v % std::size(kEdges)] : (int32_t)(h * 2654435761u);
}

int32_t SettingsReads::read(const SettingsCall& c) {
    log.push_back(c);
    if (++reads == flipAt) flags[flipFlag] = 1;
    return store.value(c);
}

long settingsRunPart(SettingsSides& s, const Vc2Run& run, const SettingsPartRow& p) {
    SettingsCounts& n = s.n;
    if (p.start) p.start(s);
    const long different = vc2CheckPart(run, p.offset, { "XT_VC2SETTINGS_FIRST", "XT_VC2SETTINGS_STOP" }, n.*p.events,
        [&](std::mt19937& rng) { return p.oneCase(s, rng, n.*p.events, n.*p.calls); });
    p.say(n);
    return different;
}

void settingsHookCallees(SettingsSides& s, std::span<const SettingsCallee> callees) {
    for (const SettingsCallee& k : callees)
        s.fw.cpu.hook(k.at, [&s, k](Sh4& c) {
            s.rom.log.push_back(k.called(c));
            c.ret(k.answer != nullptr ? k.answer(s) : 0);
        });
}

void logOut(std::vector<SettingsCall>& log, int32_t what, uint32_t value) {
    log.push_back({ what, 0, 0, 0, (int32_t)value });
}

void logSettingsWords(std::vector<SettingsCall>& log, const uint32_t* words, int32_t stereo) {
    constexpr int kPivots = 4, kPivotWords = 2;   // [FW] +0x10, +0x14
    for (int k = 0; k < kSettingsWords; ++k)
        if (k < kPivots || k >= kPivots + kPivotWords) log.push_back({ kOutSettings, 0, k, 0, (int32_t)words[k] });
    log.push_back({ kOutSettings, 0, kStereoIndex, 0, stereo });
}

uint32_t SettingsSides::romRead(Sh4& c, const SettingsCall& call) {
    const int32_t v = rom.read(call);
    if (rom.reads == rom.flipAt) c.wr8(kHeldAt + rom.flipFlag, 1);
    return (uint32_t)v;
}

SettingsSides::SettingsSides(Vc2Fw& rig) : fw(rig) {
    for (const SetUp& u : kSetUps)
        fw.cpu.hook(u.routine, [u](Sh4& c) {
            const bool byPart = u.block == Block::kPart || u.block >= Block::kKindA;
            const uint32_t part = byPart ? (c.r[5] & 0xff) << 8 : 0;
            const uint32_t kind = u.block >= Block::kKindA ? (c.r[6] & 0x3f) << 2 : 0;
            const uint32_t cosm = u.block == Block::kCosm ? c.r[7] & 3 : 0;
            c.wr32(c.r[4] + 4, kSettingsHandle | (uint32_t)u.block << 16 | part | kind | cosm);
            c.ret(0);
        });
    // Both getters (the typed one's type, r6, is the store's to know): the handle's read
    for (const uint32_t getter : { kGetter, kTypedGetter })
        fw.cpu.hook(getter, [this](Sh4& c) {
            const uint32_t h = c.r[4];
            c.ret(romRead(c, { (int32_t)(h >> 16 & 0xff), (int32_t)(h >> 8 & 0xff), (int32_t)(h >> 2 & 0x3f),
                               (int32_t)(h & 3), (int16_t)c.r[5] }));
        });
    // The OSC block's reader (v288), until the fourth part takes the hook off: its part, kind and OSC, the table's
    // address unread
    fw.cpu.hook(kOscBlock, [this](Sh4& c) {
        c.ret(romRead(c, { kOscBlockCall, (int32_t)c.r[5], (int32_t)c.r[6], (int32_t)c.r[7], 0 }));
    });
}

std::string settingsOnBoth(SettingsSides& s, const Vc2CheckRoutine& routine, std::initializer_list<uint32_t> args,
                           const std::function<uint32_t()>& mine) {
    s.rom.log.clear();
    s.rom.reads = 0;
    s.mine = s.rom;
    s.fw.cpu.write(kHeldAt, s.rom.flags.data(), s.rom.flags.size());
    if (s.rom.flags != std::array<uint8_t, 5> {} || s.rom.flipAt != 0) ++s.n.gaveUp;
    return vc2BothAnswer(s.fw, routine, args, mine, [&] { return settingsDifference(s); });
}

int main(int argc, char** argv) {
    const Vc2CheckStart start { "xt_vc2settings",
                                vc2LoadAll<vc2LoadPartSettings, vc2LoadLibMath, vc2LoadMatrix, vc2LoadKeys>,
                                { "XT_VC2SETTINGS_FIRST", "XT_VC2SETTINGS_STOP" } };
    return vc2CheckMain<SettingsSides>(argc, argv, start, settingsCase, saySettingsCounts, settingsMoreParts);
}
#endif
