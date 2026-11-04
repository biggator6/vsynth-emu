// fxplay: plays a run from fxcapture.py to a MIDI output through CoreMIDI, so the capture needs no DAW to send SysEx (any recorder takes the audio).
//   fxplay                     lists the MIDI outputs
//   fxplay <file.mid> <name>   counts down 5 s, then plays it to the output whose name contains <name>; prints each second's progress
//   fxplay --from <seconds> <file.mid> <name>     starts part-way, sending the SysEx it skips first
//   fxplay --test <file.mid>   sends the file as fast as possible to a virtual output made here, and checks every byte arrives (no hardware)
//   fxplay [--from <seconds>] --listen <file.mid>    a virtual output named "fxplay listen" for another fxplay to play to, checking the bytes and the timing (no hardware)
// Takes only what fxcapture.py writes: type 0, 500 ticks per quarter, 500000 us per quarter, so 1 ms a tick.
// Build: clang++ -std=c++17 -O2 research/tools/fxplay.cpp -framework CoreMIDI -framework CoreFoundation -o analysis/test_files/fx/fxplay
#include <CoreFoundation/CoreFoundation.h>
#include <CoreMIDI/CoreMIDI.h>
#include <mach/mach_time.h>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

namespace {
struct Event { uint32_t ms; std::vector<uint8_t> bytes; };

bool readRun(const char* path, std::vector<Event>& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", path); return false; }
    std::vector<uint8_t> b; uint8_t buf[65536]; size_t n;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) b.insert(b.end(), buf, buf + n);
    std::fclose(f);
    if (b.size() < 22 || std::memcmp(b.data(), "MThd", 4) || std::memcmp(b.data() + 14, "MTrk", 4) || b[12] != 0x01 || b[13] != 0xf4) { std::fprintf(stderr, "%s is not a run from fxcapture.py\n", path); return false; }
    size_t i = 22; uint32_t t = 0;
    const auto var = [&] { uint32_t v = 0; while (i < b.size()) { const uint8_t c = b[i++]; v = v << 7 | (c & 127); if (!(c & 128)) break; } return v; };
    while (i < b.size()) {
        t += var();
        const uint8_t c = b[i++];
        if (c == 0xff) { const uint8_t kind = b[i++]; const uint32_t ln = var(); i += ln; if (kind == 0x2f) break; }
        else if (c == 0xf0) { const uint32_t ln = var(); Event e{ t, { 0xf0 } }; e.bytes.insert(e.bytes.end(), b.begin() + i, b.begin() + i + ln); i += ln; out.push_back(std::move(e)); }
        else if ((c & 0xf0) == 0x80 || (c & 0xf0) == 0x90) { out.push_back({ t, { c, b[i], b[i + 1] } }); i += 2; }
        else { std::fprintf(stderr, "unexpected event %02x at %u ms\n", c, t); return false; }
    }
    return true;
}
std::string nameOf(MIDIObjectRef o) {
    CFStringRef s = nullptr; char c[256] = "";
    if (MIDIObjectGetStringProperty(o, kMIDIPropertyDisplayName, &s) == noErr && s) { CFStringGetCString(s, c, sizeof c, kCFStringEncodingUTF8); CFRelease(s); }
    return c;
}
void send(MIDIPortRef port, MIDIEndpointRef dest, const std::vector<uint8_t>& m) {
    Byte buffer[1024]; MIDIPacketList* list = reinterpret_cast<MIDIPacketList*>(buffer);
    MIDIPacket* p = MIDIPacketListInit(list);
    p = MIDIPacketListAdd(list, sizeof buffer, p, 0, m.size(), m.data());
    if (p) MIDISend(port, dest, list);
}
void now(uint64_t& t, const mach_timebase_info_data_t& tb) { t = mach_absolute_time() * tb.numer / tb.denom; }

std::atomic<size_t> gotBytes{ 0 }; std::vector<uint8_t> got; std::vector<uint64_t> arrivals; mach_timebase_info_data_t gTb;
void received(const MIDIPacketList* list, void*, void*) {
    const MIDIPacket* p = &list->packet[0];
    for (UInt32 i = 0; i < list->numPackets; i++) { uint64_t t; now(t, gTb); arrivals.push_back(t); got.insert(got.end(), p->data, p->data + p->length); gotBytes += p->length; p = MIDIPacketNext(p); }
}
} // namespace

int main(int argc, char** argv) {
    mach_timebase_info(&gTb);
    MIDIClientRef client = 0; MIDIPortRef port = 0;
    if (MIDIClientCreate(CFSTR("fxplay"), nullptr, nullptr, &client) != noErr || MIDIOutputPortCreate(client, CFSTR("out"), &port) != noErr) { std::fprintf(stderr, "cannot open CoreMIDI\n"); return 1; }
    if (argc == 1) {
        const ItemCount n = MIDIGetNumberOfDestinations();
        std::printf("%lu MIDI output%s:\n", (unsigned long)n, n == 1 ? "" : "s");
        for (ItemCount i = 0; i < n; i++) std::printf("  %s\n", nameOf(MIDIGetDestination(i)).c_str());
        return 0;
    }
    int a = 1; double from = 0;
    if (!std::strcmp(argv[a], "--from") && argc > a + 1) { from = std::atof(argv[a + 1]); a += 2; }
    if (!std::strcmp(argv[a], "--test") && argc > a + 1) {
        std::vector<Event> ev; if (!readRun(argv[a + 1], ev)) return 1;
        MIDIEndpointRef virt = 0;
        const OSStatus st = MIDIDestinationCreate(client, CFSTR("fxplay test"), received, nullptr, &virt);
        if (st != noErr) { std::fprintf(stderr, "cannot make a virtual output here (error %d)\n", (int)st); return 1; }
        size_t total = 0; std::vector<uint8_t> want;
        for (const auto& e : ev) {
            send(port, virt, e.bytes); total += e.bytes.size(); want.insert(want.end(), e.bytes.begin(), e.bytes.end());
            for (int k = 0; k < 2000 && gotBytes.load() < total; k++) CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.001, true);
        }
        const bool same = got == want;
        std::printf("%zu events, %zu bytes sent, %zu received, %s\n", ev.size(), total, got.size(), same ? "identical" : "DIFFERENT");
        return same ? 0 : 1;
    }
    if (!std::strcmp(argv[a], "--listen") && argc > a + 1) {
        std::vector<Event> ev, want; if (!readRun(argv[a + 1], ev)) return 1;
        const uint64_t startMs = static_cast<uint64_t>(from * 1000); size_t first = 0;      // what a player with the same --from sends: the skipped SysEx first, then the rest on time
        for (const auto& e : ev) { if (e.ms >= startMs) want.push_back(e); else if (e.bytes[0] == 0xf0) { want.push_back(e); first++; } }
        std::vector<uint8_t> bytes; for (const auto& e : want) bytes.insert(bytes.end(), e.bytes.begin(), e.bytes.end());
        MIDIEndpointRef virt = 0;
        const OSStatus st = MIDIDestinationCreate(client, CFSTR("fxplay listen"), received, nullptr, &virt);
        if (st != noErr) { std::fprintf(stderr, "cannot make a virtual output here (error %d)\n", (int)st); return 1; }
        std::puts("listening"); std::fflush(stdout);
        size_t seen = 0; auto last = std::chrono::steady_clock::now();
        while (gotBytes.load() < bytes.size() && std::chrono::steady_clock::now() - last < std::chrono::seconds(60)) {
            CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.05, true);
            if (gotBytes.load() != seen) { seen = gotBytes.load(); last = std::chrono::steady_clock::now(); }
        }
        const bool same = got == bytes && arrivals.size() == want.size(); double worst = 0;
        if (same) for (size_t i = first; i < want.size(); i++) {
            const double late = (arrivals[i] - arrivals[first]) / 1e6 - (want[i].ms - want[first].ms);
            if (late > worst) worst = late; else if (-late > worst) worst = -late;
        }
        std::printf("%zu events, %zu bytes wanted, %zu received, %s; on time to within %.2f ms\n", want.size(), bytes.size(), got.size(), same ? "identical" : "DIFFERENT", worst);
        return same ? 0 : 1;
    }
    if (argc < a + 2) { std::fprintf(stderr, "fxplay [--from s] <file.mid> <output name>\n"); return 2; }
    std::vector<Event> ev; if (!readRun(argv[a], ev)) return 1;
    MIDIEndpointRef dest = 0; const std::string want = argv[a + 1];
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations(); i++) { const MIDIEndpointRef d = MIDIGetDestination(i); if (nameOf(d).find(want) != std::string::npos) { dest = d; break; } }
    if (!dest) { std::fprintf(stderr, "no MIDI output named \"%s\"; run fxplay alone to list them\n", want.c_str()); return 1; }
    std::printf("to %s: %zu events, %.1f minutes.  Start recording now.\n", nameOf(dest).c_str(), ev.size(), (ev.empty() ? 0 : ev.back().ms) / 60000.0);
    for (int s = 5; s > 0; s--) { std::printf("%d...\n", s); std::fflush(stdout); std::this_thread::sleep_for(std::chrono::seconds(1)); }
    const uint64_t startMs = static_cast<uint64_t>(from * 1000);
    size_t k = 0;
    for (; k < ev.size() && ev[k].ms < startMs; k++)    // skipped: its settings still count, its notes don't
        if (ev[k].bytes[0] == 0xf0) { send(port, dest, ev[k].bytes); std::this_thread::sleep_for(std::chrono::milliseconds(20)); }
    uint64_t t0; now(t0, gTb); uint32_t shown = 0;       // only now: the time spent on what was skipped mustn't bunch up what follows
    for (; k < ev.size(); k++) {
        const Event& e = ev[k];
        const uint64_t at = t0 + (e.ms - startMs) * 1000000ull;
        for (uint64_t t; now(t, gTb), t < at; ) { const uint64_t left = at - t; if (left > 2000000) std::this_thread::sleep_for(std::chrono::nanoseconds(left - 1500000)); }
        send(port, dest, e.bytes);
        if (e.ms / 10000 != shown) { shown = e.ms / 10000; std::printf("%4u s of %u\n", e.ms / 1000, ev.back().ms / 1000); std::fflush(stdout); }
    }
    std::puts("done; let the recording run two more seconds");
    return 0;
}
