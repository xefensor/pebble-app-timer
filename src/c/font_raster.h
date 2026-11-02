#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

// Preserve fractional vertices through rasterization. Four samples along
// each axis give 0..16 coverage without a framebuffer or a glyph-sized mask.
#define FONT_RASTER_ONE 256
#define FONT_RASTER_SAMPLES 4
#define FONT_RASTER_COVERAGE 16
#define FONT_RASTER_COORD_LIMIT (1 << 20)

typedef struct { int32_t x, y; } FontRasterPoint;
typedef struct { uint16_t first_point, point_count; } FontRasterContour;
typedef void (*FontRasterRow)(void *context, int y, int first, int end,
                              const uint8_t *coverage);

typedef struct {
  uint8_t *coverage;
  size_t coverage_capacity;
  int32_t *intersections;
  size_t intersection_capacity;
} FontRasterScratch;

static int32_t font_raster_floor(int32_t value, int32_t divisor) {
  return value / divisor - (value % divisor < 0);
}

static int32_t font_raster_ceil(int32_t value, int32_t divisor) {
  return value / divisor + (value % divisor > 0);
}

// The font generator unions overlapping strokes first. Filling every contour
// together with the even-odd rule preserves both counters and disjoint parts.
static bool font_raster_rows(const FontRasterPoint *points, size_t point_count,
                             const FontRasterContour *contours, size_t contour_count,
                             int width, int height, FontRasterScratch scratch,
                             FontRasterRow draw_row, void *context) {
  if (!points || !point_count || !contours || !contour_count || !draw_row ||
      width <= 0 || height <= 0 || !scratch.coverage || !scratch.intersections ||
      width > 4096 || height > 4096 ||
      (size_t)width > scratch.coverage_capacity ||
      point_count > scratch.intersection_capacity) return false;
  size_t edge_count = 0;
  for (size_t i = 0; i < contour_count; ++i) {
    const FontRasterContour c = contours[i];
    if (c.point_count < 3 || (size_t)c.first_point + c.point_count > point_count ||
        c.point_count > scratch.intersection_capacity - edge_count)
      return false;
    edge_count += c.point_count;
  }
  int32_t min_x = points[0].x, max_x = min_x;
  int32_t min_y = points[0].y, max_y = min_y;
  for (size_t i = 0; i < point_count; ++i) {
    // Bound subtraction/interpolation even for malformed future input. The
    // physical 200x228 face is far inside this +/-4096px fixed-point limit.
    if (points[i].x < -FONT_RASTER_COORD_LIMIT || points[i].x > FONT_RASTER_COORD_LIMIT ||
        points[i].y < -FONT_RASTER_COORD_LIMIT || points[i].y > FONT_RASTER_COORD_LIMIT)
      return false;
    if (points[i].x < min_x) min_x = points[i].x;
    if (points[i].x > max_x) max_x = points[i].x;
    if (points[i].y < min_y) min_y = points[i].y;
    if (points[i].y > max_y) max_y = points[i].y;
  }
  int first_x = font_raster_floor(min_x, FONT_RASTER_ONE);
  int end_x = font_raster_ceil(max_x, FONT_RASTER_ONE);
  int first_y = font_raster_floor(min_y, FONT_RASTER_ONE);
  int end_y = font_raster_ceil(max_y, FONT_RASTER_ONE);
  if (first_x < 0) first_x = 0;
  if (first_y < 0) first_y = 0;
  if (end_x > width) end_x = width;
  if (end_y > height) end_y = height;
  if (first_x >= end_x || first_y >= end_y) return true;

  const int sample_step = FONT_RASTER_ONE / FONT_RASTER_SAMPLES;
  const int sample_half = sample_step / 2;
  for (int y = first_y; y < end_y; ++y) {
    memset(scratch.coverage + first_x, 0, (size_t)(end_x - first_x));
    for (int sample = 0; sample < FONT_RASTER_SAMPLES; ++sample) {
      const int32_t sample_y = y * FONT_RASTER_ONE + sample_half + sample * sample_step;
      size_t count = 0;
      for (size_t ci = 0; ci < contour_count; ++ci) {
        const FontRasterContour c = contours[ci];
        FontRasterPoint a = points[c.first_point + c.point_count - 1];
        for (size_t i = 0; i < c.point_count; ++i) {
          const FontRasterPoint b = points[c.first_point + i];
          // Half-open edges count a vertex once and ignore horizontal edges.
          if ((a.y <= sample_y && b.y > sample_y) ||
              (b.y <= sample_y && a.y > sample_y)) {
            if (count >= scratch.intersection_capacity) return false;
            // Normalize direction before integer division: the same outline
            // must have identical sample coverage when its winding reverses.
            const FontRasterPoint low = a.y < b.y ? a : b;
            const FontRasterPoint high = a.y < b.y ? b : a;
            const int64_t numerator = (int64_t)(sample_y - low.y) *
                ((int64_t)high.x - low.x);
            const int64_t denominator = (int64_t)high.y - low.y;
            // Sample x coordinates are integral Q8. Ceil preserves exact
            // membership in [left,right), including fractional intersections.
            const int32_t x = low.x + (int32_t)(numerator / denominator +
                                               (numerator % denominator > 0));
            size_t at = count++;
            while (at && scratch.intersections[at - 1] > x) {
              scratch.intersections[at] = scratch.intersections[at - 1];
              --at;
            }
            scratch.intersections[at] = x;
          }
          a = b;
        }
      }
      if (count % 2) return false;
      for (size_t i = 0; i < count; i += 2) {
        int first = font_raster_ceil(scratch.intersections[i] - sample_half, sample_step);
        int end = font_raster_ceil(scratch.intersections[i + 1] - sample_half, sample_step);
        if (first < first_x * FONT_RASTER_SAMPLES) first = first_x * FONT_RASTER_SAMPLES;
        if (end > end_x * FONT_RASTER_SAMPLES) end = end_x * FONT_RASTER_SAMPLES;
        for (int at = first; at < end; ++at) ++scratch.coverage[at / FONT_RASTER_SAMPLES];
      }
    }
    draw_row(context, y, first_x, end_x, scratch.coverage);
  }
  return true;
}
