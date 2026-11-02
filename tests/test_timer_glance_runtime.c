#include <pebble.h>
#include "../src/c/timer_glance.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

/* Execute the actual native coalescer against PebbleOS's documented UTC-second
 * DB rule: a reload at creation_time <= the stored entry is silently ignored. */
struct AppTimer {
  uint64_t due;
  AppTimerCallback callback;
  void *data;
  bool active;
};
static struct AppTimer timers[16];
static uint64_t wall_ms;
static uint64_t monotonic_ms;
static uint8_t persisted[256];
static int persisted_size;
static bool fail_timer, fail_persist, fail_slice;
static unsigned attempts, accepted, exits, staged_count, visible_count;
static size_t slice_limit;
static time_t accepted_at;
static uint32_t staged_deadlines[8], visible_deadlines[8];
static char staged_subtitles[8][128], visible_subtitles[8][128];

time_t time(time_t *result) {
  const time_t now=(time_t)(wall_ms/1000u);
  if (result) *result=now;
  return now;
}
uint16_t time_ms(time_t *seconds, uint16_t *milliseconds) {
  if (seconds) *seconds=time(NULL);
  const uint16_t fraction=(uint16_t)(wall_ms%1000u);
  if (milliseconds) *milliseconds=fraction;
  return fraction;
}
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *data) {
  if (fail_timer) return NULL;
  for (unsigned i=0;i<16;++i) if (!timers[i].active) {
    timers[i]=(struct AppTimer){monotonic_ms+timeout_ms,callback,data,true};
    return &timers[i];
  }
  assert(!"Too many native timers");
  return NULL;
}
bool app_timer_cancel(AppTimer *timer) {
  if (!timer) return false;
  const bool active=timer->active;
  timer->active=false;
  return active;
}
int persist_get_size(uint32_t key) {
  assert(key==2104);
  return persisted_size;
}
int persist_read_data(uint32_t key, void *data, size_t size) {
  assert(key==2104);
  if (persisted_size<0) return -1;
  const size_t length=size<(size_t)persisted_size?size:(size_t)persisted_size;
  memcpy(data,persisted,length);
  return (int)length;
}
int persist_write_data(uint32_t key, const void *data, size_t size) {
  assert(key==2104 && size<=sizeof(persisted));
  if (fail_persist) return -1;
  memcpy(persisted,data,size);
  persisted_size=(int)size;
  return (int)size;
}
AppGlanceResult app_glance_add_slice(AppGlanceReloadSession *session, AppGlanceSlice slice) {
  (void)session;
  if (fail_slice) return APP_GLANCE_RESULT_INVALID_TEMPLATE_STRING;
  assert(staged_count<8 && strlen(slice.layout.subtitle_template_string)<128);
  staged_deadlines[staged_count]=(uint32_t)slice.expiration_time;
  strcpy(staged_subtitles[staged_count],slice.layout.subtitle_template_string);
  ++staged_count;
  return APP_GLANCE_RESULT_SUCCESS;
}
void app_glance_reload(AppGlanceReloadCallback callback, void *context) {
  AppGlanceReloadSession session={0};
  ++attempts;
  staged_count=0;
  if (callback) callback(&session,slice_limit,context);
  if (time(NULL)<=accepted_at) return;
  ++accepted;
  accepted_at=time(NULL);
  visible_count=staged_count;
  memcpy(visible_deadlines,staged_deadlines,sizeof(visible_deadlines));
  memcpy(visible_subtitles,staged_subtitles,sizeof(visible_subtitles));
}

static void advance(uint32_t millis) {
  const uint64_t target=monotonic_ms+millis;
  unsigned callbacks=0;
  for (;;) {
    int chosen=-1;
    for (unsigned i=0;i<16;++i)
      if (timers[i].active && timers[i].due<=target &&
          (chosen<0 || timers[i].due<timers[chosen].due)) chosen=(int)i;
    if (chosen<0) break;
    assert(++callbacks<100);
    wall_ms+=timers[chosen].due-monotonic_ms;
    monotonic_ms=timers[chosen].due;
    AppTimerCallback callback=timers[chosen].callback;
    void *data=timers[chosen].data;
    timers[chosen].active=false;
    callback(data);
  }
  wall_ms+=target-monotonic_ms;
  monotonic_ms=target;
}
static void exited(void) { ++exits; }
static unsigned active_timers(void) {
  unsigned count=0;
  for (unsigned i=0;i<16;++i) if (timers[i].active) ++count;
  return count;
}
static void reset(void) {
  timer_glance_deinit();
  memset(timers,0,sizeof(timers));
  memset(persisted,0,sizeof(persisted));
  persisted_size=-1;
  wall_ms=UINT64_C(1800000000200);
  monotonic_ms=0;
  accepted_at=time(NULL); /* An unknown prior entry may share this UTC second. */
  attempts=accepted=exits=visible_count=0;
  fail_timer=fail_persist=fail_slice=false;
  slice_limit=8;
}
static TimerCollection running(uint32_t seconds) {
  TimerCollection collection;
  timer_collection_init(&collection,501,(uint32_t)time(NULL));
  assert(timer_collection_create(&collection,seconds,(uint32_t)time(NULL))==0);
  return collection;
}

static void same_second_pause_and_exit(void) {
  reset();
  TimerCollection collection=running(600);
  timer_glance_update(&collection,(uint32_t)time(NULL));
  assert(attempts==0);
  advance(800);
  assert(accepted==1 && visible_count==1);
  assert(visible_deadlines[0]==collection.slots[0].deadline);
  advance(100);
  assert(timer_collection_pause(&collection,0,(uint32_t)time(NULL)));
  timer_glance_update(&collection,(uint32_t)time(NULL));
  timer_glance_request_exit(exited);
  assert(exits==0 && attempts==1);
  advance(899);
  assert(exits==0);
  advance(1);
  assert(exits==1 && accepted==2 && visible_count==0);
  assert(active_timers()==0);
}

static void snapshots_latest_wins_and_handshakes(void) {
  reset();
  TimerCollection collection=running(600);
  timer_glance_update(&collection,(uint32_t)time(NULL));
  assert(timer_collection_pause(&collection,0,(uint32_t)time(NULL)));
  timer_glance_update(&collection,(uint32_t)time(NULL));
  assert(timer_collection_resume(&collection,0,(uint32_t)time(NULL)));
  assert(timer_collection_create(&collection,900,(uint32_t)time(NULL))==1);
  const uint32_t first=collection.slots[0].deadline, second=collection.slots[1].deadline;
  timer_glance_update(&collection,(uint32_t)time(NULL));
  /* The caller's collection is not retained by a delayed SDK callback. */
  memset(&collection,0,sizeof(collection));
  advance(800);
  assert(attempts==1 && accepted==1 && visible_count==2);
  assert(visible_deadlines[0]==first && visible_deadlines[1]==second);
  assert(strstr(visible_subtitles[0],"2 running"));
  collection=running(600);
  assert(timer_collection_create(&collection,900,(uint32_t)time(NULL)-1)==1);
  /* Use the same deadlines despite source revision/observation changes. */
  collection.slots[0].deadline=first;
  collection.slots[1].deadline=second;
  assert(timer_collection_valid(&collection));
  timer_glance_update(&collection,(uint32_t)time(NULL));
  advance(1500);
  assert(attempts==1 && accepted==1);
  timer_glance_request_exit(exited);
  assert(exits==1 && active_timers()==0);
}

static void persisted_reopen_and_corruption(void) {
  reset();
  TimerCollection collection=running(600);
  timer_glance_update(&collection,(uint32_t)time(NULL));
  advance(800);
  assert(accepted==1 && persisted_size>0);
  timer_glance_deinit();
  timer_glance_update(&collection,(uint32_t)time(NULL));
  advance(1200);
  assert(attempts==1); /* Cold reopen of unchanged saved state is quiet. */
  timer_glance_deinit();
  persisted[0]^=0xff;
  timer_glance_update(&collection,(uint32_t)time(NULL));
  assert(attempts==1);
  advance(1000);
  assert(accepted==2 && visible_count==1);
}

static void failures_never_trap_exit(void) {
  reset();
  TimerCollection collection=running(600);
  fail_timer=true;
  timer_glance_update(&collection,(uint32_t)time(NULL));
  timer_glance_request_exit(exited);
  assert(exits==1 && active_timers()==0);

  reset(); collection=running(600);
  fail_persist=true;
  timer_glance_update(&collection,(uint32_t)time(NULL));
  timer_glance_request_exit(exited);
  advance(1000);
  assert(exits==1 && active_timers()==0);

  reset(); collection=running(600);
  timer_glance_update(&collection,(uint32_t)time(NULL));
  advance(800);
  assert(timer_collection_pause(&collection,0,(uint32_t)time(NULL)));
  timer_glance_update(&collection,(uint32_t)time(NULL));
  timer_glance_request_exit(exited);
  wall_ms-=UINT64_C(3600000); /* UTC rollback must not extend a hard exit deadline. */
  advance(1000);
  assert(exits==1);

  reset(); collection=running(600);
  fail_slice=true;
  timer_glance_update(&collection,(uint32_t)time(NULL));
  timer_glance_request_exit(exited);
  advance(1000);
  assert(exits==1 && active_timers()==0);
  fail_slice=false;
  timer_glance_update(&collection,(uint32_t)time(NULL));
  advance(1000);
  assert(visible_count==1); /* A failed slice never becomes a known good cache. */
}

static void limited_slices_refresh_remaining_deadlines(void) {
  reset();
  TimerCollection collection=running(5);
  assert(timer_collection_create(&collection,20,(uint32_t)time(NULL))==1);
  slice_limit=1;
  timer_glance_update(&collection,(uint32_t)time(NULL));
  advance(800);
  assert(visible_count==1 && visible_deadlines[0]==collection.slots[0].deadline);
  advance(5000);
  timer_glance_update(&collection,(uint32_t)time(NULL));
  assert(accepted==2 && visible_count==1);
  assert(visible_deadlines[0]==collection.slots[1].deadline);
  assert(!strstr(visible_subtitles[0],"2 running"));
  timer_glance_deinit();
  advance(1000);
  timer_glance_update(&collection,(uint32_t)time(NULL));
  assert(accepted==2); /* The persisted partial queue now covers the remainder. */
}

int main(void) {
  same_second_pause_and_exit();
  snapshots_latest_wins_and_handshakes();
  persisted_reopen_and_corruption();
  failures_never_trap_exit();
  limited_slices_refresh_remaining_deadlines();
  timer_glance_deinit();
  puts("timer glance runtime: UTC collisions, latest state, snapshot ownership, persisted reopen and bounded exit passed");
  return 0;
}
