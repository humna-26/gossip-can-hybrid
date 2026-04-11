// ---------------------------------------------------------------------------
// main.cpp — Phase 1–4 test harness + 225-experiment comparison runner.
// ---------------------------------------------------------------------------

#include "simulator/Simulator.h"
#include "network/Node.h"
#include "network/Message.h"
#include "can/CANOverlay.h"
#include "can/Zone.h"
#include "metrics/CSVExporter.h"

#include <cassert>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <random>
#include <string>
#include <vector>

// ===========================================================================
// Shared utilities
// ===========================================================================

static void banner(const char* title) {
    std::cout << "\n" << std::string(66, '=') << "\n"
              << "  " << title << "\n"
              << std::string(66, '=') << "\n";
}
static void section(const char* title) {
    std::cout << "\n  --- " << title << " ---\n";
}

static int gPassed = 0;
static int gFailed = 0;

static void check(bool cond, const char* msg) {
    if (cond) { std::cout << "  [PASS] " << msg << "\n"; ++gPassed; }
    else       { std::cout << "  [FAIL] " << msg << "\n"; ++gFailed; }
}

// ===========================================================================
// Phase 1 — Event engine smoke test
// ===========================================================================

static void scheduleHeartbeats(Simulator& sim, const std::vector<Node*>& nodes,
                                double start, double end, double interval) {
    for (const Node* n : nodes) {
        int round = 1;
        for (double t = start; t <= end; t += interval, ++round) {
            Event e;
            e.timestamp=t; e.type=EventType::HEARTBEAT_INCREMENT;
            e.sourceNodeId=n->id; e.targetNodeId=-1; e.payload=round;
            sim.scheduleEvent(e);
        }
    }
}

static void runPhase1Test() {
    banner("Phase 1 — Discrete Event Simulator Test");

    g_msgCounters.reset();

    constexpr double SIM_END = 5000.0;
    Simulator sim(SIM_END);
    sim.setVerbose(true);

    std::vector<Node*> nodes;
    for (int i = 0; i < 5; ++i) { nodes.push_back(new Node(i)); sim.addNode(nodes.back()); }

    scheduleHeartbeats(sim, nodes, 500.0, 5000.0, 500.0);

    { Event e; e.timestamp=2500; e.type=EventType::NODE_CRASH;
      e.sourceNodeId=2; e.targetNodeId=-1; e.payload=0; sim.scheduleEvent(e); }

    for (int i = 0; i < 5; ++i) {
        Event e; e.timestamp=1000; e.type=EventType::GOSSIP_ROUND;
        e.sourceNodeId=i; e.targetNodeId=-1; e.payload=1; sim.scheduleEvent(e);
    }
    for (int i = 0; i < 5; ++i) {
        Event e; e.timestamp=3000; e.type=EventType::FAILURE_CHECK;
        e.sourceNodeId=i; e.targetNodeId=-1; e.payload=0; sim.scheduleEvent(e);
    }
    { Event e; e.timestamp=SIM_END; e.type=EventType::SIMULATION_END;
      e.sourceNodeId=-1; e.targetNodeId=-1; e.payload=0; sim.scheduleEvent(e); }

    g_msgCounters.recordDetectionMsg();
    g_msgCounters.recordDetectionMsg();
    g_msgCounters.recordDetectionMsg();
    g_msgCounters.recordRecoveryMsg();

    sim.run();

    banner("Phase 1 — Assertions");
    check(!nodes[2]->isAlive(),  "Node 2 is dead after NODE_CRASH");
    check(nodes[0]->isAlive(),   "Node 0 still alive");
    check(nodes[1]->isAlive(),   "Node 1 still alive");
    check(nodes[3]->isAlive(),   "Node 3 still alive");
    check(nodes[4]->isAlive(),   "Node 4 still alive");
    check(g_msgCounters.detection_messages == 3, "Detection message counter = 3");
    check(g_msgCounters.recovery_messages  == 1, "Recovery message counter = 1");
    check(g_msgCounters.total_messages     == 4, "Total message counter = 4");
    check(sim.getCurrentTime() <= SIM_END + 1.0, "Final clock <= endTime");

    for (Node* n : nodes) delete n;
}

// ===========================================================================
// Phase 2 — CAN overlay tests
// ===========================================================================

static void runPhase2Test() {
    banner("Phase 2 — CAN Overlay Test");

    section("1. Initialize CAN with 8 nodes");
    CANOverlay can;
    can.initialize(8);
    check(can.getNodeCount() == 8, "Overlay has 8 nodes after initialize");

    section("2. Zones and neighbour table");
    can.printState();

    section("3. Integrity check — initial");
    check(can.verifyIntegrity(),
          "Initial integrity: full coverage, no overlaps, symmetric neighbours");

    section("4. Routing — (0.7, 0.7) from node 0");
    {
        std::vector<int> path = can.routeToPoint(0, 0.7, 0.7);
        int dest = path.back(), owner = can.findZoneOwner(0.7, 0.7);
        std::cout << "  Route: ";
        for (int i = 0; i < (int)path.size(); ++i) { if(i) std::cout<<" → "; std::cout<<path[i]; }
        std::cout << "  (hops=" << path.size()-1 << ")\n";
        std::cout << "  Dest zone: " << can.getZone(dest).toString() << "\n";
        check(dest == owner,                            "Route ends at zone owner");
        check(can.getZone(dest).contains(0.7, 0.7),    "Dest zone contains target");
        check((int)path.size()-1 <= 6,                 "Hop count <= 6");
    }

    section("5. Routing — 5 random points (seed=42)");
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<double> dist(0.01, 0.99);
        int ok = 0;
        for (int t = 0; t < 5; ++t) {
            double tx = dist(rng), ty = dist(rng);
            std::vector<int> path = can.routeToPoint(0, tx, ty);
            int dest = path.back(), owner = can.findZoneOwner(tx, ty);
            bool correct = (dest == owner) && can.getZone(dest).contains(tx, ty);
            if (correct) ++ok;
            std::cout << "  Trial " << t+1 << " (" << std::fixed << std::setprecision(3)
                      << tx << "," << ty << ") hops=" << path.size()-1
                      << " dest=node" << dest << (correct?" OK":" FAIL") << "\n";
        }
        check(ok == 5, "All 5 random routing trials correct");
    }

    section("6. Node JOIN — node 8 at (0.3, 0.7)");
    {
        int hops = can.nodeJoin(8, 0.3, 0.7);
        std::cout << "  JOIN hops=" << hops << "  Node 8 zone: "
                  << can.getZone(8).toString() << "\n";
        check(can.getNodeCount() == 9,                   "9 nodes after JOIN");
        check(can.getZone(8).contains(0.3, 0.7),         "Node 8 zone contains join point");
    }

    section("7. Updated zones after JOIN");
    can.printState();

    section("8. Integrity after JOIN");
    check(can.verifyIntegrity(), "Post-JOIN integrity OK");

    section("9. Remove node 3");
    {
        std::vector<int> nbs3 = can.getNeighbours(3);
        std::cout << "  Node 3 zone: " << can.getZone(3).toString() << "\n";
        Zone failedZone = can.removeNode(3);
        check(!can.hasNode(3),          "Node 3 removed from overlay");
        check(can.getNodeCount() == 8,  "8 nodes after removal");
        bool gone = true;
        for (int id : {0,1,2,4,5,6,7,8}) {
            if (!can.hasNode(id)) continue;
            for (int nb : can.getNeighbours(id)) if (nb==3) { gone=false; break; }
        }
        check(gone, "Node 3 absent from all neighbour lists");

        section("10. Takeover — smallest-volume neighbour takes node 3's zone");
        std::vector<int> alive;
        for (int nb : nbs3) if (can.hasNode(nb)) alive.push_back(nb);
        check(!alive.empty(), "At least one neighbour still alive");
        if (!alive.empty()) {
            int taker = alive[0];
            for (int nb : alive)
                if (can.getZone(nb).volume() < can.getZone(taker).volume()) taker = nb;
            std::cout << "  Taker: node " << taker << "  "
                      << can.getZone(taker).toString() << "\n";
            can.performTakeover(taker, failedZone);
            std::cout << "  Merged: " << can.getZone(taker).toString() << "\n";
            check(can.getNodeCount() == 8, "8 nodes after takeover");

            section("11. Integrity after takeover");
            can.printState();
            check(can.verifyIntegrity(), "Post-takeover integrity OK");
        }
    }
}

// ===========================================================================
// Phase 3 — Gossip failure detection test
// ===========================================================================

static void runPhase3Test() {
    banner("Phase 3 — Gossip Failure Detection Test");

    constexpr int    SEED        = 42;
    constexpr int    N_NODES     = 10;
    constexpr int    CRASH_NODE  = 5;
    constexpr double CRASH_TIME  = 5000.0;
    constexpr double SIM_END     = 15000.0;

    g_msgCounters.reset();

    Simulator sim(SIM_END);
    sim.setSeed(SEED);
    sim.setVerbose(false);

    std::vector<Node*> nodes;
    for (int i = 0; i < N_NODES; ++i) {
        nodes.push_back(new Node(i));
        sim.addNode(nodes.back());
    }

    sim.initializeGossip();
    sim.schedulePeriodicEvents();

    {
        Event crash;
        crash.timestamp    = CRASH_TIME;
        crash.type         = EventType::NODE_CRASH;
        crash.sourceNodeId = CRASH_NODE;
        crash.targetNodeId = -1;
        crash.payload      = 0;
        sim.scheduleEvent(crash);
    }

    section("Running 15-second gossip simulation (10 nodes, node 5 crashes at T=5000ms)");
    sim.run();

    section("Gossip Experiment Summary");

    int    detectors       = sim.getDetectionCount(CRASH_NODE);
    double firstDetect     = sim.getFirstDetectionTime(CRASH_NODE);
    double majorityDetect  = sim.getMajorityDetectionTime(CRASH_NODE);
    int    aliveAfterCrash = N_NODES - 1;

    std::cout << std::fixed << std::setprecision(0);
    std::cout << "  Crashed node       : " << CRASH_NODE
              << " at T=" << CRASH_TIME << " ms\n";
    std::cout << "  Alive nodes        : " << aliveAfterCrash << "\n";
    std::cout << "  Detectors (FAILED) : " << detectors
              << " / " << aliveAfterCrash << " nodes\n";
    std::cout << "  First detection    : T="
              << (firstDetect >= 0 ? firstDetect : -1) << " ms\n";
    std::cout << "  Majority detection : T="
              << (majorityDetect >= 0 ? majorityDetect : -1) << " ms\n";
    std::cout << "  Detection messages : " << g_msgCounters.detection_messages << "\n";
    std::cout << "  Total messages     : " << g_msgCounters.total_messages << "\n";
    std::cout << "  Events processed   : " << sim.getTotalEventsProcessed() << "\n";

    section("Per-node view of node 5 membership status");
    for (Node* n : nodes) {
        if (!n || n->id == CRASH_NODE) continue;
        const auto& ml      = n->gossipLayer.getMembershipList();
        NodeStatus  status  = ml.getStatus(CRASH_NODE);
        const char* label   = (status == NodeStatus::ALIVE)     ? "ALIVE"
                            : (status == NodeStatus::SUSPECTED) ? "SUSPECTED"
                            :                                      "FAILED";
        int hb = ml.getEntries().count(CRASH_NODE)
               ? ml.getEntries().at(CRASH_NODE).heartbeatCounter : -1;
        std::cout << "  Node " << std::setw(2) << n->id
                  << " sees node " << CRASH_NODE
                  << " as " << std::setw(9) << std::left << label
                  << std::right << "  (last hb=" << hb << ")\n";
    }

    section("Phase 3 Assertions");
    check(detectors * 2 >= aliveAfterCrash,
          "Node 5 detected as FAILED by >= 50% of alive nodes");
    check(firstDetect >= 7000.0 && firstDetect <= 12000.0,
          "First detection time in expected window [T=7000, T=12000] ms");
    check(g_msgCounters.detection_messages > 0, "Detection messages > 0");

    bool noFalsePositives = true;
    for (int suspect = 0; suspect < N_NODES; ++suspect) {
        if (suspect == CRASH_NODE) continue;
        if (sim.getDetectionCount(suspect) > 0) {
            noFalsePositives = false;
            std::cout << "  [NOTE] Node " << suspect
                      << " was also detected as failed (possible false positive)\n";
        }
    }
    check(noFalsePositives, "No false positives — only node 5 detected as failed");

    for (Node* n : nodes) delete n;
}

// ===========================================================================
// Phase 4 — 225-experiment comparison runner
// ===========================================================================

struct ExperimentConfig {
    std::string system;    // "PureGossip", "PureCAN", "Hybrid"
    int         nodeCount;
    std::string scenario;  // "single", "dual", "cascading"
    int         seed;
};

static ExperimentResult runExperiment(const ExperimentConfig& cfg) {
    constexpr double SIM_END      = 30000.0;
    constexpr double CRASH_TIME_1 = 10000.0;
    constexpr double CRASH_TIME_2 = 12000.0;
    constexpr double CRASH_TIME_3 = 14000.0;

    g_msgCounters.reset();

    // Pick crash node IDs spread across the node space.
    int cn0 = cfg.nodeCount / 5;
    int cn1 = cfg.nodeCount / 5 + 1;
    int cn2 = cfg.nodeCount / 5 + 2;

    Simulator sim(SIM_END);
    sim.setSeed(cfg.seed);
    sim.setVerbose(false);

    std::vector<Node*> nodes;
    nodes.reserve(cfg.nodeCount);
    for (int i = 0; i < cfg.nodeCount; ++i) {
        nodes.push_back(new Node(i));
        sim.addNode(nodes.back());
    }

    // Configure mode.
    bool needCAN = (cfg.system == "PureCAN" || cfg.system == "Hybrid");

    std::unique_ptr<CANOverlay> canOverlay;
    if (needCAN) {
        canOverlay = std::make_unique<CANOverlay>();
        canOverlay->initialize(cfg.nodeCount);
        sim.setCANOverlay(canOverlay.get());
        sim.setRecoveryMode(RecoveryMode::CAN_TAKEOVER);
    } else {
        sim.setRecoveryMode(RecoveryMode::GOSSIP_FLOOD);
    }

    if (cfg.system == "PureCAN") {
        sim.setDetectionMode(DetectionMode::TIMEOUT);
    } else {
        sim.setDetectionMode(DetectionMode::GOSSIP);
    }

    sim.initializeGossip();
    sim.schedulePeriodicEvents();

    // Inject crashes.
    auto scheduleCrash = [&](int nodeId, double time) {
        Event e;
        e.timestamp    = time;
        e.type         = EventType::NODE_CRASH;
        e.sourceNodeId = nodeId;
        e.targetNodeId = -1;
        e.payload      = 0;
        sim.scheduleEvent(e);
    };

    scheduleCrash(cn0, CRASH_TIME_1);
    if (cfg.scenario == "dual" || cfg.scenario == "cascading")
        scheduleCrash(cn1, CRASH_TIME_2);
    if (cfg.scenario == "cascading")
        scheduleCrash(cn2, CRASH_TIME_3);

    sim.run();

    ExperimentResult result = sim.getMetricsCollector().exportResult(
        cfg.system, cfg.nodeCount, cfg.scenario, cfg.seed, g_msgCounters);

    for (Node* n : nodes) delete n;
    return result;
}

// ---------------------------------------------------------------------------
// Summary table: averages grouped by (system, nodeCount)
// ---------------------------------------------------------------------------
static void printSummaryTable(const std::vector<ExperimentResult>& results) {
    section("Average metrics by system × node count");

    // Accumulate sums; track separate counts for optional metrics.
    struct Acc {
        double detLatency = 0, recTime = 0, recMsgs = 0, totMsgs = 0;
        int    count = 0, detCount = 0, recCount = 0;
    };
    std::map<std::string, std::map<int, Acc>> acc;

    for (const auto& r : results) {
        auto& a = acc[r.system][r.nodeCount];
        ++a.count;
        a.recMsgs += r.recoveryMessages;
        a.totMsgs += r.totalMessages;
        if (r.detectionLatencyMs >= 0) { a.detLatency += r.detectionLatencyMs; ++a.detCount; }
        if (r.recoveryTimeMs     >= 0) { a.recTime    += r.recoveryTimeMs;     ++a.recCount; }
    }

    // Header.
    std::cout << "\n  "
              << std::left  << std::setw(12) << "System"
              << std::right << std::setw(6)  << "N"
              << std::setw(14) << "DetLat(ms)"
              << std::setw(14) << "RecTime(ms)"
              << std::setw(12) << "RecMsgs"
              << std::setw(14) << "TotMsgs" << "\n";
    std::cout << "  " << std::string(72, '-') << "\n";

    for (const auto& [sys, byN] : acc) {
        for (const auto& [n, a] : byN) {
            if (a.count == 0) continue;
            std::cout << "  "
                      << std::left  << std::setw(12) << sys
                      << std::right << std::setw(6)  << n
                      << std::fixed << std::setprecision(0);
            if (a.detCount > 0)
                std::cout << std::setw(14) << a.detLatency / a.detCount;
            else
                std::cout << std::setw(14) << "N/A";
            if (a.recCount > 0)
                std::cout << std::setw(14) << a.recTime / a.recCount;
            else
                std::cout << std::setw(14) << "N/A";
            std::cout << std::setw(12) << static_cast<int>(a.recMsgs / a.count)
                      << std::setw(14) << static_cast<int>(a.totMsgs / a.count)
                      << "\n";
        }
    }
}

// ===========================================================================
// Phase 6A — Sensitivity Analysis
// ===========================================================================

struct SensitivityConfig {
    std::string system           = "Hybrid";
    int         nodeCount        = 100;
    int         seed             = 0;
    int         gossipFanout     = 3;
    double      gossipIntervalMs = 500.0;
    double      suspectTimeoutMs = 2000.0;
    double      failTimeoutMs    = 4000.0;
};

static ExperimentResult runSensitivityExperiment(const SensitivityConfig& cfg) {
    constexpr double CRASH_TIME = 10000.0;
    // Give enough headroom for slow-timeout configs: crash + 4× T_fail, min 30 s.
    double simEnd = std::max(30000.0, CRASH_TIME + cfg.failTimeoutMs * 4.0);

    g_msgCounters.reset();

    int cn0 = std::max(0, cfg.nodeCount / 5);

    Simulator sim(simEnd);
    sim.setSeed(cfg.seed);
    sim.setVerbose(false);

    std::vector<Node*> nodes;
    nodes.reserve(cfg.nodeCount);
    for (int i = 0; i < cfg.nodeCount; ++i) {
        nodes.push_back(new Node(i));
        sim.addNode(nodes.back());
    }

    bool needCAN = (cfg.system == "PureCAN" || cfg.system == "Hybrid");
    std::unique_ptr<CANOverlay> canOverlay;
    if (needCAN) {
        canOverlay = std::make_unique<CANOverlay>();
        canOverlay->initialize(cfg.nodeCount);
        sim.setCANOverlay(canOverlay.get());
        sim.setRecoveryMode(RecoveryMode::CAN_TAKEOVER);
    } else {
        sim.setRecoveryMode(RecoveryMode::GOSSIP_FLOOD);
    }

    if (cfg.system == "PureCAN") {
        sim.setDetectionMode(DetectionMode::TIMEOUT);
    } else {
        sim.setDetectionMode(DetectionMode::GOSSIP);
    }

    // Set sensitivity parameters BEFORE initializeGossip / schedulePeriodicEvents.
    sim.setGossipFanout(cfg.gossipFanout);
    sim.setGossipInterval(cfg.gossipIntervalMs);
    sim.setSuspectTimeout(cfg.suspectTimeoutMs);
    sim.setFailTimeout(cfg.failTimeoutMs);

    sim.initializeGossip();
    sim.schedulePeriodicEvents();

    // Single failure.
    Event crash;
    crash.timestamp    = CRASH_TIME;
    crash.type         = EventType::NODE_CRASH;
    crash.sourceNodeId = cn0;
    crash.targetNodeId = -1;
    crash.payload      = 0;
    sim.scheduleEvent(crash);

    sim.run();

    ExperimentResult result = sim.getMetricsCollector().exportResult(
        cfg.system, cfg.nodeCount, "single", cfg.seed, g_msgCounters);

    for (Node* n : nodes) delete n;
    return result;
}

static void runSensitivityAnalysis() {
    std::cout << "\n" << std::string(66, '=') << "\n"
              << "  === Running Sensitivity Analysis (Phase 6A) ===\n"
              << std::string(66, '=') << "\n";

    std::filesystem::create_directories("results");

    constexpr int    SEEDS           = 5;
    constexpr int    N_DEFAULT       = 100;
    constexpr int    FANOUT_DEFAULT  = 3;
    constexpr double T_GOSSIP_DEF   = 500.0;
    constexpr double T_SUSPECT_DEF  = 2000.0;
    constexpr double T_FAIL_DEF     = 4000.0;

    // -----------------------------------------------------------------------
    // Set A: Vary Gossip Fan-out (b = 1..6)
    // -----------------------------------------------------------------------
    {
        const std::vector<int> FANOUTS = {1, 2, 3, 4, 5, 6};
        const int total = static_cast<int>(FANOUTS.size()) * SEEDS;
        int done = 0;
        std::cout << "\nSet A (Fan-out variation): 0/" << total << " complete\n";

        std::ofstream csv("results/sensitivity_fanout.csv");
        csv << "fanout,seed,detection_latency_ms,recovery_messages,"
               "total_messages,false_positive_rate\n";

        for (int b : FANOUTS) {
            for (int seed = 1; seed <= SEEDS; ++seed) {
                SensitivityConfig cfg;
                cfg.system           = "Hybrid";
                cfg.nodeCount        = N_DEFAULT;
                cfg.seed             = seed;
                cfg.gossipFanout     = b;
                cfg.gossipIntervalMs = T_GOSSIP_DEF;
                cfg.suspectTimeoutMs = T_SUSPECT_DEF;
                cfg.failTimeoutMs    = T_FAIL_DEF;

                ExperimentResult r = runSensitivityExperiment(cfg);
                csv << b << "," << seed << ","
                    << std::fixed << std::setprecision(1) << r.detectionLatencyMs << ","
                    << r.recoveryMessages << ","
                    << r.totalMessages << ","
                    << std::setprecision(6) << r.falsePositiveRate << "\n";

                ++done;
                if (done % 10 == 0 || done == total)
                    std::cout << "Set A (Fan-out variation): "
                              << done << "/" << total << " complete\n";
            }
        }
        std::cout << "  -> results/sensitivity_fanout.csv\n";
    }

    // -----------------------------------------------------------------------
    // Set B: Vary Failure Timeout Threshold
    // -----------------------------------------------------------------------
    {
        const std::vector<double> FAIL_TIMEOUTS = {
            1000.0, 2000.0, 3000.0, 4000.0, 5000.0, 6000.0, 8000.0};
        const int total = static_cast<int>(FAIL_TIMEOUTS.size()) * SEEDS;
        int done = 0;
        std::cout << "\nSet B (Timeout variation): 0/" << total << " complete\n";

        std::ofstream csv("results/sensitivity_timeout.csv");
        csv << "fail_timeout_ms,seed,detection_latency_ms,"
               "false_positive_rate,recovery_messages\n";

        for (double tf : FAIL_TIMEOUTS) {
            double ts = tf / 2.0;   // T_suspect = T_fail / 2
            for (int seed = 1; seed <= SEEDS; ++seed) {
                SensitivityConfig cfg;
                cfg.system           = "Hybrid";
                cfg.nodeCount        = N_DEFAULT;
                cfg.seed             = seed;
                cfg.gossipFanout     = FANOUT_DEFAULT;
                cfg.gossipIntervalMs = T_GOSSIP_DEF;
                cfg.suspectTimeoutMs = ts;
                cfg.failTimeoutMs    = tf;

                ExperimentResult r = runSensitivityExperiment(cfg);
                csv << std::fixed << std::setprecision(0) << tf << ","
                    << seed << ","
                    << std::setprecision(1) << r.detectionLatencyMs << ","
                    << std::setprecision(6) << r.falsePositiveRate << ","
                    << r.recoveryMessages << "\n";

                ++done;
                if (done % 10 == 0 || done == total)
                    std::cout << "Set B (Timeout variation): "
                              << done << "/" << total << " complete\n";
            }
        }
        std::cout << "  -> results/sensitivity_timeout.csv\n";
    }

    // -----------------------------------------------------------------------
    // Set C: Fine-Grained Scalability (all 3 systems)
    // -----------------------------------------------------------------------
    {
        const std::vector<int> NODE_COUNTS = {
            10, 20, 30, 40, 50, 75, 100, 150, 200, 250, 300};
        const std::vector<std::string> SYSTEMS = {"Hybrid", "PureGossip", "PureCAN"};
        const int total = static_cast<int>(NODE_COUNTS.size())
                        * static_cast<int>(SYSTEMS.size()) * SEEDS;
        int done = 0;
        std::cout << "\nSet C (Fine-grained scalability): 0/" << total << " complete\n";

        std::ofstream csv("results/sensitivity_scalability.csv");
        csv << "system,node_count,seed,recovery_messages\n";

        for (int n : NODE_COUNTS) {
            for (const auto& sys : SYSTEMS) {
                for (int seed = 1; seed <= SEEDS; ++seed) {
                    SensitivityConfig cfg;
                    cfg.system           = sys;
                    cfg.nodeCount        = n;
                    cfg.seed             = seed;
                    cfg.gossipFanout     = FANOUT_DEFAULT;
                    cfg.gossipIntervalMs = T_GOSSIP_DEF;
                    cfg.suspectTimeoutMs = T_SUSPECT_DEF;
                    cfg.failTimeoutMs    = T_FAIL_DEF;

                    ExperimentResult r = runSensitivityExperiment(cfg);
                    csv << sys << "," << n << "," << seed << ","
                        << r.recoveryMessages << "\n";

                    ++done;
                    if (done % 15 == 0 || done == total)
                        std::cout << "Set C (Fine-grained scalability): "
                                  << done << "/" << total << " complete\n";
                }
            }
        }
        std::cout << "  -> results/sensitivity_scalability.csv\n";
    }

    // -----------------------------------------------------------------------
    // Set D: Vary Gossip Interval
    // -----------------------------------------------------------------------
    {
        const std::vector<double> GOSSIP_INTERVALS = {100.0, 250.0, 500.0, 1000.0, 2000.0};
        const int total = static_cast<int>(GOSSIP_INTERVALS.size()) * SEEDS;
        int done = 0;
        std::cout << "\nSet D (Gossip interval variation): 0/" << total << " complete\n";

        std::ofstream csv("results/sensitivity_interval.csv");
        csv << "gossip_interval_ms,seed,detection_latency_ms,"
               "total_messages,false_positive_rate\n";

        for (double tg : GOSSIP_INTERVALS) {
            double tf = 8.0 * tg;   // T_fail = 8 × T_gossip
            double ts = tf / 2.0;   // T_suspect = T_fail / 2
            for (int seed = 1; seed <= SEEDS; ++seed) {
                SensitivityConfig cfg;
                cfg.system           = "Hybrid";
                cfg.nodeCount        = N_DEFAULT;
                cfg.seed             = seed;
                cfg.gossipFanout     = FANOUT_DEFAULT;
                cfg.gossipIntervalMs = tg;
                cfg.suspectTimeoutMs = ts;
                cfg.failTimeoutMs    = tf;

                ExperimentResult r = runSensitivityExperiment(cfg);
                csv << std::fixed << std::setprecision(0) << tg << ","
                    << seed << ","
                    << std::setprecision(1) << r.detectionLatencyMs << ","
                    << r.totalMessages << ","
                    << std::setprecision(6) << r.falsePositiveRate << "\n";

                ++done;
                if (done % 10 == 0 || done == total)
                    std::cout << "Set D (Gossip interval variation): "
                              << done << "/" << total << " complete\n";
            }
        }
        std::cout << "  -> results/sensitivity_interval.csv\n";
    }

    // Summary
    std::cout << "\n" << std::string(66, '=') << "\n";
    std::cout << "  Set A (Fan-out variation):        30/30 complete\n";
    std::cout << "  Set B (Timeout variation):        35/35 complete\n";
    std::cout << "  Set C (Fine-grained scalability): 165/165 complete\n";
    std::cout << "  Set D (Gossip interval):          25/25 complete\n";
    std::cout << "  Total: 255 experiments completed\n";
    std::cout << "  Results saved to: results/sensitivity_*.csv\n";
    std::cout << std::string(66, '=') << "\n";
}

// ===========================================================================
// Entry point
// ===========================================================================

int main(int argc, char* argv[]) {
    // -----------------------------------------------------------------------
    // Sensitivity analysis mode: ./gossip_can_hybrid --sensitivity
    // -----------------------------------------------------------------------
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--sensitivity") {
            runSensitivityAnalysis();
            return 0;
        }
    }

    // -----------------------------------------------------------------------
    // Phases 1–3: correctness tests
    // -----------------------------------------------------------------------
    runPhase1Test();
    runPhase2Test();
    runPhase3Test();

    // -----------------------------------------------------------------------
    // Phase 4: 225-experiment comparison
    // -----------------------------------------------------------------------
    banner("Phase 4 — System Comparison Experiments");

    const std::vector<int>         NODE_COUNTS = {10, 25, 50, 100, 200};
    const std::vector<std::string> SCENARIOS   = {"single", "dual", "cascading"};
    const std::vector<std::string> SYSTEMS     = {"PureGossip", "PureCAN", "Hybrid"};
    constexpr int                  NUM_SEEDS   = 5;

    const int total = static_cast<int>(
        NODE_COUNTS.size() * SCENARIOS.size() * SYSTEMS.size() * NUM_SEEDS);

    std::vector<ExperimentResult> results;
    results.reserve(total);

    int done = 0;
    std::cout << "  Running " << total << " experiments...\n";

    for (int n : NODE_COUNTS) {
        for (const auto& scenario : SCENARIOS) {
            for (const auto& system : SYSTEMS) {
                for (int seed = 0; seed < NUM_SEEDS; ++seed) {
                    results.push_back(runExperiment({system, n, scenario, seed}));
                    ++done;
                    if (done % 45 == 0 || done == total)
                        std::cout << "  Progress: " << done << "/" << total << "\n";
                }
            }
        }
    }

    printSummaryTable(results);

    // Export CSV.
    std::filesystem::create_directories("results");
    CSVExporter exporter;
    exporter.write("results/experiment_results.csv", results);

    // -----------------------------------------------------------------------
    // Overall test summary
    // -----------------------------------------------------------------------
    banner("Overall Test Summary");
    std::cout << "  Passed : " << gPassed << "\n";
    std::cout << "  Failed : " << gFailed << "\n";
    std::cout << (gFailed == 0 ? "\n  ALL TESTS PASSED\n" : "\n  SOME TESTS FAILED\n");

    return (gFailed == 0) ? 0 : 1;
}
