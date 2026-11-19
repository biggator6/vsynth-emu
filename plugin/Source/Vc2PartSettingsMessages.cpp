// The VC-2 part's parameters' messages (v297): see Vc2PartSettings.h.  0x89392890's switch on the message's kind as a
// table of ways (rom.4's braf table, 0…13), each way's calls in rom.4's order; the part's 0x89392830; 0x89391500,
// 0x89392750 and 0x89394370.  A message's part past 0 is refused, as the VC-2 has part 0 alone (0x89392830's test).
#include "Vc2PartSettings.h"
#include <iterator>

namespace {
constexpr uint8_t kRoutingOn = 0x80;   // [FW] kind 0's second byte: the routing reset's flag
constexpr uint32_t kNodeBytes = 16;    // [FW] a source's nodes, by part

// The node of source 1 and `part`, which these hand to its routines
uint32_t partNode(int part) { return Vc2PartNodes::kNodesAt + kNodeBytes * static_cast<uint32_t>(part); }

// 0x89392720: part 0's node's voices stopped (0x89393360), each of kinds 3 to 5's last call
void stopPartZero(Vc2PartWorld& w) { w.nodes.stop(partNode(0), w.nodeParts); }

using MessageWay = void (*)(Vc2PartSettings&, Vc2PartWorld&, uint8_t);

// [FW] kind 0: the Matrix's settings for part 0 (0x8939b0c0, its answer unread), the routing's reset, and with it
// changed the held note's switch
void routingWay(Vc2PartSettings& s, Vc2PartWorld& w, uint8_t byte1) {
    const bool on = (byte1 & kRoutingOn) != 0;
    w.setup.settings(w.g, 0);
    if (w.out.routing(on)) s.heldNoteSwitch(w.t, on, w.out);
}

// [FW] kind 3: the calibration's dispatcher, the voices' counts
void calibrationWay(Vc2PartSettings& s, Vc2PartWorld& w, uint8_t) {
    s.readCalibration(w.t);
    s.voiceCounts(w.t, w.out);
    stopPartZero(w);
}

// [FW] kind 4: the system's dispatcher, part 0's receive channel and, with it changed, Patch Remain
void systemWay(Vc2PartSettings& s, Vc2PartWorld& w, uint8_t) {
    s.readSystem(w.t, w.g, w.states);
    if (w.out.receive()) s.patchRemain(w.t);
    stopPartZero(w);
}

// [FW] kind 5: the assignments' dispatcher, part 0's reload run (0x89392b10), Time Trip's assigns
void assignsWay(Vc2PartSettings& s, Vc2PartWorld& w, uint8_t) {
    s.readAssigns(w.t, w.g, w.setup);
    w.out.reloadRun(0);
    s.tripAssigns(w.t);
    stopPartZero(w);
}

// [FW] kinds 6 and 10…13 (kind 6's first call, 0x89394360, a bare answer): the part's reload run and its node's
// voices stopped
void partWay(Vc2PartSettings&, Vc2PartWorld& w, uint8_t part) {
    if (part != 0) return;
    w.out.reloadRun(part);
    w.nodes.stop(partNode(part), w.nodeParts);
}

// [FW] by kind, 0…13; none for 1, 2 and 7…9
constexpr MessageWay kWays[] = { routingWay, nullptr, nullptr, calibrationWay, systemWay, assignsWay, partWay,
                                 nullptr,    nullptr, nullptr, partWay,        partWay,   partWay,    partWay };
} // namespace

void Vc2PartSettings::parameters(Vc2PartWorld& w, uint8_t kind, uint8_t byte1) {
    if (kind >= std::size(kWays) || kWays[kind] == nullptr) return;
    kWays[kind](*this, w, byte1);
}

void Vc2PartSettings::partParameters(Vc2PartWorld& w, uint8_t kind, uint8_t part) {
    if (kind != 0 || part != 0) return;
    w.out.reloadRequest(part);
    nodeClass(w, part, 0);
    category(w.t, w.nodes, part, w.out);
    clearSteps(w.t);
}

void Vc2PartSettings::heldNoteSwitch(Vc2PartRows& t, bool on, Vc2SettingsParts& out) {
    if (t.heldNote == 0 && on) out.truncateAll();
}

void Vc2PartSettings::clearSteps(Vc2PartRows& t) { t.steps.fill(0); }

void Vc2PartSettings::nodeClass(Vc2PartWorld& w, int part, int32_t plain) {
    w.nodes.install(partNode(part), plain == 0 ? Vc2NodeClass::kHeld : Vc2NodeClass::kPlain, w.keys);
}
