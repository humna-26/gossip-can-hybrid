#pragma once

// ---------------------------------------------------------------------------
// GossipLayer.h — Gossip protocol behaviour for one node.
//
// GossipLayer wraps a MembershipList and provides:
//   • Periodic heartbeat increment
//   • Random target selection for each gossip round
//   • Receiving and merging incoming gossip messages
//   • Failure checking (delegates to MembershipList)
//
// All methods are no-ops until initGossip() is called via Node::initGossip().
// This lets Phase-1 nodes (which don't call initGossip) run without crashing.
// ---------------------------------------------------------------------------

#include "MembershipList.h"

#include <random>
#include <vector>

class GossipLayer {
public:
    GossipLayer();

    // Called once by Node::initGossip() before the simulation starts.
    void init(int nodeId, int totalNodes, int fanout, std::mt19937* rng);

    // -----------------------------------------------------------------------
    // Per-round operations (called by Simulator event handlers)
    // -----------------------------------------------------------------------

    // Increment own heartbeat counter.
    void incrementHeartbeat();

    // Pick up to fanout_ random ALIVE/SUSPECTED targets (excluding self).
    // Returns empty vector if not initialized or no peers available.
    std::vector<int> selectGossipTargets();

    // Return a snapshot of the membership list to be sent to gossip targets.
    MembershipList prepareGossipMessage() const;

    // Merge an incoming gossip message (called on the RECEIVING node).
    void receiveGossip(const MembershipList& incoming, double currentTime);

    // Check for newly failed nodes; returns their IDs.
    std::vector<int> checkFailures(double currentTime,
                                   double suspectTimeout,
                                   double failTimeout);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    const MembershipList& getMembershipList() const;
    MembershipList&       getMembershipListMut();

    bool isInitialized() const;

private:
    bool          initialized_ = false;
    int           nodeId_      = -1;
    int           fanout_      = 3;
    std::mt19937* rng_         = nullptr;
    MembershipList membershipList_;
};
