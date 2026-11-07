#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Public-SDK equivalents of Pebble's elastic MenuLayer and SelectionLayer
 * feedback. Geometry only: owner state and button dispatch never wait here. */
typedef struct { int16_t x, y, w, h; } TimerMotionRect;

static inline int timer_motion_ease(int from, int to, uint32_t elapsed,
                                   uint32_t duration) {
  if (!duration || elapsed >= duration) return to;
  const int32_t left = (int32_t)((duration - elapsed) * 1024u / duration);
  const int32_t eased = 1024 - left * left / 1024;
  return from + (to - from) * eased / 1024;
}

static inline TimerMotionRect timer_motion_selection(TimerMotionRect from,
    TimerMotionRect to, uint32_t elapsed, uint32_t stretch, uint32_t settle) {
  if (elapsed >= stretch + settle) return to;
  const bool horizontal = from.x != to.x;
  const int a = horizontal ? from.x : from.y;
  const int b = horizontal ? to.x : to.y;
  const int size = horizontal ? to.w : to.h;
  const int from_size = horizontal ? from.w : from.h;
  int first, last;
  const uint32_t recovery = elapsed > stretch ? elapsed - stretch : 0;
  if (b > a) {
    first = timer_motion_ease(a, b, recovery, settle);
    last = elapsed < stretch ? timer_motion_ease(a + from_size, b + size + 3, elapsed, stretch) :
        timer_motion_ease(b + size + 3, b + size, recovery, settle);
  } else {
    first = elapsed < stretch ? timer_motion_ease(a, b - 3, elapsed, stretch) :
        timer_motion_ease(b - 3, b, recovery, settle);
    last = timer_motion_ease(a + from_size, b + size, recovery, settle);
  }
  TimerMotionRect result = to;
  if (horizontal) { result.x = first; result.w = last - first; }
  else { result.y = first; result.h = last - first; }
  return result;
}

static inline int timer_motion_bump(uint32_t elapsed) {
  if (elapsed >= 321u) return 0;
  return elapsed < 107u ? timer_motion_ease(0, 3, elapsed, 107u) :
      timer_motion_ease(3, 0, elapsed - 107u, 214u);
}
