#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Bounded feedback geometry. Readability takes priority over imitating the
 * native menu's long stretch; owner state and button dispatch never wait. */
typedef struct { int16_t x, y, w, h; } TimerMotionRect;
#define TIMER_MENU_MOTION_MS 120u
#define TIMER_FIELD_SLIDE_MS 107u
#define TIMER_FIELD_SETTLE_MS 179u
#define TIMER_VALUE_TEXT_MS 107u
#define TIMER_VALUE_SETTLE_MS 214u

typedef struct {
  int8_t text_y, box_y;
  uint8_t box_growth;
  bool previous_value;
} TimerValueMotion;

static inline int timer_motion_ease(int from, int to, uint32_t elapsed,
                                   uint32_t duration) {
  if (!duration || elapsed >= duration) return to;
  const int32_t left = (int32_t)((duration - elapsed) * 1024u / duration);
  const int32_t eased = 1024 - left * left / 1024;
  return from + (to - from) * eased / 1024;
}

static inline int timer_motion_ease_in(int from, int to, uint32_t elapsed,
                                      uint32_t duration) {
  if (!duration || elapsed >= duration) return to;
  const int32_t progress = (int32_t)(elapsed * 1024u / duration);
  return from + (to - from) * (progress * progress / 1024) / 1024;
}

/* Pebble Timer's SelectionLayer moves the whole field, then removes just the
 * inter-field padding. It never stretches a box across two whole fields. */
static inline TimerMotionRect timer_motion_field(TimerMotionRect from,
    TimerMotionRect to, uint32_t elapsed) {
  if (elapsed >= TIMER_FIELD_SLIDE_MS + TIMER_FIELD_SETTLE_MS) return to;
  TimerMotionRect result = to;
  const bool left = to.x < from.x;
  if (elapsed < TIMER_FIELD_SLIDE_MS) {
    result.x = timer_motion_ease_in(from.x, to.x - (left ? 3 : 0),
        elapsed, TIMER_FIELD_SLIDE_MS);
    result.w = timer_motion_ease_in(from.w, to.w + 3,
        elapsed, TIMER_FIELD_SLIDE_MS);
  } else {
    const int padding = timer_motion_ease(3, 0,
        elapsed - TIMER_FIELD_SLIDE_MS, TIMER_FIELD_SETTLE_MS);
    result.x -= left ? padding : 0;
    result.w += padding;
  }
  return result;
}

/* Vertical choices move as one fixed-height surface. No rubber band spans
 * adjacent labels, and every ordinary step settles within 120ms. */
static inline TimerMotionRect timer_motion_menu(TimerMotionRect from,
    TimerMotionRect to, uint32_t elapsed) {
  TimerMotionRect result = to;
  result.y = timer_motion_ease(from.y, to.y, elapsed, TIMER_MENU_MOTION_MS);
  return result;
}

/* Same two stages as Pebble Timer: old glyph toward the pressed edge, then
 * the new glyph in a six-pixel directional extension that settles. */
static inline TimerValueMotion timer_motion_value(uint32_t elapsed, int direction) {
  TimerValueMotion result = {0};
  if (elapsed >= TIMER_VALUE_TEXT_MS + TIMER_VALUE_SETTLE_MS) return result;
  if (elapsed < TIMER_VALUE_TEXT_MS) {
    result.previous_value = true;
    result.text_y = direction * timer_motion_ease_in(0, 10, elapsed, TIMER_VALUE_TEXT_MS);
  } else {
    result.box_growth = timer_motion_ease(6, 0,
        elapsed - TIMER_VALUE_TEXT_MS, TIMER_VALUE_SETTLE_MS);
    result.box_y = direction < 0 ? -(int)result.box_growth : 0;
    result.text_y = direction * (int)(result.box_growth / 2);
  }
  return result;
}
