#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* Rasterized chamber silhouettes follow the approved hourglass's cubic
 * curves at pixel centers. Each chamber owns the same number of pixels.
 * Fill conserves that area: remaining sand above + elapsed sand below is
 * always one full chamber, including partial rows. No simulated clock or
 * particle state participates in progress. */
#define TIMER_SAND_ROWS 31u
static const uint8_t timer_sand_width[TIMER_SAND_ROWS] = {49,49,49,49,49,49,47,47,47,45,45,43,43,41,41,39,37,35,33,33,31,29,25,23,21,19,17,13,11,9,5};

typedef struct {
  uint8_t upper[TIMER_SAND_ROWS];
  uint8_t lower[TIMER_SAND_ROWS];
  unsigned upper_pixels;
  unsigned lower_pixels;
} TimerSandFill;

typedef enum {
  TIMER_GLASS_READY, TIMER_GLASS_RUNNING, TIMER_GLASS_PAUSED
} TimerGlassPose;

typedef struct { int16_t x, y; } TimerGlassPoint;

static inline int timer_hourglass_pose(TimerGlassPose pose) {
  return pose == TIMER_GLASS_READY ? 180 : pose == TIMER_GLASS_PAUSED ? 90 : 0;
}

/* A small Q15 quadrant table keeps drawing portable and avoids floating point
 * or SDK trigonometry. Adjacent rows share transformed endpoints, so a rotating
 * sand silhouette has no independently animated particles or changing volume. */
static inline int32_t timer_hourglass_sine(unsigned degrees) {
  static const uint16_t values[] = {
    0,2856,5690,8481,11207,13848,16384,18795,21063,23170,
    25102,26842,28378,29698,30792,31651,32270,32643,32768
  };
  if(degrees > 90) degrees = 180 - degrees;
  if(degrees >= 90) return 32768;
  const unsigned index = degrees / 5, fraction = degrees % 5;
  return values[index] + ((int32_t)values[index+1] - values[index]) * fraction / 5;
}

/* Return coordinates relative to the glass centre (canonical point 0,40).
 * Scale is a percentage; angle is bounded to the actual 0..180 degree poses. */
static inline TimerGlassPoint timer_hourglass_transform(int x, int y,
                                                        int scale, int angle) {
  if(angle < 0) angle = 0;
  if(angle > 180) angle = 180;
  const int32_t sine = timer_hourglass_sine((unsigned)angle);
  const int32_t cosine = angle <= 90 ? timer_hourglass_sine((unsigned)(90-angle)) :
      -timer_hourglass_sine((unsigned)(angle-90));
  const int32_t dy = y - 40;
  const int64_t rx = ((int64_t)x*cosine - (int64_t)dy*sine)*scale;
  const int64_t ry = ((int64_t)x*sine + (int64_t)dy*cosine)*scale;
  const int32_t divisor = 3276800;
  return (TimerGlassPoint){
    (int16_t)((rx < 0 ? rx-divisor/2 : rx+divisor/2)/divisor),
    (int16_t)((ry < 0 ? ry-divisor/2 : ry+divisor/2)/divisor)
  };
}

/* Smoothstep endpoints are exact. The native event loop owns elapsed time;
 * these decorative angles never participate in timer state or deadlines. */
static inline int timer_hourglass_angle(int from, int to, uint32_t elapsed,
                                        uint32_t duration) {
  if(!duration || elapsed >= duration) return to;
  const uint32_t progress = (uint32_t)((uint64_t)elapsed*1024/duration);
  const uint32_t eased = progress*progress*(3072u-2u*progress)/1048576u;
  const int32_t delta = (to-from)*(int32_t)eased;
  return from+(delta < 0 ? delta-512 : delta+512)/1024;
}

static inline unsigned timer_sand_capacity(void) {
  unsigned capacity = 0;
  for (unsigned row = 0; row < TIMER_SAND_ROWS; ++row) capacity += timer_sand_width[row];
  return capacity;
}

static inline void timer_sand_fill(uint32_t remaining, uint32_t total,
                                   TimerSandFill *fill) {
  memset(fill, 0, sizeof(*fill));
  const unsigned capacity = timer_sand_capacity();
  if (remaining > total) remaining = total;
  unsigned upper = total ? (unsigned)(((uint64_t)remaining * capacity + total / 2) / total) : 0;
  unsigned lower = capacity - upper;
  fill->upper_pixels = upper;
  fill->lower_pixels = lower;
  for (unsigned n = TIMER_SAND_ROWS; n > 0; --n) {
    const unsigned row = n - 1;
    const unsigned width = timer_sand_width[row];
    fill->upper[row] = upper < width ? upper : width;
    upper -= fill->upper[row];
    const unsigned lower_width = timer_sand_width[TIMER_SAND_ROWS - 1 - row];
    fill->lower[row] = lower < lower_width ? lower : lower_width;
    lower -= fill->lower[row];
  }
}
