#include "timer_model.h"

#include <limits.h>
#include <string.h>

static uint32_t add_saturating(uint32_t value, uint32_t offset) {
  return value > UINT32_MAX - offset ? UINT32_MAX : value + offset;
}

static bool advance(ActivityTimerModel *timer, uint32_t now) {
  if (timer->revision == UINT32_MAX) return false;
  ++timer->revision;
  timer->observed_at = now;
  return true;
}

void activity_timer_init(ActivityTimerModel *timer, uint32_t origin, uint32_t now) {
  memset(timer, 0, sizeof(*timer));
  timer->magic = ACTIVITY_TIMER_MAGIC;
  timer->version = ACTIVITY_TIMER_MODEL_VERSION;
  timer->origin = origin ? origin : 1;
  timer->revision = 1;
  timer->state = ACTIVITY_TIMER_CANCELLED;
  timer->configured_seconds = 300;
  timer->repeat_seconds = 300;
  timer->observed_at = now;
  timer->valid_until = add_saturating(now, ACTIVITY_TIMER_MAX_SECONDS);
}

bool activity_timer_valid(const ActivityTimerModel *timer) {
  if (!timer || timer->magic != ACTIVITY_TIMER_MAGIC ||
      timer->version != ACTIVITY_TIMER_MODEL_VERSION || !timer->origin ||
      !timer->revision || timer->configured_seconds < ACTIVITY_TIMER_MIN_SECONDS ||
      timer->repeat_seconds < ACTIVITY_TIMER_MIN_SECONDS ||
      timer->repeat_seconds > ACTIVITY_TIMER_MAX_SECONDS ||
      timer->acknowledged > 1 ||
      (timer->acknowledged && timer->state != ACTIVITY_TIMER_FINISHED) ||
      timer->valid_until < timer->observed_at) return false;
  switch (timer->state) {
    case ACTIVITY_TIMER_RUNNING:
      return timer->run && timer->deadline > timer->observed_at &&
          timer->deadline <= timer->valid_until &&
          timer->deadline - timer->observed_at <= ACTIVITY_TIMER_MAX_SECONDS;
    case ACTIVITY_TIMER_PAUSED:
      return timer->run && !timer->deadline && timer->remaining_seconds &&
          timer->remaining_seconds <= ACTIVITY_TIMER_MAX_SECONDS;
    case ACTIVITY_TIMER_FINISHED:
      return timer->run && !timer->deadline && !timer->remaining_seconds;
    case ACTIVITY_TIMER_CANCELLED:
      return !timer->deadline && !timer->remaining_seconds;
    default:
      return false;
  }
}

bool activity_timer_decode(ActivityTimerModel *timer, const void *bytes,
                            size_t length, bool acknowledged) {
  if (!timer || !bytes) return false;
  ActivityTimerModel next;
  if (length == sizeof(next)) memcpy(&next, bytes, sizeof(next));
  else if (length == sizeof(ActivityTimerModelV1)) {
    memset(&next, 0, sizeof(next));
    memcpy(&next, bytes, sizeof(ActivityTimerModelV1));
    if (next.version != 1 || next.configured_seconds > ACTIVITY_TIMER_MAX_SECONDS)
      return false;
    next.version = ACTIVITY_TIMER_MODEL_VERSION;
    next.repeat_seconds = next.configured_seconds;
    next.acknowledged = next.state == ACTIVITY_TIMER_FINISHED && acknowledged;
  } else return false;
  if (!activity_timer_valid(&next)) return false;
  *timer = next;
  return true;
}

uint32_t activity_timer_remaining(const ActivityTimerModel *timer, uint32_t now) {
  if (timer->state == ACTIVITY_TIMER_RUNNING) {
    /* A backward clock adjustment must not grow the recorded countdown. */
    if (now < timer->observed_at) now = timer->observed_at;
    return timer->deadline > now ? timer->deadline - now : 0;
  }
  return timer->state == ACTIVITY_TIMER_PAUSED ? timer->remaining_seconds : 0;
}

uint32_t activity_timer_elapsed(const ActivityTimerModel *timer, uint32_t now) {
  if (timer->state == ACTIVITY_TIMER_CANCELLED) return 0;
  const uint32_t remaining = activity_timer_remaining(timer, now);
  return remaining < timer->configured_seconds ? timer->configured_seconds - remaining : 0;
}

bool activity_timer_start(ActivityTimerModel *timer, uint32_t seconds, uint32_t now) {
  if ((timer->state != ACTIVITY_TIMER_CANCELLED && timer->state != ACTIVITY_TIMER_FINISHED) ||
      seconds < ACTIVITY_TIMER_MIN_SECONDS || seconds > ACTIVITY_TIMER_MAX_SECONDS ||
      now > UINT32_MAX - seconds || timer->run == UINT32_MAX ||
      timer->revision == UINT32_MAX) return false;
  advance(timer, now);
  ++timer->run;
  timer->state = ACTIVITY_TIMER_RUNNING;
  timer->configured_seconds = seconds;
  timer->repeat_seconds = seconds;
  timer->acknowledged = 0;
  timer->remaining_seconds = 0;
  timer->deadline = now + seconds;
  timer->valid_until = add_saturating(timer->deadline, ACTIVITY_TIMER_TERMINAL_SECONDS);
  return true;
}

bool activity_timer_pause(ActivityTimerModel *timer, uint32_t now) {
  uint32_t remaining = activity_timer_remaining(timer, now);
  if (timer->state != ACTIVITY_TIMER_RUNNING || !remaining ||
      remaining > ACTIVITY_TIMER_MAX_SECONDS || !advance(timer, now)) return false;
  timer->remaining_seconds = remaining;
  timer->deadline = 0;
  timer->state = ACTIVITY_TIMER_PAUSED;
  timer->valid_until = add_saturating(now, ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS);
  return true;
}

bool activity_timer_resume(ActivityTimerModel *timer, uint32_t now) {
  if (timer->state != ACTIVITY_TIMER_PAUSED || !timer->remaining_seconds ||
      now > UINT32_MAX - timer->remaining_seconds || !advance(timer, now)) return false;
  timer->deadline = now + timer->remaining_seconds;
  timer->remaining_seconds = 0;
  timer->state = ACTIVITY_TIMER_RUNNING;
  timer->valid_until = add_saturating(timer->deadline, ACTIVITY_TIMER_TERMINAL_SECONDS);
  return true;
}

bool activity_timer_extend(ActivityTimerModel *timer, uint32_t seconds, uint32_t now) {
  uint32_t remaining = activity_timer_remaining(timer, now);
  if ((timer->state != ACTIVITY_TIMER_RUNNING && timer->state != ACTIVITY_TIMER_PAUSED) ||
      !remaining || remaining > ACTIVITY_TIMER_MAX_SECONDS || !seconds ||
      seconds > ACTIVITY_TIMER_MAX_SECONDS - remaining ||
      seconds > UINT32_MAX - timer->configured_seconds ||
      (timer->state == ACTIVITY_TIMER_RUNNING && now > UINT32_MAX - remaining - seconds) ||
      !advance(timer, now)) return false;
  timer->configured_seconds += seconds;
  if (timer->state == ACTIVITY_TIMER_RUNNING) {
    timer->deadline = now + remaining + seconds;
    timer->valid_until = add_saturating(timer->deadline, ACTIVITY_TIMER_TERMINAL_SECONDS);
  } else {
    timer->remaining_seconds = remaining + seconds;
    timer->valid_until = add_saturating(now, ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS);
  }
  return true;
}

bool activity_timer_cancel(ActivityTimerModel *timer, uint32_t now) {
  if ((timer->state != ACTIVITY_TIMER_RUNNING && timer->state != ACTIVITY_TIMER_PAUSED) ||
      !advance(timer, now)) return false;
  timer->state = ACTIVITY_TIMER_CANCELLED;
  timer->remaining_seconds = timer->deadline = 0;
  uint32_t minimum_until = add_saturating(now, ACTIVITY_TIMER_TERMINAL_SECONDS);
  if (timer->valid_until < minimum_until) timer->valid_until = minimum_until;
  return true;
}

bool activity_timer_reconfirm(ActivityTimerModel *timer, uint32_t now) {
  if (timer->state == ACTIVITY_TIMER_RUNNING && timer->deadline <= now) return false;
  if (timer->state != ACTIVITY_TIMER_RUNNING) {
    if (timer->revision == UINT32_MAX) return false;
    ++timer->revision;
    /* Reopening or re-sharing unchanged paused state must not restart its
     * watchface visibility window. Keep the original observation, including
     * after expiry; the timer remains saved and resumable in the watchapp.
     * Clamp the former day-long lease when upgrading an existing pause. */
    if (timer->state == ACTIVITY_TIMER_PAUSED) {
      const uint32_t until = add_saturating(timer->observed_at,
          ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS);
      if (timer->valid_until > until) timer->valid_until = until;
    }
    return true;
  }
  /* Keep a running deadline's validated baseline through RTC rollback. A user
   * pause/resume or extension supplies a new owner-observed baseline instead. */
  if (timer->state == ACTIVITY_TIMER_RUNNING && now < timer->observed_at)
    now = timer->observed_at;
  if (!advance(timer, now)) return false;
  timer->valid_until = add_saturating(timer->deadline, ACTIVITY_TIMER_TERMINAL_SECONDS);
  return true;
}

bool activity_timer_acknowledge(ActivityTimerModel *timer) {
  if (!timer || timer->state != ACTIVITY_TIMER_FINISHED || timer->revision == UINT32_MAX)
    return false;
  if (!timer->acknowledged) {
    ++timer->revision;
    timer->acknowledged = 1;
  }
  return true;
}

bool activity_timer_finish_if_due(ActivityTimerModel *timer, uint32_t now) {
  if (timer->state != ACTIVITY_TIMER_RUNNING || now < timer->deadline ||
      !advance(timer, now)) return false;
  timer->state = ACTIVITY_TIMER_FINISHED;
  timer->remaining_seconds = timer->deadline = 0;
  timer->valid_until = add_saturating(now, ACTIVITY_TIMER_TERMINAL_SECONDS);
  return true;
}
