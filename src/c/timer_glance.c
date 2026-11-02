#include <pebble.h>
#include <limits.h>
#include <stddef.h>
#include <string.h>
#include "timer_glance.h"
#include "timer_glance_plan.h"

#define GLANCE_CHECKPOINT_KEY 2104
#define GLANCE_CHECKPOINT_MAGIC UINT32_C(0x54474c43)
#define GLANCE_CHECKPOINT_VERSION 1u
#define GLANCE_KNOWN 1u
#define GLANCE_COMPLETE 2u

typedef struct {
  uint32_t magic, version, attempted_at;
  TimerGlanceProjection projection;
  uint32_t covered_until, flags, checksum;
} TimerGlanceCheckpoint;

static TimerGlanceCheckpoint s_last;
static TimerGlanceProjection s_desired;
static bool s_initialized, s_have_desired, s_pending;
static uint32_t s_first_at, s_not_before;
static AppTimer *s_update_timer, *s_exit_timer;
static void (*s_on_exit)(void);

static void prv_try_update(void);

static uint32_t prv_now(void) { return (uint32_t)time(NULL); }

static uint32_t prv_checksum(const TimerGlanceCheckpoint *checkpoint) {
  const uint8_t *bytes = (const uint8_t *)checkpoint;
  uint32_t checksum = UINT32_C(2166136261);
  for (size_t i = 0; i < offsetof(TimerGlanceCheckpoint, checksum); ++i)
    checksum = (checksum ^ bytes[i]) * UINT32_C(16777619);
  return checksum;
}

static bool prv_checkpoint_valid(const TimerGlanceCheckpoint *checkpoint) {
  if (checkpoint->magic != GLANCE_CHECKPOINT_MAGIC ||
      checkpoint->version != GLANCE_CHECKPOINT_VERSION || !checkpoint->attempted_at ||
      checkpoint->projection.count > TIMER_COLLECTION_CAPACITY || checkpoint->flags > 3 ||
      checkpoint->checksum != prv_checksum(checkpoint)) return false;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    const uint32_t deadline = checkpoint->projection.deadlines[i];
    if (i < checkpoint->projection.count) {
      if (!deadline || (i && deadline < checkpoint->projection.deadlines[i - 1])) return false;
    } else if (deadline) return false;
  }
  return true;
}

static void prv_init(uint32_t now) {
  if (s_initialized) return;
  s_initialized = true;
  s_first_at = now;
  if (persist_get_size(GLANCE_CHECKPOINT_KEY) == (int)sizeof(s_last) &&
      persist_read_data(GLANCE_CHECKPOINT_KEY, &s_last, sizeof(s_last)) == (int)sizeof(s_last) &&
      prv_checkpoint_valid(&s_last)) return;
  memset(&s_last, 0, sizeof(s_last));
  /* An older app may have written a glance earlier in this same second. */
  s_not_before = now == UINT32_MAX ? now : now + 1;
}

static void prv_cancel(AppTimer **timer) {
  if (*timer) app_timer_cancel(*timer);
  *timer = NULL;
}

static void prv_release_exit(void) {
  if (!s_on_exit) return;
  void (*callback)(void) = s_on_exit;
  s_on_exit = NULL;
  prv_cancel(&s_exit_timer);
  callback();
}

static void prv_update_timer(void *context) {
  (void)context;
  s_update_timer = NULL;
  prv_try_update();
}

static void prv_schedule(void) {
  if (s_update_timer) return;
  time_t seconds;
  uint16_t millis;
  time_ms(&seconds, &millis);
  const uint32_t delay = millis < 1000 ? 1000u - millis : 1u;
  s_update_timer = app_timer_register(delay, prv_update_timer, NULL);
  if (!s_update_timer) prv_release_exit();
}

typedef struct {
  uint32_t now, covered_until;
  bool success, complete;
} TimerGlanceReload;

static void prv_reload(AppGlanceReloadSession *session, size_t limit, void *context) {
  TimerGlanceReload *reload = context;
  uint32_t after = reload->now;
  for (size_t count = 0; count < limit && count < TIMER_COLLECTION_CAPACITY; ++count) {
    TimerGlanceSlice next;
    if (!timer_glance_projection_next(&s_desired, after, &next)) break;
    const AppGlanceSlice slice = {
      .layout = { .icon = APP_GLANCE_SLICE_DEFAULT_ICON,
                  .subtitle_template_string = next.subtitle },
      .expiration_time = (time_t)next.deadline
    };
    if (app_glance_add_slice(session, slice) != APP_GLANCE_RESULT_SUCCESS) {
      reload->success = false;
      break;
    }
    after = next.deadline;
    reload->covered_until = after;
  }
  reload->complete = !s_desired.count ||
      s_desired.deadlines[s_desired.count - 1] <= after;
}

static bool prv_already_current(uint32_t now) {
  if (!(s_last.flags & GLANCE_KNOWN)) return false;
  TimerGlanceProjection previous = s_last.projection;
  timer_glance_prune(&previous, now);
  return memcmp(&previous, &s_desired, sizeof(previous)) == 0 &&
      (!s_desired.count || (s_last.flags & GLANCE_COMPLETE) ||
       s_desired.deadlines[0] <= s_last.covered_until);
}

static void prv_try_update(void) {
  if (!s_pending || !s_have_desired) { prv_release_exit(); return; }
  const uint32_t now = prv_now();
  timer_glance_prune(&s_desired, now);
  if (prv_already_current(now)) {
    s_pending = false;
    prv_cancel(&s_update_timer);
    prv_release_exit();
    return;
  }
  /* App Glance rejects creation_time <= its existing creation_time. Clock
   * rollback cannot be repaired through the public API; never trap Back. */
  if ((s_last.attempted_at && now < s_last.attempted_at) || now < s_first_at) {
    prv_cancel(&s_update_timer);
    prv_release_exit();
    return;
  }
  if (now < s_not_before || (s_last.attempted_at && now == s_last.attempted_at)) {
    prv_schedule();
    return;
  }
  prv_cancel(&s_update_timer);
  TimerGlanceReload reload = { .now = now, .success = true };
  /* The public implementation invokes this callback synchronously and copies
   * subtitle strings. No app callback retains a source-collection pointer. */
  app_glance_reload(prv_reload, &reload);
  const uint32_t finished_at = prv_now();
  s_last.magic = GLANCE_CHECKPOINT_MAGIC;
  s_last.version = GLANCE_CHECKPOINT_VERSION;
  s_last.attempted_at = finished_at > now ? finished_at : now;
  s_last.projection = s_desired;
  s_last.covered_until = reload.covered_until;
  s_last.flags = (reload.success ? GLANCE_KNOWN : 0u) |
      (reload.complete ? GLANCE_COMPLETE : 0u);
  s_last.checksum = prv_checksum(&s_last);
  /* This is a derived best-attempt checkpoint, not an alarm or delivery ACK.
   * Failure to save it must not block navigation or change native timers. */
  (void)persist_write_data(GLANCE_CHECKPOINT_KEY, &s_last, sizeof(s_last));
  s_pending = false;
  prv_release_exit();
}

void timer_glance_update(const TimerCollection *collection, uint32_t now) {
  TimerGlanceProjection projection;
  if (!timer_glance_project(collection, now, &projection)) return;
  prv_init(now);
  s_desired = projection;
  s_have_desired = s_pending = true;
  prv_try_update();
}

static void prv_exit_timeout(void *context) {
  (void)context;
  s_exit_timer = NULL;
  prv_cancel(&s_update_timer);
  prv_try_update();
  prv_release_exit();
}

void timer_glance_request_exit(void (*on_ready)(void)) {
  if (!on_ready || s_on_exit) return;
  s_on_exit = on_ready;
  s_exit_timer = app_timer_register(1000, prv_exit_timeout, NULL);
  prv_try_update();
  if (s_on_exit && !s_exit_timer) prv_release_exit();
}

void timer_glance_deinit(void) {
  prv_cancel(&s_update_timer);
  prv_cancel(&s_exit_timer);
  s_on_exit = NULL;
  s_initialized = s_have_desired = s_pending = false;
  s_first_at = s_not_before = 0;
  memset(&s_last, 0, sizeof(s_last));
  memset(&s_desired, 0, sizeof(s_desired));
}
