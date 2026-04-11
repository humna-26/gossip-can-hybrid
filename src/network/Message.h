#pragma once

// ---------------------------------------------------------------------------
// Message.h — Message type enum and global message counters.
//
// MessageCounters is a singleton-style global that every part of the
// simulation increments whenever a logical message is "sent".
// This lets us compare protocol costs across experiments without threading
// complications (single-threaded DES — no locks needed).
// ---------------------------------------------------------------------------

#include <string>

// Broad categories of messages exchanged between nodes.
enum class MessageType {
    GOSSIP_HEARTBEAT,   // Periodic heartbeat piggybacked on gossip
    GOSSIP_RECOVERY,    // Gossip-flood recovery message
    CAN_ROUTE,          // CAN routing/lookup message
    CAN_TAKEOVER,       // CAN zone takeover during recovery
    TIMEOUT_PING,       // Pure-CAN baseline: ping probe
    TIMEOUT_PONG,       // Pure-CAN baseline: pong reply
    HYBRID_DETECTION,   // Hybrid: gossip-based detection message
    HYBRID_RECOVERY     // Hybrid: CAN-routed recovery message
};

// ---------------------------------------------------------------------------
// MessageCounters — tracks detection vs recovery message costs separately.
//
// Use recordDetectionMsg() for heartbeat / failure-detection traffic.
// Use recordRecoveryMsg()  for recovery-phase traffic only.
// Both also increment total_messages.
// ---------------------------------------------------------------------------
struct MessageCounters {
    int detection_messages = 0; // Heartbeat / failure-detection traffic
    int recovery_messages  = 0; // Recovery-phase traffic only
    int total_messages     = 0; // Sum of all messages across both phases

    void recordDetectionMsg() {
        detection_messages++;
        total_messages++;
    }

    void recordRecoveryMsg() {
        recovery_messages++;
        total_messages++;
    }

    // Convenience: record a generic message that belongs to neither specific
    // phase (e.g. routing overhead counted separately).
    void recordOtherMsg() {
        total_messages++;
    }

    void reset() {
        detection_messages = 0;
        recovery_messages  = 0;
        total_messages     = 0;
    }
};

// ---------------------------------------------------------------------------
// Global singleton instance — include this header and use g_msgCounters
// from anywhere in the simulation.
// (inline variable: one definition across all translation units, C++17)
// ---------------------------------------------------------------------------
inline MessageCounters g_msgCounters;
