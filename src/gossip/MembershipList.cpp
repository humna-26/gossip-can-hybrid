// ---------------------------------------------------------------------------
// MembershipList.cpp — Gossip membership table implementation.
// ---------------------------------------------------------------------------

#include "MembershipList.h"

#include <algorithm>
#include <iostream>
#include <iomanip>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

MembershipList::MembershipList() : ownId_(-1) {}

MembershipList::MembershipList(int ownId, int totalNodes) : ownId_(ownId) {
    for (int i = 0; i < totalNodes; ++i) {
        MemberEntry e;
        e.nodeId           = i;
        e.heartbeatCounter = 0;
        e.localTimestamp   = 0.0;
        e.status           = NodeStatus::ALIVE;
        entries_[i]        = e;
    }
}

// ---------------------------------------------------------------------------
// Own-node bookkeeping
// ---------------------------------------------------------------------------

void MembershipList::incrementHeartbeat() {
    if (entries_.count(ownId_)) {
        entries_[ownId_].heartbeatCounter++;
        // Note: we do NOT update localTimestamp for self — the staleness
        // clock only applies to remote entries.
    }
}

int MembershipList::getOwnHeartbeat() const {
    auto it = entries_.find(ownId_);
    return (it != entries_.end()) ? it->second.heartbeatCounter : 0;
}

// ---------------------------------------------------------------------------
// Gossip merge
// ---------------------------------------------------------------------------

void MembershipList::merge(const MembershipList& incoming, double currentTime) {
    for (const auto& [id, inEntry] : incoming.getEntries()) {
        // Never overwrite our own entry from external gossip — we are the
        // authoritative source for our own heartbeat.
        if (id == ownId_) continue;

        if (!entries_.count(id)) {
            // Previously unknown node — add it.
            MemberEntry e = inEntry;
            e.localTimestamp = currentTime;
            entries_[id]     = e;
            continue;
        }

        MemberEntry& local = entries_[id];

        // FAILED is a terminal state — never revive.
        if (local.status == NodeStatus::FAILED) continue;

        // Accept the entry only if it carries a strictly fresher heartbeat.
        if (inEntry.heartbeatCounter > local.heartbeatCounter) {
            local.heartbeatCounter = inEntry.heartbeatCounter;
            local.localTimestamp   = currentTime;   // reset staleness clock

            // Revive a node that was only suspected (not yet failed).
            if (local.status == NodeStatus::SUSPECTED) {
                local.status = NodeStatus::ALIVE;
            }
        }
    }
}

// ---------------------------------------------------------------------------
// Failure detection
// ---------------------------------------------------------------------------

std::vector<int> MembershipList::checkForFailures(double currentTime,
                                                   double suspectTimeout,
                                                   double failTimeout) {
    std::vector<int> newlyFailed;

    for (auto& [id, entry] : entries_) {
        // Never check self.
        if (id == ownId_) continue;
        // Already declared failed — nothing to do.
        if (entry.status == NodeStatus::FAILED) continue;

        double elapsed = currentTime - entry.localTimestamp;

        if (elapsed > failTimeout) {
            entry.status = NodeStatus::FAILED;
            newlyFailed.push_back(id);
        } else if (elapsed > suspectTimeout) {
            entry.status = NodeStatus::SUSPECTED;
        }
    }

    return newlyFailed;
}

// ---------------------------------------------------------------------------
// Queries
// ---------------------------------------------------------------------------

std::vector<int> MembershipList::getAliveNodes() const {
    std::vector<int> result;
    for (const auto& [id, entry] : entries_) {
        if (id == ownId_) continue;
        // Include ALIVE and SUSPECTED — we can still try to reach them.
        if (entry.status != NodeStatus::FAILED) {
            result.push_back(id);
        }
    }
    return result;
}

NodeStatus MembershipList::getStatus(int nodeId) const {
    auto it = entries_.find(nodeId);
    return (it != entries_.end()) ? it->second.status : NodeStatus::FAILED;
}

const std::map<int, MemberEntry>& MembershipList::getEntries() const {
    return entries_;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

void MembershipList::print() const {
    std::cout << "  MembershipList (ownId=" << ownId_ << "):\n";
    for (const auto& [id, entry] : entries_) {
        const char* statusStr =
            (entry.status == NodeStatus::ALIVE)     ? "ALIVE"     :
            (entry.status == NodeStatus::SUSPECTED) ? "SUSPECTED" : "FAILED";

        std::cout << "    node " << std::setw(2) << id
                  << "  hb=" << std::setw(4) << entry.heartbeatCounter
                  << "  lastSeen=" << std::fixed << std::setprecision(0)
                  << entry.localTimestamp << "ms"
                  << "  status=" << statusStr
                  << (id == ownId_ ? " (self)" : "") << "\n";
    }
}
