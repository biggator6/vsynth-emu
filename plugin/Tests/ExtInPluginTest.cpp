// VSynthEmuExtInTest's processor checks (ExtInTest.h; v207).  An EXT IN OSC on OSC 1 alone, with no wave, a note held,
// the host's input in the buffer's first channels, against XtSynth on the processor's controls given the input itself, by
// the law of HARDWARE_FIRMWARE.md ("The input with the Effects off": a frame late in the chip's 24 bits; STEREO L R,
// MONO MIX each side halved and summed, MONO L, MONO R), a block and a frame behind the processor's latency, sample for
// sample, so that nothing of the processor's way in is the reference's too:
// - the Effects off (the copy), each External In Type, a MIDI clock in each block making two spans of it; a mono input on
//   both sides; no input (the bus off), silent;
// - the Effects on (the chip: the block into its slots, its returned words through busOut into XtSynth::input, v204's
//   gap), each Type, against XtSynth on effects started as the processor's, which hand nothing back;
// - at 48 kHz, against XtSynth fed by an XtHostInput of its own (ExtInTest.cpp holds that to the sine);
// - host blocks of twice what the feed holds: rendered a span at a time, no block short, nothing overrun;
// - the layouts taken: stereo, mono and no input, not 5.1; hasInput with an input and without.
#include "ExtInTest.h"
#include "TestAgainstSynth.h"
#include "TestControls.h"
#include "TestFx.h"
#include "../Source/XtHostInput.h"
#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr double kChipRate = 44100.0;
constexpr int kBlock = 300;            // a host block that isn't a whole number of the bus's 32
constexpr int kBlocks = 60;            // 0.4 s at 44.1 kHz
constexpr int kClockAt = 77;           // a MIDI clock there in each block, which the processor's render goes round
constexpr int kNote = 60;
constexpr float kVelocity = 100.0f / 127.0f;
constexpr float kExtIn = 2.0f;         // OSC Type's EXT IN
constexpr int kTypes = 4;              // External In Type: STEREO L R, MONO MIX, MONO L, MONO R
constexpr int kExtInTypeId = 0x78;     // [FW] its ID, the effects driver's
constexpr long kBusLate = 33;          // a block and a frame: from the chip's slots, or the copy, to the EXT IN OSCs
constexpr long kAhead = 64;            // frames pushed into the reference past the block it renders

// What program A hands back of a frame, by Type, in the chip's 24 bits at the top of an int32
void law(int type, const int32_t* in, int32_t* out) {
    const int32_t l = in[0] >> 8, r = in[1] >> 8, mix = (l >> 1) + (r >> 1);
    const int32_t sides[kTypes][2] = { { l, r }, { mix, mix }, { l, l }, { r, r } };
    for (int side = 0; side < 2; ++side) out[side] = (int32_t)((uint32_t)sides[type][side] << 8);
}

struct Case {
    std::string name;
    int type = 1;
    bool effects = false;
    juce::AudioChannelSet input = juce::AudioChannelSet::stereo();
    double rate = kChipRate;
};

// The host's input as the processor should take it, the words of frame j: a mono input's left on both sides, none
// silence
void wordsOf(const Case& c, long j, int32_t* lr) {
    const bool there = !c.input.isDisabled();
    const int32_t l = there ? hostSample24(j, 0) : 0, r = c.input.size() > 1 ? hostSample24(j, 1) : l;
    lr[0] = (int32_t)((uint32_t)l << 8);
    lr[1] = (int32_t)((uint32_t)r << 8);
}

// The reference's two streams at 44.1 kHz, L then R a frame: what the chip's slots take (the host's words, the
// processor's latency late), and what the EXT IN OSCs hear (the law of those, kBusLate later)
struct Streams {
    std::vector<int32_t> slots, heard;
};
Streams streamsOf(const Case& c, long frames, long latency) {
    Streams s { std::vector<int32_t>((size_t)(2 * frames)), std::vector<int32_t>((size_t)(2 * frames)) };
    for (long n = latency; n < frames; ++n) wordsOf(c, n - latency, s.slots.data() + 2 * n);
    for (long n = kBusLate; n < frames; ++n) law(c.type, s.slots.data() + 2 * (n - kBusLate), s.heard.data() + 2 * n);
    return s;
}

// The reference's effects with nothing handed back: the test gives its EXT IN OSCs their input by the law itself
struct NoReturn final : XtBusSink {
    explicit NoReturn(XtFxUnit& u) : unit(u) {}
    void process(const float* frame, float* lr, int frames) override { unit.process(frame, lr, frames); }
    void pushInput(const int32_t* lr, int frames) override { unit.pushInput(lr, frames); }
    XtFxUnit& unit;
};

// The host's input into the buffer's first two channels, whatever its layout, from frame `from`
void fill(juce::AudioBuffer<float>& buf, long from) {
    for (int ch = 0; ch < std::min(2, buf.getNumChannels()); ++ch)
        for (int i = 0; i < buf.getNumSamples(); ++i) buf.setSample(ch, i, hostSample(from + i, ch));
}

struct Heard {
    float most = 0.0f;
    double energy = 0.0;
};

// The reference given its input: at 44.1 kHz the streams by hand, kAhead past the block; at another rate an
// XtHostInput of its own, the block's frames
struct RefInput {
    RefInput(const Case& c, XtSynth& ref, XtFxUnit* unit, long latency) : c_(c), ref_(ref), unit_(unit) {
        if (c.rate == kChipRate) streams_ = streamsOf(c, (long)kBlocks * kBlock + 2 * kAhead, latency);
        else {
            own_ = std::make_unique<XtHostInput>();
            own_->prepare(c.rate, ref.input);
            own_->setType(c.type);
        }
    }
    void block(long from) {
        if (own_) {
            std::vector<float> l((size_t)kBlock), r((size_t)kBlock);
            for (int i = 0; i < kBlock; ++i) {
                int32_t lr[2];
                wordsOf(c_, from + i, lr);
                l[(size_t)i] = (float)lr[0] * 0x1p-31f;
                r[(size_t)i] = (float)lr[1] * 0x1p-31f;
            }
            own_->push(l.data(), r.data(), kBlock);
            return;
        }
        const long to = std::min((long)streams_.heard.size() / 2, from + kBlock + kAhead);
        ref_.input.push(streams_.heard.data() + 2 * pushed_, (int)(to - pushed_));
        if (unit_) unit_->pushInput(streams_.slots.data() + 2 * pushed_, (int)(to - pushed_));
        pushed_ = to;
    }
    const Case& c_;
    XtSynth& ref_;
    XtFxUnit* unit_;
    Streams streams_;
    std::unique_ptr<XtHostInput> own_;
    long pushed_ = 0;
};

Heard play(TestChecks& check, const juce::File& rom, const Case& c) {
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    p.fxManualTicks();
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference(0) = c.input;
    check(p.setBusesLayout(layout), c.name + ": the layout should be taken");
    setParam(p, "oscType", kExtIn);
    setParam(p, "fxExtInType", (float)c.type);
    setParam(p, "fxOn", c.effects ? 1.0f : 0.0f);
    AgainstSynth both(p, c.rate, kBlock);
    std::unique_ptr<RefEffects> fx;
    std::unique_ptr<NoReturn> sink;
    if (c.effects) {
        p.fxTick();   // the processor's effects come up, its settings sent
        fx = std::make_unique<RefEffects>(rom.getFullPathName().toRawUTF8());
        fx->control().set(kExtInTypeId, c.type);
        fx->start(*both.ref);
        sink = std::make_unique<NoReturn>(*fx->unit);
        both.ref->setBusSink(sink.get());
        both.ref->useBusSink(true);
    }
    RefInput in(c, *both.ref, fx ? fx->unit.get() : nullptr, p.inputLatency());
    Heard h;
    for (int b = 0; b < kBlocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, kNote, (juce::uint8)100), 0);
        if (c.rate == kChipRate) midi.addEvent(juce::MidiMessage::midiClock(), kClockAt);
        if (fx) p.fxTick();
        const long from = (long)b * kBlock;
        both.block(
            midi,
            [&](XtSynth& ref) {
                if (fx) fx->tick(ref);
                in.block(from);
                if (b == 0) ref.noteOn(kNote, kVelocity);
            },
            [from](juce::AudioBuffer<float>& buf) { fill(buf, from); });
        for (int i = 0; i < kBlock; ++i) h.energy += (double)both.buf.getSample(0, i) * both.buf.getSample(0, i);
    }
    h.most = both.most;
    return h;
}

// A host block longer than the feed holds, at 44.1 kHz, twice its 4,096 frames: the processor renders it a span at a
// time, so no block of its own comes short and nothing overruns, and the EXT IN OSC sounds
void bigBlocks(TestChecks& check, const juce::File& rom) {
    constexpr int kBig = 8192, kBigBlocks = 4;
    VSynthEmuProcessor p;
    p.setRomFile(rom);
    p.fxManualTicks();
    setParam(p, "oscType", kExtIn);
    p.prepareToPlay(kChipRate, kBig);
    juce::AudioBuffer<float> buf(2, kBig);
    double energy = 0.0;
    for (int b = 0; b < kBigBlocks; ++b) {
        juce::MidiBuffer midi;
        if (b == 0) midi.addEvent(juce::MidiMessage::noteOn(1, kNote, (juce::uint8)100), 0);
        fill(buf, (long)b * kBig);
        p.processBlock(buf, midi);
        energy += (double)buf.getRMSLevel(0, 0, kBig);
    }
    check(p.inputFaults() == 0 && energy > 0.0, "host blocks of " + std::to_string(kBig) + " frames: " +
                                                    std::to_string(p.inputFaults()) + " short or overrun");
}

void layouts(TestChecks& check) {
    VSynthEmuProcessor p;
    const auto takes = [&p](const juce::AudioChannelSet& in) {
        auto layout = p.getBusesLayout();
        layout.inputBuses.getReference(0) = in;
        return p.checkBusesLayoutSupported(layout);
    };
    check(takes(juce::AudioChannelSet::stereo()) && takes(juce::AudioChannelSet::mono()) &&
              takes(juce::AudioChannelSet::disabled()),
          "the input should be taken stereo, mono or off");
    check(!takes(juce::AudioChannelSet::create5point1()), "and not 5.1");
    check(p.hasInput(), "a new processor should have its stereo input");
    auto layout = p.getBusesLayout();
    layout.inputBuses.getReference(0) = juce::AudioChannelSet::disabled();
    check(p.setBusesLayout(layout) && !p.hasInput(), "and none with the bus off");
}

}  // namespace

void extInPluginChecks(TestChecks& check, const juce::File& rom) {
    layouts(check);
    bigBlocks(check, rom);
    std::vector<Case> cases;
    const char* types[kTypes] = { "STEREO", "MONO MIX", "MONO L", "MONO R" };
    for (int effects = 0; effects < 2; ++effects)
        for (int type = 0; type < kTypes; ++type) {
            const std::string way = effects ? "Effects on, " : "Effects off, ";
            cases.push_back({ way + types[type], type, effects == 1 });
        }
    cases.push_back({ "a mono input, STEREO", 0, false, juce::AudioChannelSet::mono() });
    cases.push_back({ "48 kHz, MONO MIX", 1, false, juce::AudioChannelSet::stereo(), 48000.0 });
    for (const Case& c : cases) {
        const Heard h = play(check, rom, c);
        check(h.most == 0.0f && h.energy > 0.0,
              c.name + ": the processor should play what XtSynth does with the input by the law, off by " +
                  std::to_string(h.most) + ", energy " + std::to_string(h.energy));
    }
    const Heard none = play(check, rom, { "no input", 0, false, juce::AudioChannelSet::disabled() });
    check(none.most == 0.0f && none.energy == 0.0,
          "no input: silent, off by " + std::to_string(none.most) + ", energy " + std::to_string(none.energy));
}
