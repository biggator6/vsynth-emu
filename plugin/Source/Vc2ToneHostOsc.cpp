// A VC-2 tone's parts' host, its two OSC voices: see Vc2ToneHost.h (v319).  Every call the tone makes on an OSC voice is
// the voice's own (Vc2OscVoice, v230…v267) on its link, the words the tone reads the voice's; a link answers the voice's
// address and the tone's own envelopes' and LFOs' words, and passes every other call to the deriving class.
#include "Vc2ToneHost.h"

namespace {
// [FW] where the voices lie in the tone; the voice's words the tone reads besides its kind, flag, wave and getters: the
// pitch ratio its COSMs' set-up and COSM 1's update take (+0x1cc) and the byte the set-up takes (+0x58)
constexpr std::array<uint32_t, 2> kOscsAt { 0x20, 0xba4 };
constexpr uint32_t kRatio = 0x1cc, kByte = 0x58;
constexpr size_t kEnvelopeBinds = 4;   // [FW] +0xa4c…+0xa58 the envelopes', +0xa5c…+0xa68 the LFOs'
} // namespace

void Vc2ToneHost::updateOsc(int osc, int32_t n) { voice(osc).update(n, link(osc), c_.shared); }
float Vc2ToneHost::oscValue(int osc) { return voice(osc).f32(kRatio); }

// 0x893ae9f0; kind 3's routine's answer, which the tone drops
void Vc2ToneHost::renderOsc(int osc, float* buffer) { voice(osc).render(buffer, link(osc)); }

void Vc2ToneHost::countOsc(int osc, uint32_t x) { voice(osc).count(x); }
void Vc2ToneHost::countOscMatched(int osc, uint32_t x) { voice(osc).countMatched(x); }
void Vc2ToneHost::installOsc(int osc) { voice(osc).install(link(osc), c_.shared); }
int32_t Vc2ToneHost::oscKind(int osc) { return (int32_t)voice(osc).kind(); }
uint32_t Vc2ToneHost::oscFlag(int osc) { return voice(osc).tail.flag; }

// The bound parts' rom.4 addresses, the four envelopes then the four LFOs, as the tone's note-on writes them; then the
// binding's flags
void Vc2ToneHost::bindOsc(int osc, const Vc2ToneBinding<4>& b) {
    auto& binds = voice(osc).tail.binds;
    for (size_t k = 0; k < kEnvelopeBinds; ++k) {
        binds[k] = envelopeAt(b.envelopes[k]);
        binds[kEnvelopeBinds + k] = lfoAt(b.lfos[k]);
    }
    bindFlags(b.envelopes, b.lfos);
}

uint32_t Vc2ToneHost::oscWave(int osc) { return voice(osc).u32(Vc2OscVoice::kWave); }
void Vc2ToneHost::setOscWave(int osc, uint32_t wave) { voice(osc).setU32(Vc2OscVoice::kWave, wave); }

void Vc2ToneHost::noteOnOsc(int osc, const uint32_t* block, bool legato) {
    voice(osc).noteOn(block, legato ? 1u : 0u, link(osc), c_.shared);
}

// 0x893aee20, given that tone's voice of the same OSC, its object (+0x15c)
void Vc2ToneHost::takePlayer(int osc, int tone) { voice(osc).take(hostOf(tone).oscs[(size_t)osc]); }

void Vc2ToneHost::setOsc(int osc, const uint32_t* block) { voice(osc).start(block, link(osc), c_.shared); }

Vc2ToneCosmSetUp Vc2ToneHost::cosmSetUp(int osc) {
    const Vc2OscVoice& v = voice(osc);
    return { v.f32(kRatio), v.tail.hz, v.u8(kByte) };
}

void Vc2ToneHost::settingsOsc(int osc, const uint32_t* block) { voice(osc).settings(block, link(osc), c_.shared); }
void Vc2ToneHost::legatoOsc(int osc, const uint32_t* ramp) { voice(osc).legato(ramp); }
void Vc2ToneHost::keyOffOsc(int osc) { voice(osc).vibrato.w.active = 0; }   // [FW] 0x893b2b60: the vibrato's +0x34
float Vc2ToneHost::oscCost(int osc) { return voice(osc).tail.installed; }   // [FW] +0xa88, kinds 4…7's install's
int32_t Vc2ToneHost::oscWord168(int osc) { return voice(osc).word168(); }
uint32_t Vc2ToneHost::oscPosition(int osc) { return voice(osc).position(); }

Vc2OscParts& Vc2ToneOscLink::outside() const { return host_.oscOutside(osc_); }

uint32_t Vc2ToneOscLink::renderBy(uint32_t routine, float* out, uint32_t third) {
    return outside().renderBy(routine, out, third);
}

void Vc2ToneOscLink::seek(uint32_t routine, int32_t at) { outside().seek(routine, at); }
int32_t Vc2ToneOscLink::osClock() { return outside().osClock(); }
Vc2MicRing& Vc2ToneOscLink::micRing() { return outside().micRing(); }
const float* Vc2ToneOscLink::unvoiced() { return outside().unvoiced(); }
float Vc2ToneOscLink::inputPeak() { return outside().inputPeak(); }
const int32_t* Vc2ToneOscLink::inputFrames() { return outside().inputFrames(); }
Vc2OscSamples Vc2ToneOscLink::waveSamples() { return outside().waveSamples(); }

// A word of a pool tone's OSC voice among its own, that tone's host's (v328: the records an object's head names, the
// voice's own or, after a legato's take, the earlier tone's voice's); a bound part in the tone, its envelopes' and
// LFOs', the host's; any other address, a wave's record, outside it
float Vc2ToneOscLink::boundFloat(uint32_t part, uint32_t offset) {
    const uint32_t self = host_.self(), at = part + offset;
    const Vc2TonePool& pool = host_.c_.pool;
    for (size_t osc = 0; pool.holds(at) && osc < kOscsAt.size(); ++osc) {
        const uint32_t in = at - pool.addressOf(pool.indexOf(at)) - kOscsAt[osc];
        if (in - Vc2OscVoice::kOwn <= Vc2OscVoice::kOwnEnd - Vc2OscVoice::kOwn - sizeof(float)) {
            return host_.hostOf(pool.indexOf(at)).oscs[osc].f32(in);
        }
    }
    if (part - self < Vc2Tone::kSize) return host_.mods.boundFloat(self, part, offset);
    return outside().boundFloat(part, offset);
}

uint32_t Vc2ToneOscLink::address() { return host_.self() + kOscsAt[(size_t)osc_]; }
void Vc2ToneOscLink::objectCall(uint32_t routine) { outside().objectCall(routine); }
Vc2OscWaveHeader Vc2ToneOscLink::waveHeader(uint32_t at) { return outside().waveHeader(at); }
Vc2OscLoopRecord Vc2ToneOscLink::loopRecord(uint32_t at) { return outside().loopRecord(at); }
