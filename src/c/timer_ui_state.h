#pragma once

#include "timer_collection.h"

typedef enum {
  SCREEN_LIST, SCREEN_EDITOR, SCREEN_DETAIL, SCREEN_ALARM, SCREEN_OPTIONS,
  SCREEN_CONFIRM
} Screen;

typedef struct {
  Screen screen;
  const TimerCollection *collection;
  unsigned menu, selected, edit_field, option, confirm_choice;
  const uint32_t *edit;
  bool editing_new, editing_ready;
  uint32_t now;
  const char *status, *issue;
} TimerUiView;

typedef struct {
  unsigned count, menu, first, rows;
  unsigned slots[TIMER_COLLECTION_CAPACITY];
} TimerListWindow;

/* The renderer and tick decision share exactly the same visible row window. */
TimerListWindow timer_ui_list_window(const TimerUiView *view);
bool timer_ui_needs_tick(const TimerUiView *view);
/* Transport state only appears in Options; critical issues appear everywhere. */
bool timer_ui_chrome_changed(const TimerUiView *view, Screen previous_screen,
                             const char *previous_issue, const char *previous_status);
