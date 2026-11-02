#define TIMER_FONTS_GEOMETRY_ONLY
#include "../src/c/timer_fonts.h"
#include "../src/c/generated_clock_glyphs.h"
#include "../src/c/font_raster.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const ClockGlyph *glyph(char code) {
  for (unsigned i = 0; i < CLOCK_GLYPH_COUNT; ++i)
    if (clock_glyphs[i].code == (unsigned char)code) return &clock_glyphs[i];
  return NULL;
}

static void clock_format(unsigned seconds, char *text, size_t capacity) {
  if (seconds >= 3600)
    snprintf(text, capacity, "%u:%02u:%02u", seconds / 3600, seconds / 60 % 60, seconds % 60);
  else snprintf(text, capacity, "%02u:%02u", seconds / 60, seconds % 60);
}

static void corpus_bounds(void) {
  char text[16];
  for (unsigned second = 0; second <= 86400; ++second) {
    clock_format(second, text, sizeof(text));
    const int point = second < 3600 ? 58 : 36;
    const int total = timer_font_digits_width_q8(text, point);
    assert(total > 0 && total <= 156 * 256);
    int x = 83 * 256 - total / 2;
    for (const char *at = text; *at; ++at) {
      const ClockGlyph *g = glyph(*at);
      assert(g && g->contour_count && g->contour_count <= CLOCK_GLYPH_MAX_CONTOURS);
      const int cell = timer_font_cell_advance_q8(*at, point);
      const int origin = x + (cell - g->advance * point * 256 / 1000) / 2;
      const int left = origin + g->x_min * point * 256 / 1000;
      const int right = origin + g->x_max * point * 256 / 1000;
      assert(left >= 0 && right < 166 * 256);
      x += cell;
    }
  }
  assert(timer_font_digits_width_q8("24:00:00", 36) < 144 * 256);
  assert(timer_font_digits_width_q8("00:00", 58) == timer_font_digits_width_q8("11:11", 58));
  assert(timer_font_digits_width_q8("08:42", 58) == timer_font_digits_width_q8("08:41", 58));
  assert(timer_font_digits_width_q8("bad", 58) < 0);
  assert(timer_font_digits_width_q8("00:00", 97) < 0);
  assert(timer_font_digits_width_q8("00000000000000000", 58) < 0);
}

static unsigned pixels, antialias_pixels;
static void row(void *context, int y, int first, int end, const uint8_t *coverage) {
  (void)context;
  assert(y >= 0 && y < 228 && first >= 0 && end <= 200);
  for (int x = first; x < end; ++x) {
    assert(coverage[x] <= 16);
    if (coverage[x]) ++pixels;
    if (coverage[x] && coverage[x] < 16) ++antialias_pixels;
  }
}

static void glyph_raster(void) {
  uint8_t coverage[200];
  int32_t intersections[CLOCK_GLYPH_MAX_POINTS];
  FontRasterPoint points[CLOCK_GLYPH_MAX_POINTS];
  FontRasterContour contours[CLOCK_GLYPH_MAX_CONTOURS];
  for (char c = '0'; c <= '9'; ++c) {
    const ClockGlyph *g = glyph(c);
    unsigned count = 0;
    for (unsigned i = 0; i < g->contour_count; ++i) {
      const ClockContour *part = &clock_contours[g->first_contour + i];
      assert(count + part->point_count <= CLOCK_GLYPH_MAX_POINTS);
      contours[i] = (FontRasterContour){count, part->point_count};
      for (unsigned j = 0; j < part->point_count; ++j) {
        const ClockPoint p = clock_points[part->first_point + j];
        points[count++] = (FontRasterPoint){50 * 256 + p.x * 58 * 256 / 1000,
                                           82 * 256 - p.y * 58 * 256 / 1000};
      }
    }
    pixels = antialias_pixels = 0;
    const FontRasterScratch scratch = {coverage, sizeof(coverage), intersections, CLOCK_GLYPH_MAX_POINTS};
    assert(font_raster_rows(points, count, contours, g->contour_count, 200, 228, scratch, row, NULL));
    assert(pixels > 100 && antialias_pixels > 10);
  }
}

int main(void) {
  corpus_bounds();
  glyph_raster();
  puts("timer fonts: 86,401 countdown bounds, fixed cells, glyph coverage and raster samples passed");
  return 0;
}
