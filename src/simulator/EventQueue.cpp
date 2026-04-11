// ---------------------------------------------------------------------------
// EventQueue.cpp — Min-heap priority queue implementation.
// ---------------------------------------------------------------------------

#include "EventQueue.h"

void EventQueue::push(const Event& event) {
    pq_.push(event);
}

Event EventQueue::pop() {
    Event next = pq_.top();
    pq_.pop();
    return next;
}

const Event& EventQueue::top() const {
    return pq_.top();
}

bool EventQueue::empty() const {
    return pq_.empty();
}

std::size_t EventQueue::size() const {
    return pq_.size();
}

void EventQueue::clear() {
    // std::priority_queue has no clear(); swap with an empty queue.
    std::priority_queue<Event, std::vector<Event>, std::greater<Event>> empty;
    std::swap(pq_, empty);
}
