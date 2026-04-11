#pragma once

// ---------------------------------------------------------------------------
// Zone.h — A rectangular sub-region of the CAN coordinate space [0,1)×[0,1).
//
// Each node owns exactly one Zone. Ownership means the node is responsible
// for any key/point that falls within its rectangle.
// ---------------------------------------------------------------------------

#include <string>
#include <utility>   // std::pair

class Zone {
public:
    double xMin, xMax;   // [xMin, xMax) — half-open on the right
    double yMin, yMax;   // [yMin, yMax)

    // Default constructor: full coordinate space [0,1) × [0,1)
    Zone();

    Zone(double xMin, double xMax, double yMin, double yMax);

    // -----------------------------------------------------------------------
    // Containment
    // -----------------------------------------------------------------------

    // True if point (x,y) is inside this zone (half-open intervals).
    bool contains(double x, double y) const;

    // -----------------------------------------------------------------------
    // Geometry
    // -----------------------------------------------------------------------

    // Area of the rectangle (width × height).
    double volume() const;

    // Euclidean distance from the zone's center point to (x,y).
    double distanceTo(double x, double y) const;

    double centerX() const;
    double centerY() const;

    // -----------------------------------------------------------------------
    // Neighbour check
    // -----------------------------------------------------------------------

    // Two zones are CAN neighbours iff:
    //   • They are exactly adjacent on ONE dimension (one's max == other's min),
    //   • AND they have a non-zero overlap (> epsilon) on the OTHER dimension.
    // Corner-only contact does NOT count as a neighbour relationship.
    bool isNeighbour(const Zone& other) const;

    // -----------------------------------------------------------------------
    // Structural operations
    // -----------------------------------------------------------------------

    // Split this zone in half along its LONGER dimension.
    //   width >= height  →  split vertically   (along X at midX)
    //   height >  width  →  split horizontally (along Y at midY)
    // Returns {firstHalf, secondHalf} where firstHalf is lower/left.
    std::pair<Zone, Zone> split() const;

    // Bounding-box merge: smallest rectangle containing both zones.
    // Valid for CAN takeover when taker and failed node are neighbours
    // (their zones form a proper rectangle by the bisection property).
    Zone merge(const Zone& other) const;

    // -----------------------------------------------------------------------
    // Debug
    // -----------------------------------------------------------------------
    std::string toString() const;
};
