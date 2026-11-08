#pragma once

#include <pebble-xoce-ui/native-type.h>

typedef XoceUiTypeRole TimerNativeTypeRole;
#define TIMER_TYPE_HEADER XOCE_UI_TYPE_HEADER
#define TIMER_TYPE_MENU_TITLE XOCE_UI_TYPE_MENU_TITLE
#define TIMER_TYPE_MENU_SUBTITLE XOCE_UI_TYPE_MENU_SUBTITLE
#define TIMER_TYPE_FOOTER XOCE_UI_TYPE_FOOTER
#define TIMER_TYPE_ROLE_COUNT XOCE_UI_TYPE_ROLE_COUNT

/* App-local names retain the controller boundary; the shared package owns
 * PebbleOS's role table. The app supplies firmware and effective size. */
static inline const char *timer_native_type_key(unsigned size,TimerNativeTypeRole role,bool modern) {
  return xoce_ui_native_font_key(size,role,modern);
}

#ifndef TIMER_NATIVE_TYPE_PROFILE_ONLY
#include <pebble.h>

void timer_native_type_init(void);
GSize timer_native_type_measure(const char *text,unsigned size,TimerNativeTypeRole role,int width);
void timer_native_type_draw(GContext *ctx,const char *text,unsigned size,TimerNativeTypeRole role,
                            GRect bounds,GColor color,GTextAlignment align,GTextOverflowMode overflow);
void timer_native_type_countdown(GContext *ctx,const char *text,GRect bounds,GColor color);
#endif
