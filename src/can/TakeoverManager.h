#pragma once

// ---------------------------------------------------------------------------
// TakeoverManager.h — CAN zone takeover when a node fails.
// ---------------------------------------------------------------------------

#include "CANOverlay.h"

struct TakeoverResult {
    int    takerNodeId;    // Node that absorbed the failed zone
    double completionTime; // Estimated time when takeover finishes
    int    messagesSent;   // Number of coordination messages (O(d) neighbours)
    bool   success;
};

class TakeoverManager {
public:
    // Find the smallest-volume neighbour of failedNodeId, remove the failed
    // node from the overlay, and merge its zone into the taker.
    // routingDelayPerHop is the per-message latency used to estimate
    // completionTime = currentTime + messagesSent * routingDelayPerHop.
    TakeoverResult executeTakeover(CANOverlay& overlay,
                                   int         failedNodeId,
                                   double      currentTime,
                                   double      routingDelayPerHop = 10.0);
};
