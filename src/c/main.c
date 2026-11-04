#include <pebble.h>
#include <pebble-activities/pebble-activities.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "timer_collection.h"
#include "timer_ui.h"
#include "timer_fonts.h"
#include "timer_glance.h"
#include "timer_hourglass.h"

#define LEGACY_MODEL_KEY 2100
#define WAKEUP_KEY 2101
#define PUBLICATION_KEY 2102
#define MIGRATION_KEY 2103
#define BANK_HEADER_KEY 2120
#define BANK_DATA_KEY 2130
#define BANK_STRIDE 8
#define CHUNK_BYTES 200u
#define RETRY_LIMIT 3
#define BAR_WIDTH 34
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
static bool s_focused, s_ready, s_ticks_active;
static bool s_exit_pending;
static AppTimer *s_motion_timer;
static uint32_t s_motion_started;
static bool s_glass_motion;
static int s_glass_from, s_glass_to;
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

static void prv_flush(void);
static void prv_refresh(void);
static void prv_refresh_sharing(void);
static void prv_schedule_alarm(void);
static void prv_tick(struct tm *tick_time, TimeUnits units_changed);
static void prv_transition(void);
static uint32_t prv_millis(void);
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
    snprintf(text, size, "%lu h timer", (unsigned long)(timer->repeat_seconds / 3600));
  else if (!(timer->repeat_seconds % 60))
    snprintf(text, size, "%lu min timer", (unsigned long)(timer->repeat_seconds / 60));
  else {
    char duration[16];
    prv_format(timer->repeat_seconds, duration, sizeof(duration));
    snprintf(text, size, "%s timer", duration);
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
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    const ActivityTimerModel *timer = &s_collection.slots[i];
    if (timer_collection_visible(&s_collection, i) && timer->state == ACTIVITY_TIMER_RUNNING &&
        timer->deadline > now && (!deadline || timer->deadline < deadline)) deadline = timer->deadline;
  }
  time_t existing = 0;
  if (deadline && s_wakeup >= 0 && wakeup_query(s_wakeup, &existing) && (uint32_t)existing == deadline) {
    s_alarm_deadline = deadline;
    s_alarm_error = false;
    return;
  }
  wakeup_cancel_all();
  s_wakeup = -1;
  s_alarm_deadline = 0;
  s_alarm_error = false;
  if (deadline) {
    s_wakeup = wakeup_schedule((time_t)deadline, (int32_t)s_collection.revision, true);
    s_alarm_error = s_wakeup < 0 || !wakeup_query(s_wakeup, &existing) || (uint32_t)existing != deadline;
    if (!s_alarm_error) s_alarm_deadline = deadline;
  }
  if (s_wakeup >= 0) persist_write_int(WAKEUP_KEY, s_wakeup);
  else persist_delete(WAKEUP_KEY);
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
  if (focused>=0) {
    s_menu=timer_collection_count(&s_collection)?1:0;
    for (unsigned row=0;row<timer_collection_count(&s_collection);++row)
      if (timer_collection_visible_slot(&s_collection,row)==focused) {
        s_menu=row+1;break;
      }
  }
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
  if (!quiet_time_is_active()) vibes_double_pulse();
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
  uint32_t value;
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
  pa_publisher_set_connected(&s_publisher, connected);
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
  return (TimerUiView){.screen=s_screen,.collection=&s_collection,
      .menu=s_menu,.selected=s_selected,.edit_field=s_edit_field,
      .option=s_option,.confirm_choice=s_confirm_choice,.edit=s_edit,
      .editing_new=s_edit_slot<0,.now=prv_now(),
      .editing_ready=s_edit_slot>=0&&
          s_collection.slots[s_edit_slot].state==ACTIVITY_TIMER_FINISHED&&
          s_collection.slots[s_edit_slot].acknowledged,
      .issue=s_storage_error?"Changes not saved":
          s_alarm_error?"Alarm not set":NULL,
      .status=prv_status(),
      .hourglass_transition=s_glass_motion&&s_screen==SCREEN_DETAIL,
      .hourglass_angle=(int16_t)(s_glass_motion?
          timer_hourglass_angle(s_glass_from,s_glass_to,
              prv_millis()-s_motion_started,300u):0)};
}

static void prv_draw(Layer *layer, GContext *ctx) {
  (void)layer;
  const TimerUiView view = prv_view();
  timer_ui_draw(ctx, &view);
  s_drawn_screen = view.screen;
  s_drawn_issue = view.issue;
  s_drawn_status = view.status;
  s_have_drawn = true;
}

static void prv_update_ticks(void) {
  if (!s_ready) return;
  bool needed = false;
  for (unsigned i=0;i<TIMER_COLLECTION_CAPACITY;++i)
    if (timer_collection_visible(&s_collection,i) &&
        s_collection.slots[i].state==ACTIVITY_TIMER_RUNNING) needed=true;
  if (needed==s_ticks_active) return;
  s_ticks_active=needed;
  if (needed) tick_timer_service_subscribe(SECOND_UNIT,prv_tick);
  else tick_timer_service_unsubscribe();
}

static void prv_refresh(void) {
  if (s_screen==SCREEN_LIST && !s_menu && timer_collection_count(&s_collection) &&
      !timer_collection_can_create(&s_collection)) s_menu=1;
  prv_update_ticks();
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

static void prv_motion_tick(void *context) {
  (void)context;s_motion_timer=NULL;
  const uint32_t elapsed=prv_millis()-s_motion_started;
  if (!s_focused || s_screen!=SCREEN_DETAIL || elapsed>=300u) { prv_stop_motion();return; }
  if(s_canvas)layer_mark_dirty(s_canvas);
  s_motion_timer=app_timer_register(30,prv_motion_tick,NULL);
  if (!s_motion_timer) prv_stop_motion();
}

static void prv_transition(void) {
  prv_stop_motion();
  if (s_window) window_set_background_color(s_window,s_screen==SCREEN_ALARM?GColorBlack:GColorWhite);
  prv_refresh();
}

static int prv_glass_pose(void) {
  if(s_selected>=TIMER_COLLECTION_CAPACITY)return 180;
  const ActivityTimerModel *timer=&s_collection.slots[s_selected];
  return timer_hourglass_pose(timer->state==ACTIVITY_TIMER_PAUSED?TIMER_GLASS_PAUSED:
      timer->state==ACTIVITY_TIMER_FINISHED&&timer->acknowledged?TIMER_GLASS_READY:TIMER_GLASS_RUNNING);
}

static void prv_rotate_glass(int from, int to) {
  prv_stop_motion();
  if(!s_focused||!s_canvas||s_screen!=SCREEN_DETAIL||from==to)return;
  s_glass_from=from;s_glass_to=to;s_glass_motion=true;
  s_motion_started=prv_millis();prv_motion_tick(NULL);
}

static void prv_focus(bool focused) {
  s_focused=focused;
  if (!focused) prv_stop_motion();
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
    const unsigned count=timer_collection_count(&s_collection);
    const unsigned first_menu=timer_collection_can_create(&s_collection)?0:count?1:0;
    if (button==BUTTON_ID_UP && s_menu>first_menu) --s_menu;
    if (button==BUTTON_ID_DOWN && s_menu<count) ++s_menu;
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
    if (button==BUTTON_ID_UP && s_option) --s_option;
    else if (button==BUTTON_ID_DOWN && s_option<2) ++s_option;
    else if (button==BUTTON_ID_BACK) s_screen=SCREEN_DETAIL;
    else if (button==BUTTON_ID_SELECT) {
      if (s_option==0) prv_begin_edit((int)s_selected);
      else if (s_option==1) {
        TimerCollection next=s_collection;
        if (timer_collection_restart(&next,s_selected,prv_now()) &&
            prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<s_selected))) s_screen=SCREEN_DETAIL;
      } else { s_confirm_choice=0;s_screen=SCREEN_CONFIRM; }
    }
  } else if (s_screen==SCREEN_CONFIRM) {
    if (button==BUTTON_ID_UP) s_confirm_choice=0;
    else if (button==BUTTON_ID_DOWN) s_confirm_choice=1;
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
      for (unsigned row=0;row<timer_collection_count(&s_collection);++row)
        if (timer_collection_visible_slot(&s_collection,row)==(int)s_selected) {
          s_menu=row+1;break;
        }
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
  if (!click_recognizer_is_repeating(recognizer)) s_repeat_screen=s_screen;
  else if (s_repeat_screen!=s_screen || (s_screen!=SCREEN_LIST &&
      s_screen!=SCREEN_EDITOR && s_screen!=SCREEN_OPTIONS)) return;
  const Screen before=s_screen;
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
  if(s_screen==SCREEN_DETAIL&&s_selected<TIMER_COLLECTION_CAPACITY) {
    const ActivityTimerModel *timer=&s_collection.slots[s_selected];
    if(before==SCREEN_EDITOR&&
        (previous_slot!=s_selected||timer->revision!=previous_revision))
      prv_rotate_glass(180,prv_glass_pose());
    else if((before==SCREEN_DETAIL||before==SCREEN_OPTIONS)&&
        previous_slot==s_selected&&timer->state==ACTIVITY_TIMER_RUNNING&&
        timer->run!=previous_run)
      prv_rotate_glass(180,0);
    else if(before==SCREEN_DETAIL&&previous_slot==s_selected&&timer->state!=previous_state)
      prv_rotate_glass(previous_angle,prv_glass_pose());
  }
}

static void prv_click_config(void *context) {
  (void)context;
  window_single_repeating_click_subscribe(BUTTON_ID_UP,150,prv_click);
  window_single_repeating_click_subscribe(BUTTON_ID_DOWN,150,prv_click);
  window_single_click_subscribe(BUTTON_ID_SELECT,prv_click);
  window_single_click_subscribe(BUTTON_ID_BACK,prv_click);
}

static void prv_tick(struct tm *tick_time, TimeUnits units_changed) {
  (void)tick_time;(void)units_changed;
  const bool finished=prv_finish();
  const TimerUiView view=prv_view();
  if (!finished && s_focused && timer_ui_needs_tick(&view)) prv_refresh();
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
  timer_glance_update(&s_collection, prv_now());
  if (persist_exists(WAKEUP_KEY))s_wakeup=persist_read_int(WAKEUP_KEY);
  uint32_t previous_ticket=0;persist_read_data(PUBLICATION_KEY,&previous_ticket,sizeof(previous_ticket));
  pa_publisher_init(&s_publisher,previous_ticket,connection_service_peek_pebble_app_connection());
  timer_fonts_init();
  s_window=window_create();
  window_set_background_color(s_window,GColorWhite);
  window_set_click_config_provider(s_window,prv_click_config);
  Layer *root=window_get_root_layer(s_window);
  s_canvas=layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas,prv_draw);layer_add_child(root,s_canvas);
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
}

static void prv_deinit(void) {
  TIMER_DIAG("exit alarm=%ld deadline=%lu dirty=%u",(long)s_wakeup,(unsigned long)s_alarm_deadline,s_dirty);
  s_ready=false;prv_stop_motion();
  timer_glance_deinit();
  if (s_retry_timer)app_timer_cancel(s_retry_timer);
  app_focus_service_unsubscribe();
  tick_timer_service_unsubscribe();connection_service_unsubscribe();app_message_deregister_callbacks();
  layer_destroy(s_canvas);window_destroy(s_window);
  timer_fonts_deinit();
}

int main(void) { prv_init();app_event_loop();prv_deinit(); }
