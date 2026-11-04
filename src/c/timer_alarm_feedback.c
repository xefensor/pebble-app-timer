#include <pebble.h>

#include "timer_alarm_feedback.h"

#define ALARM_REPEAT_MS 3000u

static AppTimer *s_repeat_timer;
static bool s_pending;

static void prv_repeat(void *context);

static void prv_schedule(void) {
  if (s_pending && !s_repeat_timer)
    s_repeat_timer = app_timer_register(ALARM_REPEAT_MS, prv_repeat, NULL);
}

static void prv_pulse(void) {
  /* The public SDK has no Quiet Time change subscription. Check before every
   * pulse (and on native updates); retain the callback while quiet so an
   * unacknowledged alarm resumes when Quiet Time ends. */
  if (quiet_time_is_active()) vibes_cancel();
  else vibes_double_pulse();
}

static void prv_repeat(void *context) {
  (void)context;
  s_repeat_timer = NULL;
  if (!s_pending) return;
  prv_pulse();
  prv_schedule();
}

void timer_alarm_feedback_update(uint8_t pending_mask) {
  if (!pending_mask) {
    if (!s_pending) return;
    s_pending = false;
    if (s_repeat_timer) app_timer_cancel(s_repeat_timer);
    s_repeat_timer = NULL;
    vibes_cancel();
    return;
  }
  if (!s_pending) {
    s_pending = true;
    prv_pulse();
  } else if (quiet_time_is_active()) {
    vibes_cancel();
  }
  /* A failed registration can be retried by a later native refresh/tick.
   * Retaining s_pending prevents retries from replaying the immediate pulse. */
  prv_schedule();
}

void timer_alarm_feedback_deinit(void) {
  timer_alarm_feedback_update(0);
}
