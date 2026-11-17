// xt_vc2cosm <rom.4> [cases] [seed] (v234): the VC-2's COSM against rom.4's own routines; its first part the Growl
// (type 8), Vc2CosmGrowl, on a COSM object in the interpreter's RAM.  Each case the object's 0x164 bytes drawn (every
// word noise, then the Growl's own: the pitch, the block a conversion made, the position, the four buffers' addresses,
// the levels the frame holds), the lines cleared as the frame's allocator clears them or noise, then the note-on's case
// and the update's (now and then, the phase and step drawn instead, as a sounding note leaves them), then 120 events:
// a render of drawn frames, an update after new levels and pitch, a new block from a conversion of a drawn input, the
// pitch moved, the note-on's case, or a conversion alone into a block of noise.  The Matrix readers (0x893a2960,
// 0x893a2aa0) hooked to log the call and answer the next of two drawn groups of sums, the port's the same.  After each,
// the whole object, the lines, the frames, the block and the calls, word for word.  Then its second part (v235,
// Vc2CosmFrameCheck.cpp): the converters' frame.  XT_VC2COSM_FIRST=1 says where the first case to differ does,
// XT_VC2COSM_STOP=1 stops there.  XT_VC2COSM_MAIN only.
#ifdef XT_VC2COSM_MAIN
#include "Vc2CheckCalls.h"
#include "Vc2CosmCheck.h"
#include "Vc2CosmGrowl.h"
#include <array>

namespace {
constexpr uint32_t kConvert = 0x89398780, kNoteOn = 0x893b9d30, kUpdate = 0x893b9d40, kNewBlock = 0x893b9dc0;   // [FW]
constexpr uint32_t kRender = 0x893b9b40;                                                 // [FW] mono and stereo alike
constexpr std::array<uint32_t, 2> kReaders { 0x893a2960, 0x893a2aa0 };                    // [FW] COSM 1's, COSM 2's
constexpr uint32_t kD = 0x58, kCopy = 0x10;   // [FW] the DSP's state and the block's copy in the object
// In Vc2Fw's RAM: the object, the converter's input and block, the two groups of sums, the frames, the four buffers
constexpr uint32_t kObjectAt = 0x8C040000, kInputAt = 0x8C041000, kBlockAt = 0x8C041100, kSumsAt = 0x8C041200;
constexpr uint32_t kFramesAt = 0x8C041400, kLeftAt = 0x8C050000, kRightAt = 0x8C054000, kOtherAt = 0x8C058000;
constexpr uint32_t kSumsStride = 0x40, kBlockWords = 0x48 / 4, kStartLine = 3266;
constexpr int kEvents = 120, kFrameWords = 2 * Vc2CosmGrowl::kFrames;

struct GrowlCall {
    bool second;
    uint32_t matrix;
    bool operator==(const GrowlCall&) const = default;
};

// Both sides' Matrix readers, each logging its calls and answering the next of two drawn groups of sums, so a converter
// that took both its sums from one call would differ
struct GrowlMatrix : Vc2CosmMatrix {
    std::array<std::array<int16_t, kSumsStride / 2>, 2> drawn {};
    std::vector<GrowlCall> rom, port;
    size_t romNext = 0, portNext = 0;
    const int16_t* sums(bool second, uint32_t matrix) override {
        port.push_back({ second, matrix });
        return drawn[portNext++ % 2].data();
    }
};

struct GrowlCounts {
    long events = 0, conversions = 0, second = 0, noteOns = 0, updates = 0, newBlocks = 0, renders = 0, restarts = 0;
};

using Block = std::array<uint32_t, kBlockWords>;

// A setting 0…127: its ends and 64 most often, now and then past the range; a Matrix sum: 0 a third of the time, else
// small, any 16 bits, or within 256 of either end, where the setting and it pass 16 bits and the converter's exts.w
// wraps them
int16_t drawSetting(std::mt19937& rng) {
    constexpr std::array<int16_t, 6> kPicks { 0, 127, 64, -1, -1, -2 };   // -1: 0…127; -2: past it
    const int16_t pick = kPicks[rng() % kPicks.size()];
    return pick == -1 ? (int16_t)(rng() % 128) : pick == -2 ? (int16_t)(rng() % 400) - 140 : pick;
}
int16_t drawSum(std::mt19937& rng) {
    const auto near = (int16_t)(rng() % 256);
    const std::array<int16_t, 6> kinds { 0, 0, (int16_t)((int)(rng() % 129) - 64), (int16_t)rng(),
                                         (int16_t)(INT16_MAX - near), (int16_t)(INT16_MIN + near) };
    return kinds[rng() % kinds.size()];
}

// The COSM object's pitch ratio: 1, a divider's edge (where the factor × it is near a whole number), or 0.03…4
float drawGrowlPitch(std::mt19937& rng, float factor) {
    const uint32_t kind = rng() % 6;
    if (kind == 0) return 1.0f;
    if (kind == 1 && factor > 0.0f) return xtf::div(xtf::flt(2 + (int)(rng() % 60)), factor);
    return std::exp2(7.06f * vc2Unit(rng) - 5.06f);
}

// A conversion on both: a drawn input, the sums drawn, a block of noise; rom.4's converter and the port's; the block
// and the calls compared.  The port's block comes back in `mine`.
std::string convertOn(Vc2Fw& fw, GrowlMatrix& m, std::mt19937& rng, Block& mine, GrowlCounts& n) {
    Vc2CosmInput in {};
    std::array<uint32_t, sizeof in / 4> noise {};
    for (uint32_t& word : noise) word = (uint32_t)rng();
    std::memcpy(&in, noise.data(), sizeof in);
    in.second = rng() % 2 == 0 ? 0 : (uint8_t)rng();
    in.params[0] = drawSetting(rng);
    in.params[1] = drawSetting(rng);
    for (auto& group : m.drawn)
        for (int16_t& sum : group) sum = drawSum(rng);
    for (uint32_t& word : mine) word = (uint32_t)rng();
    fw.cpu.write(kInputAt, &in, sizeof in);
    fw.cpu.write(kSumsAt, m.drawn.data(), sizeof m.drawn);
    fw.cpu.write(kBlockAt, mine.data(), sizeof mine);
    m.rom.clear();
    m.port.clear();
    m.romNext = m.portNext = 0;
    ++n.conversions;
    n.second += in.second != 0 ? 1 : 0;
    Vc2CosmGrowlBlock b {};
    std::memcpy(&b, mine.data(), sizeof b);
    std::string what = vc2BothOn(fw, kConvert, { kInputAt, kBlockAt }, "the converter",
                                 [&] { vc2GrowlConvert(in, b, m); }, [] { return std::string(); });
    std::memcpy(mine.data(), &b, sizeof b);
    if (what.empty()) what = vc2FirstDifference(fw, kBlockAt, mine.data(), sizeof mine, "the block");
    const auto said = [](const GrowlCall& c) { return std::string(c.second ? "COSM 2's" : "COSM 1's") + " reader"; };
    return what.empty() ? vc2CallsDifference(m.rom, m.port, said) : what;
}

// The whole object and the lines against rom.4's
std::string growlDifference(Vc2Fw& fw, const Vc2CosmGrowl& g) {
    return vc2RegionsDifference(fw, { { kObjectAt, &g.w, sizeof g.w, "the object's words" },
                                      { kLeftAt, g.left.data(), g.left.size() * 4, "the left line" },
                                      { kRightAt, g.right.data(), g.right.size() * 4, "the right line" } });
}

// The levels and pitch the frame's update leaves, written on both
void drawLevels(Vc2Fw& fw, Vc2CosmGrowl& g, std::mt19937& rng) {
    g.w.envA = vc2Unit(rng);
    g.w.envB = vc2Unit(rng);
    g.w.lfoA = 2.0f * vc2Unit(rng) - 1.0f;
    g.w.lfoB = 2.0f * vc2Unit(rng) - 1.0f;
    g.w.pitch = drawGrowlPitch(rng, g.w.factor);
    fw.cpu.write(kObjectAt, &g.w, sizeof g.w);
}

// A routine on the object alone, both sides
template <class Mine>
std::string onObject(Vc2Fw& fw, Vc2CosmGrowl& g, uint32_t routine, const char* name, Mine mine, long& count) {
    ++count;
    return vc2BothOn(fw, routine, { kObjectAt }, name, mine, [&] { return growlDifference(fw, g); });
}

// A new block: a conversion, now and then the depths and numbers drawn into it as another type's converter would leave
// them, then rom.4's case given the block's +4 and the port's the block
std::string newBlockOn(Vc2Fw& fw, Vc2CosmGrowl& g, GrowlMatrix& m, std::mt19937& rng, GrowlCounts& n) {
    Block block {};
    std::string what = convertOn(fw, m, rng, block, n);
    if (!what.empty()) return what;
    if (rng() % 4 == 0) {
        for (uint32_t k = 3; k <= 10; ++k)
            block[k] = k % 2 ? std::bit_cast<uint32_t>(2.0f * vc2Unit(rng) - 1.0f) : (uint32_t)rng();
    }
    fw.cpu.write(kBlockAt, block.data(), sizeof block);
    Vc2CosmGrowlBlock b {};
    std::memcpy(&b, block.data(), sizeof b);
    ++n.newBlocks;
    return vc2BothOn(fw, kNewBlock, { kObjectAt, kBlockAt + 4 }, "the new block", [&] { g.newBlock(b); },
                     [&] { return growlDifference(fw, g); });
}

std::string growlRenderOn(Vc2Fw& fw, Vc2CosmGrowl& g, std::mt19937& rng, GrowlCounts& n) {
    std::array<float, kFrameWords> frames {};
    const float level = rng() % 8 == 0 ? kVc2Bus : std::exp2(-12.0f * vc2Unit(rng));
    for (float& x : frames) x = level * (2.0f * vc2Unit(rng) - 1.0f);
    fw.cpu.write(kFramesAt, frames.data(), sizeof frames);
    ++n.renders;
    n.restarts += vc2CosmTables().growl.tiny > g.w.step ? 1 : 0;
    const std::string what =
        vc2BothOn(fw, kRender, { kObjectAt + kD, kObjectAt + kCopy, kObjectAt, kFramesAt }, "the DSP",
                  [&] { g.render(frames.data()); }, [&] { return growlDifference(fw, g); });
    return what.empty() ? vc2FirstDifference(fw, kFramesAt, frames.data(), sizeof frames, "the frames") : what;
}

// A case's start: the object drawn and written on both, the lines cleared or noise; the note-on's case and the update,
// or (one case in eight) the phase and step a sounding note might leave, the restart's edge among them
std::string startCase(Vc2Fw& fw, Vc2CosmGrowl& g, GrowlMatrix& m, std::mt19937& rng, GrowlCounts& n) {
    std::array<uint32_t, sizeof g.w / 4> noise {};
    for (uint32_t& word : noise) word = (uint32_t)rng();
    std::memcpy(&g.w, noise.data(), sizeof g.w);
    Block block {};
    std::string what = convertOn(fw, m, rng, block, n);
    std::memcpy(&g.w.block, block.data(), sizeof g.w.block);
    g.w.position = rng() % 2 ? (int32_t)kStartLine : (int32_t)(rng() % Vc2CosmGrowl::kLine);
    g.w.left = kLeftAt;
    g.w.right = kRightAt;
    g.w.buffer2 = g.w.buffer4 = kOtherAt;
    g.w.factor = 0.0f;
    const bool noisy = rng() % 4 == 0;
    for (auto* line : { &g.left, &g.right })
        for (float& x : *line) x = noisy ? 2.0f * vc2Unit(rng) - 1.0f : 0.0f;
    const bool sounding = rng() % 8 == 0;
    const float tiny = vc2CosmTables().growl.tiny;
    const std::array<float, 5> steps { 0.0f, tiny, std::nextafter(tiny, 0.0f), 1e-6f, 0.02f * vc2Unit(rng) };
    g.w.phase = sounding ? 2.0f * vc2Unit(rng) - 1.0f : 0.0f;
    g.w.step = sounding ? steps[rng() % steps.size()] : 0.0f;
    drawLevels(fw, g, rng);
    fw.cpu.write(kLeftAt, g.left.data(), g.left.size() * 4);
    fw.cpu.write(kRightAt, g.right.data(), g.right.size() * 4);
    if (what.empty() && !sounding) what = onObject(fw, g, kNoteOn, "the note-on", [&] { g.noteOn(); }, n.noteOns);
    return what.empty() ? onObject(fw, g, kUpdate, "the update", [&] { g.update(); }, n.updates) : what;
}

std::string growlEventOn(Vc2Fw& fw, Vc2CosmGrowl& g, GrowlMatrix& m, std::mt19937& rng, GrowlCounts& n) {
    ++n.events;
    const uint32_t pick = rng() % 40;
    if (pick < 26) return growlRenderOn(fw, g, rng, n);
    if (pick < 31) {
        drawLevels(fw, g, rng);
        return onObject(fw, g, kUpdate, "the update", [&] { g.update(); }, n.updates);
    }
    if (pick < 34) return newBlockOn(fw, g, m, rng, n);
    if (pick < 37) {
        g.w.pitch = drawGrowlPitch(rng, g.w.factor);
        fw.cpu.write(kObjectAt, &g.w, sizeof g.w);
        return "";
    }
    if (pick < 38) return onObject(fw, g, kNoteOn, "the note-on", [&] { g.noteOn(); }, n.noteOns);
    Block block {};
    return convertOn(fw, m, rng, block, n);
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2cosm", vc2LoadCosmAndMatrix);
    if (!run) return 1;
    GrowlMatrix m;
    for (size_t k = 0; k < kReaders.size(); ++k) {
        fw.cpu.hook(kReaders[k], [&m, k](Sh4& cpu) {
            m.rom.push_back({ k == 1, cpu.r[4] });
            cpu.ret(kSumsAt + kSumsStride * (uint32_t)(m.romNext++ % 2));
        });
    }
    std::mt19937 rng((uint32_t)run->seed);
    Vc2CosmGrowl g;
    GrowlCounts n;
    Vc2Tally tally { "XT_VC2COSM_FIRST", "XT_VC2COSM_STOP" };
    for (int c = 0; c < run->cases; ++c) {
        std::string what = startCase(fw, g, m, rng, n);
        for (int e = 0; e < kEvents && what.empty(); ++e) what = growlEventOn(fw, g, m, rng, n);
        if (vc2CaseOutcome(tally, what, c, n.events)) break;
    }
    std::printf("growl: %d cases, %ld events: %ld conversions (%ld by COSM 2's reader), %ld note-ons, %ld updates, %ld "
                "new blocks, %ld renders (%ld with the phase started again)\n", run->cases, n.events, n.conversions,
                n.second, n.noteOns, n.updates, n.newBlocks, n.renders, n.restarts);
    const long frame = vc2CosmFrameCases(fw, *run);   // the second part (v235, Vc2CosmFrameCheck.cpp)
    const long matrix = vc2MatrixCases(fw, *run);     // the third part (v243, Vc2MatrixCheck.cpp)
    const long setup = vc2MatrixSetupCases(fw, *run);   // the fourth part (v247, Vc2MatrixSetupCheck.cpp)
    const long object = vc2CosmObjectCases(fw, *run);   // the fifth part (v249, Vc2CosmObjectCheck.cpp)
    std::printf("xt_vc2cosm: %ld different\n", tally.different + frame + matrix + setup + object);
    return 0;
}
#endif
