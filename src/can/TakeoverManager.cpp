// ---------------------------------------------------------------------------
// TakeoverManager.cpp — CAN zone takeover implementation.
// ---------------------------------------------------------------------------

#include "TakeoverManager.h"

#include <algorithm>

TakeoverResult TakeoverManager::executeTakeover(CANOverlay& overlay,
                                                 int         failedNodeId,
                                                 double      currentTime,
                                                 double      routingDelayPerHop) {
    TakeoverResult result;
    result.success       = false;
    result.takerNodeId   = -1;
    result.messagesSent  = 0;
    result.completionTime = currentTime;

    if (!overlay.hasNode(failedNodeId)) return result;

    std::vector<int> neighbours = overlay.getNeighbours(failedNodeId);
    if (neighbours.empty()) return result;

    Zone failedZone = overlay.getZone(failedNodeId);

    // Pick the alive neighbour with the smallest zone volume as taker.
    int taker = -1;
    for (int nb : neighbours) {
        if (!overlay.hasNode(nb)) continue;
        if (taker == -1 ||
            overlay.getZone(nb).volume() < overlay.getZone(taker).volume()) {
            taker = nb;
        }
    }
    if (taker == -1) return result;

    // Remove failed node then expand taker's zone.
    overlay.removeNode(failedNodeId);
    overlay.performTakeover(taker, failedZone);

    // Message cost = notifying all neighbours of the failed node (O(d)).
    int messages = std::max(1, static_cast<int>(neighbours.size()));

    result.takerNodeId    = taker;
    result.messagesSent   = messages;
    result.completionTime = currentTime + messages * routingDelayPerHop;
    result.success        = true;

    return result;
}
