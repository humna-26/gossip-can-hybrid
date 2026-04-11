#pragma once

// ---------------------------------------------------------------------------
// EventQueue.h — Min-heap priority queue for simulation events.
//
// Wraps std::priority_queue so that the event with the smallest timestamp
// is always at the top (i.e. the next event to fire).
// ---------------------------------------------------------------------------

#include "Event.h"

#include <functional>  // std::greater
#include <queue>
#include <vector>

class EventQueue {
public:
    // Push a new event into the queue.
    void push(const Event& event);

    // Remove and return the event with the smallest timestamp.
    // Caller must check empty() before calling.
    Event pop();

    // Peek at the next event without removing it.
    // Caller must check empty() before calling.
    const Event& top() const;

    // True when the queue holds no events.
    bool empty() const;

    // Number of pending events.
    std::size_t size() const;

    // Remove all pending events (e.g. at simulation reset).
    void clear();

private:
    // Min-heap: std::greater makes the smallest element rise to the top.
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> pq_;
};
