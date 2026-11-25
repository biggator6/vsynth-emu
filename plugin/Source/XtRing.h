#pragma once
// A ring: the circular, doubly linked list rom.2's and rom.4's libraries keep (v354, out of v188's arpeggio keys).
// Each node has a prev and a next handle (an index, an address); the head is the first, `none` an empty ring.  A node
// goes in at the end, before the head (rom.2's 0x893040a0, rom.4's tempo timers 0x8930f07a).  Taking out the head
// makes the one before it, the last, the head (0x893040c0; rom.4's timers' 0x8930f0fa alike, not the part's); the
// last node out leaves its links as they were.

// `k` in at the ring's end; nodeAt(handle) gives the node
template <class Handle, class NodeAt>
void xtRingLink(Handle& head, Handle none, Handle k, NodeAt nodeAt) {
    auto& n = nodeAt(k);
    if (head == none) {
        n.prev = n.next = head = k;
        return;
    }
    const Handle tail = nodeAt(head).prev;
    // one-off: a ring's link; XtSoloEncoder's insert places a node by position in a linear list with null ends
    n.prev = tail;
    n.next = head;
    nodeAt(head).prev = k;
    nodeAt(tail).next = k;
}

// `k` out of the ring
template <class Handle, class NodeAt>
void xtRingUnlink(Handle& head, Handle none, Handle k, NodeAt nodeAt) {
    auto& n = nodeAt(k);
    const Handle prev = n.prev;
    if (head == k) {
        if (prev == k) {
            head = none;
            return;
        }
        head = prev;
    }
    nodeAt(prev).next = n.next;
    nodeAt(n.next).prev = prev;
}
