// fxplay: plays a run from fxcapture.py to a MIDI output through CoreMIDI, so the capture needs no DAW to send SysEx (any recorder takes the audio).
//   fxplay                     lists the MIDI outputs
//   fxplay <file.mid> <name>   counts down 5 s, then plays it to the output whose name contains <name>; prints each second's progress
//   fxplay --from <seconds> <file.mid> <name>     starts part-way, sending the settings it skips first (SysEx, a
//                              patch's Bank Select and Program Change, each followed by its load); a run's "hand:"
//                              cue (vc2writer.py) is shown at its time, for the user to set at the panel in its pause
//   fxplay --test <file.mid>   sends the file as fast as possible to a virtual output made here, and checks every byte arrives (no hardware)
//   fxplay [--from <seconds>] --listen <file.mid>    a virtual output named "fxplay listen" for another fxplay to play to, checking the bytes and the timing (no hardware)
// For a capture that feeds the XT a signal too (the VC-2's, vc2capture.py; fxaudio.h):
//   fxplay [--from <s>] --audio <input.wav> [--device <name>] <file.mid> <name>
//                              also plays the input's first seconds at each of the run's cues ("probe:<seconds>"),
//                              on the audio output whose name holds <name> (the default output without --device),
//                              the MIDI timed by the audio's own clock
//   fxplay --audio-devices     lists the audio outputs
//   fxplay --test-audio <file.mid> <input.wav> <out.wav>    the input as the run would play it, rendered offline
//   fxplay --in <name> [seconds]    prints each SysEx from the MIDI input whose name holds <name>, for 120 s or the
//                              seconds given: the XT's own messages, for vc2capture.py --learn
// Takes only what fxcapture.py writes: type 0, 500 ticks per quarter, 500000 us per quarter, so 1 ms a tick.
// Build (the fx and the VC-2 guides each name their folder for the binary):
//   clang++ -std=c++17 -O2 research/tools/fxplay.cpp -framework CoreMIDI -framework CoreFoundation
//     -framework AudioToolbox -framework CoreAudio -o analysis/test_files/fx/fxplay
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

#include "fxaudio.h"
#include "fxmidiin.h"
#include "fxrun.h"

namespace {
using fxrun::Cues;
using fxrun::Event;
using fxrun::readRun;
using fxmidi::nameOf;
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

constexpr double kLead = 0.5;                          // the run's 0 ms, in seconds into the input's timeline

// A number from the command line, or `fallback` when there is none or it doesn't read as one.
double number(int argc, char** argv, int i, double fallback) {
    if (i >= argc)
        return fallback;
    char* stop = nullptr;
    const double v = std::strtod(argv[i], &stop);
    return stop != argv[i] && !*stop ? v : fallback;
}

// The input's player for a run: the WAV, and the run's cues from fromMs on, `lead` s into its timeline.
bool inputFor(fxaudio::Player& p, const char* run, const char* wav, uint32_t fromMs, double lead) {
    std::vector<Event> ev;
    Cues cues;
    return readRun(run, ev, &cues) && fxaudio::readWav(wav, p.wav) && fxaudio::schedule(p, cues, fromMs, lead);
}

// --test-audio: the run's input rendered offline into a 24-bit WAV, to hold to vc2capture.py --input-wavs' own.
int testAudio(const char* run, const char* wav, const char* outPath) {
    fxaudio::Player p;
    std::vector<int32_t> s;
    if (!inputFor(p, run, wav, 0, 0.0) || !fxaudio::render(p, s)) {
        std::fprintf(stderr, "the offline render failed\n");
        return 1;
    }
    s.resize(2 * p.end);
    std::vector<uint8_t> body;
    for (const int32_t v : s)
        for (int k = 0; k < 3; k++)
            body.push_back(uint8_t(uint32_t(v) >> (8 * k)));
    const uint32_t n = uint32_t(body.size());
    const uint32_t head[] = { 0x46464952, 36 + n, 0x45564157, 0x20746d66, 16, 0x00020001, 44100, 44100 * 6,
                              0x00180006, 0x61746164, n };
    FILE* f = std::fopen(outPath, "wb");
    const bool written = f && std::fwrite(head, 4, 11, f) == 11 && std::fwrite(body.data(), 1, n, f) == n;
    if (f && std::fclose(f) != 0)
        return 1;
    if (!written) {
        std::fprintf(stderr, "cannot write %s\n", outPath);
        return 1;
    }
    std::printf("%llu frames, %zu cues, into %s\n", (unsigned long long)p.end, p.cues.size(), outPath);
    return 0;
}

int listDevices() {
    const auto all = fxaudio::outputs();
    std::printf("%zu audio output%s:\n", all.size(), all.size() == 1 ? "" : "s");
    for (const auto& [name, uid] : all)
        std::printf("  %s\n", name.c_str());
    return 0;
}

int listOutputs() {
        const ItemCount n = MIDIGetNumberOfDestinations();
        std::printf("%lu MIDI output%s:\n", (unsigned long)n, n == 1 ? "" : "s");
        for (ItemCount i = 0; i < n; i++) std::printf("  %s\n", nameOf(MIDIGetDestination(i)).c_str());
        return 0;
}

// --test: the run at argv[a + 1] sent as fast as it goes to a virtual output made here, every byte checked.
int testRun(MIDIClientRef client, MIDIPortRef port, char** argv, int a) {
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

// --listen: a virtual output for another fxplay to play the run at argv[a + 1] to, from `from` s: bytes and timing.
int listenRun(MIDIClientRef client, char** argv, int a, double from) {
        std::vector<Event> ev, want; if (!readRun(argv[a + 1], ev)) return 1;
        const uint64_t startMs = static_cast<uint64_t>(from * 1000); size_t first = 0;      // what a player with the same --from sends: the skipped SysEx first, then the rest on time
        for (const auto& e : ev) {                     // the skipped settings (SysEx, a patch's choice), then the rest
            if (e.ms >= startMs || fxrun::isSetting(e)) want.push_back(e);
            if (e.ms < startMs && fxrun::isSetting(e)) first++;
        }
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

struct Options {
    double from = 0;                                   // --from: seconds into the run
    std::string wav, device;                           // --audio, --device: the input and where it plays
};

void waitUntil(uint64_t at) {
        for (uint64_t t; now(t, gTb), t < at; ) { const uint64_t left = at - t; if (left > 2000000) std::this_thread::sleep_for(std::chrono::nanoseconds(left - 1500000)); }
}

// The input's queue for the run at path, primed, or nullptr, said why; none wanted is (nullptr, true).
std::pair<AudioQueueRef, bool> inputQueue(fxaudio::Player& input, const char* path, const Options& opt) {
    if (opt.wav.empty())
        return { nullptr, true };
    if (!inputFor(input, path, opt.wav.c_str(), uint32_t(opt.from * 1000), kLead))
        return { nullptr, false };
    AudioQueueRef q = fxaudio::open(input, opt.device, false);
    if (q)
        std::printf("the input: %zu cues of %s, on %s\n", input.cues.size(), opt.wav.c_str(),
                    opt.device.empty() ? "the default output" : opt.device.c_str());
    return { q, q != nullptr };
}

// The run at argv[a] to the MIDI output whose name holds argv[a + 1], and its input when --audio gives one.
int play(MIDIPortRef port, char** argv, int a, const Options& opt) {
    std::vector<Event> ev;
    Cues cues;
    if (!readRun(argv[a], ev, &cues))
        return 1;
    MIDIEndpointRef dest = 0;
    const std::string want = argv[a + 1];
    for (ItemCount i = 0; i < MIDIGetNumberOfDestinations() && !dest; i++)
        if (nameOf(MIDIGetDestination(i)).find(want) != std::string::npos)
            dest = MIDIGetDestination(i);
    if (!dest) {
        std::fprintf(stderr, "no MIDI output named \"%s\"; run fxplay alone to list them\n", want.c_str());
        return 1;
    }
    fxaudio::Player input;                             // the queue's thread owns it once the queue starts
    const auto [queue, ready] = inputQueue(input, argv[a], opt);
    if (!ready)
        return 1;
    const double minutes = (ev.empty() ? 0 : ev.back().ms) / 60000.0;
    std::printf("to %s: %zu events, %.1f minutes.  Start recording now.\n", nameOf(dest).c_str(), ev.size(), minutes);
    for (int s = 5; s > 0; s--) {
        std::printf("%d...\n", s);
        std::fflush(stdout);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    const uint64_t startMs = static_cast<uint64_t>(opt.from * 1000);
    size_t k = 0;
    for (; k < ev.size() && ev[k].ms < startMs; k++)    // skipped: its settings still count, its notes don't
        if (fxrun::isSetting(ev[k])) {
            send(port, dest, ev[k].bytes);
            std::this_thread::sleep_for(std::chrono::milliseconds(fxrun::pauseAfter(ev[k])));
        }
    size_t hand = 0;                                   // the next cue: a "hand:" one is shown at its time
    while (hand < cues.size() && cues[hand].first < startMs) ++hand;
    uint64_t t0;                                       // only now: the time spent on what was skipped mustn't bunch up
    now(t0, gTb);
    if (queue)
        t0 = fxaudio::start(queue, kLead, gTb);        // the run's 0 ms is the input's timeline at kLead
    uint32_t shown = 0;
    for (; k < ev.size(); k++) {
        const Event& e = ev[k];
        for (; hand < cues.size() && cues[hand].first <= e.ms; ++hand) {
            waitUntil(t0 + (cues[hand].first - startMs) * 1000000ull);
            fxrun::showHand(cues[hand].second);
        }
        waitUntil(t0 + (e.ms - startMs) * 1000000ull);
        send(port, dest, e.bytes);
        if (e.ms / 10000 != shown) { shown = e.ms / 10000; std::printf("%4u s of %u\n", e.ms / 1000, ev.back().ms / 1000); std::fflush(stdout); }
    }
    if (queue) {                                       // the input's last cue out, and a second more
        waitUntil(t0 + uint64_t((double(input.end) / fxaudio::kRate - kLead + 1.0) * 1e9));
        AudioQueueStop(queue, true);
        AudioQueueDispose(queue, true);
    }
    std::puts("done; let the recording run two more seconds");
    return 0;
}
} // namespace

int main(int argc, char** argv) {
    mach_timebase_info(&gTb);
    MIDIClientRef client = 0; MIDIPortRef port = 0;
    if (MIDIClientCreate(CFSTR("fxplay"), nullptr, nullptr, &client) != noErr || MIDIOutputPortCreate(client, CFSTR("out"), &port) != noErr) { std::fprintf(stderr, "cannot open CoreMIDI\n"); return 1; }
    if (argc == 1)
        return listOutputs();
    int a = 1;
    Options opt;
    if (!std::strcmp(argv[a], "--from") && argc > a + 1) {
        opt.from = number(argc, argv, a + 1, 0.0);
        a += 2;
    }
    for (; a + 1 < argc && (!std::strcmp(argv[a], "--audio") || !std::strcmp(argv[a], "--device")); a += 2)
        (argv[a][2] == 'a' ? opt.wav : opt.device) = argv[a + 1];
    if (!std::strcmp(argv[a], "--audio-devices"))
        return listDevices();
    if (!std::strcmp(argv[a], "--test-audio") && argc > a + 3)
        return testAudio(argv[a + 1], argv[a + 2], argv[a + 3]);
    if (!std::strcmp(argv[a], "--in") && argc > a + 1)
        return fxmidi::dumpIn(client, argv[a + 1], number(argc, argv, a + 2, 120.0));
    if (!std::strcmp(argv[a], "--test") && argc > a + 1)
        return testRun(client, port, argv, a);
    if (!std::strcmp(argv[a], "--listen") && argc > a + 1)
        return listenRun(client, argv, a, opt.from);
    if (argc < a + 2) {
        std::fprintf(stderr, "fxplay [--from s] [--audio input.wav [--device name]] <file.mid> <output name>\n");
        return 2;
    }
    return play(port, argv, a, opt);
}
