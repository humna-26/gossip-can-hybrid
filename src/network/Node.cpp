// ---------------------------------------------------------------------------
// Node.cpp — Node implementation.
// ---------------------------------------------------------------------------

#include "Node.h"

Node::Node(int id) : id(id), alive(true) {}

void Node::crash()          { alive = false; }
bool Node::isAlive() const  { return alive; }

void Node::setZone(const Zone& z)                            { zone = z; }
Zone Node::getZone() const                                   { return zone; }
void Node::setNeighbours(const std::vector<int>& neighbours) { canNeighbours = neighbours; }
std::vector<int> Node::getNeighbours() const                 { return canNeighbours; }

void Node::initGossip(int totalNodes, int fanout, std::mt19937* rng) {
    gossipLayer.init(id, totalNodes, fanout, rng);
}
