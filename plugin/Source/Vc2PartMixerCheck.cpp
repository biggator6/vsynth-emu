// xt_vc2mixer <rom.4> [cases] [seed] (v227): the VC-2's part mixer, Vc2PartMixer, against rom.4's own routines.  The
// block (0x893b1820) on input frames in Vc2Fw's RAM, its pointer (0x8993eadc) set there, with up to three voices linked
// in the part's list (0x899996d0), their methods hooked to answer and render as drawn, and what it calls (the input
// stage, kind 6's ring, the unvoiced detector and its ring, the vocoder's stereo driver) hooked to log
// (Vc2PartMixerFakes.cpp); the port's parts and voices log and answer the same.  A block draws the input's words, each
// voice's answers and frames (now and then a NaN or an infinity), whether an engine plays and what the vocoder adds;
// one block in ten an event first: the level routine (0x893b1480) on drawn parameters, its reads (0x89320a50,
// 0x893207b0, the getter 0x89324350) hooked to answer them; the targets (0x893b1ee0), suspend (0x893b1390) or resume
// (0x893b1410); a setter (0x893b1eb0, 0x893b1ec0, 0x893b1ed0); the flag's setter (0x893b1370); the reset (0x893b1680);
// or the offsets (0x89997290) and the bypass word (0x89934550) drawn.  After each, the words, the buffers, the bypass,
// the detector's ring, the bus frames and the calls word for word.  XT_VC2MIXER_FIRST=1 says where the first case to
// differ does, XT_VC2MIXER_STOP=1 stops there.  XT_VC2MIXER_MAIN only.
#ifdef XT_VC2MIXER_MAIN
#include "Vc2PartMixerCheck.h"
#include "Vc2CheckCalls.h"
#include "Vc2PartMixerTables.h"
#include <algorithm>
#include <span>

namespace {
// [FW] rom.4's routines
constexpr uint32_t kBlockAt = 0x893b1820;
constexpr uint32_t kLevelsAt = 0x893b1480;
constexpr uint32_t kSetExtinAt = 0x893b1370;
constexpr uint32_t kResetAt = 0x893b1680;
// [FW] the level routine's reads: an object pointed at a zone's OSC (0x89320a50) or the part (0x893207b0), then the
// getter on its +4 word and an ID; the hooks put a tag of their own in that word
constexpr uint32_t kPointOscAt = 0x89320a50;
constexpr uint32_t kPointPartAt = 0x893207b0;
constexpr uint32_t kGetterAt = 0x89324350;
constexpr uint32_t kOscTag = 0x100, kPartTag = 0x200, kZoneTag = 16, kTagAt = 4;
// [FW] the mixer's state
constexpr uint32_t kWordsAt = 0x8993eac4;
constexpr uint32_t kInputPointerAt = 0x8993eadc;
constexpr uint32_t kBuffersAt = 0x8999f1d0;
constexpr uint32_t kBypassAt = 0x89934550;
constexpr uint32_t kOffsetsAt = 0x89997290;
constexpr int kBlocks = 400, kEventOneIn = 10, kLevels = 128, kTypes = 4;
constexpr int kBusWords = Vc2PartMixer::kBlock * Vc2PartMixer::kFrameWords;

// A read the level routine makes: its object's tag, the ID, the parameter that answers
struct MixRead {
    uint32_t tag, id;
    int32_t (*answer)(const Vc2MixerParams&);
};
// [FW] 0x893b1480's five: OSC Type by 0x89320a50(obj, 0, 0) and (obj, 0, 1), Direct Sound Level by the part, the sends
constexpr MixRead kReads[] = {
    { kOscTag, 0x2e5, [](const Vc2MixerParams& p) { return p.oscType[0]; } },
    { kOscTag + 1, 0x2e5, [](const Vc2MixerParams& p) { return p.oscType[1]; } },
    { kPartTag, 0x195, [](const Vc2MixerParams& p) { return p.direct; } },
    { kOscTag, 0x2e2, [](const Vc2MixerParams& p) { return p.reverbSend; } },
    { kOscTag, 0x2e0, [](const Vc2MixerParams& p) { return p.mfxSend; } },
};

// [FW] the routines that take nothing, and the setters, which take a float
struct MixPlain {
    uint32_t at;
    const char* name;
    void (Vc2PartMixer::*port)();
};
constexpr MixPlain kPlain[] = {
    { 0x893b1ee0, "the targets", &Vc2PartMixer::targets },
    { 0x893b1390, "suspend", &Vc2PartMixer::suspend },
    { 0x893b1410, "resume", &Vc2PartMixer::resume },
};
struct MixSetter {
    uint32_t at;
    const char* name;
    void (Vc2PartMixer::*port)(float);
};
constexpr MixSetter kSetters[] = {
    { 0x893b1eb0, "Direct Sound's setter", &Vc2PartMixer::setDirect },
    { 0x893b1ec0, "the Reverb send's setter", &Vc2PartMixer::setReverbSend },
    { 0x893b1ed0, "the MFX send's setter", &Vc2PartMixer::setMfxSend },
};

struct MixCounts {
    long blocks = 0, extin = 0, bypassed = 0, carriers = 0, renders = 0, vocoders = 0, events = 0;
};

// Both sides: rom.4 with its calls' log, the port's mixer, parts and voices, the draws they share, the bus frames
struct MixSides {
    Vc2Fw& fw;
    Vc2PartMixer& mixer;
    MixPortParts& parts;
    std::span<Vc2MixerVoice* const> voices;
    MixDraws& draws;
    std::vector<MixCall>& rom;
    Vc2MixerParams params {};
    bool stray = false;   // a read with no answer
    std::array<int32_t, kBusWords> in {};
    std::array<float, kBusWords> out {};
    MixCounts n;
};

std::string mixDifference(MixSides& s) {
    const Vc2PartMixer& m = s.mixer;
    const auto& ring = s.parts.ring;
    std::string what = vc2RegionsDifference(
        s.fw, { { kWordsAt, &m.w, sizeof m.w, "the words" }, { kBuffersAt, &m.b, sizeof m.b, "the buffers" },
                { kBypassAt, &m.bypass, sizeof m.bypass, "the bypass" },
                { kMixRingAt, ring.data(), sizeof ring, "the detector's ring" },
                { kMixOutAt, s.out.data(), sizeof s.out, "the bus frames" } });
    if (what.empty()) what = mixCallsDifference(s.rom, s.parts.calls);
    if (what.empty() && s.stray) what = "a read the level routine has no answer for";
    return what;
}

void freshLogs(MixSides& s) {
    s.rom.clear();
    s.parts.calls.clear();
    s.stray = false;
}

// A block's draws on both sides: the input's words, the voices and their frames, the engine, the vocoder's frames, and
// the bus frames as they were (their DIR words the mixer's to leave)
void drawBlock(MixSides& s, std::mt19937& rng) {
    MixDraws& d = s.draws;
    d.voices = (int)(rng() % (kMixVoices + 1));
    for (MixVoiceDraw& v : d.voice) {
        v.carrier = rng() % 3 == 0;
        v.held = rng() % 3 == 0;
        mixDrawFrames(v.frames, rng);
    }
    d.engine = rng() % 2 == 0;
    mixDrawFrames(d.vocoder, rng);
    std::ranges::generate(s.in, [&] { return mixDrawWord(rng); });
    std::ranges::generate(s.out, [&] { return mixDrawSample(rng); });
    const uint32_t engine = d.engine ? kMixVoicesAt : 0;   // any word but 0: an engine
    s.fw.cpu.write(kMixEngineAt, &engine, sizeof engine);
    s.fw.cpu.write(kMixInputAt, s.in.data(), sizeof s.in);
    s.fw.cpu.write(kMixOutAt, s.out.data(), sizeof s.out);
    mixLinkVoices(s.fw, d.voices);
}

// What rom.4's block called: the carriers' renders, the others', the vocoder
void tallyCalls(MixSides& s) {
    s.n.carriers += vc2CountCalls(s.rom, kRenderCarrier);
    s.n.renders += vc2CountCalls(s.rom, kRender);
    s.n.vocoders += vc2CountCalls(s.rom, kVocoder);
}

std::string mixBlockOn(MixSides& s, std::mt19937& rng) {
    drawBlock(s, rng);
    freshLogs(s);
    ++s.n.blocks;
    s.n.extin += s.mixer.w.extin != 0 ? 1 : 0;
    s.n.bypassed += s.mixer.bypass != 0 ? 1 : 0;
    const auto voices = s.voices.first((size_t)s.draws.voices);
    std::string what = vc2BothOn(
        s.fw, kBlockAt, { kMixOutAt }, "the block", [&] { s.mixer.block(s.in.data(), s.out.data(), voices, s.parts); },
        [&] { return mixDifference(s); });
    tallyCalls(s);
    return what;
}

// An event on both: rom.4's routine at `at` and the port's `port()`, the logs fresh, then what differs
template <class Port>
std::string mixEventOn(MixSides& s, uint32_t at, std::initializer_list<uint32_t> args, const char* name, Port port) {
    ++s.n.events;
    freshLogs(s);
    return vc2BothOn(s.fw, at, args, name, port, [&] { return mixDifference(s); });
}

// The level routine on drawn parameters: each OSC Type 0…3 (2 EXTIN), the levels 0…127
std::string levelsOn(MixSides& s, std::mt19937& rng) {
    for (int32_t& type : s.params.oscType) type = (int32_t)(rng() % kTypes);
    s.params.direct = (int32_t)(rng() % kLevels);
    s.params.reverbSend = (int32_t)(rng() % kLevels);
    s.params.mfxSend = (int32_t)(rng() % kLevels);
    return mixEventOn(s, kLevelsAt, {}, "the level routine", [&] { s.mixer.levels(s.params); });
}

std::string plainOn(MixSides& s, std::mt19937& rng) {
    const MixPlain& e = kPlain[rng() % std::size(kPlain)];
    return mixEventOn(s, e.at, {}, e.name, [&] { (s.mixer.*e.port)(); });
}

// A setter's value: a level, now and then past [0, 1]
std::string setterOn(MixSides& s, std::mt19937& rng) {
    const MixSetter& e = kSetters[rng() % std::size(kSetters)];
    const float v = rng() % 4 == 0 ? 3.0f * vc2Unit(rng) - 1.0f : vc2DrawLevel(rng);
    s.fw.cpu.fb[0][4] = std::bit_cast<uint32_t>(v);
    return mixEventOn(s, e.at, {}, e.name, [&] { (s.mixer.*e.port)(v); });
}

// The flag: 0 or 1, now and then any word
std::string extinOn(MixSides& s, std::mt19937& rng) {
    const uint32_t on = rng() % 4 == 0 ? rng() : rng() % 2;
    return mixEventOn(s, kSetExtinAt, { on }, "the flag's setter", [&] { s.mixer.setExtin((int32_t)on); });
}

std::string resetOn(MixSides& s, std::mt19937&) {
    return mixEventOn(s, kResetAt, {}, "the reset", [&] { s.mixer.reset(s.parts); });
}

// The offsets, the tone generator's sums (mostly in [−1, 1], now and then 0), and now and then the bypass word
std::string drawnOn(MixSides& s, std::mt19937& rng) {
    Vc2MixerOffsets& o = s.mixer.offsets;
    for (float* x : { &o.direct, &o.mfx, &o.level }) *x = rng() % 3 == 0 ? 0.0f : 2.0f * vc2Unit(rng) - 1.0f;
    s.fw.cpu.write(kOffsetsAt, &o, sizeof o);
    if (rng() % 3 == 0) s.mixer.bypass = (int32_t)(rng() % 2);
    s.fw.cpu.write(kBypassAt, &s.mixer.bypass, sizeof s.mixer.bypass);
    ++s.n.events;
    freshLogs(s);
    return mixDifference(s);
}

using MixEvent = std::string (*)(MixSides&, std::mt19937&);
constexpr MixEvent kEvents[] = { levelsOn, levelsOn, drawnOn, plainOn, setterOn, extinOn, resetOn };

// A case: the offsets and the level routine, then its blocks with an event before one in ten
std::string mixCase(MixSides& s, std::mt19937& rng) {
    std::string what = drawnOn(s, rng);
    if (what.empty()) what = levelsOn(s, rng);
    for (int k = 0; k < kBlocks && what.empty(); ++k) {
        if (rng() % kEventOneIn == 0) what = kEvents[rng() % std::size(kEvents)](s, rng);
        if (what.empty()) what = mixBlockOn(s, rng);
    }
    return what;
}

// A pointing routine's object given the hook's tag at its +4 word
void mixTagged(Sh4& cpu, uint32_t tag) {
    cpu.write(cpu.r[4] + kTagAt, &tag, sizeof tag);
    cpu.ret(0);
}

// The level routine's reads: the objects tagged, the getter answering by tag and ID from the drawn parameters
void hookReads(MixSides& s) {
    s.fw.cpu.hook(kPointOscAt, [](Sh4& cpu) { mixTagged(cpu, kOscTag + kZoneTag * cpu.r[5] + cpu.r[6]); });
    s.fw.cpu.hook(kPointPartAt, [](Sh4& cpu) { mixTagged(cpu, kPartTag + cpu.r[5]); });
    s.fw.cpu.hook(kGetterAt, [&s](Sh4& cpu) {
        const auto read = std::ranges::find_if(kReads, [&](const MixRead& r) {
            return r.tag == cpu.r[4] && r.id == cpu.r[5];
        });
        s.stray = s.stray || read == std::end(kReads);
        cpu.ret(read == std::end(kReads) ? 0 : (uint32_t)read->answer(s.params));
    });
}
} // namespace

int main(int argc, char** argv) {
    Vc2Fw fw;
    const auto run = vc2Start(argc, argv, fw, "xt_vc2mixer", vc2LoadMixer);
    if (!run) return 1;
    Vc2PartMixer mixer;
    MixDraws draws;
    MixPortParts parts(mixer, draws);
    std::vector<MixPortVoice> ports;
    ports.reserve(kMixVoices);
    for (int voice = 0; voice < kMixVoices; ++voice) ports.emplace_back(voice, parts, draws);
    std::array<Vc2MixerVoice*, kMixVoices> voices {};
    std::ranges::transform(ports, voices.begin(), [](MixPortVoice& v) { return &v; });
    std::vector<MixCall> rom;
    MixSides s { fw, mixer, parts, voices, draws, rom, {}, false, {}, {}, {} };
    mixInstallRom(fw, draws, rom);
    hookReads(s);
    fw.cpu.write(kInputPointerAt, &kMixInputAt, sizeof kMixInputAt);
    mixer.w.inputAt = kMixInputAt;
    Vc2Tally tally { "XT_VC2MIXER_FIRST", "XT_VC2MIXER_STOP" };
    vc2RunCases(*run, tally, mixDifference(s), s.n.blocks,
                [&](std::mt19937& rng, bool) { return mixCase(s, rng); });
    const MixCounts& n = s.n;
    std::printf("mixer: %d cases, %ld blocks (%ld EXTIN, %ld bypassed), %ld carriers, %ld renders, %ld vocoder calls, "
                "%ld events, %ld different\n", run->cases, n.blocks, n.extin, n.bypassed, n.carriers, n.renders,
                n.vocoders, n.events, tally.different);
    const long tasks = vc2MixerTasksPart(fw, mixer, rom, *run);
    std::printf("xt_vc2mixer: %ld different\n", tally.different + tasks);
    return 0;
}
#endif
