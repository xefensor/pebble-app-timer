#pragma once

#include <pebble.h>
#include "timer_collection.h"

typedef enum {
  SCREEN_LIST,
  SCREEN_EDITOR,
  SCREEN_DETAIL,
  SCREEN_ALARM,
  SCREEN_OPTIONS,
  SCREEN_CONFIRM
} Screen;

typedef struct {
  Screen screen;
  const TimerCollection *collection;
  unsigned menu;
  unsigned selected;
  unsigned edit_field;
  unsigned option;
  unsigned confirm_choice;
  const uint32_t *edit;
  bool editing_new;
  bool editing_ready;
  uint32_t now;
  const char *status;
  const char *issue;
} TimerUiView;

void timer_ui_draw(GContext *context, const TimerUiView *view);
bool timer_ui_needs_tick(const TimerUiView *view);
