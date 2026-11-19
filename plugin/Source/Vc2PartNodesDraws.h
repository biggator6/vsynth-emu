#pragma once
// The VC-2 part's nodes drawn for a check (xt_vc2keys' second part's, v293; shared with xt_vc2settings' twelfth part
// from v298): their sources and channels and their lists, each list in address order, and whether every list ends.
// XT_VC2KEYS_MAIN and XT_VC2SETTINGS_MAIN only.
#include "Vc2PartNodes.h"
#include <array>
#include <random>

inline constexpr int kVc2Nodes = Vc2PartNodes::kSources * Vc2PartNodes::kParts;
inline constexpr int kVc2NodeLists = Vc2PartNodes::kSources * Vc2PartNodes::kChannels;

inline uint32_t vc2NodeAt(int k) { return Vc2PartNodes::kNodesAt + 16u * (uint32_t)k; }

// The nodes' sources (their own row's mostly) and channels (−1 or 0…15) and their lists drawn: each list in address
// order, one node in eight left off its own
inline void vc2DrawNodeLists(Vc2PartNodes& p, std::mt19937& rng) {
    using N = Vc2PartNodes;
    std::array<uint32_t, kVc2NodeLists> tails {};
    p.lists = {};
    for (int k = 0; k < kVc2Nodes; ++k) {
        Vc2PartNode& n = p.nodes[(size_t)k];
        n.source = (int8_t)(rng() % 4 == 0 ? rng() % N::kSources : (uint32_t)k / N::kParts);
        n.channel = (int8_t)(rng() % 4 == 0 ? -1 : (int)(rng() % N::kChannels));
        n.next = (uint32_t)rng();
        if (n.channel < 0 || rng() % 8 == 0) continue;
        const auto list = (size_t)(N::kChannels * n.source + n.channel);
        n.next = 0;
        (tails[list] == 0 ? p.lists[list] : p.nodes[(tails[list] - N::kNodesAt) / 16].next) = vc2NodeAt(k);
        tails[list] = vc2NodeAt(k);
    }
}

// Whether every list ends within the nodes' count of links
inline bool vc2NodeListsEnd(const Vc2PartNodes& p) {
    for (uint32_t head : p.lists) {
        int links = 0;
        for (uint32_t at = head; at != 0 && links <= kVc2Nodes; ++links)
            at = p.nodes[(at - Vc2PartNodes::kNodesAt) / 16].next;
        if (links > kVc2Nodes) return false;
    }
    return true;
}
