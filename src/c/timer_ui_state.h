#pragma once

#include "timer_collection.h"
#include <pebble-xoce-ui/metrics.h>
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
  /* Emery Small/Medium/Large resolved once from preferred_content_size(). */
  unsigned text_size;
  uint8_t visible_slots;
  uint32_t now;
  const char *status, *issue;
  bool hourglass_transition;
  int16_t hourglass_angle;
  bool selection_transition;
  TimerMotionRect selection_rect;
  TimerValueMotion editor_motion;
  uint32_t editor_previous_value;
  int8_t action_offset[3];
} TimerUiView;

#define TIMER_OPTION_TOP 68
#define TIMER_OPTION_HEIGHT 38
#define TIMER_OPTION_STRIDE 44
#define TIMER_CONFIRM_TOP 160
#define TIMER_CONFIRM_HEIGHT XOCE_UI_CHOICE_HEIGHT
#define TIMER_CONFIRM_STRIDE XOCE_UI_CHOICE_STRIDE
static inline int timer_ui_option_top(const TimerUiView *view) {
  return view->issue&&view->issue[0]?94:TIMER_OPTION_TOP;
}

/* Native viewport membership includes partially visible rows during motion. */
unsigned timer_ui_menu_for_slot(const TimerCollection *collection, unsigned slot);
bool timer_ui_needs_tick(const TimerUiView *view);
/* Transport state only appears in Options; critical issues appear everywhere. */
bool timer_ui_chrome_changed(const TimerUiView *view, Screen previous_screen,
                             const char *previous_issue, const char *previous_status);
