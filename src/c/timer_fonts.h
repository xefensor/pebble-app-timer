#pragma once

#include <stdbool.h>
#include <stdint.h>

/* Matching fixed cells keep the countdown still when a glyph changes. Q8
 * geometry is shared with the portable corpus check. */
static inline int timer_font_cell_advance_q8(char code, int point) {
  if (point < 1 || point > 96) return -1;
  if (code == ':') return point * 256 / 4;
  if (code >= '0' && code <= '9') return point * 256 * 58 / 100;
  return -1;
}

static inline int timer_font_digits_width_q8(const char *text, int point) {
  if (!text) return -1;
  int width = 0;
  unsigned count = 0;
  for (; *text; ++text) {
    const int advance = timer_font_cell_advance_q8(*text, point);
    if (advance < 0 || ++count > 16) return -1;
    width += advance;
  }
  return width;
}

#ifndef TIMER_FONTS_GEOMETRY_ONLY
#include <pebble.h>

/* Coordinates use the same nominal baseline as the preview. All custom
 * assets have bounded lifetimes; drawing performs no heap allocation. */
bool timer_fonts_init(void);
void timer_fonts_deinit(void);
int timer_fonts_text_width(const char *text, int point);
void timer_fonts_text(GContext *ctx, const char *text, int point, int x,
                      int baseline, int width, GColor color, GTextAlignment align);
void timer_fonts_digits(GContext *ctx, const char *text, int center_x,
                        int baseline, int point, GColor foreground, GColor background);
#endif
