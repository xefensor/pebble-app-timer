#pragma once

#include "timer_collection.h"
#include "timer_ui_motion.h"

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
  bool dark, appearance_issue;
  uint32_t now;
  const char *status, *issue;
  bool hourglass_transition;
  int16_t hourglass_angle;
  bool selection_transition;
  TimerMotionRect selection_rect;
  int8_t editor_bump;
  int8_t action_offset[3];
} TimerUiView;

typedef struct {
  unsigned count, menu, first, rows;
  unsigned slots[TIMER_COLLECTION_CAPACITY];
} TimerListWindow;

#define TIMER_LIST_ROW_HEIGHT 80
#define TIMER_LIST_ROW_STRIDE 88
#define TIMER_OPTION_TOP 68
#define TIMER_OPTION_HEIGHT 38
#define TIMER_OPTION_STRIDE 44
#define TIMER_CONFIRM_TOP 160
#define TIMER_CONFIRM_HEIGHT 30
#define TIMER_CONFIRM_STRIDE 36
static inline int timer_ui_option_top(const TimerUiView *view) {
  return view->issue&&view->issue[0]?94:TIMER_OPTION_TOP;
}

/* The renderer and tick decision share exactly the same visible row window. */
TimerListWindow timer_ui_list_window(const TimerUiView *view);
int timer_ui_list_top(const TimerUiView *view);
bool timer_ui_needs_tick(const TimerUiView *view);
/* Transport state only appears in Options; critical issues appear everywhere. */
bool timer_ui_chrome_changed(const TimerUiView *view, Screen previous_screen,
                             const char *previous_issue, const char *previous_status);
