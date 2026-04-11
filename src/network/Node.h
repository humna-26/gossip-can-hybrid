#pragma once

// ---------------------------------------------------------------------------
// Node.h — Node with Phase-1 liveness, Phase-2 CAN state, Phase-3 gossip.
// ---------------------------------------------------------------------------

#include "can/Zone.h"
#include "gossip/GossipLayer.h"

#include <random>
#include <vector>

class Node {
public:
    int  id;
    bool alive;

    // --- Phase 2: CAN state ---
    Zone             zone;
    std::vector<int> canNeighbours;

    // --- Phase 3: Gossip state ---
    GossipLayer gossipLayer;

    explicit Node(int id);

    void crash();
    bool isAlive() const;

    // CAN helpers
    void setZone(const Zone& z);
    Zone getZone() const;
    void setNeighbours(const std::vector<int>& neighbours);
    std::vector<int> getNeighbours() const;

    // Initialise gossip layer (call once before simulation starts).
    void initGossip(int totalNodes, int fanout, std::mt19937* rng);
};
