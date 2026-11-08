#include "timer_fonts.h"
#include <pebble-xoce-ui/pebble-xoce-ui.h>

#include <stddef.h>
#include <stdio.h>
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

typedef struct { uint16_t code; int16_t top,bottom; } TimerGlyphInk;
#include "generated_body_metrics.h"
static const int s_sizes[]={XOCE_UI_SECONDARY_POINT,XOCE_UI_BODY_POINT,XOCE_UI_HEADER_POINT,18,20};
static const uint32_t s_resources[]={RESOURCE_ID_FONT_BODY_22,RESOURCE_ID_FONT_BODY_24,RESOURCE_ID_FONT_BODY_26,RESOURCE_ID_FONT_BODY_18,RESOURCE_ID_FONT_BODY_20};
static GFont s_body[5];
static unsigned prv_index(int point) { return point<=18?3:point<=20?4:point<=22?0:point<=24?1:2; }
static GFont prv_body(int point) {
  const unsigned index=prv_index(point);
  return s_body[index]?s_body[index]:fonts_get_system_font(index?FONT_KEY_GOTHIC_28_BOLD:FONT_KEY_GOTHIC_24_BOLD);
}
bool timer_fonts_init(void) {
  timer_fonts_deinit();bool loaded=true;
  for(unsigned i=0;i<5;++i){s_body[i]=fonts_load_custom_font(resource_get_handle(s_resources[i]));if(!s_body[i])loaded=false;}
  return loaded;
}
void timer_fonts_deinit(void) {
  for(unsigned i=0;i<5;++i){if(s_body[i])fonts_unload_custom_font(s_body[i]);s_body[i]=NULL;}
}
static uint32_t prv_codepoint(const unsigned char **at) {
  const unsigned char *p=*at;uint32_t code=*p++;
  if(code>=0xc2&&code<0xe0&&(p[0]&0xc0)==0x80){code=((code&31)<<6)|(p[0]&63);++p;}
  else if(code>=0xe0&&code<0xf0&&p[0]&&p[1]&&(p[0]&0xc0)==0x80&&(p[1]&0xc0)==0x80){code=((code&15)<<12)|((p[0]&63)<<6)|(p[1]&63);p+=2;}
  *at=p;return code;
}
static TimerGlyphInk prv_ink(const char *text,int point) {
  const unsigned index=prv_index(point);
  const TimerGlyphInk *rows=index==0?s_body_ink_0:index==1?s_body_ink_1:index==2?s_body_ink_2:index==3?s_body_ink_3:s_body_ink_4;
  const unsigned count=index==0?sizeof(s_body_ink_0)/sizeof(*rows):index==1?sizeof(s_body_ink_1)/sizeof(*rows):index==2?sizeof(s_body_ink_2)/sizeof(*rows):index==3?sizeof(s_body_ink_3)/sizeof(*rows):sizeof(s_body_ink_4)/sizeof(*rows);
  TimerGlyphInk result={0,255,0};
  for(const unsigned char *p=(const unsigned char *)(text?text:"");*p;) {
    const uint32_t code=prv_codepoint(&p);const TimerGlyphInk *glyph=NULL,*wildcard=NULL;
    for(unsigned i=0;i<count;++i){if(rows[i].code==code)glyph=&rows[i];if(rows[i].code==s_body_wildcards[index])wildcard=&rows[i];}
    if(!glyph)glyph=wildcard;
    if(!glyph||glyph->top==255)continue;
    if(glyph->top<result.top)result.top=glyph->top;
    if(glyph->bottom>result.bottom)result.bottom=glyph->bottom;
  }
  return result;
}
int timer_fonts_text_width(const char *text,int point) {
  return graphics_text_layout_get_content_size(text?text:"",prv_body(point),GRect(0,0,2048,128),GTextOverflowModeFill,GTextAlignmentLeft).w;
}
int timer_fonts_text_height(const char *text,int point) {
  const TimerGlyphInk ink=prv_ink(text,point);return ink.top==255?0:ink.bottom-ink.top+1;
}
static void prv_fit(char out[160],const char *text,int point,int width) {
  snprintf(out,160,"%s",text?text:"");
  const int available=width;
  while(out[0]&&timer_fonts_text_width(out,point)>available) {
    size_t n=strlen(out);do{--n;}while(n&&((unsigned char)out[n]&0xc0)==0x80);out[n]=0;
    if(n+4<160){memcpy(out+n,"…",4);if(timer_fonts_text_width(out,point)<=available)break;out[n]=0;}
  }
}
static void prv_draw(GContext *ctx,const char *text,int point,int x,int top,int width,GColor color,GTextAlignment align) {
  const int advance=timer_fonts_text_width(text,point);
  if(align==GTextAlignmentRight)x+=width-advance;
  else if(align==GTextAlignmentCenter)x+=(width-advance)/2;
  /* The string is fitted before this point. A wide paint rectangle prevents
   * SDK word wrapping from dropping a tight clock/unit field on repaint. */
  graphics_context_set_text_color(ctx,color);
  graphics_draw_text(ctx,text,prv_body(point),GRect(x,top,2048,s_sizes[prv_index(point)]+16),GTextOverflowModeFill,GTextAlignmentLeft,NULL);
}
void timer_fonts_text_box(GContext *ctx,const char *text,int point,GRect bounds,GColor color,GTextAlignment align) {
  if(!ctx||!text||bounds.size.w<=0||bounds.size.h<=0)return;
  char fitted[160];prv_fit(fitted,text,point,bounds.size.w);const TimerGlyphInk ink=prv_ink(fitted,point);
  if(ink.top==255)return;
  prv_draw(ctx,fitted,point,bounds.origin.x,xoce_ui_text_origin_y(bounds,ink.top,ink.bottom),bounds.size.w,color,align);
}
void timer_fonts_text_top(GContext *ctx,const char *text,int point,int x,int top,int width,GColor color,GTextAlignment align) {
  if(!ctx||!text||width<=0)return;
  char fitted[160];prv_fit(fitted,text,point,width);const TimerGlyphInk ink=prv_ink(fitted,point);if(ink.top==255)return;
  prv_draw(ctx,fitted,point,x,top-ink.top,width,color,align);
}
void timer_fonts_text(GContext *ctx,const char *text,int point,int x,int baseline,int width,GColor color,GTextAlignment align) {
  if(!ctx||!text||width<=0)return;
  char fitted[160];prv_fit(fitted,text,point,width);
  prv_draw(ctx,fitted,point,x,baseline-s_sizes[prv_index(point)],width,color,align);
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

void timer_fonts_digits_box(GContext *ctx,const char *text,int point,GRect bounds,GColor foreground,GColor background) {
  int ascent=0,descent=0;
  for(unsigned i=0;i<CLOCK_GLYPH_COUNT;++i)if(clock_glyphs[i].code>='0'&&clock_glyphs[i].code<='9'){
    if(clock_glyphs[i].y_max>ascent)ascent=clock_glyphs[i].y_max;
    if(-clock_glyphs[i].y_min>descent)descent=-clock_glyphs[i].y_min;
  }
  const int high=(ascent*point+999)/1000,low=(descent*point+999)/1000;
  const int baseline=bounds.origin.y+(bounds.size.h-high-low)/2+high;
  timer_fonts_digits(ctx,text,bounds.origin.x+bounds.size.w/2,baseline,point,foreground,background);
}
