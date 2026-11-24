// The part's sounding tones as the part mixer takes them: see Vc2SynthTones.h (v351).  Every address is rom.4's.
#include "Vc2SynthTones.h"
#include "Vc2Synth.h"

Vc2SynthTones::Vc2SynthTones(Vc2Synth& s) : s_(s) {
    tones_.reserve(Vc2TonePool::kTones);
    for (int k = 0; k < Vc2TonePool::kTones; ++k) tones_.emplace_back(s, k);
}

Vc2Tone& Vc2SynthTones::Tone::tone() { return s_.tones.tones[(size_t)index_]; }

Vc2SynthTone& Vc2SynthTones::Tone::host() { return s_.hosts[(size_t)index_]; }

void Vc2SynthTones::Tone::update(int32_t n) { tone().update(n, host()); }

void Vc2SynthTones::Tone::clock(float seconds) { tone().clock(seconds, host()); }

void Vc2SynthTones::Tone::count(uint32_t x) { tone().count(x, host()); }

bool Vc2SynthTones::Tone::carrier() { return tone().sounds(host()); }

void Vc2SynthTones::Tone::renderCarrier(Vc2MixerFrame* frames) {
    tone().renderWhole(&frames->left, s_.buffers, host());
}

bool Vc2SynthTones::Tone::held() { return tone().held(host()); }

void Vc2SynthTones::Tone::render(Vc2MixerFrame* frames) { tone().render(&frames->left, s_.buffers, host()); }

size_t Vc2SynthTones::walk() {
    size_t n = 0;
    for (uint32_t at = s_.lists.sounding; at != 0 && n < tasks_.size(); at = s_.tones.at(at).head.next) {
        Tone& t = tones_[(size_t)s_.tones.indexOf(at)];
        tasks_[n] = &t;
        voices_[n++] = &t;
    }
    return n;
}

std::span<Vc2MixerTone* const> Vc2SynthTones::tasksTones() { return { tasks_.data(), walk() }; }

std::span<Vc2MixerVoice* const> Vc2SynthTones::voices() { return { voices_.data(), walk() }; }
