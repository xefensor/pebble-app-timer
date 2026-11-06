#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

typedef struct AppTimer AppTimer;
typedef void (*AppTimerCallback)(void *);
AppTimer *app_timer_register(uint32_t timeout_ms, AppTimerCallback callback, void *data);
bool app_timer_cancel(AppTimer *timer);
bool quiet_time_is_active(void);
void vibes_double_pulse(void);
void vibes_cancel(void);
uint16_t time_ms(time_t *seconds, uint16_t *milliseconds);
bool persist_exists(uint32_t key);
int32_t persist_read_int(uint32_t key);
int persist_write_int(uint32_t key, int32_t value);
int persist_get_size(uint32_t key);
int persist_read_data(uint32_t key, void *data, size_t size);
int persist_write_data(uint32_t key, const void *data, size_t size);

typedef uint32_t PublishedId;
typedef struct AppGlanceReloadSession { unsigned unused; } AppGlanceReloadSession;
typedef struct {
  struct { PublishedId icon; const char *subtitle_template_string; } layout;
  time_t expiration_time;
} AppGlanceSlice;
typedef enum {
  APP_GLANCE_RESULT_SUCCESS = 0,
  APP_GLANCE_RESULT_INVALID_TEMPLATE_STRING = 1,
  APP_GLANCE_RESULT_SLICE_CAPACITY_EXCEEDED = 8
} AppGlanceResult;
#define APP_GLANCE_SLICE_DEFAULT_ICON ((PublishedId)0)
#define APP_GLANCE_SLICE_NO_EXPIRATION ((time_t)0)
typedef void (*AppGlanceReloadCallback)(AppGlanceReloadSession *, size_t, void *);
void app_glance_reload(AppGlanceReloadCallback callback, void *context);
AppGlanceResult app_glance_add_slice(AppGlanceReloadSession *session, AppGlanceSlice slice);
#define APP_LOG_LEVEL_WARNING 2
#define APP_LOG(level, ...) do { (void)(level); } while (0)
