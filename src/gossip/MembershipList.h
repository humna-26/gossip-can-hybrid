#pragma once

// ---------------------------------------------------------------------------
// MembershipList.h — Per-node membership table for gossip failure detection.
//
// Each node maintains one MembershipList: a table of every known peer and
// its latest heartbeat counter.  Gossip propagates these tables; a peer
// whose counter stops incrementing is eventually suspected then declared
// FAILED.
// ---------------------------------------------------------------------------

#include <map>
#include <string>
#include <vector>

// Life-cycle of a peer entry as observed by one node.
enum class NodeStatus {
    ALIVE,
    SUSPECTED,  // Heartbeat stale > suspectTimeout, but < failTimeout
    FAILED      // Heartbeat stale > failTimeout (terminal — never reverts)
};

// One row in the membership table.
struct MemberEntry {
    int        nodeId;
    int        heartbeatCounter;  // Last known heartbeat value seen for this node
    double     localTimestamp;    // Local sim-time when this entry was last UPDATED
    NodeStatus status;
};

class MembershipList {
public:
    // Default constructor — produces an empty, unusable list.
    MembershipList();

    // Initialise with all node IDs 0 … totalNodes-1, heartbeat=0,
    // localTimestamp=0.0, status=ALIVE.
    MembershipList(int ownId, int totalNodes);

    // -----------------------------------------------------------------------
    // Own-node bookkeeping
    // -----------------------------------------------------------------------

    // Increment the owner's own heartbeat counter by 1.
    void incrementHeartbeat();

    // Return the owner's current heartbeat value.
    int getOwnHeartbeat() const;

    // -----------------------------------------------------------------------
    // Gossip merge
    // -----------------------------------------------------------------------

    // Merge an incoming membership list received from a gossip message.
    //
    // For each entry in 'incoming':
    //   • If incoming HB > local HB AND local status != FAILED:
    //       – update local HB and set localTimestamp = currentTime
    //       – if the entry was SUSPECTED, revive it to ALIVE
    //   • FAILED entries are never updated (they stay FAILED permanently).
    // Own entry is never overwritten by external gossip.
    void merge(const MembershipList& incoming, double currentTime);

    // -----------------------------------------------------------------------
    // Failure detection
    // -----------------------------------------------------------------------

    // Scan all non-self ALIVE/SUSPECTED entries.
    // • elapsed > failTimeout    → mark FAILED, add to returned list
    // • elapsed > suspectTimeout → mark SUSPECTED (not returned here)
    // Returns node IDs that JUST transitioned to FAILED this call.
    std::vector<int> checkForFailures(double currentTime,
                                      double suspectTimeout,
                                      double failTimeout);

    // -----------------------------------------------------------------------
    // Queries
    // -----------------------------------------------------------------------

    // IDs of all non-self entries whose status is ALIVE or SUSPECTED
    // (i.e., nodes we might still reach — used for gossip target selection).
    std::vector<int> getAliveNodes() const;

    NodeStatus getStatus(int nodeId) const;

    const std::map<int, MemberEntry>& getEntries() const;

    // -----------------------------------------------------------------------
    // Debug
    // -----------------------------------------------------------------------
    void print() const;

private:
    std::map<int, MemberEntry> entries_;
    int                        ownId_;
};
