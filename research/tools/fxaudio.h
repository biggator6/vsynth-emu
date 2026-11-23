// fxaudio.h: fxplay's input player, for captures that feed the XT a known signal (the VC-2's, vc2capture.py): a
// WAV's first seconds played at each of a run's cues (a marker "probe:<seconds>"), through an AudioQueue on a named
// output device, on one clock with the run's MIDI; or rendered offline into samples, so a test needs no hardware.
// Also the file reader fxplay's run reader uses.
#pragma once
#include <AudioToolbox/AudioToolbox.h>
#include <CoreAudio/CoreAudio.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach_time.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <utility>
#include <vector>

namespace fxaudio {
constexpr double kRate = 44100.0;
constexpr uint32_t kFrames = 4096;                 // a buffer's frames: 93 ms

struct Cue {
    uint64_t at, frames;                           // on the audio's timeline, in frames
};
struct Player {
    std::vector<float> wav;                        // the input, interleaved stereo, full scale 1
    std::vector<Cue> cues;                         // in order, none overlapping
    uint64_t pos = 0;                              // the next frame the queue asks for
    uint64_t end = 0;                              // the last cue's end
    size_t next = 0;                               // the first cue not yet over
};

// A whole file's bytes from disk; false, saying so, when it can't be read.
inline bool bytesOf(const char* path, std::vector<uint8_t>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) {
        std::fprintf(stderr, "cannot open %s\n", path);
        return false;
    }
    uint8_t buf[65536];
    for (size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0; )
        out.insert(out.end(), buf, buf + n);
    std::fclose(f);
    return true;
}

inline uint32_t le(const std::vector<uint8_t>& b, size_t i, int bytes) {
    uint32_t v = 0;
    for (int k = bytes - 1; k >= 0; k--)
        v = v << 8 | b[i + k];
    return v;
}

// One sample of 16 or 24 bits as a float, full scale 1.
inline float sampleAt(const uint8_t* p, uint32_t bits) {
    const uint32_t raw = bits == 16 ? uint32_t(p[0] << 8 | p[1] << 16) : uint32_t(p[0] | p[1] << 8 | p[2] << 16);
    return float(int32_t(raw << 8) >> 8) / 8388608.0f;
}

// A 16- or 24-bit PCM WAV, stereo, at 44.1 kHz, as floats; false, saying why, for anything else.
inline bool readWav(const char* path, std::vector<float>& out) {
    std::vector<uint8_t> b;
    if (!bytesOf(path, b))
        return false;
    if (b.size() < 12 || std::memcmp(b.data(), "RIFF", 4) || std::memcmp(b.data() + 8, "WAVE", 4)) {
        std::fprintf(stderr, "%s is not a WAV\n", path);
        return false;
    }
    uint32_t ch = 0, rate = 0, bits = 0;
    for (size_t i = 12; i + 8 <= b.size(); ) {
        const uint32_t len = le(b, i + 4, 4);
        if (!std::memcmp(b.data() + i, "fmt ", 4)) {
            ch = le(b, i + 10, 2);
            rate = le(b, i + 12, 4);
            bits = le(b, i + 22, 2);
        }
        if (!std::memcmp(b.data() + i, "data", 4)) {
            if (ch != 2 || rate != 44100 || (bits != 16 && bits != 24)) {
                std::fprintf(stderr, "%s: fxplay wants stereo 16 or 24-bit at 44.1 kHz\n", path);
                return false;
            }
            const size_t w = bits / 8;
            const size_t count = std::min<size_t>(len, b.size() - i - 8) / w;
            for (size_t k = 0; k < count; k++)
                out.push_back(sampleAt(b.data() + i + 8 + k * w, bits));
            return true;
        }
        i += 8 + len + (len & 1);
    }
    std::fprintf(stderr, "%s has no data\n", path);
    return false;
}

// Each cue "probe:<seconds>" at ms (from the run's start, less fromMs), `lead` seconds into the audio's timeline.
// False, saying which, for a cue whose seconds don't read as a number.
inline bool schedule(Player& p, const std::vector<std::pair<uint32_t, std::string>>& cues, uint32_t fromMs,
                     double lead) {
    const uint64_t have = p.wav.size() / 2;
    for (const auto& [ms, text] : cues) {
        if (ms < fromMs || text.rfind("probe:", 0) != 0)
            continue;
        char* stop = nullptr;
        const double seconds = std::strtod(text.c_str() + 6, &stop);
        if (stop == text.c_str() + 6 || *stop || seconds <= 0) {
            std::fprintf(stderr, "a cue at %u ms reads \"%s\", not probe:<seconds>\n", ms, text.c_str());
            return false;
        }
        const uint64_t frames = std::min<uint64_t>(have, uint64_t(seconds * kRate + 0.5));
        p.cues.push_back({ uint64_t((lead + (ms - fromMs) / 1000.0) * kRate + 0.5), frames });
        p.end = p.cues.back().at + frames;
    }
    return true;
}

inline void fill(Player& p, float* out, uint32_t n) {
    for (uint32_t i = 0; i < n; i++, p.pos++) {
        while (p.next < p.cues.size() && p.pos >= p.cues[p.next].at + p.cues[p.next].frames)
            p.next++;
        const bool on = p.next < p.cues.size() && p.pos >= p.cues[p.next].at;
        const uint64_t k = on ? p.pos - p.cues[p.next].at : 0;
        out[2 * i] = on ? p.wav[2 * k] : 0.0f;
        out[2 * i + 1] = on ? p.wav[2 * k + 1] : 0.0f;
    }
}

inline void refill(void* user, AudioQueueRef q, AudioQueueBufferRef b) {
    fill(*static_cast<Player*>(user), static_cast<float*>(b->mAudioData), kFrames);
    b->mAudioDataByteSize = kFrames * 8;
    AudioQueueEnqueueBuffer(q, b, 0, nullptr);
}

inline AudioStreamBasicDescription format() {
    AudioStreamBasicDescription f{};
    f.mSampleRate = kRate;
    f.mFormatID = kAudioFormatLinearPCM;
    f.mFormatFlags = kLinearPCMFormatFlagIsFloat | kAudioFormatFlagIsPacked;
    f.mBytesPerPacket = 8;
    f.mBytesPerFrame = 8;
    f.mFramesPerPacket = 1;
    f.mChannelsPerFrame = 2;
    f.mBitsPerChannel = 32;
    return f;
}

// A device's string property (its name, its UID), released.
inline std::string deviceString(AudioDeviceID d, AudioObjectPropertySelector which) {
    AudioObjectPropertyAddress a{ which, kAudioObjectPropertyScopeGlobal, kAudioObjectPropertyElementMain };
    CFStringRef s = nullptr;
    UInt32 size = sizeof s;
    char c[256] = "";
    if (AudioObjectGetPropertyData(d, &a, 0, nullptr, &size, &s) == noErr && s) {
        CFStringGetCString(s, c, sizeof c, kCFStringEncodingUTF8);
        CFRelease(s);
    }
    return c;
}

// The output devices: (name, UID).
inline std::vector<std::pair<std::string, std::string>> outputs() {
    std::vector<std::pair<std::string, std::string>> out;
    AudioObjectPropertyAddress a{ kAudioHardwarePropertyDevices, kAudioObjectPropertyScopeGlobal,
                                  kAudioObjectPropertyElementMain };
    UInt32 size = 0;
    if (AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &a, 0, nullptr, &size) != noErr)
        return out;
    std::vector<AudioDeviceID> ids(size / sizeof(AudioDeviceID));
    AudioObjectGetPropertyData(kAudioObjectSystemObject, &a, 0, nullptr, &size, ids.data());
    for (const AudioDeviceID d : ids) {
        AudioObjectPropertyAddress s{ kAudioDevicePropertyStreams, kAudioObjectPropertyScopeOutput,
                                      kAudioObjectPropertyElementMain };
        UInt32 n = 0;
        if (AudioObjectGetPropertyDataSize(d, &s, 0, nullptr, &n) != noErr || n == 0)
            continue;
        out.push_back({ deviceString(d, kAudioObjectPropertyName), deviceString(d, kAudioDevicePropertyDeviceUID) });
    }
    return out;
}

// Puts q on the output whose name holds `device`; false, saying so, when there is none.
inline bool onDevice(AudioQueueRef q, const std::string& device) {
    std::string uid;
    for (const auto& [name, id] : outputs())
        if (uid.empty() && name.find(device) != std::string::npos)
            uid = id;
    if (uid.empty()) {
        std::fprintf(stderr, "no audio output named \"%s\"; fxplay --audio-devices lists them\n", device.c_str());
        return false;
    }
    CFStringRef s = CFStringCreateWithCString(nullptr, uid.c_str(), kCFStringEncodingUTF8);
    const OSStatus st = AudioQueueSetProperty(q, kAudioQueueProperty_CurrentDevice, &s, sizeof s);
    CFRelease(s);
    return st == noErr;
}

// A queue playing p on the output whose name holds `device` (the default output when empty), primed, not started.
inline AudioQueueRef open(Player& p, const std::string& device, bool offline) {
    const AudioStreamBasicDescription f = format();
    AudioQueueRef q = nullptr;
    CFRunLoopRef loop = offline ? CFRunLoopGetCurrent() : nullptr;
    if (AudioQueueNewOutput(&f, refill, &p, loop, kCFRunLoopCommonModes, 0, &q) != noErr)
        return nullptr;
    if (!device.empty() && !onDevice(q, device)) {
        AudioQueueDispose(q, true);
        return nullptr;
    }
    if (offline) {
        AudioChannelLayout l{};
        l.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
        AudioQueueSetOfflineRenderFormat(q, &f, &l);
    }
    for (int k = 0; k < 3; k++) {
        AudioQueueBufferRef b = nullptr;
        AudioQueueAllocateBuffer(q, kFrames * 8, &b);
        refill(&p, q, b);
    }
    return q;
}

// Starts q, and returns the host time (ns, by tb) its frame `at` plays at: the clock the run's MIDI goes by.
inline uint64_t start(AudioQueueRef q, double at, const mach_timebase_info_data_t& tb) {
    AudioQueueStart(q, nullptr);
    AudioTimeStamp t{};
    for (int k = 0; k < 400; k++) {
        if (AudioQueueGetCurrentTime(q, nullptr, &t, nullptr) == noErr && (t.mFlags & kAudioTimeStampHostTimeValid)
            && t.mSampleTime > 0)
            break;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.005, true);
    }
    const double host = double(t.mHostTime) * tb.numer / tb.denom;
    return uint64_t(host + (at - t.mSampleTime / kRate) * 1e9);
}

// The whole schedule rendered offline, as 24-bit values: the input's own, for a test against them.
inline bool render(Player& p, std::vector<int32_t>& out) {
    AudioQueueRef q = open(p, "", true);
    if (!q || AudioQueueStart(q, nullptr) != noErr)
        return false;
    AudioQueueBufferRef b = nullptr;
    AudioQueueAllocateBuffer(q, kFrames * 8, &b);
    AudioTimeStamp t{};
    t.mFlags = kAudioTimeStampSampleTimeValid;
    AudioQueueOfflineRender(q, &t, b, 0);
    bool ok = true;
    for (uint64_t done = 0; ok && done < p.end; done += kFrames) {
        t.mSampleTime = double(done);
        ok = AudioQueueOfflineRender(q, &t, b, kFrames) == noErr;
        const float* s = static_cast<const float*>(b->mAudioData);
        for (uint32_t i = 0; ok && i < b->mAudioDataByteSize / 4; i++)
            out.push_back(int32_t(std::lround(double(s[i]) * 8388608.0)));
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, true);
    }
    AudioQueueDispose(q, true);
    return ok;
}
} // namespace fxaudio
