#pragma once

// ---------------------------------------------------------------------------
// Simulator.h — Discrete-event simulation engine (Phase 1–4).
// ---------------------------------------------------------------------------

#include "EventQueue.h"
#include "network/Node.h"
#include "metrics/MetricsCollector.h"
#include "can/CANOverlay.h"
#include "can/TakeoverManager.h"

#include <map>
#include <random>
#include <set>
#include <vector>

// ---------------------------------------------------------------------------
// Phase 4 — detection and recovery mode selectors
// ---------------------------------------------------------------------------
enum class DetectionMode { GOSSIP, TIMEOUT };
enum class RecoveryMode  { GOSSIP_FLOOD, CAN_TAKEOVER };

class Simulator {
public:
    explicit Simulator(double endTime);

    // -----------------------------------------------------------------------
    // Node management
    // -----------------------------------------------------------------------
    void  addNode(Node* node);
    Node* getNode(int nodeId) const;

    // -----------------------------------------------------------------------
    // Event scheduling
    // -----------------------------------------------------------------------
    void scheduleEvent(const Event& event);

    // Mark a node as crashed (lazy deletion — future events from it are skipped).
    void cancelEventsForNode(int nodeId);

    // -----------------------------------------------------------------------
    // Simulation control
    // -----------------------------------------------------------------------
    void run();

    // -----------------------------------------------------------------------
    // Phase 3 — gossip setup
    // -----------------------------------------------------------------------

    void setSeed(int seed);
    void setVerbose(bool v);
    void initializeGossip();

    // Schedule initial periodic events based on current detection mode:
    //   GOSSIP mode  : HEARTBEAT_INCREMENT + GOSSIP_ROUND + FAILURE_CHECK
    //   TIMEOUT mode : HEARTBEAT_INCREMENT + TIMEOUT_CHECK
    // Also schedules SIMULATION_END at endTime_.
    void schedulePeriodicEvents();

    // -----------------------------------------------------------------------
    // Phase 4 — system configuration
    // -----------------------------------------------------------------------

    void setDetectionMode(DetectionMode m);
    void setRecoveryMode(RecoveryMode m);

    // Provide the CAN overlay used for takeover-based recovery.
    // Must remain valid for the lifetime of the simulation run.
    void setCANOverlay(CANOverlay* can);

    // Sensitivity analysis: override default gossip parameters.
    // Must be called BEFORE initializeGossip() and schedulePeriodicEvents().
    void setGossipFanout(int fanout);
    void setGossipInterval(double ms);
    void setSuspectTimeout(double ms);
    void setFailTimeout(double ms);

    // -----------------------------------------------------------------------
    // Accessors
    // -----------------------------------------------------------------------
    double getCurrentTime()         const;
    int    getTotalEventsProcessed() const;
    bool   isRunning()              const;

    // Number of alive nodes at the time of query.
    int  countAliveNodes() const;

    // -----------------------------------------------------------------------
    // Detection-tracking queries (Phase 3)
    // -----------------------------------------------------------------------

    // How many distinct nodes have declared nodeId as FAILED.
    int    getDetectionCount(int nodeId)        const;

    // Simulation time at which the FIRST node declared nodeId FAILED (-1 = never).
    double getFirstDetectionTime(int nodeId)    const;

    // Simulation time at which MAJORITY (≥ 50%) declared nodeId FAILED (-1 = never).
    double getMajorityDetectionTime(int nodeId) const;

    MetricsCollector& getMetricsCollector();

private:
    // -----------------------------------------------------------------------
    // Core engine
    // -----------------------------------------------------------------------
    EventQueue         eventQueue_;
    double             currentTime_;
    double             endTime_;
    std::vector<Node*> nodes_;          // Indexed by node id
    bool               running_;
    int                totalEventsProcessed_;

    bool isNodeAlive(int nodeId) const;
    void dispatchEvent(const Event& event);

    // -----------------------------------------------------------------------
    // Phase 3 state
    // -----------------------------------------------------------------------
    std::mt19937     rng_;
    MetricsCollector metricsCollector_;
    bool             verbose_ = true;

    // Gossip protocol parameters
    int    gossipFanout_         = 3;
    double heartbeatInterval_    = 500.0;   // ms
    double gossipInterval_       = 500.0;   // ms
    double failureCheckInterval_ = 1000.0;  // ms
    double suspectTimeout_       = 2000.0;  // ms
    double failTimeout_          = 4000.0;  // ms

    // Detection tracking: failedNodeId -> set of node IDs that declared it FAILED
    std::map<int, std::set<int>> detectionTracker_;
    std::map<int, double>        firstDetectionTime_;
    std::map<int, double>        majorityDetectionTime_;
    std::map<int, bool>          majorityReached_;

    // -----------------------------------------------------------------------
    // Phase 4 state
    // -----------------------------------------------------------------------
    DetectionMode   detectionMode_     = DetectionMode::GOSSIP;
    RecoveryMode    recoveryMode_      = RecoveryMode::GOSSIP_FLOOD;
    CANOverlay*     canOverlay_        = nullptr;
    TakeoverManager takeoverManager_;
    double          routingDelayPerHop_ = 10.0;  // ms per CAN hop

    // TIMEOUT detection: last heartbeat broadcast time seen for each node.
    std::map<int, double> directHeartbeatTime_;

    // Prevent duplicate recovery for the same crashed node.
    std::set<int> recoveryInitiated_;

    // Trigger recovery protocol once majority has declared a node failed.
    void initiateRecovery(int failedNodeId, double detectionTime);
};
