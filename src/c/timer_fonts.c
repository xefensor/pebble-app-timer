#include "timer_fonts.h"

#include <stddef.h>
#include <string.h>

#include "font_raster.h"
#include "generated_clock_glyphs.h"

#define SCREEN_WIDTH 200
#define SCREEN_HEIGHT 228

/* One glyph at a time, shared by every clock. No framebuffer, glyph bitmap,
 * allocation or unbounded text copy is needed during a draw. */
static FontRasterPoint s_points_q8[CLOCK_GLYPH_MAX_POINTS];
static FontRasterContour s_contours[CLOCK_GLYPH_MAX_CONTOURS];
static int32_t s_intersections[CLOCK_GLYPH_MAX_POINTS];
static uint8_t s_coverage[SCREEN_WIDTH];

typedef struct {
  GContext *context;
  GColor colors[FONT_RASTER_COVERAGE + 1];
} ClockPaint;

static GFont prv_body(int point) {
  /* Native18 has a small ink height; regular24 is too thin for a reflective
   * display. Keep utility ink readable and hierarchy in placement/colour and
   * the larger Fredoka primary values. */
  return fonts_get_system_font(point>=28?FONT_KEY_GOTHIC_28_BOLD:FONT_KEY_GOTHIC_24_BOLD);
}

bool timer_fonts_init(void) { return true; }
void timer_fonts_deinit(void) {}

int timer_fonts_text_width(const char *text, int point) {
  return graphics_text_layout_get_content_size(text ? text : "", prv_body(point),
      GRect(0, 0, 2048, 128), GTextOverflowModeFill, GTextAlignmentLeft).w;
}

void timer_fonts_text(GContext *ctx, const char *text, int point, int x,
                      int baseline, int width, GColor color, GTextAlignment align) {
  if (!ctx || !text || width <= 0) return;
  /* Gothic includes top bearing and descender clearance. Fit its visible ink
   * to the existing baseline contract, as checked in native Emery captures,
   * rather than treating its 18/24 name as the ink height. */
  const int size = point>=28?28:24;
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, prv_body(point),
      GRect(x, baseline - size - 2, width, size + 12), GTextOverflowModeFill, align, NULL);
}

static const ClockGlyph *prv_glyph(char code) {
  for (unsigned i = 0; i < CLOCK_GLYPH_COUNT; ++i)
    if (clock_glyphs[i].code == (unsigned char)code) return &clock_glyphs[i];
  return NULL;
}

static int32_t prv_div_round(int64_t numerator, int32_t denominator) {
  return (int32_t)((numerator < 0 ? numerator - denominator / 2 :
                   numerator + denominator / 2) / denominator);
}

static void prv_clock_row(void *context, int y, int first, int end,
                           const uint8_t *coverage) {
  ClockPaint *paint = context;
  for (int x = first; x < end;) {
    const GColor color = paint->colors[coverage[x]];
    const int begin = x++;
    while (x < end && paint->colors[coverage[x]].argb == color.argb) ++x;
    if (!coverage[begin]) continue;
    graphics_context_set_fill_color(paint->context, color);
    graphics_fill_rect(paint->context, GRect(begin, y, x - begin, 1), 0, GCornerNone);
  }
}

static bool prv_raster_glyph(ClockPaint *paint, const ClockGlyph *glyph,
                              int32_t x_q8, int baseline, int point) {
  if (glyph->contour_count > CLOCK_GLYPH_MAX_CONTOURS) return false;
  unsigned at = 0;
  for (unsigned i = 0; i < glyph->contour_count; ++i) {
    const ClockContour *contour = &clock_contours[glyph->first_contour + i];
    if (at + contour->point_count > CLOCK_GLYPH_MAX_POINTS) return false;
    s_contours[i] = (FontRasterContour){at, contour->point_count};
    for (unsigned j = 0; j < contour->point_count; ++j) {
      const ClockPoint p = clock_points[contour->first_point + j];
      s_points_q8[at++] = (FontRasterPoint){
        x_q8 + prv_div_round((int64_t)p.x * point * FONT_RASTER_ONE, CLOCK_FONT_UNITS_PER_EM),
        baseline * FONT_RASTER_ONE -
          prv_div_round((int64_t)p.y * point * FONT_RASTER_ONE, CLOCK_FONT_UNITS_PER_EM)
      };
    }
  }
  const FontRasterScratch scratch = {s_coverage, sizeof(s_coverage),
      s_intersections, CLOCK_GLYPH_MAX_POINTS};
  return font_raster_rows(s_points_q8, at, s_contours, glyph->contour_count,
      SCREEN_WIDTH, SCREEN_HEIGHT, scratch, prv_clock_row, paint);
}

void timer_fonts_digits(GContext *ctx, const char *text, int center_x,
                        int baseline, int point, GColor foreground, GColor background) {
  const int width = timer_font_digits_width_q8(text, point);
  if (!ctx || width < 0) return;
  ClockPaint paint = {.context = ctx};
  const unsigned fr = (foreground.argb >> 4) & 3, fg = (foreground.argb >> 2) & 3,
      fb = foreground.argb & 3;
  const unsigned br = (background.argb >> 4) & 3, bg = (background.argb >> 2) & 3,
      bb = background.argb & 3;
  for (unsigned i = 0; i <= FONT_RASTER_COVERAGE; ++i) {
    const unsigned back = FONT_RASTER_COVERAGE - i, half = FONT_RASTER_COVERAGE / 2;
    paint.colors[i] = (GColor){.argb = 0xc0 |
      (((fr * i + br * back + half) / FONT_RASTER_COVERAGE) << 4) |
      (((fg * i + bg * back + half) / FONT_RASTER_COVERAGE) << 2) |
       ((fb * i + bb * back + half) / FONT_RASTER_COVERAGE)};
  }
  int x_q8 = center_x * FONT_RASTER_ONE - width / 2;
  for (; *text; ++text) {
    const ClockGlyph *glyph = prv_glyph(*text);
    const int advance = timer_font_cell_advance_q8(*text, point);
    if (!glyph) return;
    const int glyph_advance = prv_div_round((int64_t)glyph->advance * point * FONT_RASTER_ONE,
                                           CLOCK_FONT_UNITS_PER_EM);
    /* Center the actual glyph advance in the stable cell, as in the SVG. */
    if (!prv_raster_glyph(&paint, glyph, x_q8 + (advance - glyph_advance) / 2, baseline, point))
      return;
    x_q8 += advance;
  }
}
