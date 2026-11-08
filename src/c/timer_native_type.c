#include "timer_native_type.h"
#include <pebble-xoce-ui/pebble-xoce-ui.h>

static bool s_modern;

void timer_native_type_init(void) {
  const WatchInfoVersion firmware=watch_info_get_firmware_version();
  s_modern=firmware.major>4||(firmware.major==4&&firmware.minor>=44);
}

GSize timer_native_type_measure(const char *text,unsigned size,TimerNativeTypeRole role,int width) {
  return xoce_ui_native_measure(text,size,role,s_modern,width);
}

void timer_native_type_draw(GContext *ctx,const char *text,unsigned size,TimerNativeTypeRole role,
                            GRect bounds,GColor color,GTextAlignment align,GTextOverflowMode overflow) {
  xoce_ui_native_draw(ctx,text,size,role,s_modern,bounds,color,align,overflow);
}

void timer_native_type_countdown(GContext *ctx,const char *text,GRect bounds,GColor color) {
  /* Native bitmap sizes only: fit with the next available system number font,
   * never scale a custom outline. This dominant readout is independent of the
   * OS's body/menu text scale, as in stock Timer's separate numeric readout. */
  static const char *const keys[]={FONT_KEY_LECO_42_NUMBERS,FONT_KEY_LECO_38_BOLD_NUMBERS,
      FONT_KEY_LECO_36_BOLD_NUMBERS,FONT_KEY_LECO_32_BOLD_NUMBERS,
      FONT_KEY_LECO_26_BOLD_NUMBERS_AM_PM,FONT_KEY_LECO_20_BOLD_NUMBERS};
  for(unsigned i=0;i<sizeof(keys)/sizeof(*keys);++i) {
    const GFont f=fonts_get_system_font(keys[i]);
    const GSize measured=graphics_text_layout_get_content_size(text,f,GRect(0,0,2048,128),
        GTextOverflowModeWordWrap,GTextAlignmentLeft);
    if(measured.w>bounds.size.w&&i+1<sizeof(keys)/sizeof(*keys))continue;
    graphics_context_set_text_color(ctx,color);
    graphics_draw_text(ctx,text,f,bounds,GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
    return;
  }
}
