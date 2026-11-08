#pragma once

#include <stdbool.h>

typedef enum {
  TIMER_TYPE_HEADER, TIMER_TYPE_MENU_TITLE, TIMER_TYPE_MENU_SUBTITLE,
  TIMER_TYPE_FOOTER, TIMER_TYPE_ROLE_COUNT
} TimerNativeTypeRole;

/* Public SDK font keys, matching PebbleOS's semantic style roles. Emery's
 * three settings are content sizes Medium/Large/ExtraLarge. The 4.44 style
 * table completes ExtraLarge; older firmware uses its original table.
 * Provenance and independent regression expectations: docs/native-type.json. */
static inline const char *timer_native_type_key(unsigned size,TimerNativeTypeRole role,
                                                bool modern) {
  static const char *const keys[3][TIMER_TYPE_ROLE_COUNT]={
    {"RESOURCE_ID_GOTHIC_18_BOLD","RESOURCE_ID_GOTHIC_24_BOLD","RESOURCE_ID_GOTHIC_18","RESOURCE_ID_GOTHIC_18"},
    {"RESOURCE_ID_GOTHIC_24_BOLD","RESOURCE_ID_GOTHIC_24_BOLD","RESOURCE_ID_GOTHIC_24","RESOURCE_ID_GOTHIC_18"},
    {"RESOURCE_ID_GOTHIC_28_BOLD","RESOURCE_ID_GOTHIC_28","RESOURCE_ID_GOTHIC_24_BOLD","RESOURCE_ID_GOTHIC_24"}
  };
  if(size>2)size=2;
  if(role>=TIMER_TYPE_ROLE_COUNT)role=TIMER_TYPE_HEADER;
  if(modern&&size==2) {
    if(role==TIMER_TYPE_MENU_TITLE)return "RESOURCE_ID_GOTHIC_28_BOLD";
    if(role==TIMER_TYPE_MENU_SUBTITLE)return "RESOURCE_ID_GOTHIC_28";
  }
  return keys[size][role];
}

#ifndef TIMER_NATIVE_TYPE_PROFILE_ONLY
#include <pebble.h>

void timer_native_type_init(void);
GSize timer_native_type_measure(const char *text,unsigned size,TimerNativeTypeRole role,int width);
void timer_native_type_draw(GContext *ctx,const char *text,unsigned size,TimerNativeTypeRole role,
                            GRect bounds,GColor color,GTextAlignment align,GTextOverflowMode overflow);
void timer_native_type_countdown(GContext *ctx,const char *text,GRect bounds,GColor color);
#endif
