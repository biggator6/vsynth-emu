// The VC-2 part's nodes' routines (v293): see Vc2PartNodes.h.  Each in rom.4's order on its words, the lists' links
// rom.4's addresses.  A source past 0…3 or a channel below −2 or past 15 reads past the lists in rom.4, and isn't
// given here; nor an object index past 15, past rom.4's table.
#include "Vc2PartNodes.h"
#include <iterator>

namespace {
constexpr int32_t kNoChannel = -1, kEveryChannel = -2;   // [FW]
constexpr uint32_t kNodeBytes = 16, kObjectBytes = 16;
// [FW] the part objects' classes' vtables, by Vc2NodeClass (the base's, 0x898cd1fc, written first and overwritten)
constexpr uint32_t kVtables[] = { 0x898cd184, 0x898cd3b8 };

// The lists a channel names: rom.4's switch on the channel + 2, −2 every one of the source's, −1 none, else its own
struct ListRun {
    int32_t from, count;
};
ListRun runOf(int32_t channel) {
    constexpr ListRun kSpecial[] = { { 0, Vc2PartNodes::kChannels }, { 0, 0 } };   // [FW]
    const auto i = static_cast<uint32_t>(channel - kEveryChannel);
    return i < std::size(kSpecial) ? kSpecial[i] : ListRun { channel, 1 };
}
} // namespace

Vc2PartNode& Vc2PartNodes::at(uint32_t address) { return nodes[(address - kNodesAt) / kNodeBytes]; }

uint32_t& Vc2PartNodes::listOf(int32_t source, int32_t channel) {
    return lists[static_cast<size_t>(kChannels * source + channel)];
}

void Vc2PartNodes::unlink(uint32_t& head, uint32_t node) {
    if (head == node) {
        head = at(node).next;
        return;
    }
    for (uint32_t before = head; before != 0 && at(before).next != 0; before = at(before).next)
        if (at(before).next == node) {
            at(before).next = at(node).next;
            return;
        }
}

void Vc2PartNodes::linkInOrder(uint32_t& head, uint32_t node) {
    if (head == 0 || head > node) {
        at(node).next = head;
        head = node;
        return;
    }
    uint32_t before = head;
    while (at(before).next != 0 && at(before).next < node) before = at(before).next;
    // rom.4's order, which matters when a list shared by an every-list node leads back to the node itself
    const uint32_t after = at(before).next;
    at(before).next = node;
    at(node).next = after;
}

void Vc2PartNodes::move(uint32_t node, int32_t source, int32_t channel) {
    Vc2PartNode& n = at(node);
    if (n.channel == channel && n.source == source) return;
    const ListRun off = runOf(n.channel);
    for (int32_t c = off.from; c < off.from + off.count; ++c) unlink(listOf(n.source, c), node);
    if (n.source != source) {
        n.channel = kNoChannel;
        return;
    }
    n.channel = static_cast<int8_t>(channel);
    const ListRun on = runOf(channel);
    // every list: the node at each one's head, its link the last's; its own list: in address order
    for (int32_t c = on.from; c < on.from + on.count; ++c) {
        uint32_t& head = listOf(source, c);
        if (on.count != kChannels) {
            linkInOrder(head, node);
            continue;
        }
        n.next = head;
        head = node;
    }
}

void Vc2PartNodes::install(uint32_t node, Vc2NodeClass cls, Vc2Keys& keys) {
    Vc2PartNode& n = at(node);
    // the old object's destructor (0x893904c0) is a bare return
    const uint8_t i = n.byteE;
    Vc2PartObject& o = objects[i];
    o.vtable = kVtables[static_cast<size_t>(cls)];
    n.object = kObjectsAt + kObjectBytes * i;
    keys.resetObjectGlides(o.index);
    classes[i] = static_cast<uint8_t>(cls);
}

void Vc2PartNodes::stop(uint32_t node, Vc2NodeParts& parts) { parts.stopVoices(at(node).word8); }
