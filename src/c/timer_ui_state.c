#include "timer_ui_state.h"

#include <string.h>

unsigned timer_ui_window_path(Screen screen,Screen edit_return,Screen path[4]) {
  unsigned count=0;path[count++]=SCREEN_LIST;
  if(screen==SCREEN_LIST)return count;
  if(screen==SCREEN_ALARM){path[count++]=SCREEN_ALARM;return count;}
  if(screen==SCREEN_EDITOR&&edit_return==SCREEN_LIST){path[count++]=SCREEN_EDITOR;return count;}
  path[count++]=SCREEN_DETAIL;
  if(screen==SCREEN_OPTIONS||screen==SCREEN_CONFIRM||
      (screen==SCREEN_EDITOR&&edit_return==SCREEN_OPTIONS))path[count++]=SCREEN_OPTIONS;
  if(screen==SCREEN_EDITOR||screen==SCREEN_CONFIRM)path[count++]=screen;
  return count;
}

unsigned timer_ui_menu_for_slot(const TimerCollection *collection,unsigned slot) {
  unsigned slots[TIMER_COLLECTION_CAPACITY];
  const unsigned count=timer_collection_visible_slots(collection,slots);
  for(unsigned row=0;row<count;++row)if(slots[row]==slot)return row+1;
  return count?1:0;
}

bool timer_ui_needs_tick(const TimerUiView *view) {
  if (!view || !view->collection) return false;
  if (view->screen == SCREEN_DETAIL && view->selected < TIMER_COLLECTION_CAPACITY)
    return view->collection->slots[view->selected].state == ACTIVITY_TIMER_RUNNING;
  if (view->screen == SCREEN_LIST) {
    for(unsigned slot=0;slot<TIMER_COLLECTION_CAPACITY;++slot)
      if((view->visible_slots&(1u<<slot))&&
          view->collection->slots[slot].state==ACTIVITY_TIMER_RUNNING)return true;
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
