// ---------------------------------------------------------------------------
// MetricsCollector.cpp — Per-experiment metric tracking implementation.
// ---------------------------------------------------------------------------

#include "MetricsCollector.h"

#include <iostream>

MetricsCollector::MetricsCollector() {
    reset();
}

void MetricsCollector::recordCrash(double time) {
    // Only record the first crash — later crashes in dual/cascading scenarios
    // would otherwise overwrite crashTime_ and distort latency calculations.
    if (crashTime_ == NOT_SET) crashTime_ = time;
}

void MetricsCollector::recordDetection(double time, int detectingNodeId) {
    // Keep only the earliest detection event.
    if (firstDetectionTime_ == NOT_SET || time < firstDetectionTime_) {
        firstDetectionTime_ = time;
        (void)detectingNodeId; // will be useful for per-node stats in later phases
    }
}

void MetricsCollector::recordMajorityDetection(double time) {
    majorityDetectionTime_ = time;
}

void MetricsCollector::recordRecoveryComplete(double time) {
    recoveryCompleteTime_ = time;
}

void MetricsCollector::recordFalsePositive() {
    falsePositives_++;
    totalChecks_++;
}

void MetricsCollector::recordCheck() {
    totalChecks_++;
}

ExperimentResult MetricsCollector::exportResult(const std::string& system,
                                                 int                nodeCount,
                                                 const std::string& scenario,
                                                 int                seed,
                                                 const MessageCounters& counters) const {
    ExperimentResult r;
    r.system   = system;
    r.nodeCount = nodeCount;
    r.scenario = scenario;
    r.seed     = seed;

    // Detection latency: time from crash to first detection.
    r.detectionLatencyMs = (crashTime_ != NOT_SET && firstDetectionTime_ != NOT_SET)
                           ? (firstDetectionTime_ - crashTime_)
                           : -1.0;

    // Recovery time: time from crash to recovery completion.
    r.recoveryTimeMs = (crashTime_ != NOT_SET && recoveryCompleteTime_ != NOT_SET)
                       ? (recoveryCompleteTime_ - crashTime_)
                       : -1.0;

    r.recoveryMessages = counters.recovery_messages;
    r.totalMessages    = counters.total_messages;

    r.falsePositiveRate = (totalChecks_ > 0)
                          ? (static_cast<double>(falsePositives_) / totalChecks_)
                          : 0.0;

    return r;
}

void MetricsCollector::reset() {
    crashTime_             = NOT_SET;
    firstDetectionTime_    = NOT_SET;
    majorityDetectionTime_ = NOT_SET;
    recoveryCompleteTime_  = NOT_SET;
    falsePositives_        = 0;
    totalChecks_           = 0;
}
