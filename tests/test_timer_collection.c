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
  assert(collection.slots[0].deadline == 2300 && collection.slots[0].configured_seconds == 1300);
  assert(collection.slots[0].repeat_seconds == 600);
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
  assert(!timer_collection_edit(&collection, 0, 40, 1500)); /* Deal with the alarm first. */
  assert(timer_collection_acknowledge(&collection, 0));
  assert(timer_collection_edit(&collection, 0, 40, 1500));
  assert(collection.slots[0].state == ACTIVITY_TIMER_FINISHED);
  assert(collection.slots[0].repeat_seconds == 40 && collection.slots[0].run == run + 1);
  assert(timer_collection_restart(&collection, 0, 1500));
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

static void elapsed_duration_baseline(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 99, 1000);
  const int slot = timer_collection_create(&collection, 600, 1000);
  assert(slot == 0);
  const uint32_t run = collection.slots[slot].run;
  assert(activity_timer_elapsed(&collection.slots[slot], 1120) == 120);
  assert(timer_collection_pause(&collection, slot, 1120));
  assert(activity_timer_elapsed(&collection.slots[slot], 2000) == 120);
  assert(timer_collection_resume(&collection, slot, 3000));
  assert(activity_timer_elapsed(&collection.slots[slot], 3010) == 130);
  /* Changing remaining time preserves active elapsed and saved repeat time. */
  assert(timer_collection_edit(&collection, slot, 300, 3020));
  assert(collection.slots[slot].run == run);
  assert(activity_timer_elapsed(&collection.slots[slot], 3020) == 140);
  assert(activity_timer_elapsed(&collection.slots[slot], 3030) == 150);
  assert(collection.slots[slot].repeat_seconds == 600);
  assert(timer_collection_pause(&collection, slot, 3030));
  const uint32_t revision = collection.slots[slot].revision;
  assert(activity_timer_elapsed(&collection.slots[slot], 4000) == 150);
  assert(collection.slots[slot].revision == revision); /* Reading time is pure. */
}

static void paused_visibility_and_restore(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 123, 1000);
  assert(timer_collection_create(&collection, 600, 1000) == 0);
  assert(timer_collection_create(&collection, 1200, 1000) == 1);
  assert(timer_collection_pause(&collection, 0, 1120));
  const uint32_t run = collection.slots[0].run;
  const uint32_t expiry = 1120 + ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS;
  /* Simulate the previous version's persisted day-long paused lease. */
  collection.slots[0].valid_until = 1120 + ACTIVITY_TIMER_MAX_SECONDS;
  TimerCollection restored;
  memcpy(&restored, &collection, sizeof(restored));
  assert(timer_collection_valid(&restored));
  assert(timer_collection_reconfirm(&restored, 0, expiry));
  assert(restored.slots[0].valid_until == expiry);
  assert(restored.slots[0].observed_at == 1120);
  assert(timer_collection_reconfirm(&restored, 0, 200000));
  assert(timer_collection_valid(&restored));
  assert(restored.slots[0].state == ACTIVITY_TIMER_PAUSED);
  assert(restored.slots[0].valid_until == expiry);
  assert(restored.slots[0].remaining_seconds == 480);
  assert(timer_collection_visible(&restored, 0) && timer_collection_count(&restored) == 2);
  assert(!memcmp(&restored.slots[1], &collection.slots[1], sizeof(ActivityTimerModel)));

  /* Editing a hidden pause creates one new visibility window; it never
   * removes the saved slot or starts its native deadline before Resume. */
  assert(timer_collection_edit(&restored, 0, 300, 200001));
  assert(restored.slots[0].state == ACTIVITY_TIMER_PAUSED);
  assert(restored.slots[0].run == run && !restored.slots[0].deadline);
  assert(restored.slots[0].observed_at == 200001);
  assert(restored.slots[0].valid_until == 200001 + ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS);
  assert(timer_collection_reconfirm(&restored, 0, 300000));
  assert(restored.slots[0].observed_at == 200001);
  assert(restored.slots[0].valid_until == 200001 + ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS);
  assert(timer_collection_resume(&restored, 0, 300001));
  assert(restored.slots[0].deadline == 300301);
  assert(restored.slots[0].valid_until == 300361);
  assert(timer_collection_valid(&restored));
}

static void restart_is_a_new_run(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 456, 1000);
  assert(timer_collection_create(&collection, 600, 1000) == 0);
  assert(timer_collection_create(&collection, 900, 1000) == 1);
  ActivityTimerModel other = collection.slots[1];
  uint32_t run = collection.slots[0].run;
  assert(timer_collection_restart(&collection, 0, 1120));
  assert(collection.slots[0].run == ++run);
  assert(collection.slots[0].deadline == 1720);
  assert(collection.slots[0].configured_seconds == 600);
  assert(activity_timer_elapsed(&collection.slots[0], 1120) == 0);
  assert(!memcmp(&other, &collection.slots[1], sizeof(other)));
  assert(timer_collection_pause(&collection, 0, 1130));
  assert(timer_collection_restart(&collection, 0, 1150));
  assert(collection.slots[0].run == ++run);
  assert(collection.slots[0].state == ACTIVITY_TIMER_RUNNING);
  assert(collection.slots[0].deadline == 1750);
  assert(timer_collection_finish_due(&collection, 1750) == 1);
  assert(timer_collection_restart(&collection, 0, 1751));
  assert(collection.slots[0].run == ++run);
  assert(collection.slots[0].deadline == 2351);
  assert(collection.slots[0].valid_until == 2411);
  assert(timer_collection_valid(&collection));
  assert(!memcmp(&other, &collection.slots[1], sizeof(other)));

  collection.slots[0].run = UINT32_MAX;
  TimerCollection previous = collection;
  assert(!timer_collection_restart(&collection, 0, 1800));
  assert(!memcmp(&previous, &collection, sizeof(collection)));
  collection.slots[0].run = run;
  collection.revision = UINT32_MAX - 1;
  previous = collection;
  assert(!timer_collection_restart(&collection, 0, 1800));
  assert(!memcmp(&previous, &collection, sizeof(collection)));
}

static void acknowledged_results_and_capacity(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 991, 1000);
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    assert(timer_collection_create(&collection, 60, 1000) == (int)i);
  assert(!timer_collection_can_create(&collection));
  assert(timer_collection_finish_due(&collection, 1060) == 255);
  assert(!timer_collection_can_create(&collection)); /* Pending alarms are never evicted. */
  const uint32_t revision = collection.revision;
  assert(timer_collection_acknowledge(&collection, 3));
  assert(collection.revision > revision && collection.slots[3].acknowledged);
  assert(collection.slots[3].state == ACTIVITY_TIMER_FINISHED);
  const uint32_t observed = collection.slots[3].observed_at;
  const uint32_t until = collection.slots[3].valid_until;
  assert(timer_collection_reconfirm(&collection, 3, 9999));
  assert(collection.slots[3].observed_at == observed && collection.slots[3].valid_until == until);
  assert(timer_collection_can_create(&collection));
  assert(timer_collection_visible_slot(&collection, 7) == 3);
  const uint32_t old_run = collection.slots[3].run;
  assert(timer_collection_create(&collection, 90, 10000) == 3);
  assert(collection.slots[3].run == old_run + 1 && !collection.slots[3].acknowledged);
  assert(collection.slots[3].repeat_seconds == 90);
  assert(timer_collection_visible_slot(&collection, 0) == 3);
  assert(!timer_collection_can_create(&collection));
  assert(timer_collection_valid(&collection));
}

static void edit_bounds_and_snooze_repeat(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 992, 1000);
  assert(timer_collection_create(&collection, 600, 1000) == 0);
  TimerCollection unchanged = collection;
  assert(timer_collection_edit(&collection, 0, 480, 1120));
  assert(!memcmp(&unchanged, &collection, sizeof(collection)));
  assert(timer_collection_edit(&collection, 0, 86400, 1120));
  assert(collection.slots[0].configured_seconds == 86520);
  assert(collection.slots[0].repeat_seconds == 600);
  assert(activity_timer_elapsed(&collection.slots[0], 1120) == 120);
  assert(timer_collection_valid(&collection));
  assert(timer_collection_finish_due(&collection, 87520) == 1);
  assert(timer_collection_snooze(&collection, 0, 87520));
  assert(collection.slots[0].configured_seconds == 60 && collection.slots[0].repeat_seconds == 600);
  assert(activity_timer_elapsed(&collection.slots[0], 87530) == 10);
  assert(timer_collection_restart(&collection, 0, 87530));
  assert(collection.slots[0].deadline == 88130 && collection.slots[0].configured_seconds == 600);

  /* Repeated extensions may span more than a day, but arithmetic never wraps. */
  assert(timer_collection_pause(&collection, 0, 87540));
  collection.slots[0].configured_seconds = UINT32_MAX;
  unchanged = collection;
  assert(!timer_collection_edit(&collection, 0, 86400, 87540));
  assert(!memcmp(&unchanged, &collection, sizeof(collection)));
}

static void reset_saved_ready(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 994, 1000);
  assert(!timer_collection_reset(NULL, 0, 1000));
  assert(!timer_collection_reset(&collection, 0, 1000));
  assert(!timer_collection_reset(&collection, TIMER_COLLECTION_CAPACITY, 1000));
  assert(timer_collection_create(&collection, 600, 1000) == 0);
  assert(timer_collection_create(&collection, 900, 1000) == 1);
  const ActivityTimerModel sibling = collection.slots[1];
  assert(timer_collection_edit(&collection, 0, 1200, 1100));
  const ActivityTimerModel before = collection.slots[0];
  const uint32_t revision = collection.revision;
  assert(timer_collection_reset(&collection, 0, 1120));
  const ActivityTimerModel *ready = &collection.slots[0];
  assert(ready->state == ACTIVITY_TIMER_CANCELLED && activity_timer_ready(ready));
  assert(!ready->deadline && !ready->remaining_seconds && !ready->acknowledged);
  assert(ready->origin == before.origin && ready->run == before.run);
  assert(ready->configured_seconds == 1300 && ready->repeat_seconds == 600);
  assert(ready->revision == revision + 1 && collection.revision == revision + 1);
  assert(ready->observed_at == 1120 && ready->valid_until == before.valid_until);
  assert(timer_collection_visible(&collection, 0) && timer_collection_count(&collection) == 2);
  assert(!timer_collection_has_due(&collection, 1800));
  assert(!timer_collection_finish_due(&collection, 1800));
  assert(!activity_timer_finish_if_due(&collection.slots[0], 200000));
  assert(!memcmp(&sibling, &collection.slots[1], sizeof(sibling)));

  /* Ready is saved data. Repeated reset, a no-op edit and later reopen cannot
   * manufacture a countdown, another observation, or a renewed tombstone. */
  TimerCollection previous = collection;
  assert(timer_collection_reset(&collection, 0, 200000));
  assert(timer_collection_edit(&collection, 0, 600, 200000));
  assert(!memcmp(&previous, &collection, sizeof(collection)));
  assert(timer_collection_edit(&collection, 0, 1200, 200000));
  assert(ready->repeat_seconds == 1200 && ready->configured_seconds == 1300);
  assert(ready->run == before.run && !ready->deadline);
  assert(ready->observed_at == 1120 && ready->valid_until == before.valid_until);
  assert(timer_collection_reconfirm(&collection, 0, 200000));
  assert(ready->observed_at == 1120 && ready->valid_until == before.valid_until);
  assert(timer_collection_valid(&collection));

  TimerCollection restored;
  assert(timer_collection_decode(&restored, &collection, sizeof(collection), 0));
  assert(!memcmp(&restored, &collection, sizeof(collection)));
  assert(!timer_collection_decode(&restored, &collection, sizeof(collection), 1));
  assert(!memcmp(&restored, &collection, sizeof(collection)));
  assert(timer_collection_restart(&restored, 0, 200001));
  assert(restored.slots[0].state == ACTIVITY_TIMER_RUNNING);
  assert(restored.slots[0].run == before.run + 1);
  assert(restored.slots[0].deadline == 201201 && restored.slots[0].repeat_seconds == 1200);
  assert(restored.slots[0].configured_seconds == 1200);
  assert(!memcmp(&sibling, &restored.slots[1], sizeof(sibling)));

  /* Deleting a reset row publishes another cancellation before hiding it. */
  const uint32_t until = ready->valid_until;
  const uint32_t delete_revision = collection.revision;
  assert(timer_collection_delete(&collection, 0, 1000)); /* RTC rollback. */
  assert(!timer_collection_visible(&collection, 0));
  assert(collection.slots[0].revision == delete_revision + 1);
  assert(collection.slots[0].valid_until == until);
  assert(collection.slots[0].origin == before.origin && collection.slots[0].run == before.run);
  assert(timer_collection_valid(&collection));

  /* Current banks still reject a visible unused slot or hidden live timer. */
  previous = collection;
  collection.visible_mask |= 1u << 7;
  assert(!timer_collection_valid(&collection));
  assert(!timer_collection_decode(&restored, &collection, sizeof(collection), 0));
  collection = previous;
  collection.visible_mask &= ~(1u << 1);
  assert(!timer_collection_valid(&collection));
  assert(!timer_collection_decode(&restored, &collection, sizeof(collection), 0));
}

static void reset_paused_finished_and_limits(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 995, 1000);
  assert(timer_collection_create(&collection, 600, 1000) == 0);
  assert(timer_collection_pause(&collection, 0, 1120));
  const uint32_t run = collection.slots[0].run;
  assert(timer_collection_reset(&collection, 0, 200000));
  assert(collection.slots[0].run == run && collection.slots[0].repeat_seconds == 600);
  assert(collection.slots[0].valid_until == 200060 && collection.slots[0].observed_at == 200000);
  assert(activity_timer_ready(&collection.slots[0]) && timer_collection_valid(&collection));
  assert(timer_collection_restart(&collection, 0, 200001));
  assert(timer_collection_finish_due(&collection, 200601) == 1);
  assert(timer_collection_snooze(&collection, 0, 200601));
  assert(collection.slots[0].configured_seconds == 60 && collection.slots[0].repeat_seconds == 600);
  assert(timer_collection_reset(&collection, 0, 200602));
  assert(collection.slots[0].configured_seconds == 60 && collection.slots[0].repeat_seconds == 600);
  assert(timer_collection_restart(&collection, 0, 200603));
  assert(collection.slots[0].deadline == 201203); /* Saved duration, not snooze. */
  assert(timer_collection_finish_due(&collection, 201203) == 1);
  const uint32_t pending_run = collection.slots[0].run;
  assert(timer_collection_reset(&collection, 0, 201204));
  assert(collection.slots[0].state == ACTIVITY_TIMER_CANCELLED);
  assert(!collection.slots[0].acknowledged && collection.slots[0].run == pending_run);
  assert(collection.slots[0].valid_until == 201264);
  assert(timer_collection_restart(&collection, 0, 201205));
  assert(timer_collection_finish_due(&collection, 201805) == 1);
  assert(timer_collection_acknowledge(&collection, 0));
  TimerCollection previous = collection;
  assert(timer_collection_reset(&collection, 0, 999999));
  assert(!memcmp(&collection, &previous, sizeof(collection))); /* Done already is Ready. */

  collection.revision = UINT32_MAX;
  previous = collection;
  assert(timer_collection_reset(&collection, 0, 999999));
  assert(timer_collection_edit(&collection, 0, 600, 999999));
  assert(!timer_collection_edit(&collection, 0, 900, 999999));
  assert(!memcmp(&collection, &previous, sizeof(collection)));
  collection = previous;
  collection.revision = collection.slots[0].revision;
  assert(timer_collection_restart(&collection, 0, 201806));
  collection.revision = UINT32_MAX;
  previous = collection;
  assert(!timer_collection_reset(&collection, 0, 201807));
  assert(!memcmp(&collection, &previous, sizeof(collection)));
  collection.revision = UINT32_MAX - 1;
  collection.slots[0].run = UINT32_MAX;
  assert(timer_collection_reset(&collection, 0, UINT32_MAX));
  assert(collection.slots[0].valid_until == UINT32_MAX && timer_collection_valid(&collection));
  previous = collection;
  assert(timer_collection_reset(&collection, 0, UINT32_MAX));
  assert(timer_collection_edit(&collection, 0, 600, UINT32_MAX));
  assert(!timer_collection_restart(&collection, 0, 201808));
  assert(!memcmp(&collection, &previous, sizeof(collection)));
}

static void reset_ready_order_and_capacity(void) {
  TimerCollection collection;
  timer_collection_init(&collection, 996, 1000);
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    assert(timer_collection_create(&collection, i ? 600 : 60, 1000) == (int)i);
  assert(timer_collection_reset(&collection, 3, 1020));
  assert(timer_collection_reset(&collection, 5, 1010));
  assert(timer_collection_pause(&collection, 2, 1015));
  assert(timer_collection_finish_due(&collection, 1060) == 1);
  const unsigned expected[] = {1, 4, 6, 7, 2, 0, 3, 5};
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    assert(timer_collection_visible_slot(&collection, i) == (int)expected[i]);
  assert(timer_collection_can_create(&collection));
  const ActivityTimerModel sibling = collection.slots[3];
  const uint32_t previous_run = collection.slots[5].run;
  assert(timer_collection_create(&collection, 90, 1061) == 5); /* Oldest Ready. */
  assert(collection.slots[5].run == previous_run + 1 && collection.slots[5].deadline == 1151);
  assert(!memcmp(&sibling, &collection.slots[3], sizeof(sibling)));
  assert(timer_collection_valid(&collection));
}

static void v1_collection_upgrade(void) {
  typedef struct {
    uint32_t magic, version, seed, revision, visible_mask;
    ActivityTimerModelV1 slots[TIMER_COLLECTION_CAPACITY];
  } LegacyCollection;
  TimerCollection collection;
  timer_collection_init(&collection, 993, 1000);
  assert(timer_collection_create(&collection, 60, 1000) == 0);
  assert(timer_collection_create(&collection, 600, 1000) == 1);
  assert(timer_collection_create(&collection, 60, 1000) == 2);
  assert(timer_collection_finish_due(&collection, 1060) == 5);
  LegacyCollection old;
  memcpy(&old, &collection, offsetof(TimerCollection, slots));
  old.version = 1;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    memcpy(&old.slots[i], &collection.slots[i], sizeof(old.slots[i]));
    old.slots[i].version = 1;
  }
  TimerCollection decoded;
  assert(sizeof(old) == 372 && sizeof(decoded) == 436);
  assert(timer_collection_decode(&decoded, &old, sizeof(old), 1));
  assert(!decoded.slots[0].acknowledged && decoded.slots[2].acknowledged);
  assert(decoded.slots[1].repeat_seconds == 600 && decoded.slots[1].deadline == 1600);
  assert(decoded.slots[0].observed_at == 1060 && decoded.slots[0].valid_until == 1120);
  assert(decoded.revision == collection.revision && decoded.visible_mask == collection.visible_mask);
  assert(timer_collection_valid(&decoded));
  TimerCollection previous = decoded;
  TimerCollection inplace;
  memcpy(&inplace, &old, sizeof(old));
  assert(timer_collection_decode(&inplace, &inplace, sizeof(old), 1));
  assert(!memcmp(&previous, &inplace, sizeof(previous)));
  unsigned char unaligned[sizeof(old) + 1];
  memcpy(unaligned + 1, &old, sizeof(old));
  assert(timer_collection_decode(&decoded, unaligned + 1, sizeof(old), 1));
  assert(!memcmp(&previous, &decoded, sizeof(previous)));
  assert(!timer_collection_decode(&decoded, &old, sizeof(old) - 1, 1));
  assert(!memcmp(&previous, &decoded, sizeof(decoded)));
  old.slots[1].version = 99;
  assert(!timer_collection_decode(&decoded, &old, sizeof(old), 1));
  assert(!memcmp(&previous, &decoded, sizeof(decoded)));
  assert(!timer_collection_decode(&decoded, &previous, sizeof(previous), 5));
  old.slots[1].version = 1;
  old.slots[7].version = 99;
  assert(!timer_collection_decode(&decoded, &old, sizeof(old), 1));
  assert(!memcmp(&previous, &decoded, sizeof(decoded)));
  old.slots[7].version = 1;
  old.slots[0].state = ACTIVITY_TIMER_CANCELLED;
  assert(!timer_collection_decode(&decoded, &old, sizeof(old), 0));
  assert(!memcmp(&previous, &decoded, sizeof(decoded))); /* v1 never had visible Ready cancellations. */
  old.visible_mask &= ~1u;
  assert(timer_collection_decode(&decoded, &old, sizeof(old), 0));
  assert(!timer_collection_visible(&decoded, 0) && decoded.slots[0].run);
  assert(timer_collection_valid(&decoded));
}

int main(void) {
  legacy_migration();
  independent_timers();
  edit_and_slot_reuse();
  capacity_and_validation();
  revision_floor_and_persistence();
  elapsed_duration_baseline();
  paused_visibility_and_restore();
  restart_is_a_new_run();
  acknowledged_results_and_capacity();
  edit_bounds_and_snooze_repeat();
  reset_saved_ready();
  reset_paused_finished_and_limits();
  reset_ready_order_and_capacity();
  v1_collection_upgrade();
  puts("timer collection: migration, eight slots, clocks, revisions, editing and persistence passed");
  return 0;
}
