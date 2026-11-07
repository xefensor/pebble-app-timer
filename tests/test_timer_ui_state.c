#include "../src/c/timer_ui_state.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 42, 1000);
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    assert(timer_collection_create(&collection, 60 + i * 60, 1000) == (int)i);
  for (unsigned i = 1; i < TIMER_COLLECTION_CAPACITY; ++i)
    assert(timer_collection_pause(&collection, i, 1010));
  TimerUiView view = {.screen=SCREEN_LIST, .collection=&collection, .menu=1, .now=1010};
  TimerListWindow list = timer_ui_list_window(&view);
  assert(list.first == 0 && list.rows == 2 && list.count == 8);
  assert(timer_ui_needs_tick(&view));
  view.menu = 8;
  list = timer_ui_list_window(&view);
  assert(list.first == 6 && list.slots[6] == 6);
  assert(!timer_ui_needs_tick(&view));
  /* Running slot zero is offscreen, but it must still become due natively. */
  assert(!timer_collection_has_due(&collection, 1059));
  assert(timer_collection_has_due(&collection, 1060));
  assert(timer_collection_finish_due(&collection, 1060) == 1);
  assert(!timer_collection_has_due(&collection, 1060));
  view.menu = 1;
  view.issue = "Alarm not set";
  list = timer_ui_list_window(&view);
  assert(list.rows == 1 && list.first == 0);
  view.menu=8;list=timer_ui_list_window(&view);
  assert(list.first==7&&list.first+list.rows==8);
  assert(!timer_ui_needs_tick(&view));
  /* Every selected item remains inside the same renderer/tick window, in
   * either direction; a warning reduces the viewport to one readable row. */
  for(unsigned warning=0;warning<2;++warning) {
    view.issue=warning?"Not saved":NULL;
    for(unsigned step=0;step<16;++step) {
      view.menu=step<8?step+1:16-step;
      list=timer_ui_list_window(&view);
      assert(view.menu-1>=list.first&&view.menu-1<list.first+list.rows);
      const int top=timer_ui_list_top(&view);
      assert(top>=40);
      assert(top+TIMER_LIST_ROW_HEIGHT+(int)(list.rows-1)*TIMER_LIST_ROW_STRIDE<= (warning?174:228));
    }
  }
  view.issue="Alarm not set";
  for (unsigned screen = SCREEN_EDITOR; screen <= SCREEN_CONFIRM; ++screen) {
    view.screen = (Screen)screen;
    view.selected = 1;
    assert(!timer_ui_needs_tick(&view));
  }
  assert(timer_collection_resume(&collection, 1, 1070));
  view.screen = SCREEN_DETAIL;
  assert(timer_ui_needs_tick(&view));
  view.selected = 2;
  assert(!timer_ui_needs_tick(&view));
  /* Transport state is invisible outside Options; critical warnings remain
   * immediately visible on every screen, including list and alarm. */
  for (unsigned screen = SCREEN_LIST; screen <= SCREEN_CONFIRM; ++screen) {
    view.screen = (Screen)screen;
    view.issue = NULL;
    view.status = "Shared with phone";
    assert(timer_ui_chrome_changed(&view, view.screen, NULL, "Waiting to share") ==
        (view.screen == SCREEN_OPTIONS));
    assert(!timer_ui_chrome_changed(&view, view.screen, NULL, "Shared with phone"));
    view.issue = "Changes not saved";
    assert(timer_ui_chrome_changed(&view, view.screen, NULL, view.status));
    assert(!timer_ui_chrome_changed(&view, view.screen, view.issue, view.status));
  }
  puts("timer UI state: offscreen clocks, shared row windows, native completion and visible status invalidation passed");
  return 0;
}
