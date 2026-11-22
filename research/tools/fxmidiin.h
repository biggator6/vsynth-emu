// fxmidiin.h: fxplay's MIDI side shared by its parts: an endpoint's name, and `--in`, which prints each SysEx a
// MIDI input sends, for vc2capture.py --learn (the XT's own DT1s, sent as its panel is edited with Transmit Edit
// Data on, name each address by the map the runs use).
#pragma once
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

namespace fxmidi {
inline std::string nameOf(MIDIObjectRef o) {
    CFStringRef s = nullptr; char c[256] = "";
    if (MIDIObjectGetStringProperty(o, kMIDIPropertyDisplayName, &s) == noErr && s) { CFStringGetCString(s, c, sizeof c, kCFStringEncodingUTF8); CFRelease(s); }
    return c;
}

// --in: each SysEx heard, F0 to F7 across packets, as hex with its time; real-time bytes between are skipped.
// CoreMIDI's thread owns it while the port is open; the main thread only waits.
struct Listener {
    std::vector<uint8_t> sysex;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
};

inline void heard(const MIDIPacketList* list, void* ref, void*) {
    Listener& l = *static_cast<Listener*>(ref);
    const MIDIPacket* p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; i++, p = MIDIPacketNext(p))
        for (UInt16 k = 0; k < p->length; k++) {
            const uint8_t c = p->data[k];
            if (c == 0xf0)
                l.sysex.assign(1, c);
            else if (!l.sysex.empty() && c < 0xf8)
                l.sysex.push_back(c);
            if (c != 0xf7 || l.sysex.empty())
                continue;
            const std::chrono::duration<double> t = std::chrono::steady_clock::now() - l.start;
            std::printf("%9.3f s:", t.count());
            for (const uint8_t b : l.sysex)
                std::printf(" %02x", b);
            std::printf("\n");
            std::fflush(stdout);
            l.sysex.clear();
        }
}

inline int dumpIn(MIDIClientRef client, const char* name, double seconds) {
    MIDIEndpointRef src = 0;
    for (ItemCount i = 0; i < MIDIGetNumberOfSources() && !src; i++)
        if (nameOf(MIDIGetSource(i)).find(name) != std::string::npos)
            src = MIDIGetSource(i);
    Listener l;
    MIDIPortRef in = 0;
    if (!src || MIDIInputPortCreate(client, CFSTR("in"), heard, &l, &in) != noErr) {
        std::fprintf(stderr, "no MIDI input named \"%s\"; the inputs:\n", name);
        for (ItemCount i = 0; i < MIDIGetNumberOfSources(); i++)
            std::fprintf(stderr, "  %s\n", nameOf(MIDIGetSource(i)).c_str());
        return 1;
    }
    MIDIPortConnectSource(in, src, nullptr);
    std::printf("listening to %s for %.0f s\n", nameOf(src).c_str(), seconds);
    std::fflush(stdout);
    CFRunLoopRunInMode(kCFRunLoopDefaultMode, seconds, false);
    MIDIPortDispose(in);
    return 0;
}
} // namespace fxmidi
