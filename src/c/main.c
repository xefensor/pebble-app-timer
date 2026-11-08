#include <pebble.h>
#include <pebble-xoce-ui/pebble-xoce-ui.h>
#include <pebble-activities/pebble-activities.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "timer_collection.h"
#include "timer_ui.h"
#include "timer_native_type.h"
#include "localization.h"
#include "timer_glance.h"
#include "timer_hourglass.h"
#include "timer_alarm_feedback.h"
#include "timer_appearance.h"
#include "timer_wakeup.h"

#define LEGACY_MODEL_KEY 2100
#define WAKEUP_KEY 2101
#define PUBLICATION_KEY 2102
#define MIGRATION_KEY 2103
#define BANK_HEADER_KEY 2120
#define BANK_DATA_KEY 2130
#define BANK_STRIDE 8
#define CHUNK_BYTES 200u
#define RETRY_LIMIT 3
#define BAR_WIDTH ACTION_BAR_WIDTH
#define CONTENT_WIDTH (200 - BAR_WIDTH)

#if defined(ACTIVITY_TIMER_DIAGNOSTICS) && ACTIVITY_TIMER_DIAGNOSTICS
#define TIMER_DIAG(...) APP_LOG(APP_LOG_LEVEL_INFO, __VA_ARGS__)
#else
#define TIMER_DIAG(...) ((void)0)
#endif

typedef struct {
  uint32_t magic, version, generation, bytes, checksum, alarms;
} SavedHeader;

static Window *s_window;
static Layer *s_canvas;
static Window *s_windows[SCREEN_CONFIRM+1];
static Layer *s_canvases[SCREEN_CONFIRM+1];
static Layer *s_list_canvas;
static MenuLayer *s_options_menu,*s_confirm_menu;
static MenuLayer *s_list;
static unsigned s_text_size;
static unsigned s_list_slots[TIMER_COLLECTION_CAPACITY], s_list_count;
static int16_t s_list_heights[TIMER_COLLECTION_CAPACITY];
static int16_t s_list_viewport_height;
static bool s_list_critical, s_list_dark;
static TimerCollection s_collection;
static uint8_t s_dirty, s_alarm_mask;
static uint32_t s_accepted[TIMER_COLLECTION_CAPACITY];
static unsigned s_bank;
static uint32_t s_generation;
static Screen s_screen = SCREEN_LIST;
static Screen s_edit_return_screen = SCREEN_LIST;
static unsigned s_menu, s_selected;
static int s_edit_slot = -1;
static uint32_t s_edit[3] = {0, 10, 0};
static uint32_t s_edit_initial_seconds;
static unsigned s_edit_field, s_option, s_confirm_choice;
static bool s_focused, s_ready;
static bool s_dark, s_appearance_error;
static TimeUnits s_tick_units;
static bool s_exit_pending;
static AppTimer *s_motion_timer;
static uint32_t s_motion_started;
static bool s_glass_motion;
static int s_glass_from, s_glass_to;
static AppTimer *s_ui_motion_timer;
static AppTimer *s_native_chrome_timer;
static uint32_t s_ui_motion_started;
static enum { UI_MOTION_NONE, UI_MOTION_MENU, UI_MOTION_FIELD, UI_MOTION_VALUE } s_ui_motion;
static TimerMotionRect s_ui_from, s_ui_to;
static Screen s_ui_motion_screen;
static uint32_t s_ui_previous_value;
static int8_t s_ui_value_direction;
static AppTimer *s_icon_timer;
static int s_icon_button=-1;
static bool s_icon_releasing;
static uint32_t s_icon_started;
static Screen s_icon_screen;
static Screen s_repeat_screen;
static PaPublisher s_publisher;
static ActivityTimerModel s_flight_model;
static unsigned s_flight_slot;
static bool s_have_flight, s_transport_busy, s_phone_ready;
static bool s_storage_error, s_alarm_error, s_share_error;
static AppTimer *s_retry_timer;
static unsigned s_retries;
static uint32_t s_transport_ticket;
static WakeupId s_wakeup = -1;
static uint32_t s_alarm_deadline;
static Screen s_drawn_screen = SCREEN_LIST;
static const char *s_drawn_issue, *s_drawn_status;
static bool s_have_drawn;
static uint32_t s_drawn_minute;

static void prv_flush(void);
static void prv_refresh(void);
static void prv_refresh_sharing(void);
static void prv_schedule_alarm(void);
static void prv_tick(struct tm *tick_time, TimeUnits units_changed);
static void prv_transition(void);
static uint32_t prv_millis(void);
static void prv_stop_motion(void);
static void prv_stop_ui_motion(void);
static void prv_stop_icon_motion(void);
static void prv_sync_list(void);
static void prv_sync_aux_menus(void);
static void prv_sync_windows(void);
static bool prv_load_bank(unsigned bank, TimerCollection *collection, SavedHeader *header);

static uint32_t prv_now(void) { return (uint32_t)time(NULL); }

static uint32_t prv_checksum_bytes(const void *data, size_t length, uint32_t alarms) {
  const uint8_t *bytes = data;
  uint32_t value = UINT32_C(2166136261) ^ alarms;
  for (size_t i = 0; i < length; ++i) value = (value ^ bytes[i]) * UINT32_C(16777619);
  return value;
}

static uint32_t prv_checksum(const TimerCollection *collection, uint32_t alarms) {
  return prv_checksum_bytes(collection, sizeof(*collection), alarms);
}

/* Commit a complete bank only after every bounded chunk was written. A failed
 * write leaves the previous bank readable. No record exceeds the SDK's limit. */
static bool prv_save(const TimerCollection *next, uint8_t alarms) {
  if (!timer_collection_valid(next) || s_generation == UINT32_MAX) {
    s_storage_error = true;
    return false;
  }
  const unsigned bank = s_generation ? 1u - s_bank : 0;
  const uint8_t *bytes = (const uint8_t *)next;
  for (size_t offset = 0, chunk = 0; offset < sizeof(*next); offset += CHUNK_BYTES, ++chunk) {
    const size_t count = sizeof(*next) - offset < CHUNK_BYTES ? sizeof(*next) - offset : CHUNK_BYTES;
    if (persist_write_data(BANK_DATA_KEY + bank * BANK_STRIDE + chunk,
        bytes + offset, count) != (int)count) {
      s_storage_error = true;
      return false;
    }
  }
  /* Verify the candidate data before the header makes it authoritative. A
   * post-commit verification failure must never report an unchanged timer in
   * RAM while reboot would restore the newly committed operation. */
  /* Persistence is serialized on Pebble's event loop. Keep full-bank scratch
   * off the 2 KiB app stack, including during nested model/UI operations. */
  static TimerCollection verified;
  uint8_t *verified_bytes = (uint8_t *)&verified;
  for (size_t offset = 0, chunk = 0; offset < sizeof(verified); offset += CHUNK_BYTES, ++chunk) {
    const size_t count = sizeof(verified) - offset < CHUNK_BYTES ? sizeof(verified) - offset : CHUNK_BYTES;
    if (persist_read_data(BANK_DATA_KEY + bank * BANK_STRIDE + chunk,
        verified_bytes + offset, count) != (int)count) {
      s_storage_error = true;
      return false;
    }
  }
  if (memcmp(&verified, next, sizeof(*next))) {
    s_storage_error = true;
    return false;
  }
  const SavedHeader header = { TIMER_COLLECTION_MAGIC, TIMER_COLLECTION_VERSION,
      s_generation + 1, sizeof(*next), prv_checksum(next, alarms), alarms };
  if (persist_write_data(BANK_HEADER_KEY + bank, &header, sizeof(header)) != (int)sizeof(header)) {
    s_storage_error = true;
    return false;
  }
  s_bank = bank;
  s_generation = header.generation;
  s_storage_error = false;
  return true;
}

static bool prv_load_bank(unsigned bank, TimerCollection *collection, SavedHeader *header) {
  if (persist_read_data(BANK_HEADER_KEY + bank, header, sizeof(*header)) != (int)sizeof(*header) ||
      header->magic != TIMER_COLLECTION_MAGIC ||
      (header->version != 1 && header->version != TIMER_COLLECTION_VERSION) ||
      header->bytes > sizeof(*collection) || header->bytes < 20 ||
      !header->generation || header->alarms > 255) return false;
  static uint8_t bytes[sizeof(*collection)];
  for (size_t offset = 0, chunk = 0; offset < header->bytes; offset += CHUNK_BYTES, ++chunk) {
    const size_t count = header->bytes - offset < CHUNK_BYTES ? header->bytes - offset : CHUNK_BYTES;
    if (persist_read_data(BANK_DATA_KEY + bank * BANK_STRIDE + chunk,
        bytes + offset, count) != (int)count) return false;
  }
  uint32_t version;
  memcpy(&version, bytes + sizeof(uint32_t), sizeof(version));
  return version == header->version &&
      header->checksum == prv_checksum_bytes(bytes, header->bytes, header->alarms) &&
      timer_collection_decode(collection, bytes, header->bytes, (uint8_t)header->alarms);
}

static void prv_load(void) {
  static TimerCollection candidate;
  SavedHeader header;
  bool upgrade = false;
  for (unsigned bank = 0; bank < 2; ++bank) {
    if (prv_load_bank(bank, &candidate, &header) && header.generation > s_generation) {
      s_collection = candidate;
      s_generation = header.generation;
      s_bank = bank;
      s_alarm_mask = (uint8_t)header.alarms;
      upgrade = header.version != TIMER_COLLECTION_VERSION;
    }
  }
  if (s_generation) {
    if (upgrade && !prv_save(&s_collection, s_alarm_mask)) return;
    if (persist_write_int(MIGRATION_KEY, 1) >= 0 && persist_read_int(MIGRATION_KEY) == 1)
      persist_delete(LEGACY_MODEL_KEY);
    return;
  }
  const bool migrated = persist_exists(MIGRATION_KEY) && persist_read_int(MIGRATION_KEY) == 1;
  const bool damaged_bank = persist_exists(BANK_HEADER_KEY) || persist_exists(BANK_HEADER_KEY + 1);
  if (migrated) {
    timer_collection_init(&s_collection, prv_now(), prv_now());
    s_storage_error = true;
    return;
  }
  ActivityTimerModel legacy;
  uint8_t legacy_bytes[sizeof(legacy)];
  const int legacy_length = persist_read_data(LEGACY_MODEL_KEY, legacy_bytes, sizeof(legacy_bytes));
  if (legacy_length > 0 && activity_timer_decode(&legacy, legacy_bytes, (size_t)legacy_length, true) &&
      timer_collection_migrate(&s_collection, &legacy, prv_now())) {
    /* A previously finished timer already alerted under the previous app. */
    s_alarm_mask = 0;
  } else if (damaged_bank) {
    timer_collection_init(&s_collection, prv_now(), prv_now());
    s_storage_error = true;
    return;
  } else {
    time_t seconds;
    uint16_t millis;
    time_ms(&seconds, &millis);
    timer_collection_init(&s_collection, (uint32_t)seconds ^ ((uint32_t)millis << 20), prv_now());
  }
  if (prv_save(&s_collection, s_alarm_mask) && persist_write_int(MIGRATION_KEY, 1) >= 0 &&
      persist_read_int(MIGRATION_KEY) == 1) persist_delete(LEGACY_MODEL_KEY);
}

static bool prv_u32(const Tuple *tuple, uint32_t *value) {
  if (!tuple || !value || (tuple->type != TUPLE_UINT && tuple->type != TUPLE_INT)) return false;
  if (tuple->type == TUPLE_UINT) {
    if (tuple->length == 1) *value = tuple->value->uint8;
    else if (tuple->length == 2) *value = tuple->value->uint16;
    else if (tuple->length == 4) *value = tuple->value->uint32;
    else return false;
  } else {
    int32_t signed_value;
    if (tuple->length == 1) signed_value = tuple->value->int8;
    else if (tuple->length == 2) signed_value = tuple->value->int16;
    else if (tuple->length == 4) signed_value = tuple->value->int32;
    else return false;
    if (signed_value < 0) return false;
    *value = (uint32_t)signed_value;
  }
  return true;
}

static const char *prv_string(const Tuple *tuple) {
  if (!tuple || tuple->type != TUPLE_CSTRING || !tuple->length || tuple->length > PA_ID_BYTES + 1) return NULL;
  const char *value = tuple->value->cstring;
  return memchr(value, '\0', tuple->length) == value + tuple->length - 1 ? value : NULL;
}

static void prv_identity(const ActivityTimerModel *timer, char *text, size_t size) {
  /* Slot zero retains the precise legacy run identity; all slot origins are
   * distinct. ACTIVITY_SLOT conveys ownership separately from a run's identity. */
  snprintf(text, size, "%08lx:%08lx", (unsigned long)timer->origin, (unsigned long)timer->run);
}

static void prv_format(uint32_t seconds, char *text, size_t size) {
  if (seconds >= 3600)
    snprintf(text, size, "%lu:%02lu:%02lu", (unsigned long)(seconds / 3600),
        (unsigned long)((seconds / 60) % 60), (unsigned long)(seconds % 60));
  else
    snprintf(text, size, "%02lu:%02lu", (unsigned long)(seconds / 60), (unsigned long)(seconds % 60));
}

static void prv_name(const ActivityTimerModel *timer, char *text, size_t size) {
  if (!(timer->repeat_seconds % 3600))
    snprintf(text, size, app_text("%lu h timer"), (unsigned long)(timer->repeat_seconds / 3600));
  else if (!(timer->repeat_seconds % 60))
    snprintf(text, size, app_text("%lu min timer"), (unsigned long)(timer->repeat_seconds / 60));
  else {
    char duration[16];
    prv_format(timer->repeat_seconds, duration, sizeof(duration));
    snprintf(text, size, app_text("%s timer"), duration);
  }
}

static unsigned prv_first_alarm(void) {
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    if (s_alarm_mask & (1u << i)) return i;
  return TIMER_COLLECTION_CAPACITY;
}

static void prv_open_alarm(void) {
  const unsigned slot = prv_first_alarm();
  if (slot < TIMER_COLLECTION_CAPACITY) {
    const bool changed = s_screen != SCREEN_ALARM || s_selected != slot;
    s_selected = slot; s_screen = SCREEN_ALARM;
    if (changed) prv_transition();
  }
}

/* Only the earliest deadline needs a native Wakeup. Reconciling all due models
 * on each callback avoids the SDK's exclusion of alarms less than 60s apart. */
static void prv_schedule_alarm(void) {
  uint32_t deadline = 0;
  const uint32_t now = prv_now();
  time_t existing = 0;
  if (s_wakeup >= 0 && !wakeup_query(s_wakeup, &existing)) existing = 0;
  TimerWakeup previous = { .id = -1, .deadline = 0 };
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    const ActivityTimerModel *timer = &s_collection.slots[i];
    if (!timer_collection_visible(&s_collection, i) || timer->state != ACTIVITY_TIMER_RUNNING ||
        timer->deadline <= now) continue;
    if (!deadline || timer->deadline < deadline) deadline = timer->deadline;
    if (existing == (time_t)timer->deadline) previous = (TimerWakeup){s_wakeup, timer->deadline};
  }
  const TimerWakeup next = timer_wakeup_replace(previous, deadline, (int32_t)s_collection.revision);
  s_alarm_deadline = next.deadline;
  s_alarm_error = deadline != next.deadline;
  if (next.id < 0) persist_delete(WAKEUP_KEY);
  else if (next.id != s_wakeup) persist_write_int(WAKEUP_KEY, next.id);
  s_wakeup = next.id;
  TIMER_DIAG("alarm now=%lu deadline=%lu id=%ld error=%d", (unsigned long)now,
      (unsigned long)deadline, (long)s_wakeup, s_alarm_error);
}

static bool prv_apply(const TimerCollection *next, uint8_t alarms) {
  if (!prv_save(next, alarms)) { prv_refresh(); return false; }
  const int focused=s_screen==SCREEN_LIST&&s_menu?
      timer_collection_visible_slot(&s_collection,s_menu-1):-1;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    if (s_accepted[i] != next->slots[i].revision) s_dirty |= (uint8_t)(1u << i);
  s_collection = *next;
  s_alarm_mask = alarms;
  if (focused>=0)s_menu=timer_ui_menu_for_slot(&s_collection,(unsigned)focused);
  s_retries = 0;
  s_share_error = false;
  prv_schedule_alarm();
  timer_glance_update(&s_collection, prv_now());
  prv_flush();
  prv_refresh();
  return true;
}

static bool prv_finish_at(uint32_t now) {
  if (!timer_collection_has_due(&s_collection, now)) return false;
  /* Completion can run under either a button or inbox transaction frame.
   * Keep its collection scratch off the SDK's 2 KiB stack as well. */
  static TimerCollection next;
  next = s_collection;
  const uint8_t finished = timer_collection_finish_due(&next, now);
  if (!finished) return false;
  if (!prv_apply(&next, s_alarm_mask | finished)) {
    /* The visible local alarm still alerts when flash fails; keep the error
     * visible. Never publish an unsaved authoritative completion. */
    s_collection = next;
    s_alarm_mask |= finished;
    prv_schedule_alarm();
  }
  prv_open_alarm();
  prv_refresh();
  return true;
}

static bool prv_finish(void) { return prv_finish_at(prv_now()); }

static void prv_retry(void *context) {
  (void)context;
  s_retry_timer = NULL;
  if (s_have_flight && s_publisher.awaiting_receipt) {
    ++s_retries;
    pa_publisher_retry(&s_publisher);
  }
  prv_flush();
  prv_refresh_sharing();
}

static int prv_pending_slot(void) {
  int chosen = -1;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    if ((s_dirty & (1u << i)) && (chosen < 0 || s_collection.slots[i].revision < s_collection.slots[chosen].revision)) chosen = (int)i;
  return chosen;
}

static void prv_flush(void) {
  if (s_storage_error || !s_phone_ready || s_transport_busy || !connection_service_peek_pebble_app_connection()) return;
  if (s_retries >= RETRY_LIMIT) { s_share_error = true; prv_refresh_sharing(); return; }
  if (!s_have_flight) {
    const int slot = prv_pending_slot();
    if (slot < 0) return;
    s_flight_slot = (unsigned)slot;
    s_flight_model = s_collection.slots[slot];
    char id[32];
    prv_identity(&s_flight_model, id, sizeof(id));
    if (!pa_publisher_update(&s_publisher, id, s_flight_model.revision)) return;
    s_have_flight = true;
  }
  PaPublication publication;
  if (!pa_publisher_begin(&s_publisher, &publication)) return;
  s_transport_ticket = publication.ticket;
  if (persist_write_data(PUBLICATION_KEY, &s_publisher.next_ticket, sizeof(s_publisher.next_ticket)) != (int)sizeof(s_publisher.next_ticket)) {
    pa_publisher_transport_result(&s_publisher, publication.ticket, false);
    s_share_error = true;
    return;
  }
  DictionaryIterator *out;
  AppMessageResult result = app_message_outbox_begin(&out);
  if (result != APP_MSG_OK) {
    pa_publisher_transport_result(&s_publisher, publication.ticket, false);
    ++s_retries;
    if (!s_retry_timer && s_retries < RETRY_LIMIT) s_retry_timer = app_timer_register(500, prv_retry, NULL);
    return;
  }
  const ActivityTimerModel *timer = &s_flight_model;
  const bool active = timer->state == ACTIVITY_TIMER_RUNNING || timer->state == ACTIVITY_TIMER_PAUSED;
  const char *state = timer->state == ACTIVITY_TIMER_RUNNING ? "active" :
      timer->state == ACTIVITY_TIMER_PAUSED ? "paused" : timer->state == ACTIVITY_TIMER_FINISHED ? "completed" : "cancelled";
  char name[32];
  prv_name(timer, name, sizeof(name));
  dict_write_uint8(out, MESSAGE_KEY_ACTIVITY_SCHEMA_VERSION, 1);
  dict_write_uint8(out, MESSAGE_KEY_ACTIVITY_SLOT, s_flight_slot);
  dict_write_cstring(out, MESSAGE_KEY_ACTIVITY_RUN, publication.run_id);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_REVISION, timer->revision);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_PUBLISH_SESSION, publication.ticket);
  dict_write_cstring(out, MESSAGE_KEY_ACTIVITY_STATE, state);
  dict_write_cstring(out, MESSAGE_KEY_ACTIVITY_KIND, "timer");
  dict_write_cstring(out, MESSAGE_KEY_ACTIVITY_CLOCK, active ? "deadline" : "none");
  dict_write_cstring(out, MESSAGE_KEY_ACTIVITY_NAME, name);
  dict_write_cstring(out, MESSAGE_KEY_ACTIVITY_MAIN, "");
  dict_write_cstring(out, MESSAGE_KEY_ACTIVITY_FOOTER, name);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_OBSERVED_AT, timer->observed_at);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_VALID_UNTIL, timer->valid_until);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_DEADLINE, timer->state == ACTIVITY_TIMER_RUNNING ? timer->deadline : 0);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_DURATION, timer->state == ACTIVITY_TIMER_PAUSED ? timer->remaining_seconds : 0);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_CLOCK_AT, 0);
  dict_write_uint32(out, MESSAGE_KEY_ACTIVITY_TOTAL_DURATION, timer->configured_seconds);
  dict_write_uint8(out, MESSAGE_KEY_ACTIVITY_SHOW_TERMINAL,
      timer->state == ACTIVITY_TIMER_FINISHED && !timer->acknowledged);
  dict_write_uint8(out, MESSAGE_KEY_ACTIVITY_PRIVATE, 0);
  dict_write_end(out);
  s_transport_busy = true;
  result = app_message_outbox_send();
  if (result != APP_MSG_OK) {
    s_transport_busy = false;
    pa_publisher_transport_result(&s_publisher, publication.ticket, false);
    ++s_retries;
    if (!s_retry_timer && s_retries < RETRY_LIMIT) s_retry_timer = app_timer_register(500, prv_retry, NULL);
  }
}

static void prv_outbox_sent(DictionaryIterator *iter, void *context) {
  (void)context;
  uint32_t ticket = s_transport_ticket;
  if (iter) prv_u32(dict_find(iter, MESSAGE_KEY_ACTIVITY_PUBLISH_SESSION), &ticket);
  s_transport_busy = false;
  pa_publisher_transport_result(&s_publisher, ticket, true);
  if (s_have_flight && s_publisher.awaiting_receipt && !s_retry_timer)
    s_retry_timer = app_timer_register(2000, prv_retry, NULL);
  prv_flush();
  prv_refresh_sharing();
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  (void)reason; (void)context;
  uint32_t ticket = s_transport_ticket;
  if (iter) prv_u32(dict_find(iter, MESSAGE_KEY_ACTIVITY_PUBLISH_SESSION), &ticket);
  s_transport_busy = false;
  if (pa_publisher_transport_result(&s_publisher, ticket, false)) {
    ++s_retries;
    if (!s_retry_timer && s_retries < RETRY_LIMIT) s_retry_timer = app_timer_register(500, prv_retry, NULL);
    if (s_retries >= RETRY_LIMIT) s_share_error = true;
  }
  prv_flush();
  prv_refresh_sharing();
}

static void prv_reset_publication(void) {
  const uint32_t ticket = s_publisher.next_ticket;
  pa_publisher_init(&s_publisher, ticket, connection_service_peek_pebble_app_connection());
  s_have_flight = false;
  s_retries = 0;
  if (s_retry_timer) { app_timer_cancel(s_retry_timer); s_retry_timer = NULL; }
}

static void prv_inbox(DictionaryIterator *iter, void *context) {
  (void)context;
  uint32_t value,appearance_version,appearance;
  if (prv_u32(dict_find(iter,MESSAGE_KEY_TIMER_APPEARANCE_VERSION),&appearance_version)&&
      appearance_version==1&&prv_u32(dict_find(iter,MESSAGE_KEY_TIMER_APPEARANCE),&appearance)&&appearance<=1) {
    const bool dark=appearance==1;
    if (timer_appearance_save(dark)) {
      const bool changed=s_dark!=dark||s_appearance_error;
      s_dark=dark;s_appearance_error=false;
      if (changed) { if(s_window)window_set_background_color(s_window,s_screen==SCREEN_ALARM||s_dark?GColorBlack:GColorWhite);prv_refresh(); }
    } else { s_appearance_error=true;prv_refresh(); }
  }
  if (prv_u32(dict_find(iter, MESSAGE_KEY_ACTIVITY_REQUEST), &value) && value) {
    s_phone_ready = false;
    const uint32_t now = prv_now();
    prv_finish_at(now);
    TimerCollection confirmed = s_collection;
    uint32_t floor = 0;
    prv_u32(dict_find(iter, MESSAGE_KEY_ACTIVITY_LAST_REVISION), &floor);
    if (!timer_collection_raise_floor(&confirmed, floor)) { s_share_error = true; prv_refresh(); return; }
    bool valid = true;
    for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
      if (!timer_collection_reconfirm(&confirmed, i, now)) valid = false;
    if (!valid) { s_share_error = true; prv_refresh(); return; }
    /* Old outbox callbacks may still arrive, but their persisted tickets can
     * never acknowledge the new collection. The SDK queue remains single. */
    prv_reset_publication();
    if (prv_apply(&confirmed, s_alarm_mask)) { s_phone_ready = true; s_dirty = UINT8_MAX; prv_flush(); }
  }
  const char *run = prv_string(dict_find(iter, MESSAGE_KEY_ACTIVITY_ACCEPTED_RUN));
  uint32_t revision, ticket;
  const Tuple *session = dict_find(iter, MESSAGE_KEY_ACTIVITY_PUBLISH_SESSION);
  if (s_have_flight && run && prv_u32(dict_find(iter, MESSAGE_KEY_ACTIVITY_ACCEPTED_REVISION), &revision) &&
      prv_u32(session, &ticket) && pa_publisher_receipt(&s_publisher, run, revision, ticket)) {
    s_accepted[s_flight_slot] = revision;
    if (s_collection.slots[s_flight_slot].revision == revision) s_dirty &= (uint8_t)~(1u << s_flight_slot);
    s_have_flight = false;
    s_share_error = false;
    s_retries = 0;
    if (s_retry_timer) { app_timer_cancel(s_retry_timer); s_retry_timer = NULL; }
  }
  const char *error = prv_string(dict_find(iter, MESSAGE_KEY_ACTIVITY_ERROR));
  const char *rejected = prv_string(dict_find(iter, MESSAGE_KEY_ACTIVITY_RUN));
  char id[32];
  prv_identity(&s_flight_model, id, sizeof(id));
  if (s_have_flight && error && rejected && !strcmp(rejected, id) &&
      prv_u32(dict_find(iter, MESSAGE_KEY_ACTIVITY_REVISION), &revision) && revision == s_flight_model.revision &&
      prv_u32(session, &ticket) && ticket == s_publisher.flight.ticket) {
    s_share_error = true;
    s_retries = RETRY_LIMIT;
  }
  prv_flush();
  prv_refresh_sharing();
}

static void prv_connection(bool connected) {
  if (!pa_publisher_set_connected(&s_publisher, connected)) return;
  if (connected) {
    s_retries = 0;
    if (s_have_flight) pa_publisher_retry(&s_publisher);
    prv_flush();
  }
  prv_refresh_sharing();
}

static const char *prv_status(void) {
  if (!connection_service_peek_pebble_app_connection()) return "Phone disconnected";
  if (s_share_error) return "Sharing unavailable";
  if (!s_phone_ready) return "Waiting to share";
  if (s_screen != SCREEN_LIST && s_accepted[s_selected] == s_collection.slots[s_selected].revision)
    return "Shared with phone";
  return s_dirty ? "Waiting to share" : "Shared with phone";
}

static TimerUiView prv_view(void) {
  TimerUiView view={.screen=s_screen,.collection=&s_collection,
      .menu=s_menu,.selected=s_selected,.edit_field=s_edit_field,
      .option=s_option,.confirm_choice=s_confirm_choice,.edit=s_edit,
      .editing_new=s_edit_slot<0,.dark=s_dark,.now=prv_now(),
      .text_size=s_text_size,
      .editing_ready=s_edit_slot>=0&&
          activity_timer_ready(&s_collection.slots[s_edit_slot]),
      .issue=s_storage_error?"Changes not saved":
          s_alarm_error?"Alarm not set":s_appearance_error?"Theme not saved":NULL,
      .appearance_issue=s_appearance_error&&!s_storage_error&&!s_alarm_error,
      .status=prv_status(),
      .hourglass_transition=s_glass_motion&&s_screen==SCREEN_DETAIL,
      .hourglass_angle=(int16_t)(s_glass_motion?
          timer_hourglass_angle(s_glass_from,s_glass_to,
              prv_millis()-s_motion_started,300u):0)};
  if(s_list) {
    int top=scroll_layer_get_content_offset(menu_layer_get_scroll_layer(s_list)).y;
    const int height=layer_get_frame(menu_layer_get_layer(s_list)).size.h;
    for(unsigned row=0;row<s_list_count;++row) {
      if(top+s_list_heights[row]>0&&top<height)view.visible_slots|=(uint8_t)(1u<<s_list_slots[row]);
      top+=s_list_heights[row];
    }
  }
  MenuLayer *context_menu=s_screen==SCREEN_OPTIONS?s_options_menu:s_screen==SCREEN_CONFIRM?s_confirm_menu:NULL;
  if(context_menu)view.context_above=scroll_layer_get_content_offset(menu_layer_get_scroll_layer(context_menu)).y<0;
  const uint32_t elapsed=prv_millis()-s_ui_motion_started;
  view.selection_transition=s_ui_motion_screen==s_screen&&
      (s_ui_motion==UI_MOTION_MENU||s_ui_motion==UI_MOTION_FIELD);
  view.selection_rect=s_ui_motion==UI_MOTION_MENU?
      timer_motion_menu(s_ui_from,s_ui_to,elapsed):
      timer_motion_field(s_ui_from,s_ui_to,elapsed);
  if(s_ui_motion_screen==s_screen&&s_ui_motion==UI_MOTION_VALUE) {
    view.editor_motion=timer_motion_value(elapsed,s_ui_value_direction);
    view.editor_previous_value=s_ui_previous_value;
  }
  if(s_icon_button>=0&&s_icon_button<3)view.action_offset[s_icon_button]=
      -(s_icon_releasing?xoce_ui_press_offset(prv_millis()-s_icon_started):XOCE_UI_PRESS_DISTANCE);
  return view;
}

static void prv_draw(Layer *layer, GContext *ctx) {
  TimerUiView view = prv_view();
  /* Outgoing windows remain alive and keep their own composition throughout
   * the native push/pop. Never repaint both surfaces as the destination. */
  view.screen=*(Screen *)layer_get_data(layer);
  timer_ui_draw(ctx, &view);
  s_drawn_screen = view.screen;
  s_drawn_issue = view.issue;
  s_drawn_status = view.status;
  s_drawn_minute = view.now / 60u;
  s_have_drawn = true;
}

static uint16_t prv_list_rows(MenuLayer *menu,uint16_t section,void *context) {
  (void)menu;(void)section;(void)context;
  return s_list_count?s_list_count:1;
}
static int16_t prv_list_height(MenuLayer *menu,MenuIndex *index,void *context) {
  (void)menu;(void)context;
  return index->row<s_list_count?s_list_heights[index->row]:1;
}
static void prv_list_draw(GContext *ctx,const Layer *cell,MenuIndex *index,void *context) {
  (void)context;
  if(index->row>=s_list_count)return;
  TimerUiView view=prv_view();view.screen=SCREEN_LIST;
  timer_ui_draw_row(ctx,cell,&view,s_list_slots[index->row]);
}
static void prv_list_disappear(Window *window) {
  (void)window;
  /* Stop native motion while covered; both surfaces remain alive until the
   * OS finishes the transition. Return to the selected identity fully visible. */
  if(s_list&&s_list_count&&s_menu) {
    ScrollLayer *scroll=menu_layer_get_scroll_layer(s_list);
    const GPoint offset=scroll_layer_get_content_offset(scroll);
    scroll_layer_set_content_offset(scroll,offset,false);
    int top=0;
    for(unsigned i=0;i+1<s_menu;++i)top+=s_list_heights[i];
    const int height=layer_get_frame(menu_layer_get_layer(s_list)).size.h;
    const MenuRowAlign align=top+offset.y<0?MenuRowAlignTop:
        top+offset.y+s_list_heights[s_menu-1]>height?MenuRowAlignBottom:MenuRowAlignNone;
    menu_layer_set_selected_index(s_list,(MenuIndex){.row=s_menu-1},align,false);
  }
  prv_stop_motion();prv_stop_ui_motion();prv_stop_icon_motion();
}
static void prv_sync_list(void) {
  if(!s_list||s_screen!=SCREEN_LIST)return;
  unsigned slots[TIMER_COLLECTION_CAPACITY];
  const unsigned count=timer_collection_visible_slots(&s_collection,slots);
  const TimerUiView view=prv_view();
  const bool critical=view.issue&&view.issue[0];
  const bool replacing=count==TIMER_COLLECTION_CAPACITY&&
      timer_collection_can_create(&s_collection)&&!s_menu;
  const int16_t viewport_height=critical?178-timer_ui_issue_height(&view):replacing?144:178;
  bool changed=count!=s_list_count||critical!=s_list_critical||
      viewport_height!=s_list_viewport_height;
  for(unsigned row=0;row<count;++row) {
    const int16_t height=timer_ui_row_height(&view,slots[row]);
    if(s_list_slots[row]!=slots[row]||s_list_heights[row]!=height)changed=true;
    s_list_slots[row]=slots[row];s_list_heights[row]=height;
  }
  s_list_count=count;s_list_critical=critical;
  s_list_viewport_height=viewport_height;
  if(changed) {
    layer_set_frame(menu_layer_get_layer(s_list),GRect(0,50,CONTENT_WIDTH,viewport_height));
    menu_layer_reload_data(s_list);
  }
  if(s_list_dark!=s_dark||changed) {
    s_list_dark=s_dark;
    menu_layer_set_normal_colors(s_list,s_dark?GColorBlack:GColorWhite,s_dark?GColorWhite:GColorBlack);
    menu_layer_set_highlight_colors(s_list,s_dark?GColorWhite:GColorBlack,s_dark?GColorBlack:GColorWhite);
  }
  layer_set_hidden(menu_layer_get_layer(s_list),!count);
  if(count&&s_menu) {
    const MenuIndex index={.section=0,.row=s_menu-1};
    if(changed||menu_layer_get_selected_index(s_list).row!=index.row)
      menu_layer_set_selected_index(s_list,index,s_menu==1?MenuRowAlignTop:MenuRowAlignBottom,false);
  }
  /* Do not reload on ticks or raw icon feedback: that would cancel the SDK's
   * in-flight selection/scroll animation. */
  layer_mark_dirty(menu_layer_get_layer(s_list));
}
static MenuLayer *prv_action_menu(Screen screen) {return screen==SCREEN_OPTIONS?s_options_menu:s_confirm_menu;}
static TimerUiView prv_action_view(void *context) {TimerUiView view=prv_view();view.screen=(Screen)(uintptr_t)context;return view;}
static uint16_t prv_action_rows(MenuLayer *menu,uint16_t section,void *context) {
  (void)menu;(void)section;return (Screen)(uintptr_t)context==SCREEN_OPTIONS?3:2;
}
static int16_t prv_action_height(MenuLayer *menu,MenuIndex *index,void *context) {
  (void)menu;const TimerUiView view=prv_action_view(context);return timer_ui_action_row_height(&view,index->row);
}
static int16_t prv_context_height(MenuLayer *menu,uint16_t section,void *context) {
  (void)menu;(void)section;const TimerUiView view=prv_action_view(context);return timer_ui_context_height(&view);
}
static void prv_action_draw(GContext *ctx,const Layer *cell,MenuIndex *index,void *context) {
  const TimerUiView view=prv_action_view(context);timer_ui_draw_action_row(ctx,cell,&view,index->row);
}
static void prv_context_draw(GContext *ctx,const Layer *cell,uint16_t section,void *context) {
  (void)cell;(void)section;const TimerUiView view=prv_action_view(context);timer_ui_draw_context(ctx,&view);
}
static void prv_sync_aux_menus(void) {
  static int16_t geometry[2][4];static bool palettes[2];
  for(unsigned i=0;i<2;++i) {
    const Screen screen=i?SCREEN_CONFIRM:SCREEN_OPTIONS;MenuLayer *menu=prv_action_menu(screen);if(!menu)continue;
    if(s_screen!=screen)continue;
    TimerUiView view=prv_view();view.screen=screen;
    bool changed=false;
    const int16_t header=timer_ui_context_height(&view);
    if(geometry[i][0]!=header){geometry[i][0]=header;changed=true;}
    const unsigned count=i?2:3;
    for(unsigned row=0;row<count;++row) {
      const int16_t height=timer_ui_action_row_height(&view,row);
      if(geometry[i][row+1]!=height){geometry[i][row+1]=height;changed=true;}
    }
    if(changed)menu_layer_reload_data(menu);
    if(changed||palettes[i]!=s_dark) {
      palettes[i]=s_dark;
      menu_layer_set_normal_colors(menu,s_dark?GColorBlack:GColorWhite,s_dark?GColorWhite:GColorBlack);
      menu_layer_set_highlight_colors(menu,s_dark?GColorWhite:GColorBlack,s_dark?GColorBlack:GColorWhite);
    }
    const unsigned row=i?s_confirm_choice:s_option;
    if(changed||menu_layer_get_selected_index(menu).row!=row)
      menu_layer_set_selected_index(menu,(MenuIndex){.row=row},MenuRowAlignBottom,false);
    layer_mark_dirty(menu_layer_get_layer(menu));
  }
}
static void prv_native_chrome_settled(void *context) {
  (void)context;s_native_chrome_timer=NULL;
  if(s_focused&&s_canvas)layer_mark_dirty(s_canvas);
}
static void prv_action_step(bool up) {
  /* The OS animates the menu. Repaint only its surrounding rail once the
   * native scroll has settled, so the context-Up affordance stays accurate. */
  if(s_native_chrome_timer)app_timer_cancel(s_native_chrome_timer);
  s_native_chrome_timer=app_timer_register(400,prv_native_chrome_settled,NULL);
  MenuLayer *menu=prv_action_menu(s_screen);unsigned *row=s_screen==SCREEN_OPTIONS?&s_option:&s_confirm_choice;
  ScrollLayer *scroll=menu_layer_get_scroll_layer(menu);const GPoint offset=scroll_layer_get_content_offset(scroll);
  if(!*row&&up&&offset.y<0) {
    scroll_layer_set_content_offset(scroll,GPoint(0,offset.y+60>0?0:offset.y+60),true);return;
  }
  if(!*row&&!up) {
    TimerUiView view=prv_view();const int bottom=timer_ui_context_height(&view)+timer_ui_action_row_height(&view,0)+offset.y;
    if(bottom>188){menu_layer_set_selected_index(menu,(MenuIndex){0,0},MenuRowAlignBottom,true);return;}
  }
  const unsigned maximum=s_screen==SCREEN_OPTIONS?2:1;
  if((up&&!*row)||(!up&&*row==maximum))return;
  menu_layer_set_selected_next(menu,up,MenuRowAlignBottom,true);
  *row=menu_layer_get_selected_index(menu).row;
}

static void prv_sync_windows(void) {
  if(!s_windows[SCREEN_DETAIL]||!window_stack_contains_window(s_window))return;
  Screen path[4];const unsigned count=timer_ui_window_path(s_screen,s_edit_return_screen,path);
  bool wanted[SCREEN_CONFIRM+1]={false};
  for(unsigned i=0;i<count;++i)wanted[path[i]]=true;
  Window *old_top=window_stack_get_top_window();
  const bool existing=window_stack_contains_window(s_windows[s_screen]);
  /* Ancestors stay underneath their destination. All windows/layers remain
   * alive until app exit, including outgoing animated surfaces. */
  for(unsigned i=1;i+1<count;++i)
    if(!window_stack_contains_window(s_windows[path[i]]))window_stack_push(s_windows[path[i]],false);
  for(unsigned screen=1;screen<=SCREEN_CONFIRM;++screen) {
    Window *window=s_windows[screen];
    if(!wanted[screen]&&window!=old_top&&window_stack_contains_window(window))window_stack_remove(window,false);
  }
  s_canvas=s_canvases[s_screen];
  window_set_background_color(s_windows[s_screen],s_screen==SCREEN_ALARM||s_dark?GColorBlack:GColorWhite);
  if(s_screen==SCREEN_ALARM) {
    if(old_top!=s_windows[SCREEN_ALARM]&&old_top!=s_window)window_stack_remove(old_top,false);
    if(!existing)window_stack_push(s_windows[SCREEN_ALARM],false);
  } else if(existing) {
    if(old_top!=s_windows[s_screen]&&old_top!=s_window)window_stack_remove(old_top,true);
  } else {
    window_stack_push(s_windows[s_screen],true);
    if(old_top!=s_window) {
      bool keep=false;
      for(unsigned i=0;i<count;++i)if(old_top==s_windows[path[i]])keep=true;
      if(!keep)window_stack_remove(old_top,false);
    }
  }
  if(old_top!=s_windows[s_screen])TIMER_DIAG("ui heap=%u screen=%d",(unsigned)heap_bytes_free(),(int)s_screen);
}

static void prv_list_step(bool up) {
  if(!s_list_count)return;
  const unsigned first=timer_collection_can_create(&s_collection)?0:1;
  if(up&&s_menu==1&&first==0) {s_menu=0;return;}
  if(!up&&!s_menu) {s_menu=1;return;}
  if(!s_menu)return;
  const unsigned row=s_menu-1;
  if((up&&!row)||(!up&&row+1>=s_list_count))return;
  const unsigned next=up?row-1:row+1;
  int top=0;
  for(unsigned i=0;i<next;++i)top+=s_list_heights[i];
  const int offset=scroll_layer_get_content_offset(menu_layer_get_scroll_layer(s_list)).y;
  const int height=layer_get_frame(menu_layer_get_layer(s_list)).size.h;
  const MenuRowAlign align=top+offset<0?MenuRowAlignTop:
      top+offset+s_list_heights[next]>height?MenuRowAlignBottom:MenuRowAlignNone;
  menu_layer_set_selected_next(s_list,up,align,true);
  /* Native logical selection is immediate, even while its highlight travels.
   * Select during motion always opens precisely this production timer. */
  s_menu=menu_layer_get_selected_index(s_list).row+1;
}

static void prv_update_ticks(void) {
  if (!s_ready) return;
  bool needed = false;
  for (unsigned i=0;i<TIMER_COLLECTION_CAPACITY;++i)
    if (timer_collection_visible(&s_collection,i) &&
        s_collection.slots[i].state==ACTIVITY_TIMER_RUNNING) needed=true;
  const TimeUnits units=needed?SECOND_UNIT:MINUTE_UNIT;
  if (units==s_tick_units) return;
  if (s_tick_units) tick_timer_service_unsubscribe();
  s_tick_units=units;
  tick_timer_service_subscribe(units,prv_tick);
}

static void prv_refresh(void) {
  /* Native completion/Wakeup can change screens without a click. Settle
   * every decorative family before drawing those owner-driven transitions. */
  if(s_ui_motion!=UI_MOTION_NONE&&s_ui_motion_screen!=s_screen)prv_stop_ui_motion();
  if(s_icon_button>=0&&s_icon_screen!=s_screen)prv_stop_icon_motion();
  if(s_glass_motion&&s_screen!=SCREEN_DETAIL)prv_stop_motion();
  const TimerUiView layout=prv_view();
  if(s_have_drawn&&((s_drawn_issue&&s_drawn_issue[0])!=(layout.issue&&layout.issue[0])))prv_stop_ui_motion();
  if (s_screen==SCREEN_LIST && !s_menu && timer_collection_count(&s_collection) &&
      !timer_collection_can_create(&s_collection)) s_menu=1;
  timer_alarm_feedback_update(s_alarm_mask);
  prv_update_ticks();
  prv_sync_list();
  prv_sync_aux_menus();
  prv_sync_windows();
  if (s_canvas) layer_mark_dirty(s_canvas);
}

static void prv_refresh_sharing(void) {
  /* Native timer mutations use prv_refresh. Sharing callbacks only repaint
   * information that is actually visible; status strings have static lifetime. */
  const TimerUiView view = prv_view();
  if (!s_have_drawn || timer_ui_chrome_changed(&view, s_drawn_screen,
      s_drawn_issue, s_drawn_status)) prv_refresh();
}

static uint32_t prv_millis(void) {
  time_t seconds;uint16_t milliseconds;
  time_ms(&seconds,&milliseconds);
  return (uint32_t)seconds*1000u+milliseconds;
}

static void prv_stop_motion(void) {
  if (s_motion_timer) { app_timer_cancel(s_motion_timer);s_motion_timer=NULL; }
  const bool was_moving=s_glass_motion;
  s_glass_motion=false;
  if(was_moving&&s_focused&&s_canvas)layer_mark_dirty(s_canvas);
}

static void prv_stop_ui_motion(void) {
  if(s_ui_motion_timer){app_timer_cancel(s_ui_motion_timer);s_ui_motion_timer=NULL;}
  const bool moving=s_ui_motion!=UI_MOTION_NONE;
  s_ui_motion=UI_MOTION_NONE;
  if(moving&&s_focused&&s_canvas)layer_mark_dirty(s_canvas);
}

static void prv_ui_motion_tick(void *context) {
  (void)context;s_ui_motion_timer=NULL;
  const uint32_t duration=s_ui_motion==UI_MOTION_MENU?TIMER_MENU_MOTION_MS:
      s_ui_motion==UI_MOTION_FIELD?TIMER_FIELD_SLIDE_MS+TIMER_FIELD_SETTLE_MS:
      TIMER_VALUE_TEXT_MS+TIMER_VALUE_SETTLE_MS;
  if(!s_focused||s_ui_motion_screen!=s_screen||s_ui_motion==UI_MOTION_NONE||
      prv_millis()-s_ui_motion_started>=duration) {
    prv_stop_ui_motion();return;
  }
  if(s_canvas)layer_mark_dirty(s_canvas);
  s_ui_motion_timer=app_timer_register(XOCE_UI_PRESS_FRAME_MS,prv_ui_motion_tick,NULL);
  if(!s_ui_motion_timer)prv_stop_ui_motion();
}

static void prv_stop_icon_motion(void) {
  if(s_icon_timer){app_timer_cancel(s_icon_timer);s_icon_timer=NULL;}
  const bool moving=s_icon_button>=0;
  s_icon_button=-1;s_icon_releasing=false;
  if(moving&&s_focused&&s_canvas)layer_mark_dirty(s_canvas);
}

static void prv_icon_tick(void *context) {
  (void)context;s_icon_timer=NULL;
  if(!s_focused||prv_millis()-s_icon_started>=XOCE_UI_PRESS_RELEASE_MS){prv_stop_icon_motion();return;}
  if(s_canvas)layer_mark_dirty(s_canvas);
  s_icon_timer=app_timer_register(XOCE_UI_PRESS_FRAME_MS,prv_icon_tick,NULL);
  if(!s_icon_timer)prv_stop_icon_motion();
}

static void prv_raw_press(unsigned icon) {
  prv_stop_icon_motion();
  if(!s_focused)return;
  s_icon_button=(int)icon;
  s_icon_screen=s_screen;
  if(s_canvas)layer_mark_dirty(s_canvas);
}

static void prv_raw_release(unsigned icon) {
  if(s_icon_button!=(int)icon||!s_focused)return;
  s_icon_releasing=true;s_icon_started=prv_millis();prv_icon_tick(NULL);
}

/* Raw identity is explicit and independent of recognizer internals. A release
 * of a second held button cannot settle the current button's feedback. */
static void prv_up_press(ClickRecognizerRef r,void *c){(void)r;(void)c;prv_raw_press(0);}
static void prv_up_release(ClickRecognizerRef r,void *c){(void)r;(void)c;prv_raw_release(0);}
static void prv_select_press(ClickRecognizerRef r,void *c){(void)r;(void)c;prv_raw_press(1);}
static void prv_select_release(ClickRecognizerRef r,void *c){(void)r;(void)c;prv_raw_release(1);}
static void prv_down_press(ClickRecognizerRef r,void *c){(void)r;(void)c;prv_raw_press(2);}
static void prv_down_release(ClickRecognizerRef r,void *c){(void)r;(void)c;prv_raw_release(2);}

static bool prv_selection_rect(const TimerUiView *view, TimerMotionRect *rect) {
  if(view->screen==SCREEN_EDITOR) {
    const GRect frame=timer_ui_editor_field_rect(view,view->edit_field);
    *rect=(TimerMotionRect){frame.origin.x,frame.origin.y,frame.size.w,frame.size.h};
  }
  else return false;
  return true;
}

static void prv_animate_ui(const TimerUiView *before, bool repeating, ButtonId button,
                           uint32_t previous_value) {
  const TimerUiView after=prv_view();
  const bool pending=before->selection_transition;
  TimerMotionRect from,to;
  const bool have_from=prv_selection_rect(before,&from),have_to=prv_selection_rect(&after,&to);
  if(pending)from=before->selection_rect;
  prv_stop_ui_motion();
  if(repeating||!s_focused||before->screen!=after.screen||!have_from||!have_to)return;
  if(after.screen==SCREEN_EDITOR) {
    const uint32_t seconds=s_edit[0]*3600+s_edit[1]*60+s_edit[2];
    /* Locked and invalid cells keep their explicit semantic fill. */
    if(s_edit[0]==24||seconds<ACTIVITY_TIMER_MIN_SECONDS)return;
    if(before->edit_field!=after.edit_field)s_ui_motion=UI_MOTION_FIELD;
    else if(button==BUTTON_ID_UP||button==BUTTON_ID_DOWN) {
      s_ui_motion=UI_MOTION_VALUE;
      s_ui_previous_value=previous_value;
      s_ui_value_direction=button==BUTTON_ID_UP?-1:1;
    }
  } else if(from.y!=to.y) {
    /* Skips and scroll changes settle immediately. Animate only one visible
     * neighbour so a rapid event cannot send the highlight through the page. */
    const int step=after.screen==SCREEN_OPTIONS?TIMER_OPTION_STRIDE:TIMER_CONFIRM_STRIDE;
    if(from.y-to.y>step||to.y-from.y>step)return;
    s_ui_motion=UI_MOTION_MENU;
  }
  if(s_ui_motion==UI_MOTION_NONE)return;
  s_ui_from=from;s_ui_to=to;s_ui_motion_screen=s_screen;
  s_ui_motion_started=prv_millis();prv_ui_motion_tick(NULL);
}

static void prv_motion_tick(void *context) {
  (void)context;s_motion_timer=NULL;
  const uint32_t elapsed=prv_millis()-s_motion_started;
  if (!s_focused || s_screen!=SCREEN_DETAIL || elapsed>=300u) { prv_stop_motion();return; }
  if(s_canvas)layer_mark_dirty(s_canvas);
  s_motion_timer=app_timer_register(30,prv_motion_tick,NULL);
  if (!s_motion_timer) prv_stop_motion();
}

static void prv_transition(void) {
  if(s_native_chrome_timer){app_timer_cancel(s_native_chrome_timer);s_native_chrome_timer=NULL;}
  prv_stop_motion();
  prv_stop_ui_motion();
  prv_stop_icon_motion();
  if (s_window) window_set_background_color(s_window,s_screen==SCREEN_ALARM||s_dark?GColorBlack:GColorWhite);
  prv_refresh();
}

static int prv_glass_pose(void) {
  if(s_selected>=TIMER_COLLECTION_CAPACITY)return 180;
  const ActivityTimerModel *timer=&s_collection.slots[s_selected];
  return timer_hourglass_pose(timer->state==ACTIVITY_TIMER_PAUSED?TIMER_GLASS_PAUSED:
      activity_timer_ready(timer)?TIMER_GLASS_READY:TIMER_GLASS_RUNNING);
}

static void prv_rotate_glass(int from, int to) {
  prv_stop_motion();
  if(!s_focused||!s_canvas||s_screen!=SCREEN_DETAIL||from==to)return;
  s_glass_from=from;s_glass_to=to;s_glass_motion=true;
  s_motion_started=prv_millis();prv_motion_tick(NULL);
}

static void prv_focus(bool focused) {
  s_focused=focused;
  if (!focused) {
    if(s_native_chrome_timer){app_timer_cancel(s_native_chrome_timer);s_native_chrome_timer=NULL;}
    if(s_screen==SCREEN_LIST)prv_list_disappear(s_window);
    prv_stop_motion();prv_stop_ui_motion();prv_stop_icon_motion();
  }
  else { prv_finish();prv_refresh(); }
}

static void prv_will_focus(bool focused) {
  /* Stop decorative work as soon as a covering window starts moving in.
   * Resuming waits for did_focus, once the app is fully visible again. */
  if (!focused) prv_focus(false);
}

static void prv_begin_edit(int slot) {
  s_edit_return_screen=slot<0?SCREEN_LIST:
      s_screen==SCREEN_OPTIONS?SCREEN_OPTIONS:SCREEN_DETAIL;
  s_edit_slot=slot;
  uint32_t seconds=slot<0?600:activity_timer_remaining(&s_collection.slots[slot],prv_now());
  if (slot>=0 && !seconds) seconds=s_collection.slots[slot].repeat_seconds;
  s_edit_initial_seconds=seconds;
  s_edit[0]=seconds/3600;s_edit[1]=(seconds/60)%60;s_edit[2]=seconds%60;
  s_edit_field=seconds>=3600?0:1;
  s_screen=SCREEN_EDITOR;
  prv_refresh();
}

static void prv_alarm_dismiss(bool snooze) {
  const unsigned slot=s_selected;
  /* The button dispatcher already owns a collection-sized transaction frame.
   * Keep this second transaction off the 2 KiB stack. Events are serialized;
   * no callback retains the pointer after prv_apply returns. */
  static TimerCollection next;
  next=s_collection;
  if (snooze ? !timer_collection_snooze(&next,slot,prv_now()) :
      !timer_collection_acknowledge(&next,slot)) return;
  if (!prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<slot))) return;
  if (s_alarm_mask) prv_open_alarm();
  else {
    s_screen=snooze?SCREEN_DETAIL:SCREEN_LIST;
    if (!snooze) s_menu=timer_collection_count(&s_collection)?1:0;
  }
  prv_refresh();
}

static void prv_exit_after_glance(void) {
  if (!s_exit_pending) return;
  s_exit_pending=false;
  /* A real deadline may arrive during the sub-second glance flush. Keep its
   * alarm on screen rather than closing over newly pending native attention. */
  if (s_screen==SCREEN_LIST) window_stack_pop(true);
}

static void prv_press(ButtonId button) {
  if (s_exit_pending) return;
  if (prv_finish() && s_screen==SCREEN_ALARM) return;
  if (s_screen==SCREEN_ALARM) {
    if (button==BUTTON_ID_UP) prv_alarm_dismiss(true);
    else if (button==BUTTON_ID_SELECT || button==BUTTON_ID_DOWN) prv_alarm_dismiss(false);
    else if (button==BUTTON_ID_BACK) {
      /* Back acknowledges like Done. On a storage error it still permits
       * leaving this view; the unsaved pending alarm remains recoverable. */
      prv_alarm_dismiss(false);
      if (s_screen == SCREEN_ALARM && s_storage_error) {
        s_screen = SCREEN_LIST;
        prv_refresh();
      }
    }
    return;
  }
  if (s_screen==SCREEN_LIST) {
    if (button==BUTTON_ID_UP||button==BUTTON_ID_DOWN)prv_list_step(button==BUTTON_ID_UP);
    if (button==BUTTON_ID_SELECT) {
      if (!s_menu) { if (timer_collection_can_create(&s_collection))prv_begin_edit(-1); else vibes_short_pulse(); }
      else {
        const int slot=timer_collection_visible_slot(&s_collection,s_menu-1);
        if (slot>=0) { s_selected=(unsigned)slot;s_screen=SCREEN_DETAIL; }
      }
    }
    if (button==BUTTON_ID_BACK) {
      s_exit_pending=true;
      timer_glance_request_exit(prv_exit_after_glance);
      return;
    }
  } else if (s_screen==SCREEN_EDITOR) {
    if (button==BUTTON_ID_UP || button==BUTTON_ID_DOWN) {
      const unsigned max=s_edit_field?(s_edit[0]==24?0:59):24;
      s_edit[s_edit_field]=(s_edit[s_edit_field]+(button==BUTTON_ID_UP?1:max))%(max+1);
      if (s_edit[0]==24) s_edit[1]=s_edit[2]=0;
    } else if (button==BUTTON_ID_BACK) {
      if (s_edit_field)--s_edit_field;
      else s_screen=s_edit_return_screen;
    } else if (button==BUTTON_ID_SELECT) {
      if (s_edit_field<2)++s_edit_field;
      else {
        const uint32_t seconds=s_edit[0]*3600+s_edit[1]*60+s_edit[2];
        if (seconds<ACTIVITY_TIMER_MIN_SECONDS) { vibes_short_pulse();prv_refresh();return; }
        TimerCollection next=s_collection;
        int slot=s_edit_slot;
        if (slot>=0 && seconds==s_edit_initial_seconds) {
          s_screen=SCREEN_DETAIL;prv_refresh();return;
        }
        if (slot<0)slot=timer_collection_create(&next,seconds,prv_now());
        else if (!timer_collection_edit(&next,(unsigned)slot,seconds,prv_now()))slot=-1;
        if (slot>=0 && prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<slot))) {
          s_selected=(unsigned)slot;s_screen=SCREEN_DETAIL;
        }
      }
    }
  } else if (s_screen==SCREEN_OPTIONS) {
    if(button==BUTTON_ID_UP||button==BUTTON_ID_DOWN)prv_action_step(button==BUTTON_ID_UP);
    else if (button==BUTTON_ID_BACK) s_screen=SCREEN_DETAIL;
    else if (button==BUTTON_ID_SELECT) {
      if (s_option==0) prv_begin_edit((int)s_selected);
      else if (s_option==1) {
        TimerCollection next=s_collection;
        if (timer_collection_reset(&next,s_selected,prv_now()) &&
            (next.revision==s_collection.revision||
             prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<s_selected)))) s_screen=SCREEN_DETAIL;
      } else { s_confirm_choice=0;s_screen=SCREEN_CONFIRM; }
    }
  } else if (s_screen==SCREEN_CONFIRM) {
    if(button==BUTTON_ID_UP||button==BUTTON_ID_DOWN)prv_action_step(button==BUTTON_ID_UP);
    else if (button==BUTTON_ID_BACK || (button==BUTTON_ID_SELECT && !s_confirm_choice))
      s_screen=SCREEN_OPTIONS;
    else if (button==BUTTON_ID_SELECT) {
      TimerCollection next=s_collection;
      if (timer_collection_delete(&next,s_selected,prv_now()) &&
          prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<s_selected))) {
        s_screen=SCREEN_LIST;
        if (s_menu>timer_collection_count(&s_collection)) s_menu=timer_collection_count(&s_collection);
      }
    }
  } else {
    TimerCollection next=s_collection;
    if (button==BUTTON_ID_UP)prv_begin_edit((int)s_selected);
    else if (button==BUTTON_ID_BACK) {
      s_screen=SCREEN_LIST;
      s_menu=timer_ui_menu_for_slot(&s_collection,s_selected);
    }
    else if (button==BUTTON_ID_DOWN) {
      s_option=0;s_screen=SCREEN_OPTIONS;
    } else if (button==BUTTON_ID_SELECT) {
      bool changed=next.slots[s_selected].state==ACTIVITY_TIMER_RUNNING?
          timer_collection_pause(&next,s_selected,prv_now()):next.slots[s_selected].state==ACTIVITY_TIMER_PAUSED?
          timer_collection_resume(&next,s_selected,prv_now()):
          timer_collection_restart(&next,s_selected,prv_now());
      if (changed)prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<s_selected));
    }
  }
  prv_refresh();
}

static void prv_click(ClickRecognizerRef recognizer, void *context) {
  (void)context;
  const ButtonId button=click_recognizer_get_button_id(recognizer);
  const bool repeating=click_recognizer_is_repeating(recognizer);
  if (!repeating) s_repeat_screen=s_screen;
  else if (s_repeat_screen!=s_screen || (s_screen!=SCREEN_LIST &&
      s_screen!=SCREEN_EDITOR && s_screen!=SCREEN_OPTIONS)) return;
  const Screen before=s_screen;
  /* Activation settles the visual selection before consuming its immediate
   * logical target, matching MenuLayer without delaying the action. */
  if(button==BUTTON_ID_SELECT)prv_stop_ui_motion();
  const TimerUiView previous_view=prv_view();
  const uint32_t previous_edit_value=s_edit[s_edit_field];
  const unsigned previous_slot=s_selected;
  const uint32_t previous_run=s_selected<TIMER_COLLECTION_CAPACITY?
      s_collection.slots[s_selected].run:0;
  const uint32_t previous_state=s_selected<TIMER_COLLECTION_CAPACITY?
      s_collection.slots[s_selected].state:0;
  const uint32_t previous_revision=s_selected<TIMER_COLLECTION_CAPACITY?
      s_collection.slots[s_selected].revision:0;
  const int previous_angle=s_glass_motion?
      timer_hourglass_angle(s_glass_from,s_glass_to,
          prv_millis()-s_motion_started,300u):prv_glass_pose();
  prv_press(button);
  if (s_screen!=before) prv_transition();
  else prv_animate_ui(&previous_view,repeating,button,previous_edit_value);
  if(s_screen==SCREEN_DETAIL&&s_selected<TIMER_COLLECTION_CAPACITY) {
    const ActivityTimerModel *timer=&s_collection.slots[s_selected];
    if(before==SCREEN_EDITOR&&
        (previous_slot!=s_selected||timer->revision!=previous_revision))
      prv_rotate_glass(180,prv_glass_pose());
    else if((before==SCREEN_DETAIL||before==SCREEN_OPTIONS)&&
        previous_slot==s_selected&&timer->state==ACTIVITY_TIMER_RUNNING&&
        timer->run!=previous_run)
      prv_rotate_glass(180,0);
    else if((before==SCREEN_DETAIL||before==SCREEN_OPTIONS)&&
        previous_slot==s_selected&&timer->state!=previous_state)
      prv_rotate_glass(previous_angle,prv_glass_pose());
  }
}

static void prv_click_config(void *context) {
  (void)context;
  window_single_repeating_click_subscribe(BUTTON_ID_UP,150,prv_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN,150,prv_click);
  window_single_click_subscribe(BUTTON_ID_SELECT,prv_click);
  window_single_click_subscribe(BUTTON_ID_BACK,prv_click);
  window_raw_click_subscribe(BUTTON_ID_UP,prv_up_press,prv_up_release,NULL);
  window_raw_click_subscribe(BUTTON_ID_SELECT,prv_select_press,prv_select_release,NULL);
  window_raw_click_subscribe(BUTTON_ID_DOWN,prv_down_press,prv_down_release,NULL);
}

static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  (void)tick_time;
  const bool finished=prv_finish();
  timer_alarm_feedback_update(s_alarm_mask);
  const TimerUiView view=prv_view();
  /* A newly subscribed clock or format change can report zero changed units.
   * Compare against the last painted minute so an idle screen remains current
   * without turning its wall-clock header into a per-second repaint. */
  const bool clock_changed=!s_have_drawn||s_drawn_minute!=view.now/60u||!units_changed;
  if (!finished && s_focused && (clock_changed||timer_ui_needs_tick(&view))) prv_refresh();
}

static void prv_wakeup(WakeupId id,int32_t cookie) {
  TIMER_DIAG("wakeup now=%lu id=%ld cookie=%ld",(unsigned long)prv_now(),(long)id,(long)cookie);
  (void)id;(void)cookie;
  prv_finish();prv_schedule_alarm();prv_refresh();
}

static void prv_init(void) {
  WakeupId launch_id=-1;int32_t cookie=0;
  const bool wakeup_launch=wakeup_get_launch_event(&launch_id,&cookie);
  TIMER_DIAG("launch reason=%d wakeup=%d id=%ld cookie=%ld",(int)launch_reason(),wakeup_launch,(long)launch_id,(long)cookie);
  (void)wakeup_launch;(void)launch_id;(void)cookie;
  prv_load();
  s_dark=timer_appearance_load();
  timer_glance_update(&s_collection, prv_now());
  if (persist_exists(WAKEUP_KEY))s_wakeup=persist_read_int(WAKEUP_KEY);
  uint32_t previous_ticket=0;persist_read_data(PUBLICATION_KEY,&previous_ticket,sizeof(previous_ticket));
  pa_publisher_init(&s_publisher,previous_ticket,connection_service_peek_pebble_app_connection());
  timer_native_type_init();
  const PreferredContentSize size=preferred_content_size();
  s_text_size=size<=PreferredContentSizeMedium?0:size==PreferredContentSizeLarge?1:2;
  for(unsigned screen=SCREEN_LIST;screen<=SCREEN_CONFIRM;++screen) {
    Window *window=s_windows[screen]=window_create();
    window_set_background_color(window,screen==SCREEN_ALARM||s_dark?GColorBlack:GColorWhite);
    window_set_click_config_provider(window,prv_click_config);
    Layer *root=window_get_root_layer(window);
    Layer *canvas=s_canvases[screen]=layer_create_with_data(layer_get_bounds(root),sizeof(Screen));
    *(Screen *)layer_get_data(canvas)=(Screen)screen;
    layer_set_update_proc(canvas,prv_draw);layer_add_child(root,canvas);
  }
  s_window=s_windows[SCREEN_LIST];s_list_canvas=s_canvas=s_canvases[SCREEN_LIST];
  s_list=menu_layer_create(GRect(0,50,CONTENT_WIDTH,178));
  menu_layer_set_callbacks(s_list,NULL,(MenuLayerCallbacks){
      .get_num_rows=prv_list_rows,.get_cell_height=prv_list_height,.draw_row=prv_list_draw});
  menu_layer_set_center_focused(s_list,false);menu_layer_pad_bottom_enable(s_list,false);
  layer_add_child(window_get_root_layer(s_window),menu_layer_get_layer(s_list));
  window_set_window_handlers(s_window,(WindowHandlers){.disappear=prv_list_disappear});
  for(unsigned i=0;i<2;++i) {
    const Screen screen=i?SCREEN_CONFIRM:SCREEN_OPTIONS;
    MenuLayer *menu=menu_layer_create(GRect(0,40,CONTENT_WIDTH,188));
    if(i)s_confirm_menu=menu;else s_options_menu=menu;
    menu_layer_set_callbacks(menu,(void *)(uintptr_t)screen,(MenuLayerCallbacks){.get_num_rows=prv_action_rows,
        .get_cell_height=prv_action_height,.get_header_height=prv_context_height,
        .draw_row=prv_action_draw,.draw_header=prv_context_draw});
    menu_layer_set_center_focused(menu,false);menu_layer_pad_bottom_enable(menu,false);
    layer_add_child(window_get_root_layer(s_windows[screen]),menu_layer_get_layer(menu));
  }
  app_message_register_inbox_received(prv_inbox);
  app_message_register_outbox_sent(prv_outbox_sent);
  app_message_register_outbox_failed(prv_outbox_failed);
  app_message_open(512,1024);
  connection_service_subscribe((ConnectionHandlers){.pebble_app_connection_handler=prv_connection});
  wakeup_service_subscribe(prv_wakeup);
  app_focus_service_subscribe_handlers((AppFocusHandlers){
      .will_focus=prv_will_focus,.did_focus=prv_focus});
  s_ready=true;
  prv_finish();
  if (s_alarm_mask)prv_open_alarm();
  else if (timer_collection_count(&s_collection))s_menu=1;
  prv_schedule_alarm();prv_refresh();
  /* Initial foreground launch may precede focus subscription. Later cover/
   * reveal callbacks keep this in sync without requiring a launch callback. */
  s_focused=true;
  window_stack_push(s_window,true);
  prv_sync_windows();
}

static void prv_deinit(void) {
  TIMER_DIAG("exit alarm=%ld deadline=%lu dirty=%u",(long)s_wakeup,(unsigned long)s_alarm_deadline,s_dirty);
  s_ready=false;prv_stop_motion();prv_stop_ui_motion();prv_stop_icon_motion();
  if(s_native_chrome_timer)app_timer_cancel(s_native_chrome_timer);
  timer_alarm_feedback_deinit();
  timer_glance_deinit();
  if (s_retry_timer)app_timer_cancel(s_retry_timer);
  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();connection_service_unsubscribe();app_message_deregister_callbacks();
  menu_layer_destroy(s_list);
  if(s_options_menu)menu_layer_destroy(s_options_menu);
  if(s_confirm_menu)menu_layer_destroy(s_confirm_menu);
  for(unsigned screen=0;screen<=SCREEN_CONFIRM;++screen) {
    layer_destroy(s_canvases[screen]);window_destroy(s_windows[screen]);
  }
}

int main(void) { prv_init();app_event_loop();prv_deinit(); }
