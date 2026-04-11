// ---------------------------------------------------------------------------
// Zone.cpp — CAN zone rectangle implementation.
// ---------------------------------------------------------------------------

#include "Zone.h"

#include <algorithm>  // std::max, std::min
#include <cmath>      // std::sqrt, std::abs
#include <sstream>
#include <iomanip>

// ---------------------------------------------------------------------------
// Constructors
// ---------------------------------------------------------------------------

Zone::Zone() : xMin(0.0), xMax(1.0), yMin(0.0), yMax(1.0) {}

Zone::Zone(double xMin, double xMax, double yMin, double yMax)
    : xMin(xMin), xMax(xMax), yMin(yMin), yMax(yMax) {}

// ---------------------------------------------------------------------------
// Containment
// ---------------------------------------------------------------------------

bool Zone::contains(double x, double y) const {
    return (x >= xMin && x < xMax) &&
           (y >= yMin && y < yMax);
}

// ---------------------------------------------------------------------------
// Geometry
// ---------------------------------------------------------------------------

double Zone::volume() const {
    return (xMax - xMin) * (yMax - yMin);
}

double Zone::centerX() const { return (xMin + xMax) * 0.5; }
double Zone::centerY() const { return (yMin + yMax) * 0.5; }

double Zone::distanceTo(double x, double y) const {
    double dx = centerX() - x;
    double dy = centerY() - y;
    return std::sqrt(dx * dx + dy * dy);
}

// ---------------------------------------------------------------------------
// Neighbour check
// ---------------------------------------------------------------------------

bool Zone::isNeighbour(const Zone& other) const {
    constexpr double EPS = 1e-9;

    // --- Case 1: X-adjacent ---
    // One zone's right edge exactly touches the other's left edge.
    bool xAdj = (std::abs(xMax - other.xMin) < EPS) ||
                (std::abs(other.xMax - xMin) < EPS);

    if (xAdj) {
        // They must also overlap on Y by more than a point.
        double yOverlapLo = std::max(yMin, other.yMin);
        double yOverlapHi = std::min(yMax, other.yMax);
        if (yOverlapHi - yOverlapLo > EPS) return true;
    }

    // --- Case 2: Y-adjacent ---
    // One zone's top edge exactly touches the other's bottom edge.
    bool yAdj = (std::abs(yMax - other.yMin) < EPS) ||
                (std::abs(other.yMax - yMin) < EPS);

    if (yAdj) {
        // They must also overlap on X by more than a point.
        double xOverlapLo = std::max(xMin, other.xMin);
        double xOverlapHi = std::min(xMax, other.xMax);
        if (xOverlapHi - xOverlapLo > EPS) return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// Split
// ---------------------------------------------------------------------------

std::pair<Zone, Zone> Zone::split() const {
    double width  = xMax - xMin;
    double height = yMax - yMin;

    if (width >= height) {
        // Split vertically: cut at midX along the X axis.
        double mid = (xMin + xMax) * 0.5;
        return { Zone(xMin, mid,  yMin, yMax),   // left half
                 Zone(mid,  xMax, yMin, yMax) };  // right half
    } else {
        // Split horizontally: cut at midY along the Y axis.
        double mid = (yMin + yMax) * 0.5;
        return { Zone(xMin, xMax, yMin, mid),   // bottom half
                 Zone(xMin, xMax, mid,  yMax) }; // top half
    }
}

// ---------------------------------------------------------------------------
// Merge (bounding box)
// ---------------------------------------------------------------------------

Zone Zone::merge(const Zone& other) const {
    return Zone(
        std::min(xMin, other.xMin),
        std::max(xMax, other.xMax),
        std::min(yMin, other.yMin),
        std::max(yMax, other.yMax)
    );
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

std::string Zone::toString() const {
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(4)
       << "[" << xMin << "," << xMax << ")×["
       << yMin << "," << yMax << ")"
       << " area=" << volume();
    return ss.str();
}
