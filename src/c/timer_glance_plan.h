#pragma once

#include "timer_collection.h"
#include <stdio.h>
#include <string.h>

typedef struct {
  uint32_t deadline;
  char subtitle[128];
} TimerGlanceSlice;

typedef struct {
  uint32_t count;
  uint32_t deadlines[TIMER_COLLECTION_CAPACITY];
} TimerGlanceProjection;

static inline bool timer_glance_project(const TimerCollection *collection,
                                       uint32_t now, TimerGlanceProjection *out) {
  if (!out || !timer_collection_valid(collection)) return false;
  memset(out, 0, sizeof(*out));
  for (unsigned slot = 0; slot < TIMER_COLLECTION_CAPACITY; ++slot) {
    const ActivityTimerModel *timer = &collection->slots[slot];
    if (!timer_collection_visible(collection, slot) ||
        timer->state != ACTIVITY_TIMER_RUNNING || timer->deadline <= now) continue;
    unsigned at = out->count;
    while (at && out->deadlines[at - 1] > timer->deadline) {
      out->deadlines[at] = out->deadlines[at - 1];
      --at;
    }
    out->deadlines[at] = timer->deadline;
    ++out->count;
  }
  return true;
}

static inline void timer_glance_prune(TimerGlanceProjection *projection, uint32_t now) {
  unsigned count = 0;
  for (unsigned i = 0; i < projection->count; ++i)
    if (projection->deadlines[i] > now) projection->deadlines[count++] = projection->deadlines[i];
  memset(projection->deadlines + count, 0,
         (TIMER_COLLECTION_CAPACITY - count) * sizeof(projection->deadlines[0]));
  projection->count = count;
}

/* Each slice expires at a distinct running deadline. Queuing the next slice
 * lets the OS show the next timer even before the app handles its Wakeup. */
static inline bool timer_glance_projection_next(const TimerGlanceProjection *projection,
                                               uint32_t after, TimerGlanceSlice *out) {
  if (!out || !projection || projection->count > TIMER_COLLECTION_CAPACITY) return false;
  uint32_t deadline = 0;
  unsigned running = 0;
  for (unsigned i = 0; i < projection->count; ++i) {
    if (projection->deadlines[i] <= after) continue;
    ++running;
    if (!deadline || projection->deadlines[i] < deadline) deadline = projection->deadlines[i];
  }
  if (!deadline) return false;
  out->deadline = deadline;
  const int length = snprintf(out->subtitle, sizeof(out->subtitle),
      "{time_until(%lu)|format(>=1H:'%%fH:%%0M:%%0S','%%fM:%%0S')} left",
      (unsigned long)deadline);
  if (length < 0 || (size_t)length >= sizeof(out->subtitle)) return false;
  if (running > 1) {
    const int extra = snprintf(out->subtitle + length, sizeof(out->subtitle) - (size_t)length,
                              " / %u running", running);
    if (extra < 0 || (size_t)extra >= sizeof(out->subtitle) - (size_t)length) return false;
  }
  return true;
}

static inline bool timer_glance_next(const TimerCollection *collection,
                                    uint32_t after, TimerGlanceSlice *out) {
  TimerGlanceProjection projection;
  return timer_glance_project(collection, after, &projection) &&
      timer_glance_projection_next(&projection, after, out);
}
