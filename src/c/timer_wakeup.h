#pragma once

#include <pebble.h>

typedef struct {
  WakeupId id;
  uint32_t deadline;
} TimerWakeup;

static inline TimerWakeup timer_wakeup_try(uint32_t deadline, int32_t cookie) {
  if (deadline) {
    const WakeupId id = wakeup_schedule((time_t)deadline, cookie, true);
    if (id >= 0) {
      time_t actual;
      if (wakeup_query(id, &actual) && actual == (time_t)deadline) {
        return (TimerWakeup){id, deadline};
      }
      wakeup_cancel(id);
    }
  }
  return (TimerWakeup){-1, 0};
}

/* The caller passes a previous Wakeup only while its timer is still running.
 * SDK 4.33.1 rejects events less than 60 seconds apart. Keep the previous event
 * in place when possible, or restore it if a close replacement is rejected. */
static inline TimerWakeup timer_wakeup_replace(TimerWakeup previous, uint32_t deadline,
                                               int32_t cookie) {
  if (previous.deadline && previous.deadline == deadline) return previous;
  const bool can_keep = deadline && previous.deadline > deadline &&
      previous.deadline - deadline >= 60;
  if (!can_keep) wakeup_cancel_all();

  const TimerWakeup next = timer_wakeup_try(deadline, cookie);
  if (next.id >= 0) {
    if (can_keep) wakeup_cancel(previous.id);
    return next;
  }
  if (can_keep) return previous;
  return timer_wakeup_try(previous.deadline, cookie);
}
