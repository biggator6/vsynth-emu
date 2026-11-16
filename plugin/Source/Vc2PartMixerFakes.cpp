// xt_vc2mixer's fakes: see Vc2PartMixerCheck.h (v227).  XT_VC2MIXER_MAIN only.
#ifdef XT_VC2MIXER_MAIN
#include "Vc2PartMixerCheck.h"
#include <algorithm>
#include <cfenv>
#include <utility>

namespace {
// [FW] what the mixer calls
constexpr uint32_t kInputStageAt = 0x893ebb00;    // the input stage
constexpr uint32_t kRingWriteAt = 0x893ec820;     // kind 6's ring
constexpr uint32_t kUnvoicedAt = 0x893ed040;      // the unvoiced detector
constexpr uint32_t kDetectorRingAt = 0x893ed410;  // where the detector's ring takes the block
constexpr uint32_t kVocoderAt = 0x893f0580;       // the vocoder's stereo driver
constexpr uint32_t kTrackerResetAt = 0x893ee040;  // the reset's three
constexpr uint32_t kUnvoicedResetAt = 0x893ecf60;
constexpr uint32_t kRingResetAt = 0x893ec7d0;
constexpr uint32_t kVoiceListAt = 0x899996d0;     // the part's voices
// The calls that only log themselves, and whether each takes a buffer (in r4)
struct MixLogged {
    uint32_t at;
    MixKind kind;
    bool buffer;
};
constexpr MixLogged kLogged[] = {
    { kInputStageAt, kInputStage, true }, { kRingWriteAt, kMicRing, true }, { kUnvoicedAt, kUnvoiced, true },
    { kTrackerResetAt, kResetTracker, false }, { kUnvoicedResetAt, kResetUnvoiced, false },
    { kRingResetAt, kResetRing, false },
};
// [FW] the mixer's buffers, rom.4's addresses
constexpr std::pair<uint32_t, MixArg> kRomBuffers[] = {
    { 0x8999f450, kSecondBuffer }, { 0x8999f3d0, kMicBuffer }, { kMixRingAt, kRingBuffer },
    { 0x8999f2d0, kCarrierBuffer }, { 0x8999f1d0, kDirectBuffer },
};
// [FW] a voice's methods the mixer calls, by their slot in its table, each at a hooked address of its own
constexpr uint32_t kSlots[] = { 0x54, 0x18, 0x60, 0x24 };   // carrier, renderCarrier, held, render
constexpr uint32_t kTableAt = 0x18, kMethodStride = 0x10;
constexpr float kVocoderKeeps = 0.5f;
constexpr uint32_t kSpecialOneIn = 48;   // a special word, one draw in so many

int32_t romBuffer(uint32_t at) {
    const auto* found = std::ranges::find(kRomBuffers, at, &std::pair<uint32_t, MixArg>::first);
    return found == std::end(kRomBuffers) ? kOther : found->second;
}

int voiceOf(uint32_t node) { return (int)((node - kMixVoicesAt) / kMixVoiceStride); }

uint32_t methodAt(int k) { return kMixMethodsAt + kMethodStride * (uint32_t)k; }

// A buffer of frames in rom.4's RAM read, changed and written back
template <class Change>
void framesAt(Sh4& cpu, uint32_t at, Change change) {
    std::array<Vc2MixerFrame, Vc2PartMixer::kBlock> f {};
    cpu.read(at, f.data(), sizeof f);
    change(f.data());
    cpu.write(at, f.data(), sizeof f);
}

// A call hooked to log itself, with its first argument's buffer when it takes one, and return
void logged(Vc2Fw& fw, const MixLogged& call, std::vector<MixCall>& log) {
    fw.cpu.hook(call.at, [&log, call](Sh4& cpu) {
        log.push_back({ call.kind, -1, call.buffer ? romBuffer(cpu.r[4]) : kNoBuffer });
        cpu.ret(0);
    });
}

// A voice's question hooked: logged, answered from the draws by `answer`
template <class Answer>
void asked(Vc2Fw& fw, int k, MixKind kind, std::vector<MixCall>& log, Answer answer) {
    fw.cpu.hook(methodAt(k), [&log, kind, answer](Sh4& cpu) {
        const int voice = voiceOf(cpu.r[4]);
        log.push_back({ kind, voice, kNoBuffer });
        cpu.ret(answer(voice) ? 1 : 0);
    });
}

// A voice's render hooked: logged, the drawn frames added to the buffer it is given
void renders(Vc2Fw& fw, int k, MixKind kind, const MixDraws& draws, std::vector<MixCall>& log) {
    fw.cpu.hook(methodAt(k), [&draws, &log, kind](Sh4& cpu) {
        const int voice = voiceOf(cpu.r[4]);
        log.push_back({ kind, voice, romBuffer(cpu.r[5]) });
        framesAt(cpu, cpu.r[5], [&](Vc2MixerFrame* f) { mixAdd(f, draws.voice[(size_t)voice].frames); });
        cpu.ret(0);
    });
}
} // namespace

float mixDrawSample(std::mt19937& rng) {
    constexpr uint32_t kSpecials[] = { 0x7fc00000, 0xffc00000, 0x7f800000, 0xff800000, 0x7f800001 };
    if (rng() % kSpecialOneIn != 0) return kVc2Bus * (2.0f * vc2Unit(rng) - 1.0f);
    const uint32_t payload = rng() % 4 == 0 ? rng() & 0x3fffff : 0;
    return std::bit_cast<float>(kSpecials[rng() % std::size(kSpecials)] | payload);
}

void mixDrawFrames(std::array<Vc2MixerFrame, Vc2PartMixer::kBlock>& frames, std::mt19937& rng) {
    std::ranges::generate(frames, [&] { return Vc2MixerFrame { mixDrawSample(rng), mixDrawSample(rng) }; });
}

int32_t mixDrawWord(std::mt19937& rng) {
    const int32_t word = (int32_t)rng();
    if (rng() % kSpecialOneIn == 0) return word % 2 == 0 ? 0 : INT32_MIN;
    return rng() % 4 == 0 ? word >> (rng() % 24) : word;
}

std::string mixCallsDifference(const std::vector<MixCall>& rom, const std::vector<MixCall>& port) {
    const auto [a, b] = std::ranges::mismatch(rom, port);
    if (a == rom.end() && b == port.end()) return "";
    const auto said = [](const auto& at, const std::vector<MixCall>& log) {
        if (at == log.end()) return std::string("none");
        return std::to_string(at->kind) + "/" + std::to_string(at->voice) + "/" + std::to_string(at->arg);
    };
    return "the calls, call " + std::to_string(a - rom.begin()) + ": " + said(b, port) + ", against " + said(a, rom);
}

// The fakes' sums at round to nearest on both sides: rom.4's hooks run in the interpreter's call, at the SH-4's
// round to zero, and the port's calls at the host's
template <class Work>
void atNearest(Work work) {
    const int saved = std::fegetround();
    std::fesetround(FE_TONEAREST);
    work();
    std::fesetround(saved);
}

void mixAdd(Vc2MixerFrame* frames, const std::array<Vc2MixerFrame, Vc2PartMixer::kBlock>& added) {
    atNearest([&] {
        for (size_t i = 0; i < added.size(); ++i) {
            frames[i].left += added[i].left;
            frames[i].right += added[i].right;
        }
    });
}

void mixVocoder(Vc2MixerFrame* frames, const std::array<Vc2MixerFrame, Vc2PartMixer::kBlock>& added) {
    atNearest([&] {
        for (size_t i = 0; i < added.size(); ++i) {
            frames[i].left = frames[i].left * kVocoderKeeps + added[i].left;
            frames[i].right = frames[i].right * kVocoderKeeps + added[i].right;
        }
    });
}

int32_t MixPortParts::bufferOf(const void* p) const {
    const std::pair<const void*, MixArg> buffers[] = {
        { mixer_.b.second, kSecondBuffer }, { mixer_.b.mic, kMicBuffer }, { ring.data(), kRingBuffer },
        { mixer_.b.carrier, kCarrierBuffer }, { mixer_.b.direct, kDirectBuffer },
    };
    const auto* found = std::ranges::find(buffers, p, &std::pair<const void*, MixArg>::first);
    return found == std::end(buffers) ? kOther : found->second;
}

float* MixPortParts::detectorRing() {
    calls.push_back({ kDetectorRing, -1, kNoBuffer });
    return ring.data();
}

void MixPortParts::vocoder(const float* r, Vc2MixerFrame* frames) {
    calls.push_back({ kVocoder, bufferOf(r), bufferOf(frames) });
    mixVocoder(frames, draws_.vocoder);
}

bool MixPortVoice::answered(MixKind kind, bool answer) {
    parts_.calls.push_back({ kind, index_, kNoBuffer });
    return answer;
}

void MixPortVoice::rendered(MixKind kind, Vc2MixerFrame* frames) {
    parts_.calls.push_back({ kind, index_, parts_.bufferOf(frames) });
    mixAdd(frames, draw().frames);
}

void mixLinkVoices(Vc2Fw& fw, int count) {
    for (int voice = 0; voice < kMixVoices; ++voice) {
        const uint32_t node = kMixVoicesAt + kMixVoiceStride * (uint32_t)voice;
        const uint32_t next = voice + 1 < count ? node + kMixVoiceStride : 0;
        fw.cpu.write(node, &next, sizeof next);
        fw.cpu.write(node + kTableAt, &kMixVtableAt, sizeof kMixVtableAt);
    }
    const uint32_t head = count > 0 ? kMixVoicesAt : 0;
    fw.cpu.write(kVoiceListAt, &head, sizeof head);
}

void mixInstallRom(Vc2Fw& fw, const MixDraws& draws, std::vector<MixCall>& log) {
    for (int k = 0; k < (int)std::size(kSlots); ++k) {   // an entry: the this-delta, a word, the method
        const uint32_t entry[3] = { 0, 0, methodAt(k) };
        fw.cpu.write(kMixVtableAt + kSlots[k], entry, sizeof entry);
    }
    asked(fw, 0, kCarrier, log, [&draws](int voice) { return draws.voice[(size_t)voice].carrier; });
    asked(fw, 2, kHeld, log, [&draws](int voice) { return draws.voice[(size_t)voice].held; });
    renders(fw, 1, kRenderCarrier, draws, log);
    renders(fw, 3, kRender, draws, log);
    for (const MixLogged& call : kLogged) logged(fw, call, log);
    fw.cpu.hook(kDetectorRingAt, [&log](Sh4& cpu) {
        log.push_back({ kDetectorRing, -1, kNoBuffer });
        cpu.ret(kMixRingAt);
    });
    fw.cpu.hook(kVocoderAt, [&draws, &log](Sh4& cpu) {
        log.push_back({ kVocoder, romBuffer(cpu.r[5]), romBuffer(cpu.r[6]) });
        framesAt(cpu, cpu.r[6], [&](Vc2MixerFrame* f) { mixVocoder(f, draws.vocoder); });
        cpu.ret(0);
    });
}
#endif
