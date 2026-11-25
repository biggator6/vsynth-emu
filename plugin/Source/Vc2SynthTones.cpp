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
    for (uint32_t at = s_.lists.sounding; at != 0 && n < voices_.size(); at = s_.tones.at(at).head.next) {
        voices_[n++] = toneAt(at);
    }
    return n;
}

Vc2SynthTones::Tone* Vc2SynthTones::toneAt(uint32_t at) {
    return at == 0 ? nullptr : &tones_[(size_t)s_.tones.indexOf(at)];
}

Vc2MixerTone* Vc2SynthTones::first() { return toneAt(s_.lists.sounding); }

Vc2MixerTone* Vc2SynthTones::next(Vc2MixerTone* tone) { return toneAt(static_cast<Tone*>(tone)->tone().head.next); }

std::span<Vc2MixerVoice* const> Vc2SynthTones::voices() { return { voices_.data(), walk() }; }
