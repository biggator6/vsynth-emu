// xt_midi (XT_MIDI_MAIN, v462): XtMidiPort against rom.2's own MIDI port handler (0x89474a10) in the interpreter, on a
// port object whose one listener is an OS part object built by the firmware's own constructor (0x89475dc0).  The edit
// object's three routines are hooked to log the part, each ID and value written and each commit; the channel's store
// (0x89475370) and reset (0x89475200), the tone generator's and not these edits', answer at once.  Each case draws the
// part, the Receive Bank Select flag and the port's words, then messages aimed at the handler's ways; now and then the
// port's words are noise on both sides before one.  After each message the port's three words and the edits are
// compared.  xt_midi <rom> [cases] [seed]; XT_MIDI_FIRST=1 says where the first case to differ does, XT_MIDI_STOP=1
// stops there.
#ifdef XT_MIDI_MAIN
#include "XtArgs.h"
#include "XtFwMachine.h"
#include "XtMidiEdits.h"
#include "XtSwitches.h"
#include <cstdio>
#include <iterator>
#include <random>
#include <vector>

namespace {
constexpr uint32_t kHandler = 0x89474a10, kPartBuild = 0x89475dc0;   // [FW 0x89474a10] [FW 0x89475dc0]
constexpr uint32_t kStore = 0x89475370, kReset = 0x89475200;         // [FW 0x89475370] [FW 0x89475200]
constexpr uint32_t kEditBegin = 0x8947a550, kEditSet = 0x89479e50, kEditCommit = 0x8947a590;   // [FW 0x8947a550]
constexpr uint32_t kRxBank = 0x8987a51d;                             // [FW 0x89474a70] Receive Bank Select's flag
constexpr uint32_t kPort = 0x8C0F4000, kNode = kPort + 0x20, kPart = kPort + 0x40;   // the harness's scratch RAM
constexpr int kMessages = 400, kSaid = 20;

// The cases' numbers, lo…hi, from one generator
struct Draws {
    std::mt19937 r;
    int in(int lo, int hi) { return lo + (int)(r() % (uint32_t)(hi - lo + 1)); }
    template <class Table>
    int of(const Table& t) { return t[(size_t)in(0, (int)std::size(t) - 1)]; }
};

struct Fw {
    xtfw::Machine m;
    std::vector<XtStoredEdit> edits;
    int commits = 0, parts = 0, part = -1;
    explicit Fw(const std::string& rom) : m(rom) {
        m.cpu.hook(kStore, [](Sh4& c) { c.ret(0); });
        m.cpu.hook(kReset, [](Sh4& c) { c.ret(0); });
        m.cpu.hook(kEditBegin, [this](Sh4& c) {
            part = (int)c.r[5];
            ++parts;
            c.ret(c.r[4]);
        });
        m.cpu.hook(kEditSet, [this](Sh4& c) {
            edits.push_back({ (int)(int16_t)c.r[6], (int)(int32_t)c.r[5] });
            c.ret(0);
        });
        m.cpu.hook(kEditCommit, [this](Sh4& c) {
            ++commits;
            c.ret(0);
        });
    }
    bool call(uint32_t addr, std::initializer_list<uint32_t> args) { return m.cpu.call(addr, args, xtfw::kStack); }
    void words(const XtMidiPort& p) {
        m.cpu.wr16(kPort + 4, p.rpn);
        m.cpu.wr16(kPort + 6, p.nrpn);
        m.cpu.wr16(kPort + 8, p.data);
    }
};

struct Tally {
    long cases = 0, messages = 0, edits = 0, compared = 0, different = 0;
    bool failed = false;
};

// A control change aimed at the handler's ways: the numbers, Data Entry and the part object's four, most often
int aimedController(Draws& d) {
    static const int kAimed[] = { 5, 6, 38, 65, 98, 99, 100, 101, 121, 126, 127, 0, 32, 6, 38, 100, 101 };
    return d.in(0, 99) < 85 ? d.of(kAimed) : d.in(0, 127);
}

// Its value: for a number's halves mostly 0…3, an RPN that edits; for Data Entry's MSB about the limits (Bend Range 24,
// Fine Tune 32 and 96, Coarse Tune 16 and 112); CC 65 about 64; else anything
int aimedValue(Draws& d, int cc) {
    static const int kEdges[] = { 0, 23, 24, 25, 31, 32, 33, 63, 64, 65, 95, 96, 97, 15, 16, 17, 111, 112, 113 };
    if (d.in(0, 4) == 0) return d.in(0, 127);
    switch (cc) {
        case 98: case 99: case 100: case 101: return d.in(0, 3);
        case 6: return d.of(kEdges);
        case 65: return d.in(62, 66);
        case 121: return d.in(0, 1);
        default: return d.in(0, 127);
    }
}

// A port word: none, an RPN that edits, one with the other kind chosen last, or noise
uint16_t portWord(Draws& d) {
    switch (d.in(0, 4)) {
        case 0: return XtMidiPort::kNone;
        case 1: return (uint16_t)d.in(0, 2);
        case 2: return (uint16_t)(XtMidiPort::kUnselected | d.in(0, 2));
        default: return (uint16_t)d.in(0, 0xffff);
    }
}

struct Case {
    Fw& fw;
    XtMidiPort port;
    Tally& t;
    bool first;
    long n;
    void differs(const char* what, long fwValue, long portValue) {
        if (t.different++ < kSaid && first)
            std::printf("  case %ld message %ld %s: firmware %ld, port %ld\n", n, t.messages, what, fwValue, portValue);
    }
    void noise(Draws& d) {
        port.rpn = portWord(d);
        port.nrpn = portWord(d);
        port.data = (uint16_t)d.in(0, 0x3fff);
        fw.words(port);
    }
    void compareEdits(const XtStoredEdits& e) {
        ++t.compared;
        if ((int)fw.edits.size() != e.count) differs("edits", (long)fw.edits.size(), e.count);
        for (int i = 0; i < e.count && i < (int)fw.edits.size(); ++i, t.compared += 2) {
            const XtStoredEdit& w = fw.edits[(size_t)i];
            if (w.id != e.at[i].id) differs("an edit's ID", w.id, e.at[i].id);
            if (w.value != e.at[i].value) differs("an edit's value", w.value, e.at[i].value);
        }
        ++t.compared;
        if (fw.commits != (e.count ? 1 : 0)) differs("commits", fw.commits, e.count ? 1 : 0);
        t.edits += e.count;
    }
    void message(int cc, int value) {
        fw.edits.clear();
        fw.commits = 0;
        if (!fw.call(kHandler, { kPort, (uint32_t)cc, (uint32_t)value })) {
            t.failed = true;
            return;
        }
        const XtStoredEdits e = port.controlChange(cc, value);
        ++t.messages;
        const uint16_t want[] = { fw.m.cpu.rd16(kPort + 4), fw.m.cpu.rd16(kPort + 6), fw.m.cpu.rd16(kPort + 8) };
        const uint16_t have[] = { port.rpn, port.nrpn, port.data };
        static const char* kWords[] = { "RPN", "NRPN", "Data Entry" };
        for (int i = 0; i < 3; ++i, ++t.compared)
            if (want[i] != have[i]) differs(kWords[i], want[i], have[i]);
        compareEdits(e);
    }
};
}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "xt_midi <rom.2> [cases] [seed]\n");
        return 2;
    }
    const long cases = argc > 2 ? intArg(argv[2]).value_or(200) : 200;
    const unsigned seed = argc > 3 ? (unsigned)intArg(argv[3]).value_or(2024) : 2024u;
    const bool first = xtSwitch("XT_MIDI_FIRST") != nullptr, stop = xtSwitch("XT_MIDI_STOP") != nullptr;
    Fw fw(argv[1]);
    if (!fw.m.ok) {
        std::printf("xt_midi: no image\n");
        return 1;
    }
    Draws d { std::mt19937(seed) };
    Tally t;
    for (long n = 0; n < cases && !t.failed; ++n) {
        const long before = t.different;
        for (uint32_t o = 0; o < 0x50; o += 4) fw.m.cpu.wr32(kPort + o, 0);
        fw.m.cpu.wr32(kPort + 0xc, kNode);   // the port's one listener: the node, then the part object
        fw.m.cpu.wr32(kNode, kPart);
        if (!fw.call(kPartBuild, { kPart, (uint32_t)d.in(0, 15) })) t.failed = true;
        fw.m.cpu.wr8(kRxBank, (uint8_t)d.in(0, 1));
        Case c { fw, {}, t, first, n };
        c.noise(d);
        for (int k = 0; k < kMessages && !t.failed; ++k) {
            if (d.in(0, 49) == 0) c.noise(d);
            const int cc = aimedController(d);
            c.message(cc, aimedValue(d, cc));
        }
        ++t.cases;
        if (stop && t.different > before) break;
    }
    if (t.failed) std::printf("xt_midi: a call failed: %s\n", fw.m.cpu.error().c_str());
    std::printf("messages: %ld cases, %ld messages, %ld edits, %ld compared, %ld different\n", t.cases, t.messages,
                t.edits, t.compared, t.different);
    std::printf("xt_midi: %ld different\n", t.failed ? -1 : t.different);
    return t.failed || t.different ? 1 : 0;
}
#endif
