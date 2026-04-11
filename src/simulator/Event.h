#pragma once

// ---------------------------------------------------------------------------
// Event.h — Defines the EventType enum and the core Event struct.
//
// Every discrete action in the simulation is represented as an Event.
// Events are ordered by timestamp (smallest = earliest = highest priority).
// ---------------------------------------------------------------------------

enum class EventType {
    GOSSIP_ROUND,        // A node initiates one gossip round
    HEARTBEAT_INCREMENT, // A node increments its own heartbeat counter
    FAILURE_CHECK,       // A node scans its membership list for timed-out peers
    NODE_CRASH,          // Simulated node crash (injected by the experiment harness)
    CAN_TAKEOVER_TIMER,  // CAN zone takeover: timer has fired
    CAN_TAKEOVER_MSG,    // CAN zone takeover: message delivery
    TIMEOUT_PING,        // Pure-CAN baseline: periodic ping to a peer
    TIMEOUT_CHECK,       // Pure-CAN baseline: check whether a ping timed out
    RECOVERY_GOSSIP,     // Pure-Gossip baseline: recovery flood message
    SIMULATION_END       // Sentinel — tells the engine to stop
};

// Returns a human-readable name for an EventType (used in log output).
inline const char* eventTypeName(EventType t) {
    switch (t) {
        case EventType::GOSSIP_ROUND:        return "GOSSIP_ROUND";
        case EventType::HEARTBEAT_INCREMENT: return "HEARTBEAT_INCREMENT";
        case EventType::FAILURE_CHECK:       return "FAILURE_CHECK";
        case EventType::NODE_CRASH:          return "NODE_CRASH";
        case EventType::CAN_TAKEOVER_TIMER:  return "CAN_TAKEOVER_TIMER";
        case EventType::CAN_TAKEOVER_MSG:    return "CAN_TAKEOVER_MSG";
        case EventType::TIMEOUT_PING:        return "TIMEOUT_PING";
        case EventType::TIMEOUT_CHECK:       return "TIMEOUT_CHECK";
        case EventType::RECOVERY_GOSSIP:     return "RECOVERY_GOSSIP";
        case EventType::SIMULATION_END:      return "SIMULATION_END";
        default:                             return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Event — the fundamental unit of simulation state change.
//
// Fields:
//   timestamp    — simulation time (ms) at which this event fires
//   type         — what kind of action to perform
//   sourceNodeId — the node that owns / initiates this event
//   targetNodeId — the receiving node (-1 means broadcast or self-directed)
//   payload      — generic integer payload (round number, sequence id, etc.)
// ---------------------------------------------------------------------------
struct Event {
    double    timestamp;
    EventType type;
    int       sourceNodeId;
    int       targetNodeId; // -1 = broadcast / self
    int       payload;      // context-specific (e.g. round number)

    // Min-heap ordering: earlier timestamp → higher priority.
    // std::priority_queue with std::greater<Event> uses this operator.
    bool operator>(const Event& other) const {
        return timestamp > other.timestamp;
    }
};
