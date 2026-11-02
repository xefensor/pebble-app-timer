#include "../src/c/timer_collection.h"

#include <assert.h>
#include <limits.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static void legacy_migration(void) {
  ActivityTimerModel legacy;
  activity_timer_init(&legacy, 417, 1000);
  assert(activity_timer_start(&legacy, 600, 1000));
  assert(activity_timer_pause(&legacy, 1100));
  TimerCollection collection;
  assert(timer_collection_migrate(&collection, &legacy, 1500));
  assert(timer_collection_valid(&collection));
  assert(timer_collection_count(&collection) == 1);
  assert(timer_collection_visible_slot(&collection, 0) == 0);
  assert(!memcmp(&collection.slots[0], &legacy, sizeof(legacy)));
  assert(collection.slots[0].remaining_seconds == 500);
  assert(collection.slots[0].origin == 417 && collection.slots[0].run == 1);
  assert(collection.revision == legacy.revision);
  TimerCollection previous = collection;
  legacy.version = 99;
  assert(!timer_collection_migrate(&collection, &legacy, 1500));
  assert(!memcmp(&collection, &previous, sizeof(collection)));
}

static void independent_timers(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 100, 1000);
  assert(timer_collection_valid(&collection));
  assert(timer_collection_count(&collection) == 0);
  assert(timer_collection_create(&collection, 600, 1000) == 0);
  assert(timer_collection_create(&collection, 1200, 1000) == 1);
  assert(collection.slots[0].origin != collection.slots[1].origin);
  assert(collection.slots[0].revision < collection.slots[1].revision);
  assert(activity_timer_remaining(&collection.slots[0], 1100) == 500);
  assert(activity_timer_remaining(&collection.slots[1], 1100) == 1100);
  assert(timer_collection_pause(&collection, 0, 1100));
  assert(activity_timer_remaining(&collection.slots[0], 1500) == 500);
  assert(activity_timer_remaining(&collection.slots[1], 1500) == 700);
  assert(timer_collection_resume(&collection, 0, 1500));
  assert(collection.slots[0].deadline == 2000);
  assert(collection.slots[1].deadline == 2200);
  assert(timer_collection_finish_due(&collection, 2000) == 1u);
  assert(collection.slots[0].state == ACTIVITY_TIMER_FINISHED);
  assert(collection.slots[1].state == ACTIVITY_TIMER_RUNNING);
  assert(timer_collection_finish_due(&collection, 2000) == 0);
  uint32_t previous_run = collection.slots[0].run;
  uint32_t previous_revision = collection.revision;
  assert(timer_collection_snooze(&collection, 0, 2000));
  assert(collection.slots[0].run == previous_run + 1);
  assert(collection.slots[0].deadline == 2060);
  assert(collection.slots[0].revision > previous_revision);
  assert(timer_collection_finish_due(&collection, 2300) == 3u);
  assert(timer_collection_valid(&collection));
}

static void edit_and_slot_reuse(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 200, 1000);
  int slot = timer_collection_create(&collection, 600, 1000);
  assert(slot == 0);
  uint32_t origin = collection.slots[0].origin;
  uint32_t run = collection.slots[0].run;
  assert(timer_collection_edit(&collection, 0, 1200, 1100));
  assert(collection.slots[0].origin == origin && collection.slots[0].run == run);
  assert(collection.slots[0].deadline == 2300 && collection.slots[0].configured_seconds == 1200);
  assert(timer_collection_pause(&collection, 0, 1200));
  assert(timer_collection_edit(&collection, 0, 35, 1250));
  assert(collection.slots[0].state == ACTIVITY_TIMER_PAUSED);
  assert(collection.slots[0].remaining_seconds == 35 && !collection.slots[0].deadline);
  assert(collection.slots[0].run == run);
  assert(timer_collection_delete(&collection, 0, 1300));
  assert(timer_collection_count(&collection) == 0);
  assert(collection.slots[0].state == ACTIVITY_TIMER_CANCELLED);
  assert(collection.slots[0].origin == origin && collection.slots[0].run == run);
  uint32_t tombstone_revision = collection.revision;
  assert(timer_collection_create(&collection, 60, 1350) == 0);
  assert(collection.slots[0].origin == origin && collection.slots[0].run == run + 1);
  assert(collection.slots[0].revision > tombstone_revision);
  assert(timer_collection_finish_due(&collection, 1410) == 1);
  assert(timer_collection_edit(&collection, 0, 40, 1500));
  assert(collection.slots[0].state == ACTIVITY_TIMER_RUNNING);
  assert(collection.slots[0].run == run + 2 && collection.slots[0].deadline == 1540);
  assert(timer_collection_finish_due(&collection, 1540) == 1);
  assert(timer_collection_delete(&collection, 0, 1541));
  assert(!timer_collection_visible(&collection, 0));
  assert(timer_collection_valid(&collection));
}

static void capacity_and_validation(void) {
  TimerCollection collection;
  timer_collection_init(&collection, UINT32_MAX - 2, 1000);
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    assert(timer_collection_create(&collection, 5 + i, 1000) == (int)i);
    assert(timer_collection_valid(&collection));
    assert(timer_collection_visible_slot(&collection, i) == (int)i);
  }
  assert(timer_collection_count(&collection) == TIMER_COLLECTION_CAPACITY);
  assert(timer_collection_visible_slot(&collection, TIMER_COLLECTION_CAPACITY) == -1);
  TimerCollection previous = collection;
  assert(timer_collection_create(&collection, 5, 1000) == -1);
  assert(!timer_collection_edit(&collection, 8, 60, 1000));
  assert(!timer_collection_edit(&collection, 0, 4, 1000));
  assert(!timer_collection_edit(&collection, 0, ACTIVITY_TIMER_MAX_SECONDS + 1, 1000));
  assert(!timer_collection_snooze(&collection, 0, 1000));
  assert(!memcmp(&collection, &previous, sizeof(collection)));
  collection.visible_mask |= 1u << 8;
  assert(!timer_collection_valid(&collection));
  collection = previous;
  collection.slots[1].origin = collection.slots[0].origin;
  assert(!timer_collection_valid(&collection));
  collection = previous;
  collection.revision = 1;
  assert(!timer_collection_valid(&collection));
}

static void revision_floor_and_persistence(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 77, 1000);
  assert(timer_collection_create(&collection, 60, 1000) == 0);
  assert(timer_collection_create(&collection, 60, 1000) == 1);
  uint32_t observed = collection.slots[0].observed_at;
  assert(timer_collection_raise_floor(&collection, 500));
  assert(collection.revision == 500 && collection.slots[0].observed_at == observed);
  assert(timer_collection_reconfirm(&collection, 0, 1020));
  assert(collection.slots[0].revision == 501 && collection.slots[0].observed_at == 1020);
  assert(timer_collection_reconfirm(&collection, 1, 1020));
  assert(collection.slots[1].revision == 502);
  /* Empty native slots can retract phone state left by a native data reset. */
  assert(timer_collection_reconfirm(&collection, 2, 1020));
  assert(collection.slots[2].state == ACTIVITY_TIMER_CANCELLED && !collection.slots[2].run);
  assert(collection.slots[2].revision == 503);
  assert(!timer_collection_reconfirm(&collection, 0, 1060));
  assert(timer_collection_finish_due(&collection, 1060) == 3u);
  assert(collection.slots[0].revision == 504 && collection.slots[1].revision == 505);
  assert(timer_collection_delete(&collection, 0, 1061));
  assert(timer_collection_reconfirm(&collection, 0, 1100));
  assert(collection.slots[0].state == ACTIVITY_TIMER_CANCELLED && !timer_collection_visible(&collection, 0));
  TimerCollection restored;
  memset(&restored, 0, sizeof(restored));
  memcpy(&restored, &collection, offsetof(TimerCollection, slots));
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    memcpy(&restored.slots[i], &collection.slots[i], sizeof(ActivityTimerModel));
  assert(timer_collection_valid(&restored));
  assert(!memcmp(&restored, &collection, sizeof(collection)));
  TimerCollection previous = collection;
  assert(!timer_collection_raise_floor(&collection, UINT32_MAX));
  assert(!memcmp(&collection, &previous, sizeof(collection)));
  collection.revision = UINT32_MAX;
  previous = collection;
  assert(!timer_collection_pause(&collection, 1, 1100));
  assert(timer_collection_create(&collection, 5, 1100) == -1);
  assert(!memcmp(&collection, &previous, sizeof(collection)));
  assert(sizeof(ActivityTimerModel) <= 256);
  assert(offsetof(TimerCollection, slots) <= 256);
}

int main(void) {
  legacy_migration();
  independent_timers();
  edit_and_slot_reuse();
  capacity_and_validation();
  revision_floor_and_persistence();
  puts("timer collection: migration, eight slots, clocks, revisions, editing and persistence passed");
  return 0;
}
