#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define ACTIVITY_TIMER_MAGIC UINT32_C(0x4154494d)
#define ACTIVITY_TIMER_MODEL_VERSION 2u
#define ACTIVITY_TIMER_MIN_SECONDS 5u
#define ACTIVITY_TIMER_MAX_SECONDS 86400u
#define ACTIVITY_TIMER_TERMINAL_SECONDS 60u
#define ACTIVITY_TIMER_PAUSED_VISIBLE_SECONDS 300u

typedef enum {
  ACTIVITY_TIMER_RUNNING = 1,
  ACTIVITY_TIMER_PAUSED = 2,
  ACTIVITY_TIMER_FINISHED = 3,
  ACTIVITY_TIMER_CANCELLED = 4
} ActivityTimerState;

/* Fixed-width persisted model. Countdown and alarm truth stays on the watch. */
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t origin;
  uint32_t run;
  uint32_t revision;
  uint32_t state;
  uint32_t configured_seconds;
  uint32_t remaining_seconds;
  uint32_t deadline;
  uint32_t observed_at;
  uint32_t valid_until;
  uint32_t repeat_seconds;
  uint32_t acknowledged;
} ActivityTimerModel;

/* Exact pre-0.5.0 record, retained solely for safe on-watch migration. */
typedef struct {
  uint32_t magic, version, origin, run, revision, state, configured_seconds;
  uint32_t remaining_seconds, deadline, observed_at, valid_until;
} ActivityTimerModelV1;

void activity_timer_init(ActivityTimerModel *timer, uint32_t origin, uint32_t now);
bool activity_timer_valid(const ActivityTimerModel *timer);
/* A saved duration with no pending countdown/alarm. Cancelled records still
 * need the collection's visible bit to distinguish Ready from deleted slots. */
bool activity_timer_ready(const ActivityTimerModel *timer);
bool activity_timer_decode(ActivityTimerModel *timer, const void *bytes,
                            size_t length, bool acknowledged);
uint32_t activity_timer_remaining(const ActivityTimerModel *timer, uint32_t now);
/* Active time within the current run. Remaining-time edits preserve elapsed;
 * pauses do not add elapsed time. repeat_seconds owns the saved repeat duration. */
uint32_t activity_timer_elapsed(const ActivityTimerModel *timer, uint32_t now);
bool activity_timer_start(ActivityTimerModel *timer, uint32_t seconds, uint32_t now);
bool activity_timer_pause(ActivityTimerModel *timer, uint32_t now);
bool activity_timer_resume(ActivityTimerModel *timer, uint32_t now);
bool activity_timer_extend(ActivityTimerModel *timer, uint32_t seconds, uint32_t now);
bool activity_timer_cancel(ActivityTimerModel *timer, uint32_t now);
bool activity_timer_finish_if_due(ActivityTimerModel *timer, uint32_t now);
bool activity_timer_reconfirm(ActivityTimerModel *timer, uint32_t now);
bool activity_timer_acknowledge(ActivityTimerModel *timer);
