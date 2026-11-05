#include "../src/c/timer_model.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

static void paused_visibility(void) {
  ActivityTimerModel timer;
  activity_timer_init(&timer, 789, 1000);
  assert(activity_timer_start(&timer, 600, 1000));
  assert(activity_timer_pause(&timer, 1120));
  const uint32_t expiry = 1120 + ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS;
  assert(timer.observed_at == 1120 && timer.valid_until == expiry);
  uint32_t revision = timer.revision;
  /* A cold handshake immediately before, at and long after the visibility
   * boundary changes only the revision. A backward RTC does not renew it. */
  const uint32_t confirmations[] = {expiry - 1, expiry, 200000, 1000};
  for (unsigned i = 0; i < sizeof(confirmations) / sizeof(confirmations[0]); ++i) {
    assert(activity_timer_reconfirm(&timer, confirmations[i]));
    assert(timer.revision == ++revision);
    assert(timer.observed_at == 1120 && timer.valid_until == expiry);
    assert(timer.state == ACTIVITY_TIMER_PAUSED && timer.remaining_seconds == 480);
    assert(activity_timer_elapsed(&timer, confirmations[i]) == 120);
    assert(activity_timer_valid(&timer));
  }
  ActivityTimerModel restored;
  memcpy(&restored, &timer, sizeof(restored));
  assert(activity_timer_valid(&restored));
  assert(activity_timer_resume(&restored, 200000));
  assert(restored.deadline == 200480 && restored.valid_until == 200540);
  assert(restored.observed_at == 200000 && restored.run == timer.run);

  /* Upgrade a saved old lease without treating a later handshake as a new
   * pause. A deliberately earlier source expiry must stay earlier. */
  timer.valid_until = timer.observed_at + ACTIVITY_TIMER_MAX_SECONDS;
  assert(activity_timer_reconfirm(&timer, 200000));
  assert(timer.valid_until == expiry && timer.observed_at == 1120);
  timer.valid_until = timer.observed_at + 1;
  assert(activity_timer_reconfirm(&timer, 300000));
  assert(timer.valid_until == 1121 && activity_timer_valid(&timer));

  /* An explicit extension is a newly observed paused value and may appear
   * again for one bounded window. Ordinary reconfirmation cannot extend it. */
  assert(activity_timer_extend(&timer, 60, 300001));
  assert(timer.remaining_seconds == 540 && timer.observed_at == 300001);
  assert(timer.valid_until == 300001 + ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS);
  assert(activity_timer_reconfirm(&timer, 400000));
  assert(timer.observed_at == 300001 && timer.valid_until < 400000);
  timer.revision = UINT32_MAX;
  ActivityTimerModel previous = timer;
  assert(!activity_timer_reconfirm(&timer, 400001));
  assert(!memcmp(&timer, &previous, sizeof(timer)));

  /* Saturating visibility arithmetic cannot wrap into an earlier epoch. */
  activity_timer_init(&timer, 789, UINT32_MAX - 100);
  assert(activity_timer_start(&timer, 60, UINT32_MAX - 100));
  assert(activity_timer_pause(&timer, UINT32_MAX - 90));
  assert(timer.valid_until == UINT32_MAX && activity_timer_valid(&timer));
  assert(activity_timer_reconfirm(&timer, UINT32_MAX));
  assert(timer.observed_at == UINT32_MAX - 90 && timer.valid_until == UINT32_MAX);
  assert(activity_timer_valid(&timer));
}

static void ready_states(void) {
  ActivityTimerModel timer;
  assert(!activity_timer_ready(NULL));
  activity_timer_init(&timer, 901, 1000);
  assert(!activity_timer_ready(&timer)); /* An unused slot is not saved Ready. */
  assert(activity_timer_start(&timer, 60, 1000));
  assert(!activity_timer_ready(&timer));
  assert(activity_timer_pause(&timer, 1010));
  assert(!activity_timer_ready(&timer));
  assert(activity_timer_resume(&timer, 1020));
  assert(activity_timer_finish_if_due(&timer, 1070));
  assert(!activity_timer_ready(&timer)); /* A pending alarm still needs Done. */
  assert(activity_timer_acknowledge(&timer));
  ActivityTimerModel previous = timer;
  assert(activity_timer_ready(&timer));
  assert(!memcmp(&timer, &previous, sizeof(timer)));
  assert(activity_timer_start(&timer, 60, 2000));
  assert(activity_timer_cancel(&timer, 2010));
  assert(activity_timer_ready(&timer));
  assert(activity_timer_valid(&timer));
}

int main(void) {
  ActivityTimerModel timer;
  activity_timer_init(&timer, 123, 1000);
  assert(activity_timer_valid(&timer));
  assert(timer.run == 0 && timer.revision == 1);
  assert(!activity_timer_start(&timer, 0, 1000));
  assert(activity_timer_start(&timer, 600, 1000));
  assert(timer.run == 1 && timer.deadline == 1600 && timer.observed_at == 1000);
  assert(activity_timer_remaining(&timer, 1120) == 480);
  assert(activity_timer_elapsed(&timer, 1120) == 120);
  assert(timer.valid_until >= timer.deadline); /* Not music's 120-second lease. */
  assert(!activity_timer_start(&timer, 60, 1000));

  assert(activity_timer_pause(&timer, 1120));
  uint32_t revision = timer.revision;
  assert(timer.remaining_seconds == 480 && !timer.deadline);
  assert(activity_timer_remaining(&timer, 2000) == 480);
  assert(activity_timer_elapsed(&timer, 2000) == 120); /* Pauses add no active time. */
  assert(!activity_timer_pause(&timer, 2000));
  assert(timer.revision == revision);
  assert(activity_timer_extend(&timer, 60, 2000));
  assert(timer.remaining_seconds == 540);
  assert(activity_timer_elapsed(&timer, 2000) == 120);
  assert(activity_timer_resume(&timer, 3000));
  assert(timer.deadline == 3540 && timer.remaining_seconds == 0 && timer.run == 1);
  assert(activity_timer_extend(&timer, 60, 3010));
  assert(timer.deadline == 3600 && timer.observed_at == 3010);
  assert(!activity_timer_finish_if_due(&timer, 3599));
  assert(activity_timer_finish_if_due(&timer, 3600));
  assert(timer.state == ACTIVITY_TIMER_FINISHED);
  assert(activity_timer_elapsed(&timer, 4000) == timer.configured_seconds);
  revision = timer.revision;
  assert(!activity_timer_finish_if_due(&timer, 3601));
  assert(timer.revision == revision); /* Completion is emitted once by owner. */
  assert(activity_timer_valid(&timer));

  /* A restart is another run; delayed receipts cannot acknowledge it. */
  assert(activity_timer_start(&timer, 5, 4000));
  assert(timer.run == 2 && timer.revision > revision);
  assert(activity_timer_cancel(&timer, 4001));
  assert(timer.state == ACTIVITY_TIMER_CANCELLED && !timer.deadline);
  assert(activity_timer_elapsed(&timer, 4001) == 0);
  assert(timer.valid_until == 4065); /* Tombstone covers previous running lease. */
  assert(!activity_timer_finish_if_due(&timer, 9000));
  assert(activity_timer_valid(&timer));

  /* Persisted roundtrip has no SDK/phone dependency and retains original times. */
  ActivityTimerModel restored;
  memcpy(&restored, &timer, sizeof(timer));
  assert(activity_timer_valid(&restored));
  assert(restored.observed_at == 4001 && restored.valid_until == 4065);
  assert(activity_timer_reconfirm(&restored, 9000));
  assert(restored.observed_at == 4001 && restored.valid_until == 4065);
  restored.version = 99;
  assert(!activity_timer_valid(&restored));

  activity_timer_init(&timer, 123, 100);
  assert(activity_timer_start(&timer, ACTIVITY_TIMER_MAX_SECONDS, 100));
  assert(!activity_timer_extend(&timer, 1, 100));
  assert(activity_timer_pause(&timer, 101));
  assert(!activity_timer_extend(&timer, ACTIVITY_TIMER_MAX_SECONDS, 101));
  timer.revision = UINT32_MAX;
  assert(!activity_timer_resume(&timer, 102));
  activity_timer_init(&timer, 123, 1000);
  assert(activity_timer_start(&timer, ACTIVITY_TIMER_MAX_SECONDS, 1000));
  assert(activity_timer_reconfirm(&timer, 500));
  assert(activity_timer_valid(&timer));
  assert(timer.observed_at == 1000 && timer.deadline == 87400);
  assert(activity_timer_remaining(&timer, 500) == ACTIVITY_TIMER_MAX_SECONDS);
  memcpy(&restored, &timer, sizeof(timer));
  assert(activity_timer_valid(&restored));
  assert(activity_timer_pause(&timer, 500));
  assert(activity_timer_valid(&timer) && timer.remaining_seconds == ACTIVITY_TIMER_MAX_SECONDS);
  activity_timer_init(&timer, 123, UINT32_MAX - 1);
  assert(!activity_timer_start(&timer, 5, UINT32_MAX - 1));
  assert(sizeof(ActivityTimerModelV1) == 44);
  assert(sizeof(ActivityTimerModel) == 52);

  /* Completion acknowledgment is durable source metadata, not cancellation.
   * Reopening never renews either a pending or acknowledged result. */
  activity_timer_init(&timer, 456, 1000);
  assert(activity_timer_start(&timer, 60, 1000));
  assert(activity_timer_finish_if_due(&timer, 1060));
  assert(!timer.acknowledged);
  const uint32_t completion_revision = timer.revision;
  assert(activity_timer_reconfirm(&timer, 2000));
  assert(timer.observed_at == 1060 && timer.valid_until == 1120);
  assert(activity_timer_acknowledge(&timer));
  assert(timer.acknowledged && timer.state == ACTIVITY_TIMER_FINISHED);
  assert(timer.revision > completion_revision);
  const uint32_t ack_revision = timer.revision;
  assert(activity_timer_acknowledge(&timer) && timer.revision == ack_revision);
  assert(activity_timer_reconfirm(&timer, 3000));
  assert(timer.acknowledged && timer.observed_at == 1060 && timer.valid_until == 1120);
  ActivityTimerModelV1 old;
  memcpy(&old, &timer, sizeof(old));
  old.version = 1;
  assert(activity_timer_decode(&restored, &old, sizeof(old), true));
  assert(restored.version == 2 && restored.repeat_seconds == 60 && restored.acknowledged);
  assert(activity_timer_decode(&restored, &old, sizeof(old), false));
  assert(!restored.acknowledged);
  assert(activity_timer_valid(&restored));
  ActivityTimerModel before = restored;
  old.version = 99;
  assert(!activity_timer_decode(&restored, &old, sizeof(old), false));
  assert(!memcmp(&before, &restored, sizeof(before)));
  paused_visibility();
  ready_states();
  puts("timer model: lifecycle, persistence, expiry and boundary checks passed");
  return 0;
}
