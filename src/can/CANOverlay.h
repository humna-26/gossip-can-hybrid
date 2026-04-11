#pragma once

// ---------------------------------------------------------------------------
// CANOverlay.h — Centralized bookkeeper for the CAN overlay topology.
//
// In the real CAN protocol each node only knows its own zone and its direct
// neighbours. Here the CANOverlay acts as an oracle (centralised view) so
// the discrete-event simulator can verify routing and integrity without
// actually exchanging messages. Event-driven routing messages are layered on
// top in Phase 3.
// ---------------------------------------------------------------------------

#include "Zone.h"

#include <map>
#include <set>
#include <vector>

class CANOverlay {
public:
    CANOverlay();

    // -----------------------------------------------------------------------
    // Initialization
    // -----------------------------------------------------------------------

    // Partition [0,1)×[0,1) evenly among numNodes nodes (ids 0 … numNodes-1).
    // Strategy: repeatedly split the largest zone until we have ≥ numNodes
    // zones, then assign one zone per node and rebuild the neighbour table.
    void initialize(int numNodes);

    // -----------------------------------------------------------------------
    // Node JOIN
    // -----------------------------------------------------------------------

    // Add a new node to the overlay:
    //   1. Route from node 0 to the owner of (x,y).
    //   2. Split owner's zone; new node takes the half containing (x,y).
    //   3. Rebuild neighbour tables.
    // Returns the hop count used during routing (path.size() - 1).
    int nodeJoin(int newNodeId, double x, double y);

    // -----------------------------------------------------------------------
    // Routing
    // -----------------------------------------------------------------------

    // Greedy CAN routing from sourceNodeId toward point (x,y).
    // At each hop, move to the neighbour (or stay) whose zone centre is
    // closest to (x,y). Stop when the current node owns (x,y).
    // Returns the full path (list of node ids) including source.
    std::vector<int> routeToPoint(int sourceNodeId, double x, double y) const;

    // Find which node's zone contains (x,y). Returns -1 if none found.
    int findZoneOwner(double x, double y) const;

    // -----------------------------------------------------------------------
    // Failure & recovery
    // -----------------------------------------------------------------------

    // Return a copy of the failed node's neighbours (call BEFORE removeNode).
    std::vector<int> getNeighbours(int nodeId) const;

    // Remove nodeId from the overlay (zones + neighbour table).
    // Does NOT perform takeover — just excises the node.
    // Returns the zone that was owned by the removed node.
    Zone removeNode(int nodeId);

    // Merge failedZone into takerNodeId's zone and rebuild neighbour tables.
    void performTakeover(int takerNodeId, const Zone& failedZone);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    Zone getZone(int nodeId) const;
    int  getNodeCount() const;
    bool hasNode(int nodeId) const;

    // -----------------------------------------------------------------------
    // Integrity verification
    // -----------------------------------------------------------------------

    // Returns true iff:
    //   (1) Sum of zone areas ≈ 1.0  (full coverage)
    //   (2) No two zones share positive-area intersection  (no overlaps)
    //   (3) All neighbour relationships are symmetric
    bool verifyIntegrity() const;

    // -----------------------------------------------------------------------
    // Debug output
    // -----------------------------------------------------------------------
    void printState() const;

private:
    std::map<int, Zone>              nodeZones_;       // nodeId → zone
    std::map<int, std::vector<int>>  neighbourTable_;  // nodeId → neighbour ids

    // Recompute neighbours for one node by comparing against all other zones.
    void updateNeighbours(int nodeId);

    // Recompute neighbours for every node.
    void rebuildAllNeighbours();
};
