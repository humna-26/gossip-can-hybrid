// ---------------------------------------------------------------------------
// CSVExporter.cpp
// ---------------------------------------------------------------------------

#include "CSVExporter.h"

#include <fstream>
#include <iomanip>
#include <iostream>

void CSVExporter::write(const std::string&                  filename,
                         const std::vector<ExperimentResult>& results) {
    std::ofstream f(filename);
    if (!f) {
        std::cerr << "[CSV] Cannot open " << filename << " for writing\n";
        return;
    }

    f << "system,node_count,scenario,seed,"
         "detection_latency_ms,recovery_time_ms,"
         "recovery_messages,total_messages,false_positive_rate\n";

    for (const auto& r : results) {
        f << r.system      << ","
          << r.nodeCount   << ","
          << r.scenario    << ","
          << r.seed        << ","
          << std::fixed << std::setprecision(1)
          << r.detectionLatencyMs << ","
          << r.recoveryTimeMs     << ","
          << r.recoveryMessages   << ","
          << r.totalMessages      << ","
          << std::setprecision(4)
          << r.falsePositiveRate  << "\n";
    }

    std::cout << "[CSV] Wrote " << results.size()
              << " rows → " << filename << "\n";
}
