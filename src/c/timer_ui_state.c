#include "timer_ui_state.h"

#include <string.h>

TimerListWindow timer_ui_list_window(const TimerUiView *view) {
  TimerListWindow list = {0};
  if (!view || !view->collection) return list;
  list.count = timer_collection_visible_slots(view->collection, list.slots);
  list.menu = view->menu > list.count ? list.count : view->menu;
  list.rows = view->issue && view->issue[0] ? 2 : 3;
  list.first = list.menu > 1 ? list.menu - 2 : 0;
  if (list.first + list.rows > list.count)
    list.first = list.count > list.rows ? list.count - list.rows : 0;
  return list;
}

bool timer_ui_needs_tick(const TimerUiView *view) {
  if (!view || !view->collection) return false;
  if (view->screen == SCREEN_DETAIL && view->selected < TIMER_COLLECTION_CAPACITY)
    return view->collection->slots[view->selected].state == ACTIVITY_TIMER_RUNNING;
  if (view->screen == SCREEN_LIST) {
    const TimerListWindow list = timer_ui_list_window(view);
    for (unsigned row = list.first; row < list.count && row < list.first + list.rows; ++row)
      if (view->collection->slots[list.slots[row]].state == ACTIVITY_TIMER_RUNNING)
        return true;
  }
  return false;
}

static bool same(const char *first, const char *second) {
  return !strcmp(first ? first : "", second ? second : "");
}

bool timer_ui_chrome_changed(const TimerUiView *view, Screen previous_screen,
                             const char *previous_issue, const char *previous_status) {
  return view && (view->screen != previous_screen || !same(view->issue, previous_issue) ||
      (view->screen == SCREEN_OPTIONS && !same(view->status, previous_status)));
}
