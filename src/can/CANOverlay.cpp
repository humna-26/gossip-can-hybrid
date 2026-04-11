// ---------------------------------------------------------------------------
// CANOverlay.cpp — CAN overlay topology management.
// ---------------------------------------------------------------------------

#include "CANOverlay.h"

#include <algorithm>   // std::remove, std::min_element
#include <cmath>       // std::abs
#include <iostream>
#include <iomanip>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

CANOverlay::CANOverlay() = default;

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void CANOverlay::updateNeighbours(int nodeId) {
    const Zone& myZone = nodeZones_.at(nodeId);
    std::vector<int> nbs;
    for (const auto& [otherId, otherZone] : nodeZones_) {
        if (otherId == nodeId) continue;
        if (myZone.isNeighbour(otherZone)) {
            nbs.push_back(otherId);
        }
    }
    neighbourTable_[nodeId] = std::move(nbs);
}

void CANOverlay::rebuildAllNeighbours() {
    for (const auto& [nodeId, _] : nodeZones_) {
        updateNeighbours(nodeId);
    }
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void CANOverlay::initialize(int numNodes) {
    nodeZones_.clear();
    neighbourTable_.clear();

    if (numNodes <= 0) return;

    // Start with a single zone covering the full space.
    std::vector<Zone> zones;
    zones.push_back(Zone()); // [0,1)×[0,1)

    // Repeatedly split the largest-volume zone until we have ≥ numNodes zones.
    while (static_cast<int>(zones.size()) < numNodes) {
        // Find index of zone with the greatest area.
        int maxIdx = 0;
        for (int i = 1; i < static_cast<int>(zones.size()); ++i) {
            if (zones[i].volume() > zones[maxIdx].volume()) {
                maxIdx = i;
            }
        }

        auto [half1, half2] = zones[maxIdx].split();
        zones.erase(zones.begin() + maxIdx);
        zones.push_back(half1);
        zones.push_back(half2);
    }

    // Assign one zone per node (first numNodes zones in the list).
    for (int i = 0; i < numNodes; ++i) {
        nodeZones_[i]      = zones[i];
        neighbourTable_[i] = {};
    }

    rebuildAllNeighbours();
}

// ---------------------------------------------------------------------------
// Node JOIN
// ---------------------------------------------------------------------------

int CANOverlay::nodeJoin(int newNodeId, double x, double y) {
    if (nodeZones_.empty()) {
        // Bootstrap: new node takes the full space.
        nodeZones_[newNodeId] = Zone();
        neighbourTable_[newNodeId] = {};
        return 0;
    }

    // 1. Greedy-route from node 0 to find the current owner of (x,y).
    int startNode = nodeZones_.begin()->first; // first live node
    std::vector<int> path = routeToPoint(startNode, x, y);
    int ownerNodeId = path.back();

    // 2. Split owner's zone; give the half containing (x,y) to the new node.
    auto [half1, half2] = nodeZones_.at(ownerNodeId).split();

    Zone newZone, ownerZone;
    if (half1.contains(x, y)) {
        newZone   = half1;
        ownerZone = half2;
    } else {
        newZone   = half2;
        ownerZone = half1;
    }

    // 3. Update zones.
    nodeZones_[ownerNodeId] = ownerZone;
    nodeZones_[newNodeId]   = newZone;
    neighbourTable_[newNodeId] = {};

    // 4. Rebuild all neighbour tables (both nodes' neighbours changed).
    rebuildAllNeighbours();

    return static_cast<int>(path.size()) - 1; // hop count
}

// ---------------------------------------------------------------------------
// Routing
// ---------------------------------------------------------------------------

std::vector<int> CANOverlay::routeToPoint(int sourceNodeId, double x, double y) const {
    std::vector<int> path;
    std::set<int>    visited;

    int current = sourceNodeId;

    // Safety limit: O(N) hops is far more than needed for a valid CAN.
    const int maxHops = static_cast<int>(nodeZones_.size()) * 4 + 4;

    while (static_cast<int>(path.size()) <= maxHops) {
        path.push_back(current);
        visited.insert(current);

        // If this node owns the point, we're done.
        if (nodeZones_.at(current).contains(x, y)) break;

        // Greedy: pick the neighbour (or self) whose zone centre is closest.
        double bestDist = nodeZones_.at(current).distanceTo(x, y);
        int    bestNode = current;

        for (int nb : neighbourTable_.at(current)) {
            if (visited.count(nb)) continue;
            double d = nodeZones_.at(nb).distanceTo(x, y);
            if (d < bestDist) {
                bestDist = d;
                bestNode = nb;
            }
        }

        // If no neighbour is closer than self, we can't make progress.
        // (Should not happen in a correct CAN topology; exit to avoid loop.)
        if (bestNode == current) break;

        current = bestNode;
    }

    return path;
}

int CANOverlay::findZoneOwner(double x, double y) const {
    for (const auto& [nodeId, zone] : nodeZones_) {
        if (zone.contains(x, y)) return nodeId;
    }
    return -1;
}

// ---------------------------------------------------------------------------
// Failure & recovery
// ---------------------------------------------------------------------------

std::vector<int> CANOverlay::getNeighbours(int nodeId) const {
    auto it = neighbourTable_.find(nodeId);
    if (it == neighbourTable_.end()) return {};
    return it->second;
}

Zone CANOverlay::removeNode(int nodeId) {
    if (!nodeZones_.count(nodeId)) {
        throw std::runtime_error("CANOverlay::removeNode — node not found");
    }

    Zone failedZone = nodeZones_.at(nodeId);

    nodeZones_.erase(nodeId);
    neighbourTable_.erase(nodeId);

    // Excise nodeId from every neighbour list.
    for (auto& [id, nbs] : neighbourTable_) {
        nbs.erase(std::remove(nbs.begin(), nbs.end(), nodeId), nbs.end());
    }

    return failedZone;
}

void CANOverlay::performTakeover(int takerNodeId, const Zone& failedZone) {
    if (!nodeZones_.count(takerNodeId)) {
        throw std::runtime_error("CANOverlay::performTakeover — taker not found");
    }
    // Merge the two zones (bounding box).
    nodeZones_[takerNodeId] = nodeZones_[takerNodeId].merge(failedZone);
    rebuildAllNeighbours();
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

Zone CANOverlay::getZone(int nodeId) const {
    return nodeZones_.at(nodeId);
}

int CANOverlay::getNodeCount() const {
    return static_cast<int>(nodeZones_.size());
}

bool CANOverlay::hasNode(int nodeId) const {
    return nodeZones_.count(nodeId) > 0;
}

// ---------------------------------------------------------------------------
// Integrity verification
// ---------------------------------------------------------------------------

bool CANOverlay::verifyIntegrity() const {
    constexpr double EPS = 1e-9;
    bool ok = true;

    // --- (1) Total area must equal 1.0 ---
    double totalArea = 0.0;
    for (const auto& [id, zone] : nodeZones_) {
        totalArea += zone.volume();
    }
    if (std::abs(totalArea - 1.0) > EPS) {
        std::cout << "  [INTEGRITY FAIL] Total area = " << totalArea
                  << " (expected 1.0)\n";
        ok = false;
    }

    // --- (2) No pairwise overlapping zones ---
    std::vector<std::pair<int,Zone>> entries(nodeZones_.begin(), nodeZones_.end());
    for (int i = 0; i < static_cast<int>(entries.size()); ++i) {
        for (int j = i + 1; j < static_cast<int>(entries.size()); ++j) {
            const Zone& A = entries[i].second;
            const Zone& B = entries[j].second;

            double xOverlap = std::min(A.xMax, B.xMax) - std::max(A.xMin, B.xMin);
            double yOverlap = std::min(A.yMax, B.yMax) - std::max(A.yMin, B.yMin);

            if (xOverlap > EPS && yOverlap > EPS) {
                std::cout << "  [INTEGRITY FAIL] Nodes " << entries[i].first
                          << " and " << entries[j].first << " zones overlap! "
                          << "overlap area = " << xOverlap * yOverlap << "\n";
                ok = false;
            }
        }
    }

    // --- (3) Neighbour relationships must be symmetric ---
    for (const auto& [nodeId, nbs] : neighbourTable_) {
        for (int nb : nbs) {
            const auto& nbNbs = neighbourTable_.at(nb);
            bool found = std::find(nbNbs.begin(), nbNbs.end(), nodeId) != nbNbs.end();
            if (!found) {
                std::cout << "  [INTEGRITY FAIL] Asymmetric neighbour: "
                          << nodeId << " lists " << nb
                          << " but not vice-versa\n";
                ok = false;
            }
        }
    }

    return ok;
}

// ---------------------------------------------------------------------------
// Debug output
// ---------------------------------------------------------------------------

void CANOverlay::printState() const {
    std::cout << std::fixed << std::setprecision(4);
    std::cout << "  CAN Overlay (" << nodeZones_.size() << " nodes):\n";

    for (const auto& [nodeId, zone] : nodeZones_) {
        std::cout << "    Node " << std::setw(2) << nodeId
                  << "  zone=" << zone.toString()
                  << "  neighbours=[";
        const auto& nbs = neighbourTable_.at(nodeId);
        for (int i = 0; i < static_cast<int>(nbs.size()); ++i) {
            if (i) std::cout << ",";
            std::cout << nbs[i];
        }
        std::cout << "]\n";
    }
}
