# Gossip-Based Failure Detection with CAN-Assisted Recovery

## Project Overview

**Course:** CS-432 Parallel & Distributed Computing (SEECS NUST)  
**Theme:** Theme 6 — Gossip vs Structured Overlay Aggregation  
**Solo Project:** Custom C++ discrete-event simulator

This project compares three failure detection and recovery systems:
1. **PureGossip** — Gossip-based detection + gossip flood recovery (baseline)
2. **PureCAN** — Timeout-based detection + CAN takeover recovery (baseline)
3. **Hybrid** — Gossip-based detection + CAN-routed recovery (our approach)

## Key Results

**480 Total Experiments** (225 baseline + 255 sensitivity analysis)

### Core Finding
The Hybrid system achieves **O(d) message complexity** for recovery vs. PureGossip's **O(N log N)**:

| Network Size | Hybrid Recovery Msgs | PureGossip Recovery Msgs | Reduction |
|---|---|---|---|
| N=10 | 4 | 8 | 50% |
| N=100 | 4 | 98 | **96%** |
| N=200 | 4 | 198 | **98%** |
| N=300 | 4 | 298 | **98.7%** |

### Sensitivity Analysis Findings
- **Optimal gossip fan-out (b):** 3 — higher fan-out doesn't improve detection speed, only burns bandwidth
- **Optimal failure timeout:** 4000ms — below 2000ms causes 99% false positives, above 6000ms detection gets too slow
- **Optimal gossip interval:** 500ms — balances detection latency (~2-3s) with reasonable message overhead
- **Scalability proof:** Hybrid maintains constant recovery messages across N=10–300; PureGossip scales linearly

## Implementation Details

### Architecture
- **Gossip Layer:** Fast failure detection (decentralized, ~4s detection latency)
- **CAN Overlay:** Efficient recovery coordination (O(d) routed messages where d ≈ 4 CAN hops)
- **Hybrid Approach:** Combines best of both — fast detection + efficient recovery

### Technologies
- **Language:** C++17
- **Build System:** CMake (or direct g++ compilation)
- **Simulation:** Custom discrete-event simulator (NOT OMNeT++)
- **Metrics Collection:** Automated CSV logging of all 480 experiments
- **Visualization:** Python matplotlib for 12 comparison graphs

### Communication Model
- **Message Types:** Gossip infect, suspect, ACK; CAN route, takeover, recovery
- **Network Assumptions:** FIFO delivery, bounded network delay, reliable links
- **Failure Model:** Crash failures only; bounded detection time assumption

## Building & Running

### Prerequisites
- C++17 compiler (MinGW-w64 on Windows)
- Python 3.x (for graphing)
- CMake (optional) or direct g++ compilation

### Build

**Option 1: Using CMake**
```bash
cd C:\Users\humna\Desktop\Parallel and Distributed Computing\PROJECT\gossip-can-hybrid\
cmake .
make
```

**Option 2: Direct g++ compilation (Windows CMD)**
```bash
cd C:\Users\humna\Desktop\Parallel and Distributed Computing\PROJECT\gossip-can-hybrid\
g++ -std=c++17 -O2 src/main.cpp src/simulator/*.cpp src/network/*.cpp src/gossip/*.cpp src/can/*.cpp src/systems/*.cpp src/metrics/*.cpp -o gossip_can_hybrid.exe
```

### Run Baseline Experiments (225 runs, ~3-5 minutes)
```bash
.\gossip_can_hybrid.exe
# Output: results/experiment_results.csv + 7 baseline graphs in docs/report_figures/
```

### Run Sensitivity Analysis (255 runs, ~5-8 minutes)
```bash
.\gossip_can_hybrid.exe --sensitivity
# Output: 4 sensitivity CSVs + 5 sensitivity graphs
```

### Generate Graphs
```bash
python scripts/plot_results.py          # Baseline graphs
python scripts/plot_sensitivity.py       # Sensitivity graphs
```

## Project Structure
```
gossip-can-hybrid/
├── README.md                           # This file
├── .gitignore                          # Git exclusion rules
├── CMakeLists.txt                      # Build configuration
├── src/
│   ├── main.cpp                        # Experiment runner + CLI
│   ├── main_sensitivity.cpp            # Sensitivity experiment orchestrator
│   ├── simulator/
│   │   ├── Simulator.h/.cpp            # Event queue, core simulation engine
│   │   └── Event.h                     # Event definition
│   ├── network/
│   │   ├── Node.h/.cpp                 # Node implementation, message handling
│   │   └── MessageCounter.h            # Global message metrics
│   ├── gossip/
│   │   ├── MembershipList.h/.cpp       # Gossip membership tracking
│   │   ├── GossipLayer.h/.cpp          # Gossip protocol implementation
│   │   └── FailureDetector.h/.cpp      # Detection logic
│   ├── can/
│   │   ├── Zone.h/.cpp                 # CAN zone management
│   │   ├── CANOverlay.h/.cpp           # CAN overlay routing
│   │   └── TakeoverManager.h/.cpp      # Takeover coordination
│   ├── systems/
│   │   ├── SystemBase.h                # Abstract base class
│   │   ├── PureGossipSystem.h/.cpp     # Baseline 1
│   │   ├── PureCANSystem.h/.cpp        # Baseline 2
│   │   └── HybridSystem.h/.cpp         # Our approach
│   └── metrics/
│       ├── MetricsCollector.h/.cpp     # Results logging
│       └── CSVExporter.h/.cpp          # CSV output
├── scripts/
│   ├── plot_results.py                 # Baseline graph generation
│   └── plot_sensitivity.py             # Sensitivity graph generation
├── results/
│   ├── experiment_results.csv          # 225 baseline experiments
│   ├── sensitivity_fanout.csv          # 30 sensitivity runs (fan-out)
│   ├── sensitivity_timeout.csv         # 35 sensitivity runs (timeout tradeoff)
│   ├── sensitivity_scalability.csv     # 165 sensitivity runs (scalability)
│   └── sensitivity_interval.csv        # 25 sensitivity runs (gossip interval)
└── docs/
    └── report_figures/
        ├── recovery_messages_vs_N.png/.pdf
        ├── detection_latency_vs_N.png/.pdf
        ├── recovery_time_vs_N.png/.pdf
        ├── total_messages_vs_N.png/.pdf
        ├── recovery_by_scenario.png/.pdf
        ├── scalability_dashboard.png/.pdf
        ├── message_breakdown_stacked.png/.pdf
        ├── sensitivity_fanout.png/.pdf
        ├── sensitivity_timeout_tradeoff.png/.pdf
        ├── sensitivity_scalability_fine.png/.pdf
        ├── sensitivity_interval.png/.pdf
        └── sensitivity_dashboard.png/.pdf
```

## Experimental Methodology

### Baseline Experiments (225 runs)
- **3 Systems:** PureGossip, PureCAN, Hybrid
- **5 Network Sizes:** N = 50, 100, 150, 200, 250
- **3 Failure Scenarios:** Single failure, dual failure, cascading
- **5 Random Seeds:** Per configuration for statistical rigor
- **Parameters:** b=3, T_gossip=500ms, T_fail=4000ms, T_suspect=2000ms

### Sensitivity Analysis (255 runs)
- **Set A:** Gossip fan-out (b=1–6), N=100, 5 seeds → 30 runs
- **Set B:** Failure timeout (T_fail=1000–8000ms), N=100, 5 seeds → 35 runs
- **Set C:** Fine-grained scalability (N=10–300), 3 systems, 5 seeds → 165 runs
- **Set D:** Gossip interval (T_gossip=100–2000ms), N=100, 5 seeds → 25 runs

## Key Insights from Results

### 1. Fan-out Sensitivity
- **b=1:** Gossip too sparse (90%+ false positive rate) — unreliable
- **b=2:** Minimum viable fan-out (0% FPR, ~4000ms detection)
- **b=3:** Sweet spot selected — optimal redundancy + efficiency
- **b≥4:** Diminishing returns — faster detection not gained, +6k messages wasted per config

### 2. Timeout Sensitivity (THE CRITICAL TRADEOFF)
- **T_fail ≤ 2000ms:** False positive rate explodes to 99% — system unusable
- **T_fail = 3000–4000ms:** Reliable operating zone (0% FPR, ~3–4s detection)
- **T_fail ≥ 5000ms:** Detection becomes slow (5–8s), but perfectly reliable
- **Conclusion:** 4000ms is the empirically justified balanced choice

### 3. Scalability
- **Hybrid:** O(d) recovery messages, constant at ~4 across all N
- **PureGossip:** O(N log N) recovery messages, scales to 298 at N=300
- **Proof:** Fine-grained scalability graph shows clear separation

### 4. Gossip Interval Tradeoff
- **100ms:** Detection very fast (1s) but 8x more bandwidth (85k msgs)
- **500ms (baseline):** Balanced (2s detection, 30k msgs)
- **2000ms:** Detection too slow (15s) for practical use

## Performance Characteristics

| Metric | PureGossip | PureCAN | Hybrid |
|--------|-----------|---------|--------|
| Detection Latency (ms) | 4000 | 3910 | 4000 |
| Recovery Messages (N=200) | 198 | 4 | 4 |
| Recovery Time (ms) | 10,815 | 6,460 | 7,040 |
| Total Messages (N=200) | 34,918 | 2,299,142 | 34,534 |
| False Positive Rate | 0% | 0% | 0% |
| Message Complexity | O(N log N) | O(d) | O(d) |
| Scalability | Superlinear | Constant | Constant |

**Winner:** Hybrid achieves PureGossip's fast detection with PureCAN's efficient recovery.

## Future Work

1. **Byzantine Fault Tolerance:** Extend to handle malicious failures (not in current scope)
2. **Network Partitions:** Test CAN overlay behavior during network splits
3. **Adaptive Parameters:** Dynamic T_fail adjustment based on network conditions
4. **Real Network Deployment:** Validate on actual distributed cluster

## Author

**Humna** — CS-432 Student, SEECS NUST  

## References

- Tanenbaum, A. S., & Van Steen, M. (2017). *Distributed Systems* (3rd ed.)
- Gupta, I., et al. (2001). "Scalable Weakly-Consistent Gossip-Based Broadcast." *FTCS*
- Ratnasamy, S., et al. (2001). "A Scalable Content-Addressable Network." *SIGCOMM*
- Indranil Gupta's UIUC Cloud Computing Lectures on Failure Detection

---

**Last Updated:** March 2026  
**Status:** Phase 6A Complete (Sensitivity Analysis), Ready for Phase 6B (System Design Document)
