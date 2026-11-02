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
