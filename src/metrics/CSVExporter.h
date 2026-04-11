#pragma once

// ---------------------------------------------------------------------------
// CSVExporter.h — Write ExperimentResult rows to a CSV file.
// ---------------------------------------------------------------------------

#include "MetricsCollector.h"

#include <string>
#include <vector>

class CSVExporter {
public:
    // Write all results to filename (overwrites if exists).
    // Prints a confirmation line to stdout.
    void write(const std::string&                  filename,
               const std::vector<ExperimentResult>& results);
};
