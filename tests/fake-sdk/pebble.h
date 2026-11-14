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

typedef int32_t WakeupId;
WakeupId wakeup_schedule(time_t timestamp, int32_t cookie, bool notify_if_missed);
bool wakeup_query(WakeupId id, time_t *timestamp);
void wakeup_cancel(WakeupId id);
void wakeup_cancel_all(void);

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

#ifdef PBL_PLATFORM_EMERY
typedef enum {SpeakerWaveformSine,SpeakerWaveformSquare} SpeakerWaveform;
typedef struct {uint8_t midi_note,waveform;uint16_t duration_ms;uint8_t velocity,reserved;} SpeakerNote;
typedef enum {SpeakerStatusIdle,SpeakerStatusPlaying,SpeakerStatusDraining} SpeakerStatus;
typedef enum {SpeakerFinishReasonDone,SpeakerFinishReasonStopped,SpeakerFinishReasonPreempted,SpeakerFinishReasonError} SpeakerFinishReason;
typedef void (*SpeakerFinishedCallback)(SpeakerFinishReason,void *);
void speaker_set_finish_callback(SpeakerFinishedCallback callback,void *context);
bool speaker_play_notes(const SpeakerNote *notes,uint32_t count,uint8_t volume);
bool speaker_is_muted(void);
SpeakerStatus speaker_get_status(void);
void speaker_stop(void);
#endif
