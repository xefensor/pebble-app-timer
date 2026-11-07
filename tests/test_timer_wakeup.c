#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../src/c/timer_wakeup.h"

static time_t wakeups[16];
static WakeupId next_id;
static time_t other_app;
static const int32_t s_cookie = 42;

static bool conflicts(time_t first, time_t second) {
  return second && first > second - 60 && first < second + 60;
}

WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify_if_missed) {
  assert(cookie == s_cookie && notify_if_missed);
  if (conflicts(timestamp, other_app)) return -1;
  for (WakeupId id = 0; id < next_id; ++id) {
    if (conflicts(timestamp, wakeups[id])) return -1;
  }
  assert(next_id < (WakeupId)(sizeof(wakeups) / sizeof(wakeups[0])));
  wakeups[next_id] = timestamp;
  return next_id++;
}

bool wakeup_query(WakeupId id, time_t *timestamp) {
  if (id < 0 || id >= next_id || !wakeups[id]) return false;
  *timestamp = wakeups[id];
  return true;
}

void wakeup_cancel(WakeupId id) {
  assert(id >= 0 && id < next_id);
  wakeups[id] = 0;
}

void wakeup_cancel_all(void) { memset(wakeups, 0, sizeof(wakeups)); }

static void assert_only_wakeup(TimerWakeup expected, uint32_t deadline) {
  assert(expected.deadline == deadline);
  for (WakeupId id = 0; id < next_id; ++id) {
    assert(wakeups[id] == (id == expected.id ? (time_t)deadline : 0));
  }
}

int main(void) {
  const TimerWakeup none = {-1, 0};
  TimerWakeup previous = timer_wakeup_replace(none, 1300, s_cookie);
  assert(previous.id >= 0);
  assert_only_wakeup(previous, 1300);

  other_app = 1030;
  TimerWakeup result = timer_wakeup_replace(previous, 1030, s_cookie);
  assert(result.id == previous.id);
  assert_only_wakeup(result, 1300);

  /* A 45-second replacement overlaps our old event. The other app at 1220
   * rejects 1255 but permits restoration at 1300. */
  other_app = 1220;
  result = timer_wakeup_replace(previous, 1255, s_cookie);
  assert(result.id != previous.id);
  assert_only_wakeup(result, 1300);

  other_app = 0;
  previous = result;
  result = timer_wakeup_replace(previous, 1255, s_cookie);
  assert(result.id != previous.id);
  assert_only_wakeup(result, 1255);
  previous = result;
  result = timer_wakeup_replace(previous, 1255, s_cookie);
  assert(result.id == previous.id);
  assert_only_wakeup(result, 1255);

  /* A rejected replacement exactly 60 seconds away can retain its old ID. */
  other_app = 1195;
  result = timer_wakeup_replace(previous, 1195, s_cookie);
  assert(result.id == previous.id);
  assert_only_wakeup(result, 1255);

  /* If no timer still owns the earlier Wakeup, it is not a fallback. */
  result = timer_wakeup_replace(none, 1195, s_cookie);
  assert(result.id < 0);
  assert_only_wakeup(result, 0);

  puts("Timer Wakeup: conflict preserves the valid later alarm and keeps one scheduled event");
  return 0;
}
