// -*- mode: c++; c-basic-offset: 4 indent-tabs-mode: nil -*- */
//
// Copyright 2016 Juho Snellman, released under a MIT license (see
// LICENSE).
//
// A timer queue which allows events to be scheduled for execution
// at some later point. Reasons you might want to use this implementation
// instead of some other are:
//
// - A single-file C++11 implementation with no external dependencies.
// - Optimized for high occupancy rates, on the assumption that the
//   utilization of the timer queue is proportional to the utilization
//   of the system as a whole. When a tradeoff needs to be made
//   between efficiency of one operation at a low occupancy rate and
//   another operation at a high rate, we choose the latter.
// - Tries to minimize the cost of event rescheduling or cancelation,
//   on the assumption that a large percentage of events will never
//   be triggered. The implementation avoids unnecessary work when an
//   event is rescheduled, and provides a way for the user specify a
//   range of acceptable execution times instead of just an exact one.
// - Facility for limiting the number of events to execute on a
//   single invocation, to allow fine grained interleaving of timer
//   processing and application logic.
// - An interface that at least the author finds convenient.
//
// The exact implementation strategy is a hierarchical timer
// wheel. A timer wheel is effectively a ring buffer of linked lists
// of events, and a pointer to the ring buffer. As the time advances,
// the pointer moves forward, and any events in the ring buffer slots
// that the pointer passed will get executed.
//
// A hierarchical timer wheel layers multiple timer wheels running at
// different resolutions on top of each other. When an event is
// scheduled so far in the future than it does not fit the innermost
// (core) wheel, it instead gets scheduled on one of the outer
// wheels. On each rotation of the inner wheel, one slot's worth of
// events are promoted from the second wheel to the core. On each
// rotation of the second wheel, one slot's worth of events is
// promoted from the third wheel to the second, and so on.
//
// The basic usage is to create a single TimerWheel object and
// multiple TimerEvent or MemberTimerEvent objects. The events are
// scheduled for execution using TimerWheel::schedule() or
// TimerWheel::schedule_in_range(), or unscheduled using the event's
// cancel() method.
//
// Example usage:
//
//      typedef std::function<void()> Callback;
//      TimerWheel timers;
//      int count = 0;
//      TimerEvent<Callback> timer([&count] () { ++count; });
//
//      timers.schedule(&timer, 5);
//      timers.advance(4);
//      assert(count == 0);
//      timers.advance(1);
//      assert(count == 1);
//
//      timers.schedule(&timer, 5);
//      timer.cancel();
//      timers.advance(4);
//      assert(count == 1);
//
// To tie events to specific member functions of an object instead of
// a callback function, use MemberTimerEvent instead of TimerEvent.
// For example:
//
//      class Test {
//        public:
//            Test() : inc_timer_(this) {
//            }
//            void start(TimerWheel* timers) {
//                timers->schedule(&inc_timer_, 10);
//            }
//            void on_inc() {
//                count_++;
//            }
//            int count() { return count_; }
//        private:
//            MemberTimerEvent<Test, &Test::on_inc> inc_timer_;
//            int count_ = 0;
//      };

#ifndef RATAS_TIMER_WHEEL_H
#define RATAS_TIMER_WHEEL_H

#include <cassert>
#include <cstdlib>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <memory>

typedef uint64_t tick_t;

class TimerWheelSlot;
class TimerWheel;

// An abstract class representing an event that can be scheduled to
// happen at some later time.
struct TimerEventInterface {
public:
    // Unschedule this event. It's safe to cancel an event that is inactive.
    inline void cancel();

    // Return true iff the event is currently scheduled for execution.
    bool active() const {
        return slot_ != NULL;
    }

    // Return the absolute tick this event is scheduled to be executed on.
    tick_t scheduled_at() const { return scheduled_at_; }

private:
    TimerEventInterface(const TimerEventInterface& other) = delete;
    TimerEventInterface& operator=(const TimerEventInterface& other) = delete;
    friend TimerWheelSlot;
    friend TimerWheel;


    void set_scheduled_at(tick_t ts) { scheduled_at_ = ts; }
    // Move the event to another slot. (It's safe for either the current
    // or new slot to be NULL).
    inline void relink(TimerWheelSlot* slot);

private:
    /* one CACHELINE 32 byte in 64bit processor */

    tick_t          scheduled_at_;
    // The slot this event is currently in (NULL if not currently scheduled).
    TimerWheelSlot* slot_ = NULL;
    // The events are linked together in the slot using an internal
    // doubly-linked list; this iterator does double duty as the
    // linked list node for this event.
    TimerEventInterface* next_ = NULL;
    TimerEventInterface* prev_ = NULL;
};

#if 0
// An event that takes the callback (of type CBType) to execute as
// a constructor parameter.
template<typename CBType>
class TimerEvent : public TimerEventInterface {
public:
    explicit TimerEvent<CBType>(const CBType& callback)
      : callback_(callback) {
    }

    void execute() {
        callback_();
    }

private:
    TimerEvent<CBType>(const TimerEvent<CBType>& other) = delete;
    TimerEvent<CBType>& operator=(const TimerEvent<CBType>& other) = delete;
    CBType callback_;
};

// An event that's specialized with a (static) member function of class T,
// and a dynamic instance of T. Event execution causes an invocation of the
// member function on the instance.
template<typename T, void(T::*MFun)() >
class MemberTimerEvent : public TimerEventInterface {
public:
    MemberTimerEvent(T* obj) : obj_(obj) {
    }

    virtual void execute () {
        (obj_->*MFun)();
    }

private:
    T* obj_;
};

#endif

// Purely an implementation detail.
class TimerWheelSlot {
public:
    TimerWheelSlot() {
    }

private:
    // Return the first event queued in this slot.
    const TimerEventInterface* events() const { return events_; }
    // Deque the first event from the slot, and return it.
    TimerEventInterface* pop_event() {
        auto event = events_;
        events_ = event->next_;
        if (events_) {
            events_->prev_ = NULL;
        }
        event->next_ = NULL;
        event->slot_ = NULL;
        return event;
    }

    TimerWheelSlot(const TimerWheelSlot& other) = delete;
    TimerWheelSlot& operator=(const TimerWheelSlot& other) = delete;
    friend TimerEventInterface;
    friend TimerWheel;

    // Doubly linked (inferior) list of events.
    TimerEventInterface* events_ = NULL;
};

// A TimerWheel is the entity that TimerEvents can be scheduled on
// for execution (with schedule() or schedule_in_range()), and will
// eventually be executed once the time advances far enough with the
// advance() method.

class TimerWheel {
public:
    bool Create(tick_t now = 0){
        for (int i = 0; i < NUM_LEVELS; ++i) {
            now_[i] = now >> (WIDTH_BITS * i);
        }
        ticks_pending_ = 0;
    }
    void Delete(){
    }

    // Advance the TimerWheel by the specified number of ticks, and execute
    // any events scheduled for execution at or before that time. The
    // number of events executed can be restricted using the max_execute
    // parameter. If that limit is reached, the function will return false,
    // and the excess events will be processed on a subsequent call.
    //
    // - It is safe to cancel or schedule events from within event callbacks.
    // - During the execution of the callback the observable event tick will
    //   be the tick it was scheduled to run on; not the tick the clock will
    //   be advanced to.
    // - Events will happen in order; all events scheduled for tick X will
    //   be executed before any event scheduled for tick X+1.
    //
    // Delta should be non-0. The only exception is if the previous
    // call to advance() returned false.
    //
    // advance() should not be called from an event callback.
    inline bool advance(tick_t delta,
                        size_t max_execute=std::numeric_limits<size_t>::max(),
                        int level = 0);

    // Schedule the event to be executed delta ticks from the current time.
    // The delta must be non-0.
    inline void schedule(TimerEventInterface* event, tick_t delta);

    // Schedule the event to happen at some time between start and end
    // ticks from the current time. The actual time will be determined
    // by the TimerWheel to minimize rescheduling and promotion overhead.
    // Both start and end must be non-0, and the end must be greater than
    // the start.
    inline void schedule_in_range(TimerEventInterface* event,
                                  tick_t start, tick_t end);

    // Return the current tick value. Note that if the time increases
    // by multiple ticks during a single call to advance(), during the
    // execution of the event callback now() will return the tick that
    // the event was scheduled to run on.
    tick_t now() const { return now_[0]; }

    // Return the number of ticks remaining until the next event will get
    // executed. If the max parameter is passed, that will be the maximum
    // tick value that gets returned. The max parameter's value will also
    // be returned if no events have been scheduled.
    //
    // Will return 0 if the wheel still has unprocessed events from the
    // previous call to advance().
    inline tick_t ticks_to_next_event(tick_t max = std::numeric_limits<tick_t>::max(),
                                    int level = 0);

private:
    TimerWheel(const TimerWheel& other) = delete;
    TimerWheel& operator=(const TimerWheel& other) = delete;

    // This handles the actual work of executing event callbacks and
    // recursing to the outer wheels.
    inline bool process_current_slot(tick_t now, size_t max_execute, int level);

    static const int WIDTH_BITS = 8;
    static const int NUM_LEVELS = (64 + WIDTH_BITS - 1) / WIDTH_BITS;
    static const int MAX_LEVEL = NUM_LEVELS - 1;
    static const int NUM_SLOTS = 1 << WIDTH_BITS;
    // A bitmask for looking at just the bits in the timestamp relevant to
    // this wheel.
    static const int MASK = (NUM_SLOTS - 1);

    // The current timestamp for this wheel. This will be right-shifted
    // such that each slot is separated by exactly one tick even on
    // the outermost wheels.
    tick_t now_[NUM_LEVELS];
    // We've done a partial tick advance. This is how many ticks remain
    // unprocessed.
    tick_t ticks_pending_;
    TimerWheelSlot slots_[NUM_LEVELS][NUM_SLOTS];
};

// Implementation

inline void TimerEventInterface::relink(TimerWheelSlot* new_slot) {
    if (new_slot == slot_) {
        return;
    }

    // Unlink from old location.
    if (slot_) {
        auto prev = prev_;
        auto next = next_;
        if (next) {
            next->prev_ = prev;
        }
        if (prev) {
            prev->next_ = next;
        } else {
            // Must be at head of slot. Move the next item to the head.
            slot_->events_ = next;
        }
    }

    // Insert in new slot.
    {
        if (new_slot) {
            auto old = new_slot->events_;
            next_ = old;
            if (old) {
                old->prev_ = this;
            }
            new_slot->events_ = this;
        } else {
            next_ = NULL;
        }
        prev_ = NULL;
    }
    slot_ = new_slot;
}

inline void TimerEventInterface::cancel() {
    // It's ok to cancel a event that's not scheduled.
    if (!slot_) {
        return;
    }

    relink(NULL);
}

inline bool TimerWheel::advance(tick_t delta, size_t max_events, int level) {
    if (ticks_pending_) {
        if (level == 0) {
            // Continue collecting a backlog of ticks to process if
            // we're called with non-zero deltas.
            ticks_pending_ += delta;
        }
        // We only partially processed the last tick. Process the
        // current slot, rather incrementing like advance() normally
        // does.
        tick_t now = now_[level];
        if (!process_current_slot(now, max_events, level)) {
            // Outer layers are still not done, propagate that information
            // back up.
            return false;
        }
        if (level == 0) {
            // The core wheel has been fully processed. We can now close
            // down the partial tick and pretend that we've just been
            // called with a delta containing both the new and original
            // amounts.
            delta = (ticks_pending_ - 1);
            ticks_pending_ = 0;
        } else {
            return true;
        }
    } else {
        // Zero deltas are only ok when in the middle of a partially
        // processed tick.
        assert(delta > 0);
    }

    while (delta--) {
        tick_t now = ++now_[level];
        if (!process_current_slot(now, max_events, level)) {
            ticks_pending_ = (delta + 1);
            return false;
        }
    }
    return true;
}

inline bool TimerWheel::process_current_slot(tick_t now, size_t max_events, int level) {
    size_t slot_index = now & MASK;
    auto slot = &slots_[level][slot_index];
    if (slot_index == 0 && level < MAX_LEVEL) {
        if (!advance(1, max_events, level + 1)) {
            return false;
        }
    }
    while (slot->events()) {
        auto event = slot->pop_event();
        if (level > 0) {
            assert((now_[0] & MASK) == 0);
            if (now_[0] >= event->scheduled_at()) {
                event->execute();
                if (!--max_events) {
                    return false;
                }
            } else {
                // There's a case to be made that promotion should
                // also count as work done. And that would simplify
                // this code since the max_events manipulation could
                // move to the top of the loop. But it's an order of
                // magnitude more expensive to execute a typical
                // callback, and promotions will naturally clump while
                // events triggering won't.
                schedule(event,
                         event->scheduled_at() - now_[0]);
            }
        } else {
            event->execute();
            if (!--max_events) {
                return false;
            }
        }
    }
    return true;
}

inline void TimerWheel::schedule(TimerEventInterface* event, tick_t delta) {
    event->set_scheduled_at(now_[0] + delta);

    int level = 0;
    while (delta >= NUM_SLOTS) {
        delta = (delta + (now_[level] & MASK)) >> WIDTH_BITS;
        ++level;
    }

    size_t slot_index = (now_[level] + delta) & MASK;
    auto slot = &slots_[level][slot_index];
    event->relink(slot);
}

inline void TimerWheel::schedule_in_range(TimerEventInterface* event,
                                   tick_t start, tick_t end) {
    assert(end > start);
    if (event->active()) {
        auto current = event->scheduled_at() - now_[0];
        // Event is already scheduled to happen in this range. Instead
        // of always using the old slot, we could check compute the
        // new slot and switch iff it's aligned better than the old one.
        // But it seems hard to believe that could be worthwhile.
        if (current >= start && current <= end) {
            return;
        }
    }

    // Zero as many bits (in WIDTH_BITS chunks) as possible
    // from "end" while still keeping the output in the
    // right range.
    tick_t mask = ~0;
    while ((start & mask) != (end & mask)) {
        mask = (mask << WIDTH_BITS);
    }

    tick_t delta = end & (mask >> WIDTH_BITS);

    schedule(event, delta);
}

inline tick_t TimerWheel::ticks_to_next_event(tick_t max, int level) {
    if (ticks_pending_) {
        return 0;
    }
    // The actual current time (not the bitshifted time)
    tick_t now = now_[0];

    // Smallest tick (relative to now) we've found.
    tick_t min = max;
    for (int i = 0; i < NUM_SLOTS; ++i) {
        // Note: Unlike the uses of "now", slot index calculations really
        // need to use now_.
        auto slot_index = (now_[level] + 1 + i) & MASK;
        // We've reached slot 0. In normal scheduling this would
        // mean advancing the next wheel and promoting or executing
        // those events.  So we need to look in that slot too
        // before proceeding with the rest of this wheel. But we
        // can't just accept those results outright, we need to
        // check the best result there against the next slot on
        // this wheel.
        if (slot_index == 0 && level < MAX_LEVEL) {
            // Exception: If we're in the core wheel, and slot 0 is
            // not empty, there's no point in looking in the outer wheel.
            // It's guaranteed that the events actually in slot 0 will be
            // executed no later than anything in the outer wheel.
            if (level > 0 || !slots_[level][slot_index].events()) {
                auto up_slot_index = (now_[level + 1] + 1) & MASK;
                const auto& slot = slots_[level + 1][up_slot_index];
                for (auto event = slot.events(); event != NULL;
                     event = event->next_) {
                    min = std::min(min, event->scheduled_at() - now);
                }
            }
        }
        bool found = false;
        const auto& slot = slots_[level][slot_index];
        for (auto event = slot.events(); event != NULL;
             event = event->next_) {
            min = std::min(min, event->scheduled_at() - now);
            // In the core wheel all the events in a slot are guaranteed to
            // run at the same time, so it's enough to just look at the first
            // one.
            if (level == 0) {
                return min;
            } else {
                found = true;
            }
        }
        if (found) {
            return min;
        }
    }

    // Nothing found on this wheel, try the next one (unless the wheel can't
    // possibly contain an event scheduled earlier than "max").
    if (level < MAX_LEVEL &&
        (max >> (WIDTH_BITS * level + 1)) > 0) {
        return ticks_to_next_event(max, level + 1);
    }

    return max;
}

#endif //  RATAS_TIMER_WHEEL_H

