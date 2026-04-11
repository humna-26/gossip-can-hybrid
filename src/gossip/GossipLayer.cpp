// ---------------------------------------------------------------------------
// GossipLayer.cpp — Gossip protocol per-node logic.
// ---------------------------------------------------------------------------

#include "GossipLayer.h"

#include <algorithm>  // std::shuffle

// ---------------------------------------------------------------------------
// Construction / initialisation
// ---------------------------------------------------------------------------

GossipLayer::GossipLayer() = default;

void GossipLayer::init(int nodeId, int totalNodes, int fanout, std::mt19937* rng) {
    nodeId_          = nodeId;
    fanout_          = fanout;
    rng_             = rng;
    membershipList_  = MembershipList(nodeId, totalNodes);
    initialized_     = true;
}

// ---------------------------------------------------------------------------
// Per-round operations
// ---------------------------------------------------------------------------

void GossipLayer::incrementHeartbeat() {
    if (!initialized_) return;
    membershipList_.incrementHeartbeat();
}

std::vector<int> GossipLayer::selectGossipTargets() {
    if (!initialized_ || rng_ == nullptr) return {};

    // Candidates: all non-self ALIVE/SUSPECTED peers.
    std::vector<int> candidates = membershipList_.getAliveNodes();

    if (candidates.empty()) return {};

    // Fisher-Yates shuffle, then take the first min(fanout, size) entries.
    std::shuffle(candidates.begin(), candidates.end(), *rng_);

    int n = std::min(fanout_, static_cast<int>(candidates.size()));
    return std::vector<int>(candidates.begin(), candidates.begin() + n);
}

MembershipList GossipLayer::prepareGossipMessage() const {
    // Return a snapshot of our current membership list.
    return membershipList_;
}

void GossipLayer::receiveGossip(const MembershipList& incoming, double currentTime) {
    if (!initialized_) return;
    membershipList_.merge(incoming, currentTime);
}

std::vector<int> GossipLayer::checkFailures(double currentTime,
                                             double suspectTimeout,
                                             double failTimeout) {
    if (!initialized_) return {};
    return membershipList_.checkForFailures(currentTime, suspectTimeout, failTimeout);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

const MembershipList& GossipLayer::getMembershipList() const {
    return membershipList_;
}

MembershipList& GossipLayer::getMembershipListMut() {
    return membershipList_;
}

bool GossipLayer::isInitialized() const {
    return initialized_;
}
