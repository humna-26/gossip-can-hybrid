// ---------------------------------------------------------------------------
// Simulator.cpp — Discrete-event simulation engine.
// ---------------------------------------------------------------------------

#include "Simulator.h"
#include "network/Message.h"   // g_msgCounters

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

Simulator::Simulator(double endTime)
    : currentTime_(0.0)
    , endTime_(endTime)
    , running_(false)
    , totalEventsProcessed_(0)
{}

// ---------------------------------------------------------------------------
// Node management
// ---------------------------------------------------------------------------

void Simulator::addNode(Node* node) {
    if (!node) return;
    if (node->id >= static_cast<int>(nodes_.size())) {
        nodes_.resize(static_cast<std::size_t>(node->id) + 1, nullptr);
    }
    nodes_[static_cast<std::size_t>(node->id)] = node;
    directHeartbeatTime_[node->id] = 0.0;   // initialise for TIMEOUT detection
}

Node* Simulator::getNode(int nodeId) const {
    if (nodeId < 0 || nodeId >= static_cast<int>(nodes_.size())) return nullptr;
    return nodes_[static_cast<std::size_t>(nodeId)];
}

bool Simulator::isNodeAlive(int nodeId) const {
    Node* n = getNode(nodeId);
    return n && n->isAlive();
}

int Simulator::countAliveNodes() const {
    int count = 0;
    for (Node* n : nodes_) { if (n && n->isAlive()) ++count; }
    return count;
}

// ---------------------------------------------------------------------------
// Event scheduling
// ---------------------------------------------------------------------------

void Simulator::scheduleEvent(const Event& event) {
    eventQueue_.push(event);
}

void Simulator::cancelEventsForNode(int nodeId) {
    Node* n = getNode(nodeId);
    if (n && n->isAlive()) {
        n->crash();
        std::cout << "[SIM] Node " << nodeId << " marked crashed at T="
                  << std::fixed << std::setprecision(1) << currentTime_ << " ms\n";
    }
}

// ---------------------------------------------------------------------------
// Phase 3 setup
// ---------------------------------------------------------------------------

void Simulator::setSeed(int seed) {
    rng_.seed(static_cast<std::mt19937::result_type>(seed));
}

void Simulator::setVerbose(bool v) {
    verbose_ = v;
}

void Simulator::setDetectionMode(DetectionMode m) { detectionMode_ = m; }
void Simulator::setRecoveryMode(RecoveryMode m)   { recoveryMode_  = m; }
void Simulator::setCANOverlay(CANOverlay* can)     { canOverlay_    = can; }

void Simulator::setGossipFanout(int fanout)  { gossipFanout_   = fanout; }
void Simulator::setGossipInterval(double ms) { gossipInterval_ = ms;     }
void Simulator::setSuspectTimeout(double ms) { suspectTimeout_ = ms;     }
void Simulator::setFailTimeout(double ms)    { failTimeout_    = ms;     }

void Simulator::initializeGossip() {
    int total = static_cast<int>(nodes_.size());
    for (Node* n : nodes_) {
        if (n) n->initGossip(total, gossipFanout_, &rng_);
    }
}

void Simulator::schedulePeriodicEvents() {
    for (Node* n : nodes_) {
        if (!n || !n->isAlive()) continue;

        // Small per-node desync jitter prevents all nodes firing at identical ticks.
        double jitter = n->id * 10.0;

        // HEARTBEAT_INCREMENT always fires regardless of detection mode.
        scheduleEvent({jitter, EventType::HEARTBEAT_INCREMENT, n->id, -1, 1});

        if (detectionMode_ == DetectionMode::GOSSIP) {
            // Gossip round offset by 250 ms to interleave with heartbeats.
            scheduleEvent({250.0 + jitter, EventType::GOSSIP_ROUND,   n->id, -1, 1});
            // Failure check starts after 4 warm-up rounds (~2 s).
            scheduleEvent({2000.0,         EventType::FAILURE_CHECK,  n->id, -1, 0});
        } else {
            // Timeout-based detection: direct heartbeat tracking.
            scheduleEvent({2000.0 + jitter, EventType::TIMEOUT_CHECK, n->id, -1, 0});
        }
    }

    // Simulation end sentinel.
    scheduleEvent({endTime_, EventType::SIMULATION_END, -1, -1, 0});
}

// ---------------------------------------------------------------------------
// Main simulation loop
// ---------------------------------------------------------------------------

void Simulator::run() {
    running_ = true;

    if (verbose_) {
        std::cout << "[SIM] Starting simulation (endTime=" << endTime_ << " ms)\n";
        std::cout << std::string(60, '-') << "\n";
    }

    while (!eventQueue_.empty()) {
        const Event next = eventQueue_.pop();
        currentTime_ = next.timestamp;

        if (currentTime_ > endTime_) {
            if (verbose_)
                std::cout << "[SIM] End time reached (" << endTime_ << " ms). Stopping.\n";
            break;
        }

        // Lazy deletion: skip events from dead nodes (except the crash itself).
        if (next.type != EventType::NODE_CRASH) {
            if (next.sourceNodeId >= 0 && !isNodeAlive(next.sourceNodeId)) continue;
        }

        dispatchEvent(next);
        ++totalEventsProcessed_;

        // SIMULATION_END advances currentTime_ past endTime_ — stop immediately
        // without popping another event (which would overwrite currentTime_).
        if (currentTime_ > endTime_) break;
    }

    running_ = false;

    if (verbose_) {
        std::cout << std::string(60, '-') << "\n";
    }
    std::cout << "[SIM] Simulation finished."
              << "  Events processed: " << totalEventsProcessed_
              << "  Final clock: " << std::fixed << std::setprecision(1)
              << currentTime_ << " ms\n";
}

// ---------------------------------------------------------------------------
// Event dispatch
// ---------------------------------------------------------------------------

void Simulator::dispatchEvent(const Event& event) {

    // Print event header only in verbose mode (suppressed for gossip experiments).
    if (verbose_) {
        std::cout << std::fixed << std::setprecision(1)
                  << "[T=" << std::setw(8) << event.timestamp << " ms] "
                  << std::setw(22) << std::left << eventTypeName(event.type)
                  << std::right
                  << "  src=" << event.sourceNodeId
                  << "  tgt=" << event.targetNodeId
                  << "  payload=" << event.payload;
    }

    switch (event.type) {

        // ------------------------------------------------------------------
        case EventType::HEARTBEAT_INCREMENT: {
            Node* n = getNode(event.sourceNodeId);
            if (n) {
                n->gossipLayer.incrementHeartbeat();

                if (detectionMode_ == DetectionMode::TIMEOUT) {
                    // Record direct broadcast time; count (N-1) peer messages.
                    directHeartbeatTime_[event.sourceNodeId] = currentTime_;
                    int peers = countAliveNodes() - 1;
                    if (peers > 0) {
                        g_msgCounters.detection_messages += peers;
                        g_msgCounters.total_messages     += peers;
                    }
                }
            }

            if (verbose_) std::cout << "  → hb=" <<
                (n && n->gossipLayer.isInitialized()
                    ? n->gossipLayer.getMembershipList().getOwnHeartbeat()
                    : -1) << "\n";

            // Self-reschedule.
            scheduleEvent({currentTime_ + heartbeatInterval_,
                           EventType::HEARTBEAT_INCREMENT,
                           event.sourceNodeId, -1, event.payload + 1});
            break;
        }

        // ------------------------------------------------------------------
        case EventType::GOSSIP_ROUND: {
            Node* src = getNode(event.sourceNodeId);
            if (!src) { if (verbose_) std::cout << "\n"; break; }

            auto targets = src->gossipLayer.selectGossipTargets();
            MembershipList msg = src->gossipLayer.prepareGossipMessage();

            for (int tgtId : targets) {
                Node* tgt = getNode(tgtId);
                if (!tgt || !tgt->isAlive()) continue;
                tgt->gossipLayer.receiveGossip(msg, currentTime_);
                g_msgCounters.recordDetectionMsg();
            }

            if (verbose_) {
                std::cout << "  → gossiped to [";
                for (int i = 0; i < static_cast<int>(targets.size()); ++i) {
                    if (i) std::cout << ",";
                    std::cout << targets[i];
                }
                std::cout << "]\n";
            }

            // Self-reschedule.
            scheduleEvent({currentTime_ + gossipInterval_,
                           EventType::GOSSIP_ROUND,
                           event.sourceNodeId, -1, event.payload + 1});
            break;
        }

        // ------------------------------------------------------------------
        case EventType::FAILURE_CHECK: {
            Node* n = getNode(event.sourceNodeId);
            if (!n) { if (verbose_) std::cout << "\n"; break; }

            auto newlyFailed = n->gossipLayer.checkFailures(
                currentTime_, suspectTimeout_, failTimeout_);

            if (verbose_) std::cout << "\n";

            for (int failedId : newlyFailed) {
                std::cout << "[DETECT] Node " << event.sourceNodeId
                          << " declared node " << failedId << " FAILED"
                          << " at T=" << std::fixed << std::setprecision(0)
                          << currentTime_ << " ms\n";

                // False-positive / true-positive accounting.
                Node* declaredNode = getNode(failedId);
                if (declaredNode && declaredNode->isAlive()) {
                    metricsCollector_.recordFalsePositive();
                } else {
                    metricsCollector_.recordCheck();
                }

                metricsCollector_.recordDetection(currentTime_, event.sourceNodeId);

                if (!firstDetectionTime_.count(failedId))
                    firstDetectionTime_[failedId] = currentTime_;
                detectionTracker_[failedId].insert(event.sourceNodeId);

                if (!majorityReached_[failedId]) {
                    int aliveCount = countAliveNodes();
                    int detected   = static_cast<int>(detectionTracker_[failedId].size());
                    if (detected * 2 >= aliveCount) {
                        majorityDetectionTime_[failedId] = currentTime_;
                        majorityReached_[failedId]       = true;
                        metricsCollector_.recordMajorityDetection(currentTime_);
                        initiateRecovery(failedId, currentTime_);
                        std::cout << "[DETECT] Majority reached for node " << failedId
                                  << " at T=" << std::fixed << std::setprecision(0)
                                  << currentTime_ << " ms  ("
                                  << detected << "/" << aliveCount << " nodes)\n";
                    }
                }
            }

            // Self-reschedule.
            scheduleEvent({currentTime_ + failureCheckInterval_,
                           EventType::FAILURE_CHECK,
                           event.sourceNodeId, -1, 0});
            break;
        }

        // ------------------------------------------------------------------
        case EventType::NODE_CRASH: {
            Node* n = getNode(event.sourceNodeId);
            if (n && n->isAlive()) {
                n->crash();
                metricsCollector_.recordCrash(currentTime_);
                std::cout << "[CRASH] Node " << event.sourceNodeId
                          << " crashed at T=" << std::fixed << std::setprecision(0)
                          << currentTime_ << " ms\n";
            } else {
                if (verbose_)
                    std::cout << "  → Node " << event.sourceNodeId
                              << " already dead (duplicate ignored)\n";
            }
            break;
        }

        // ------------------------------------------------------------------
        case EventType::TIMEOUT_CHECK: {
            Node* n = getNode(event.sourceNodeId);
            if (!n) { if (verbose_) std::cout << "\n"; break; }

            for (int j = 0; j < static_cast<int>(nodes_.size()); ++j) {
                if (j == n->id) continue;
                Node* peer = getNode(j);
                if (!peer) continue;

                // Skip peers already declared FAILED by this observer.
                if (detectionTracker_.count(j) &&
                    detectionTracker_[j].count(n->id)) continue;

                auto it = directHeartbeatTime_.find(j);
                if (it == directHeartbeatTime_.end()) continue;

                double gap = currentTime_ - it->second;
                if (gap <= failTimeout_) continue;

                // Declare peer failed via timeout.
                std::cout << "[DETECT] Node " << n->id
                          << " (timeout) declared node " << j << " FAILED"
                          << " at T=" << std::fixed << std::setprecision(0)
                          << currentTime_ << " ms\n";

                if (peer->isAlive()) {
                    metricsCollector_.recordFalsePositive();
                } else {
                    metricsCollector_.recordCheck();
                }

                metricsCollector_.recordDetection(currentTime_, n->id);
                if (!firstDetectionTime_.count(j)) firstDetectionTime_[j] = currentTime_;
                detectionTracker_[j].insert(n->id);

                if (!majorityReached_[j]) {
                    int aliveCount = countAliveNodes();
                    int detected   = static_cast<int>(detectionTracker_[j].size());
                    if (detected * 2 >= aliveCount) {
                        majorityDetectionTime_[j] = currentTime_;
                        majorityReached_[j]       = true;
                        metricsCollector_.recordMajorityDetection(currentTime_);
                        initiateRecovery(j, currentTime_);
                        std::cout << "[DETECT] Majority reached for node " << j
                                  << " at T=" << std::fixed << std::setprecision(0)
                                  << currentTime_ << " ms  ("
                                  << detected << "/" << aliveCount << " nodes)\n";
                    }
                }
            }

            if (verbose_) std::cout << "\n";

            // Self-reschedule.
            scheduleEvent({currentTime_ + failureCheckInterval_,
                           EventType::TIMEOUT_CHECK,
                           event.sourceNodeId, -1, 0});
            break;
        }

        // ------------------------------------------------------------------
        case EventType::RECOVERY_GOSSIP:
            // Gossip flood complete — record recovery.
            metricsCollector_.recordRecoveryComplete(currentTime_);
            if (verbose_) std::cout << "  → gossip flood recovery complete\n";
            break;

        // ------------------------------------------------------------------
        case EventType::CAN_TAKEOVER_TIMER:
            // CAN takeover routing complete — record recovery.
            metricsCollector_.recordRecoveryComplete(currentTime_);
            if (verbose_) std::cout << "  → CAN takeover complete\n";
            break;

        case EventType::CAN_TAKEOVER_MSG:
            if (verbose_) std::cout << "  → CAN takeover message delivered\n";
            break;

        case EventType::TIMEOUT_PING:
            if (verbose_) std::cout << "  → timeout ping\n";
            break;

        // ------------------------------------------------------------------
        case EventType::SIMULATION_END:
            if (verbose_)
                std::cout << "[T=" << std::setw(8) << std::fixed << std::setprecision(1)
                          << event.timestamp << " ms] SIMULATION_END\n";
            else
                std::cout << "[SIM] SIMULATION_END sentinel at T="
                          << std::fixed << std::setprecision(0)
                          << event.timestamp << " ms\n";
            currentTime_ = endTime_ + 1.0;
            break;

        default:
            if (verbose_) std::cout << "  → [WARN] Unknown event type\n";
            break;
    }
}

// ---------------------------------------------------------------------------
// Phase 4 — recovery initiation
// ---------------------------------------------------------------------------

void Simulator::initiateRecovery(int failedNodeId, double detectionTime) {
    if (recoveryInitiated_.count(failedNodeId)) return;
    recoveryInitiated_.insert(failedNodeId);

    int aliveCount = countAliveNodes();

    if (recoveryMode_ == RecoveryMode::GOSSIP_FLOOD) {
        // Flood to all alive nodes: O(N-1) recovery messages.
        int msgs = std::max(1, aliveCount - 1);
        g_msgCounters.recovery_messages += msgs;
        g_msgCounters.total_messages    += msgs;

        // Flood propagates in O(log N) gossip rounds.
        double logN  = std::max(1.0, std::log2(static_cast<double>(aliveCount)));
        double delay = logN * gossipInterval_;

        // Use sourceNodeId=-1 so lazy deletion does NOT skip this event
        // (the failed node is dead; its ID as source would be filtered out).
        scheduleEvent({detectionTime + delay,
                       EventType::RECOVERY_GOSSIP,
                       -1, failedNodeId, 0});

    } else if (recoveryMode_ == RecoveryMode::CAN_TAKEOVER) {
        if (!canOverlay_ || !canOverlay_->hasNode(failedNodeId)) return;

        TakeoverResult tr = takeoverManager_.executeTakeover(
            *canOverlay_, failedNodeId, detectionTime, routingDelayPerHop_);

        if (!tr.success) return;

        g_msgCounters.recovery_messages += tr.messagesSent;
        g_msgCounters.total_messages    += tr.messagesSent;

        // Use sourceNodeId=-1 for the same reason.
        scheduleEvent({tr.completionTime,
                       EventType::CAN_TAKEOVER_TIMER,
                       -1, tr.takerNodeId, 0});
    }
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

double Simulator::getCurrentTime()          const { return currentTime_; }
int    Simulator::getTotalEventsProcessed() const { return totalEventsProcessed_; }
bool   Simulator::isRunning()               const { return running_; }

int Simulator::getDetectionCount(int nodeId) const {
    auto it = detectionTracker_.find(nodeId);
    return (it != detectionTracker_.end()) ? static_cast<int>(it->second.size()) : 0;
}

double Simulator::getFirstDetectionTime(int nodeId) const {
    auto it = firstDetectionTime_.find(nodeId);
    return (it != firstDetectionTime_.end()) ? it->second : -1.0;
}

double Simulator::getMajorityDetectionTime(int nodeId) const {
    auto it = majorityDetectionTime_.find(nodeId);
    return (it != majorityDetectionTime_.end()) ? it->second : -1.0;
}

MetricsCollector& Simulator::getMetricsCollector() {
    return metricsCollector_;
}
