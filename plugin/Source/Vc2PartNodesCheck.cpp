// xt_vc2keys' second part (v293): the VC-2 part's nodes, Vc2PartNodes, against rom.4's own routines on its own nodes
// (0x899888d0), lists (0x899890d0), class bytes (0x899888bd) and part objects (0x8998246c), with the keys' span of the
// first part: the channel's move 0x89393100, the class installs 0x893932c0 and 0x89393310 (rom.4's 0x893904d0 its own
// on the glides), the voices' stop 0x89393360 (0x89390f70 hooked to log its word) and 0x893904d0 alone.  Each case the
// nodes drawn: noise, a source mostly their own row's, a channel −1…15, an index 0…15, and the lists made from them in
// address order, a node in eight left off its list; the part objects noise with an index 0…15.  Then 400 events on a
// drawn node: a move (its own source mostly, a channel −1…15, −2 one time in 32, its own now and then), an install of
// either class, a stop, or the glides' reset on a part object.  After each the keys' span, the four regions and the
// calls.  Not drawn: a source past 0…3, a channel below −2 or past 15, an index past 15 (rom.4 reads past its lists
// and its table there).  Every-list moves make lists share their chains, as rom.4's do; a list that no longer ends is
// drawn again, on both sides, before rom.4 or the port could loop on it.  XT_VC2KEYS_MAIN only.
#ifdef XT_VC2KEYS_MAIN
#include "Vc2KeysCheck.h"
#include "Vc2PartNodesDraws.h"

namespace {
using N = Vc2PartNodes;
constexpr uint32_t kStopVoices = 0x89390f70;                         // [FW]
constexpr uint32_t kMove = 0x89393100, kHeld = 0x893932c0, kPlain = 0x89393310, kStop = 0x89393360;
constexpr uint32_t kResetGlides = 0x893904d0;
constexpr int kNodesEvents = 400;

struct NodesCounts {
    long events = 0, calls = 0, moves = 0, every = 0, held = 0, plain = 0, stops = 0, resets = 0, redrawn = 0;
};

// The port's call out, logged as rom.4's hook logs its
struct NodesPortParts final : Vc2NodeParts {
    std::vector<uint32_t>& log;
    explicit NodesPortParts(std::vector<uint32_t>& l) : log(l) {}
    void stopVoices(uint32_t word) override { log.push_back(word); }
};

// The part's two sides: the first part's, the port's nodes, each side's log of the voices' stops
struct NodesSides {
    explicit NodesSides(KeysSides& k) : keys(k) {
        keys.fw.cpu.hook(kStopVoices, [this](Sh4& c) {
            romLog.push_back(c.r[4]);
            c.ret(0);
        });
    }
    ~NodesSides() { keys.fw.cpu.unhook(kStopVoices); }
    NodesSides(const NodesSides&) = delete;
    NodesSides& operator=(const NodesSides&) = delete;
    KeysSides& keys;
    Vc2PartNodes port {};
    std::vector<uint32_t> romLog, portLog;
    NodesPortParts parts { portLog };
    NodesCounts n {};
};

void drawNodes(NodesSides& s, std::mt19937& rng) {
    for (Vc2PartNode& n : s.port.nodes) {
        n.object = (uint32_t)rng();
        n.word8 = (uint32_t)rng();
        n.byteE = (uint8_t)(rng() % N::kObjects);
        n.byteF = (uint8_t)rng();
    }
    for (uint8_t& c : s.port.classes) c = (uint8_t)rng();
    for (Vc2PartObject& o : s.port.objects) {
        o.index = (uint8_t)(rng() % N::kObjects);
        o.vtable = (uint32_t)rng();
        for (uint8_t& b : o.unread1) b = (uint8_t)rng();
        for (uint32_t& w : o.unread8) w = (uint32_t)rng();
    }
    vc2DrawNodeLists(s.port, rng);
}

std::string nodesOnBoth(NodesSides& s, const Vc2CheckRoutine& r, std::initializer_list<uint32_t> args,
                        const std::function<void()>& mine) {
    Sh4& cpu = s.keys.fw.cpu;
    cpu.write(N::kNodesAt, s.port.nodes.data(), sizeof s.port.nodes);
    cpu.write(N::kListsAt, s.port.lists.data(), sizeof s.port.lists);
    cpu.write(N::kClassesAt, s.port.classes.data(), sizeof s.port.classes);
    cpu.write(N::kObjectsAt, s.port.objects.data(), sizeof s.port.objects);
    s.romLog.clear();
    s.portLog.clear();
    const std::string keys = keysOnBoth(s.keys, r, args, [&] {
        mine();
        return 0u;
    });
    s.n.calls += (long)s.portLog.size();
    if (!keys.empty()) return keys;
    const std::string words = vc2RegionsDifference(s.keys.fw, {
        { N::kNodesAt, s.port.nodes.data(), sizeof s.port.nodes, "the nodes" },
        { N::kListsAt, s.port.lists.data(), sizeof s.port.lists, "the lists" },
        { N::kClassesAt, s.port.classes.data(), sizeof s.port.classes, "the class bytes" },
        { N::kObjectsAt, s.port.objects.data(), sizeof s.port.objects, "the part objects" } });
    const std::string calls = s.romLog == s.portLog ? "" : "the voices' stops";
    return words.empty() && calls.empty() ? "" : std::string(r.name) + ": " + (words.empty() ? calls : words);
}

// A node's move: its own source mostly, a channel −1…15, −2 one time in 32, its own channel now and then
std::string moveEvent(NodesSides& s, std::mt19937& rng, uint32_t node) {
    const Vc2PartNode& n = s.port.nodes[(node - N::kNodesAt) / 16];
    const int32_t source = rng() % 4 == 0 ? (int32_t)(rng() % N::kSources) : n.source;
    const std::array<int32_t, 4> channels { -1, n.channel, (int32_t)(rng() % N::kChannels),
                                            (int32_t)(rng() % N::kChannels) };
    const int32_t channel = rng() % 32 == 0 ? -2 : channels[rng() % channels.size()];
    ++s.n.moves;
    s.n.every += channel == -2;
    return nodesOnBoth(s, { kMove, "the channel's move", nullptr }, { node, (uint32_t)source, (uint32_t)channel },
                       [&] { s.port.move(node, source, channel); });
}

std::string installEvent(NodesSides& s, std::mt19937& rng, uint32_t node) {
    const bool held = rng() % 2 == 0;
    ++(held ? s.n.held : s.n.plain);
    const Vc2NodeClass cls = held ? Vc2NodeClass::kHeld : Vc2NodeClass::kPlain;
    return nodesOnBoth(s, { held ? kHeld : kPlain, "a class's install", nullptr }, { node },
                       [&] { s.port.install(node, cls, s.keys.port); });
}

std::string stopEvent(NodesSides& s, std::mt19937&, uint32_t node) {
    ++s.n.stops;
    return nodesOnBoth(s, { kStop, "the voices' stop", nullptr }, { node }, [&] { s.port.stop(node, s.parts); });
}

std::string resetEvent(NodesSides& s, std::mt19937& rng, uint32_t) {
    const auto i = (uint32_t)(rng() % N::kObjects);
    ++s.n.resets;
    return nodesOnBoth(s, { kResetGlides, "the glides' reset", nullptr }, { N::kObjectsAt + 16 * i },
                       [&] { s.keys.port.resetObjectGlides(s.port.objects[i].index); });
}

using NodesEvent = std::string (*)(NodesSides&, std::mt19937&, uint32_t);
constexpr NodesEvent kNodesEventsOf[] = { moveEvent, moveEvent, installEvent, stopEvent, resetEvent };

std::string nodesCase(NodesSides& s, std::mt19937& rng) {
    drawNodes(s, rng);
    std::string what;
    for (int k = 0; k < kNodesEvents && what.empty(); ++k, ++s.n.events) {
        if (!vc2NodeListsEnd(s.port)) {
            vc2DrawNodeLists(s.port, rng);
            ++s.n.redrawn;
        }
        what = kNodesEventsOf[rng() % std::size(kNodesEventsOf)](s, rng, vc2NodeAt((int)(rng() % kVc2Nodes)));
    }
    return what;
}
} // namespace

long keysNodesPart(KeysSides& keys, const Vc2Run& run) {
    NodesSides s { keys };
    const long different = vc2CheckPart(run, 1, { "XT_VC2KEYS_FIRST", "XT_VC2KEYS_STOP" }, s.n.events,
                                        [&](std::mt19937& rng) { return nodesCase(s, rng); });
    const NodesCounts& n = s.n;
    std::printf("nodes: %ld events: %ld moves (%ld onto every list), %ld held installs, %ld plain, %ld stops, %ld "
                "glides' resets; %ld lists drawn again (%ld calls)\n", n.events, n.moves, n.every, n.held, n.plain,
                n.stops, n.resets, n.redrawn, n.calls);
    return different;
}
#endif
