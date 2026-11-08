#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

enum {
  TIMER_UI_PREFERENCES_BYTES=20,
  TIMER_UI_APPLIED=0,
  TIMER_UI_SAVE_FAILED=1,
  TIMER_UI_HELLO=1,
  TIMER_UI_FOLLOW=0,
  TIMER_UI_LIGHT=1,
  TIMER_UI_DARK=2
};
typedef struct {
  uint32_t revision;
  uint8_t text_size,appearance,last_explicit;
} TimerUiPreferences;

/* A local UI preference never belongs to a timer bank or publication receipt. */
TimerUiPreferences timer_ui_preferences_defaults(bool legacy_dark);
bool timer_ui_preferences_valid(const TimerUiPreferences *value);
void timer_ui_preferences_encode(const TimerUiPreferences *value,uint8_t status,
    uint8_t flags,uint8_t bytes[TIMER_UI_PREFERENCES_BYTES]);
bool timer_ui_preferences_decode(const uint8_t *bytes,size_t length,
    TimerUiPreferences *value,uint8_t *status,uint8_t *flags);
bool timer_ui_preferences_dark(const TimerUiPreferences *value);
bool timer_ui_preferences_load(TimerUiPreferences *value);
bool timer_ui_preferences_save(const TimerUiPreferences *value);
