#pragma once

// ---------------------------------------------------------------------------
// MetricsCollector.h — Per-experiment metric tracking.
//
// One MetricsCollector instance is used per experiment run. The caller feeds
// in timing observations (crash time, first detection, majority detection,
// recovery complete) and at the end calls exportResult() to get a flat struct
// suitable for CSV output.
// ---------------------------------------------------------------------------

#include "network/Message.h"  // MessageCounters

#include <string>

// ---------------------------------------------------------------------------
// ExperimentResult — one row in the output CSV.
// ---------------------------------------------------------------------------
struct ExperimentResult {
    std::string system;          // "Hybrid", "PureGossip", "PureCAN"
    int         nodeCount;
    std::string scenario;        // "single", "dual", "cascading"
    int         seed;

    double detectionLatencyMs;   // firstDetectionTime - crashTime
    double recoveryTimeMs;       // recoveryCompleteTime - crashTime
    int    recoveryMessages;     // MessageCounters::recovery_messages
    int    totalMessages;        // MessageCounters::total_messages
    double falsePositiveRate;    // falsePositives / totalChecks (0.0 if no checks)
};

// ---------------------------------------------------------------------------
// MetricsCollector
// ---------------------------------------------------------------------------
class MetricsCollector {
public:
    MetricsCollector();

    // --- Feed observations into the collector ---

    // Call when the simulated crash is injected.
    void recordCrash(double time);

    // Call each time any node first detects the failure.
    // The collector records only the very first detection (smallest time).
    void recordDetection(double time, int detectingNodeId);

    // Call when ≥ 50 % of live nodes have detected the failure.
    void recordMajorityDetection(double time);

    // Call when the recovery protocol declares completion.
    void recordRecoveryComplete(double time);

    // --- False-positive tracking ---
    void recordFalsePositive(); // A live node was incorrectly flagged as dead
    void recordCheck();         // Any liveness check was performed

    // --- Export ---

    // Build and return the result struct using the recorded observations and
    // the provided message counters snapshot.
    ExperimentResult exportResult(const std::string& system,
                                  int                nodeCount,
                                  const std::string& scenario,
                                  int                seed,
                                  const MessageCounters& counters) const;

    // Reset all state so the collector can be reused for the next experiment.
    void reset();

private:
    double crashTime_;             // Time crash was injected (-1 = not set)
    double firstDetectionTime_;    // Time of very first detection (-1 = not set)
    double majorityDetectionTime_; // Time majority detected   (-1 = not set)
    double recoveryCompleteTime_;  // Time recovery finished   (-1 = not set)
    int    falsePositives_;
    int    totalChecks_;

    static constexpr double NOT_SET = -1.0;
};
