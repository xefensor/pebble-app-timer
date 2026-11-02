#include <pebble.h>
#include <pebble-activities/pebble-activities.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "timer_collection.h"

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

typedef enum { SCREEN_LIST, SCREEN_EDITOR, SCREEN_DETAIL, SCREEN_ALARM } Screen;
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
static unsigned s_menu, s_selected;
static int s_edit_slot = -1;
static uint32_t s_edit[3] = {0, 5, 0};
static unsigned s_edit_field;
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

static void prv_flush(void);
static void prv_refresh(void);
static void prv_schedule_alarm(void);
static bool prv_load_bank(unsigned bank, TimerCollection *collection, SavedHeader *header);

static uint32_t prv_now(void) { return (uint32_t)time(NULL); }
static GColor prv_accent(void) { return PBL_IF_COLOR_ELSE(GColorPictonBlue, GColorBlack); }

static uint32_t prv_checksum(const TimerCollection *collection, uint32_t alarms) {
  const uint8_t *bytes = (const uint8_t *)collection;
  uint32_t value = UINT32_C(2166136261) ^ alarms;
  for (size_t i = 0; i < sizeof(*collection); ++i) value = (value ^ bytes[i]) * UINT32_C(16777619);
  return value;
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
  TimerCollection verified;
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
      header->magic != TIMER_COLLECTION_MAGIC || header->version != TIMER_COLLECTION_VERSION ||
      header->bytes != sizeof(*collection) || !header->generation || header->alarms > 255) return false;
  uint8_t *bytes = (uint8_t *)collection;
  for (size_t offset = 0, chunk = 0; offset < sizeof(*collection); offset += CHUNK_BYTES, ++chunk) {
    const size_t count = sizeof(*collection) - offset < CHUNK_BYTES ? sizeof(*collection) - offset : CHUNK_BYTES;
    if (persist_read_data(BANK_DATA_KEY + bank * BANK_STRIDE + chunk,
        bytes + offset, count) != (int)count) return false;
  }
  if (!timer_collection_valid(collection) || header->checksum != prv_checksum(collection, header->alarms)) return false;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    if ((header->alarms & (1u << i)) && collection->slots[i].state != ACTIVITY_TIMER_FINISHED) return false;
  return true;
}

static void prv_load(void) {
  TimerCollection candidate;
  SavedHeader header;
  for (unsigned bank = 0; bank < 2; ++bank) {
    if (prv_load_bank(bank, &candidate, &header) && header.generation > s_generation) {
      s_collection = candidate;
      s_generation = header.generation;
      s_bank = bank;
      s_alarm_mask = (uint8_t)header.alarms;
    }
  }
  if (s_generation) {
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
  if (persist_read_data(LEGACY_MODEL_KEY, &legacy, sizeof(legacy)) == (int)sizeof(legacy) &&
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
  if (!(timer->configured_seconds % 3600))
    snprintf(text, size, "%lu h timer", (unsigned long)(timer->configured_seconds / 3600));
  else if (!(timer->configured_seconds % 60))
    snprintf(text, size, "%lu min timer", (unsigned long)(timer->configured_seconds / 60));
  else {
    char duration[16];
    prv_format(timer->configured_seconds, duration, sizeof(duration));
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
  if (slot < TIMER_COLLECTION_CAPACITY) { s_selected = slot; s_screen = SCREEN_ALARM; }
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
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    if (s_accepted[i] != next->slots[i].revision) s_dirty |= (uint8_t)(1u << i);
  s_collection = *next;
  s_alarm_mask = alarms;
  s_retries = 0;
  s_share_error = false;
  prv_schedule_alarm();
  prv_flush();
  prv_refresh();
  return true;
}

static bool prv_finish_at(uint32_t now) {
  TimerCollection next = s_collection;
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
  prv_refresh();
}

static int prv_pending_slot(void) {
  int chosen = -1;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    if ((s_dirty & (1u << i)) && (chosen < 0 || s_collection.slots[i].revision < s_collection.slots[chosen].revision)) chosen = (int)i;
  return chosen;
}

static void prv_flush(void) {
  if (s_storage_error || !s_phone_ready || s_transport_busy || !connection_service_peek_pebble_app_connection()) return;
  if (s_retries >= RETRY_LIMIT) { s_share_error = true; prv_refresh(); return; }
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
  prv_refresh();
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
  prv_refresh();
  prv_flush();
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
  prv_refresh();
}

static void prv_connection(bool connected) {
  pa_publisher_set_connected(&s_publisher, connected);
  if (connected) {
    s_retries = 0;
    if (s_have_flight) pa_publisher_retry(&s_publisher);
    prv_flush();
  }
  prv_refresh();
}

static void prv_text(GContext *ctx, const char *text, const char *font, GRect box, GColor color, GTextAlignment align) {
  graphics_context_set_text_color(ctx, color);
  graphics_draw_text(ctx, text, fonts_get_system_font(font), box, GTextOverflowModeTrailingEllipsis, align, NULL);
}

static void prv_line(GContext *ctx, int x1, int y1, int x2, int y2) {
  graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x2, y2));
}

static void prv_icon(GContext *ctx, int which, int x, int y) {
  graphics_context_set_stroke_color(ctx, GColorWhite);
  graphics_context_set_fill_color(ctx, GColorWhite);
  graphics_context_set_stroke_width(ctx, 2);
  if (which == 0) { /* edit */
    prv_line(ctx, x-6, y+6, x+5, y-5); prv_line(ctx, x-4, y+8, x+7, y-3);
    prv_line(ctx, x-6, y+6, x-7, y+9); prv_line(ctx, x-7, y+9, x-4, y+8);
    prv_line(ctx, x+4, y-6, x+8, y-2);
  } else if (which == 1) { /* pause */
    graphics_fill_rect(ctx, GRect(x-6,y-7,4,14), 0, GCornerNone);
    graphics_fill_rect(ctx, GRect(x+2,y-7,4,14), 0, GCornerNone);
  } else if (which == 2) { /* play */
    for (int row = -7; row <= 7; ++row) prv_line(ctx, x-4,y+row,x+7-abs(row)*11/7,y+row);
  } else if (which == 3) { /* trash */
    graphics_draw_rect(ctx, GRect(x-6,y-4,12,12)); prv_line(ctx,x-8,y-7,x+8,y-7);
    prv_line(ctx,x-3,y-9,x+3,y-9); prv_line(ctx,x-2,y-1,x-2,y+5); prv_line(ctx,x+2,y-1,x+2,y+5);
  } else if (which == 4) { /* plus */
    prv_line(ctx,x-7,y,x+7,y); prv_line(ctx,x,y-7,x,y+7);
  } else if (which == 5) { /* minus */
    prv_line(ctx,x-7,y,x+7,y);
  } else if (which == 6) { /* advance */
    prv_line(ctx,x-7,y,x+7,y); prv_line(ctx,x+2,y-5,x+7,y); prv_line(ctx,x+2,y+5,x+7,y);
  } else if (which == 7) { /* snooze */
    graphics_draw_circle(ctx,GPoint(x,y),7); prv_line(ctx,x-7,y-7,x-7,y-2); prv_line(ctx,x-7,y-2,x-2,y-2);
  } else if (which == 8) { /* dismiss */
    prv_line(ctx,x-7,y,x-2,y+6); prv_line(ctx,x-2,y+6,x+8,y-6);
  }
  graphics_context_set_stroke_width(ctx, 1);
}

static void prv_bar(GContext *ctx, int up, int select, int down) {
  graphics_context_set_fill_color(ctx, GColorBlack);
  graphics_fill_rect(ctx, GRect(CONTENT_WIDTH,0,BAR_WIDTH,228),0,GCornerNone);
  if (up >= 0) prv_icon(ctx,up,183,44);
  if (select >= 0) prv_icon(ctx,select,183,107);
  if (down >= 0) prv_icon(ctx,down,183,170);
}

static const char *prv_status(void) {
  if (s_storage_error) return "Storage failed; keep open";
  if (s_alarm_error) return "Alarm not set; keep open";
  if (!connection_service_peek_pebble_app_connection()) return "Phone disconnected";
  if (s_share_error) return "Sharing unavailable";
  if (!s_phone_ready) return "Waiting to share";
  if (s_screen != SCREEN_LIST && s_accepted[s_selected] == s_collection.slots[s_selected].revision) return "Shared with phone";
  return s_dirty ? "Waiting to share" : "Shared with phone";
}

static void prv_draw_list(GContext *ctx) {
  const unsigned count = timer_collection_count(&s_collection);
  if (s_menu > count) s_menu = count;
  const bool plus = s_menu == 0;
  graphics_context_set_fill_color(ctx, plus ? GColorBlack : GColorWhite);
  graphics_fill_rect(ctx,GRect(0,0,200,40),0,GCornerNone);
  graphics_context_set_stroke_color(ctx,plus?GColorWhite:GColorBlack);
  graphics_context_set_stroke_width(ctx,2);
  if (count < TIMER_COLLECTION_CAPACITY) {
    prv_line(ctx,93,20,107,20); prv_line(ctx,100,13,100,27);
  } else prv_text(ctx,"8 timers · full",FONT_KEY_GOTHIC_18_BOLD,GRect(5,7,190,29),
      plus?GColorWhite:GColorBlack,GTextAlignmentCenter);
  graphics_context_set_stroke_width(ctx,1);
  graphics_context_set_stroke_color(ctx,GColorBlack);
  prv_line(ctx,0,39,200,39);
  if (!count) {
    prv_text(ctx,"New timer",FONT_KEY_GOTHIC_24_BOLD,GRect(10,91,180,38),GColorBlack,GTextAlignmentCenter);
  } else {
    const unsigned first = s_menu > 2 ? s_menu - 2 : 0;
    for (unsigned row = first; row < count && row < first + 2; ++row) {
      const int slot = timer_collection_visible_slot(&s_collection,row);
      const ActivityTimerModel *timer = &s_collection.slots[slot];
      const int y = 40 + (int)(row-first)*74;
      const bool selected = s_menu == row+1;
      graphics_context_set_fill_color(ctx,selected?GColorBlack:GColorWhite);
      graphics_fill_rect(ctx,GRect(0,y,200,74),0,GCornerNone);
      const GColor ink = selected?GColorWhite:GColorBlack;
      char remaining[20], name[32], detail[52];
      prv_format(activity_timer_remaining(timer,prv_now()),remaining,sizeof(remaining));
      prv_name(timer,name,sizeof(name));
      snprintf(detail,sizeof(detail),"%s · %s",timer->state==ACTIVITY_TIMER_RUNNING?"Running":
          timer->state==ACTIVITY_TIMER_PAUSED?"Paused":"Finished",name);
      prv_text(ctx,remaining,FONT_KEY_BITHAM_30_BLACK,GRect(10,y+2,180,39),ink,GTextAlignmentLeft);
      prv_text(ctx,detail,FONT_KEY_GOTHIC_18,GRect(10,y+40,180,27),ink,GTextAlignmentLeft);
      graphics_context_set_stroke_color(ctx,GColorLightGray); prv_line(ctx,0,y+73,200,y+73);
    }
    if (count > 2) {
      char position[32];
      snprintf(position,sizeof(position),"%u / %u",s_menu?s_menu:1,count);
      prv_text(ctx,position,FONT_KEY_GOTHIC_14,GRect(10,190,180,19),GColorDarkGray,GTextAlignmentCenter);
    }
  }
  prv_text(ctx,prv_status(),FONT_KEY_GOTHIC_14,GRect(4,209,192,18),GColorDarkGray,GTextAlignmentCenter);
}

static void prv_draw(Layer *layer, GContext *ctx) {
  (void)layer;
  graphics_context_set_fill_color(ctx,GColorWhite); graphics_fill_rect(ctx,GRect(0,0,200,228),0,GCornerNone);
  if (s_screen == SCREEN_LIST) { prv_draw_list(ctx); return; }
  const ActivityTimerModel *timer = &s_collection.slots[s_selected];
  if (s_screen == SCREEN_EDITOR) {
    prv_bar(ctx,4,s_edit_field<2?6:2,5);
    prv_text(ctx,s_edit_slot<0?"New timer":"Edit timer",FONT_KEY_GOTHIC_24_BOLD,
        GRect(6,10,154,33),GColorBlack,GTextAlignmentCenter);
    const char *units[] = {"HOURS","MIN","SEC"};
    for (unsigned i = 0; i < 3; ++i) {
      char number[4]; snprintf(number,sizeof(number),"%02lu",(unsigned long)s_edit[i]);
      const int x = 8+(int)i*51;
      graphics_context_set_fill_color(ctx,i==s_edit_field?GColorBlack:GColorWhite);
      graphics_fill_rect(ctx,GRect(x,83,47,41),0,GCornerNone);
      prv_text(ctx,number,FONT_KEY_BITHAM_30_BLACK,GRect(x,81,47,43),
          i==s_edit_field?GColorWhite:GColorBlack,GTextAlignmentCenter);
      prv_text(ctx,units[i],FONT_KEY_GOTHIC_14_BOLD,GRect(x,128,47,20),GColorDarkGray,GTextAlignmentCenter);
    }
    const uint32_t seconds=s_edit[0]*3600+s_edit[1]*60+s_edit[2];
    if (seconds > 900) {
      time_t end=(time_t)(prv_now()+seconds);struct tm *clock=localtime(&end);
      char text[24];strftime(text,sizeof(text),"Ends %H:%M",clock);
      prv_text(ctx,text,FONT_KEY_GOTHIC_18,GRect(6,159,154,25),GColorDarkGray,GTextAlignmentCenter);
    } else if (seconds < ACTIVITY_TIMER_MIN_SECONDS)
      prv_text(ctx,"Minimum 5 seconds",FONT_KEY_GOTHIC_18,GRect(6,166,154,27),GColorDarkGray,GTextAlignmentCenter);
    if (s_edit[0] == 24)
      prv_text(ctx,"Maximum 24 hours",FONT_KEY_GOTHIC_18,GRect(6,184,154,24),GColorDarkGray,GTextAlignmentCenter);
  } else if (s_screen == SCREEN_ALARM) {
    prv_bar(ctx,7,-1,8);
    graphics_context_set_stroke_color(ctx,GColorBlack);graphics_context_set_stroke_width(ctx,3);
    graphics_draw_circle(ctx,GPoint(83,51),20);prv_line(ctx,83,51,83,37);prv_line(ctx,83,51,94,57);
    prv_line(ctx,65,30,59,36);prv_line(ctx,101,30,107,36);graphics_context_set_stroke_width(ctx,1);
    prv_text(ctx,"Time’s Up!",FONT_KEY_GOTHIC_28_BOLD,GRect(4,87,158,42),GColorBlack,GTextAlignmentCenter);
    char duration[20];prv_format(timer->configured_seconds,duration,sizeof(duration));
    prv_text(ctx,duration,FONT_KEY_BITHAM_34_MEDIUM_NUMBERS,GRect(4,136,158,46),GColorBlack,GTextAlignmentCenter);
    prv_text(ctx,"Timer complete",FONT_KEY_GOTHIC_18,GRect(4,183,158,25),GColorDarkGray,GTextAlignmentCenter);
  } else {
    const bool running=timer->state==ACTIVITY_TIMER_RUNNING;
    prv_bar(ctx,0,running?1:2,3);
    char name[32],remaining[20];prv_name(timer,name,sizeof(name));
    const uint32_t now=prv_now();
    const uint32_t seconds=activity_timer_remaining(timer,now);prv_format(seconds,remaining,sizeof(remaining));
    prv_text(ctx,name,FONT_KEY_GOTHIC_24_BOLD,GRect(5,7,156,37),GColorBlack,GTextAlignmentCenter);
    prv_text(ctx,"Remaining",FONT_KEY_GOTHIC_14,GRect(5,48,156,19),GColorDarkGray,GTextAlignmentCenter);
    prv_text(ctx,remaining,seconds>=3600?FONT_KEY_BITHAM_34_MEDIUM_NUMBERS:FONT_KEY_BITHAM_42_BOLD,
        GRect(3,69,160,59),GColorBlack,GTextAlignmentCenter);
    prv_text(ctx,running?"Running":timer->state==ACTIVITY_TIMER_PAUSED?"Paused":"Finished",
        FONT_KEY_GOTHIC_18_BOLD,GRect(5,125,156,28),GColorDarkGray,GTextAlignmentCenter);
    char elapsed[20],support[36];
    prv_format(activity_timer_elapsed(timer,now),elapsed,sizeof(elapsed));
    snprintf(support,sizeof(support),"Elapsed %s",elapsed);
    prv_text(ctx,support,FONT_KEY_GOTHIC_18,GRect(5,154,156,26),GColorBlack,GTextAlignmentCenter);
    unsigned lit=timer->configured_seconds?(seconds*10+timer->configured_seconds/2)/timer->configured_seconds:0;
    if (lit>10)lit=10;
    for (unsigned i=0;i<10;++i) {
      graphics_context_set_fill_color(ctx,i<lit?prv_accent():GColorLightGray);
      graphics_fill_circle(ctx,GPoint(24+(int)(i%5)*29,186+(int)(i/5)*15),6);
    }
  }
  if (s_screen != SCREEN_EDITOR || s_storage_error || s_alarm_error)
    prv_text(ctx,prv_status(),FONT_KEY_GOTHIC_14,GRect(3,209,160,18),GColorDarkGray,GTextAlignmentCenter);
}

static void prv_refresh(void) { if (s_canvas) layer_mark_dirty(s_canvas); }

static void prv_begin_edit(int slot) {
  s_edit_slot=slot;
  uint32_t seconds=slot<0?300:activity_timer_remaining(&s_collection.slots[slot],prv_now());
  if (slot>=0 && !seconds) seconds=s_collection.slots[slot].configured_seconds;
  s_edit[0]=seconds/3600;s_edit[1]=(seconds/60)%60;s_edit[2]=seconds%60;
  s_edit_field=0;
  s_screen=SCREEN_EDITOR;
  prv_refresh();
}

static void prv_alarm_dismiss(bool snooze) {
  const unsigned slot=s_selected;
  TimerCollection next=s_collection;
  if (snooze && !timer_collection_snooze(&next,slot,prv_now())) return;
  if (!prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<slot))) return;
  if (s_alarm_mask) prv_open_alarm();
  else s_screen=snooze?SCREEN_DETAIL:SCREEN_LIST;
  prv_refresh();
}

static void prv_press(ButtonId button) {
  if (prv_finish() && s_screen==SCREEN_ALARM) return;
  if (s_screen==SCREEN_ALARM) {
    if (button==BUTTON_ID_UP) prv_alarm_dismiss(true);
    else if (button==BUTTON_ID_DOWN) prv_alarm_dismiss(false);
    else if (button==BUTTON_ID_BACK) {
      /* Navigation never depends on saving a dismissal. An undismissed alarm
       * remains recorded and is shown on the next launch. */
      s_screen = SCREEN_LIST;
      prv_refresh();
    }
    return;
  }
  if (s_screen==SCREEN_LIST) {
    const unsigned count=timer_collection_count(&s_collection);
    if (button==BUTTON_ID_UP && s_menu) --s_menu;
    if (button==BUTTON_ID_DOWN && s_menu<count) ++s_menu;
    if (button==BUTTON_ID_SELECT) {
      if (!s_menu) { if (count<TIMER_COLLECTION_CAPACITY)prv_begin_edit(-1); else vibes_short_pulse(); }
      else {
        const int slot=timer_collection_visible_slot(&s_collection,s_menu-1);
        if (slot>=0) { s_selected=(unsigned)slot;s_screen=SCREEN_DETAIL; }
      }
    }
    if (button==BUTTON_ID_BACK) { window_stack_pop(true); return; }
  } else if (s_screen==SCREEN_EDITOR) {
    if (button==BUTTON_ID_UP || button==BUTTON_ID_DOWN) {
      const unsigned max=s_edit_field?(s_edit[0]==24?0:59):24;
      s_edit[s_edit_field]=(s_edit[s_edit_field]+(button==BUTTON_ID_UP?1:max))%(max+1);
      if (s_edit[0]==24) s_edit[1]=s_edit[2]=0;
    } else if (button==BUTTON_ID_BACK) {
      if (s_edit_field)--s_edit_field;
      else s_screen=s_edit_slot<0?SCREEN_LIST:SCREEN_DETAIL;
    } else if (button==BUTTON_ID_SELECT) {
      if (s_edit_field<2)++s_edit_field;
      else {
        const uint32_t seconds=s_edit[0]*3600+s_edit[1]*60+s_edit[2];
        if (seconds<ACTIVITY_TIMER_MIN_SECONDS) { vibes_short_pulse();prv_refresh();return; }
        TimerCollection next=s_collection;
        int slot=s_edit_slot;
        if (slot<0)slot=timer_collection_create(&next,seconds,prv_now());
        else if (!timer_collection_edit(&next,(unsigned)slot,seconds,prv_now()))slot=-1;
        if (slot>=0 && prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<slot))) {
          s_selected=(unsigned)slot;s_screen=SCREEN_DETAIL;
        }
      }
    }
  } else {
    TimerCollection next=s_collection;
    if (button==BUTTON_ID_UP)prv_begin_edit((int)s_selected);
    else if (button==BUTTON_ID_BACK)s_screen=SCREEN_LIST;
    else if (button==BUTTON_ID_DOWN) {
      if (timer_collection_delete(&next,s_selected,prv_now()) &&
          prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<s_selected))) {
        s_screen=SCREEN_LIST;
        if (s_menu>timer_collection_count(&s_collection))s_menu=timer_collection_count(&s_collection);
      }
    } else if (button==BUTTON_ID_SELECT) {
      bool changed=next.slots[s_selected].state==ACTIVITY_TIMER_RUNNING?
          timer_collection_pause(&next,s_selected,prv_now()):next.slots[s_selected].state==ACTIVITY_TIMER_PAUSED?
          timer_collection_resume(&next,s_selected,prv_now()):
          timer_collection_edit(&next,s_selected,next.slots[s_selected].configured_seconds,prv_now());
      if (changed)prv_apply(&next,s_alarm_mask & (uint8_t)~(1u<<s_selected));
    }
  }
  prv_refresh();
}

static void prv_click(ClickRecognizerRef recognizer, void *context) {
  (void)context;
  prv_press(click_recognizer_get_button_id(recognizer));
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
  prv_finish();prv_refresh();
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
  if (persist_exists(WAKEUP_KEY))s_wakeup=persist_read_int(WAKEUP_KEY);
  uint32_t previous_ticket=0;persist_read_data(PUBLICATION_KEY,&previous_ticket,sizeof(previous_ticket));
  pa_publisher_init(&s_publisher,previous_ticket,connection_service_peek_pebble_app_connection());
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
  tick_timer_service_subscribe(SECOND_UNIT,prv_tick);
  prv_finish();
  if (s_alarm_mask)prv_open_alarm();
  else if (timer_collection_count(&s_collection))s_menu=1;
  prv_schedule_alarm();prv_refresh();
  window_stack_push(s_window,true);
}

static void prv_deinit(void) {
  TIMER_DIAG("exit alarm=%ld deadline=%lu dirty=%u",(long)s_wakeup,(unsigned long)s_alarm_deadline,s_dirty);
  if (s_retry_timer)app_timer_cancel(s_retry_timer);
  tick_timer_service_unsubscribe();connection_service_unsubscribe();app_message_deregister_callbacks();
  layer_destroy(s_canvas);window_destroy(s_window);
}

int main(void) { prv_init();app_event_loop();prv_deinit(); }
