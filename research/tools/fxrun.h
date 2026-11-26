// fxrun.h: a run as fxplay reads it, fxcapture.py's and vc2capture.py's files: type 0, 500 ticks per quarter, 500000 us
// per quarter, so 1 ms a tick.  Its events (SysEx, notes, and the VC-2's Bank Select and Program Change, which pick a
// patch from the card: vc2cardrun.py) and its markers (FF 06): an input's cues ("probe:<seconds>", fxaudio.h) and what
// the user sets at the panel while the run waits ("hand:<words>", vc2writer.py).
#pragma once
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

#include "fxaudio.h"

namespace fxrun {
struct Event {
    uint32_t ms;
    std::vector<uint8_t> bytes;
};
using Cues = std::vector<std::pair<uint32_t, std::string>>;
constexpr uint32_t kSysExMs = 20, kLoadMs = 1500;   // the XT's own packets' interval; a patch's load (vc2cardrun.py)

// A channel message's kind, by its status's high nibble
inline uint8_t messageKind(const Event& e) {
    return e.bytes[0] & 0xf0;
}

// What a part of a run skipped by --from still sends, in order: its settings (SysEx, a patch's Bank Select and Program
// Change), not its notes
inline bool isSetting(const Event& e) {
    return e.bytes[0] == 0xf0 || messageKind(e) == 0xb0 || messageKind(e) == 0xc0;
}

// The pause after a setting sent while skipping: a SysEx's, or a patch's load after its Program Change
inline uint32_t pauseAfter(const Event& e) {
    return messageKind(e) == 0xc0 ? kLoadMs : kSysExMs;
}

// A "hand:<words>" cue said as what to set at the XT's panel now, while the run waits for it; another says nothing
inline void showHand(const std::string& cue) {
    if (cue.rfind("hand:", 0) != 0) return;
    std::printf("\n>>> by hand at the XT's panel, now: %s\n\n", cue.c_str() + 5);
    std::fflush(stdout);
}

// A variable-length number at b[i], i past it
inline uint32_t varAt(const std::vector<uint8_t>& b, size_t& i) {
    uint32_t v = 0;
    while (i < b.size()) {
        const uint8_t c = b[i++];
        v = v << 7 | (c & 127);
        if (!(c & 128)) break;
    }
    return v;
}

// The channel message whose status is b[i - 1]: a note's two data bytes, a Control Change's two, a Program Change's
// one, i past them; false for another kind
inline bool readChannelMessage(const std::vector<uint8_t>& b, size_t& i, uint32_t t, std::vector<Event>& out) {
    const uint8_t kind = b[i - 1] & 0xf0;
    const size_t n = kind == 0xc0 ? 1 : 2;
    if (kind != 0x80 && kind != 0x90 && kind != 0xb0 && kind != 0xc0) return false;
    out.push_back({ t, std::vector<uint8_t>(b.begin() + i - 1, b.begin() + i + n) });
    i += n;
    return true;
}

// A run's events; its markers (FF 06) into `cues` when asked for.  A run's file holds no real-time bytes (0xF8 on), so
// a status is a channel message's by its high nibble, and anything else is refused.
inline bool readRun(const char* path, std::vector<Event>& out, Cues* cues = nullptr) {
    std::vector<uint8_t> b;
    if (!fxaudio::bytesOf(path, b)) return false;
    const bool head = b.size() >= 22 && !std::memcmp(b.data(), "MThd", 4) && !std::memcmp(b.data() + 14, "MTrk", 4);
    if (!head || b[12] != 0x01 || b[13] != 0xf4) {
        std::fprintf(stderr, "%s is not a run from fxcapture.py\n", path);
        return false;
    }
    size_t i = 22;
    uint32_t t = 0;
    while (i < b.size()) {
        t += varAt(b, i);
        const uint8_t c = b[i++];
        if (c == 0xff) {
            const uint8_t kind = b[i++];
            const uint32_t ln = varAt(b, i);
            if (kind == 0x06 && cues) cues->push_back({ t, std::string(b.begin() + i, b.begin() + i + ln) });
            i += ln;
            if (kind == 0x2f) break;
        } else if (c == 0xf0) {
            const uint32_t ln = varAt(b, i);
            Event e { t, { 0xf0 } };
            e.bytes.insert(e.bytes.end(), b.begin() + i, b.begin() + i + ln);
            i += ln;
            out.push_back(std::move(e));
        } else if (c >= 0xf8) {
            std::fprintf(stderr, "a real-time byte %02x at %u ms: not a run from fxcapture.py\n", c, t);
            return false;
        } else if (!readChannelMessage(b, i, t, out)) {
            std::fprintf(stderr, "unexpected event %02x at %u ms\n", c, t);
            return false;
        }
    }
    return true;
}
} // namespace fxrun
