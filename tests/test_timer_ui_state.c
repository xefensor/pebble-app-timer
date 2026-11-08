#include "../src/c/timer_ui_state.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
  Screen path[4];
  assert(timer_ui_window_path(SCREEN_LIST,SCREEN_LIST,path)==1&&path[0]==SCREEN_LIST);
  assert(timer_ui_window_path(SCREEN_EDITOR,SCREEN_LIST,path)==2&&path[1]==SCREEN_EDITOR);
  assert(timer_ui_window_path(SCREEN_EDITOR,SCREEN_OPTIONS,path)==4&&path[1]==SCREEN_DETAIL&&path[2]==SCREEN_OPTIONS&&path[3]==SCREEN_EDITOR);
  assert(timer_ui_window_path(SCREEN_CONFIRM,SCREEN_DETAIL,path)==4&&path[3]==SCREEN_CONFIRM);
  assert(timer_ui_window_path(SCREEN_ALARM,SCREEN_OPTIONS,path)==2&&path[1]==SCREEN_ALARM);
  TimerCollection collection;
  timer_collection_init(&collection, 42, 1000);
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    assert(timer_collection_create(&collection, 60 + i * 60, 1000) == (int)i);
  for (unsigned i = 1; i < TIMER_COLLECTION_CAPACITY; ++i)
    assert(timer_collection_pause(&collection, i, 1010));
  TimerUiView view = {.screen=SCREEN_LIST, .collection=&collection, .menu=1,
      .now=1010,.visible_slots=1};
  assert(timer_ui_needs_tick(&view));
  view.visible_slots=0xfe;
  assert(!timer_ui_needs_tick(&view));
  /* Reordering after an owner action restores by timer slot, never by old
   * row index. A paused timer moves below every running timer. */
  assert(timer_ui_menu_for_slot(&collection,0)==1);
  assert(timer_collection_resume(&collection,7,1010));
  assert(timer_ui_menu_for_slot(&collection,7)==2);
  assert(timer_collection_pause(&collection,0,1011));
  assert(timer_ui_menu_for_slot(&collection,7)==1);
  assert(timer_ui_menu_for_slot(&collection,0)==2);
  view.visible_slots=1u<<7;
  assert(timer_ui_needs_tick(&view));
  view.visible_slots=1;
  assert(!timer_ui_needs_tick(&view));
  /* Due processing still sees a running timer outside the native viewport. */
  assert(timer_collection_has_due(&collection,1480));
  assert(timer_collection_finish_due(&collection,1480)==(1u<<7));
  assert(!timer_collection_has_due(&collection,1480));
  assert(timer_collection_delete(&collection,7,1480));
  assert(timer_ui_menu_for_slot(&collection,7)==1);
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
  puts("timer UI state: native viewport clocks, selection identity, native completion and visible status invalidation passed");
  return 0;
}
