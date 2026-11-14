#include <pebble.h>

#include "timer_alarm_feedback.h"

#define ALARM_REPEAT_MS 3000u

static AppTimer *s_repeat_timer;
static bool s_pending;
static bool s_sound_enabled=true;
#ifdef PBL_PLATFORM_EMERY
static bool s_owns_sound;
static void prv_sound_finished(SpeakerFinishReason reason,void *context){(void)reason;(void)context;s_owns_sound=false;}
static void prv_stop_sound(void){if(s_owns_sound){s_owns_sound=false;speaker_stop();}}
#else
static void prv_stop_sound(void){}
#endif
void timer_alarm_feedback_set_sound(bool enabled){s_sound_enabled=enabled;if(!enabled)prv_stop_sound();}


static void prv_repeat(void *context);

static void prv_schedule(void) {
  if (s_pending && !s_repeat_timer)
    s_repeat_timer = app_timer_register(ALARM_REPEAT_MS, prv_repeat, NULL);
}

static void prv_pulse(void) {
  /* The public SDK has no Quiet Time change subscription. Check before every
   * pulse (and on native updates); retain the callback while quiet so an
   * unacknowledged alarm resumes when Quiet Time ends. */
  if (quiet_time_is_active()) { vibes_cancel(); prv_stop_sound(); }
  else {
    vibes_double_pulse();
#ifdef PBL_PLATFORM_EMERY
    if(speaker_is_muted()){prv_stop_sound();return;}
    if(s_sound_enabled&&!s_owns_sound&&speaker_get_status()==SpeakerStatusIdle){
      static const SpeakerNote notes[]={
        {.midi_note=93,.waveform=SpeakerWaveformSquare,.duration_ms=150},
        {.midi_note=0,.duration_ms=100},
        {.midi_note=93,.waveform=SpeakerWaveformSquare,.duration_ms=150}
      };
      speaker_set_finish_callback(prv_sound_finished,NULL);
      s_owns_sound=speaker_play_notes(notes,3,70);
    }
#endif
  }
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
    prv_stop_sound();
    return;
  }
  if (!s_pending) {
    s_pending = true;
    prv_pulse();
  } else if (quiet_time_is_active()) {
    vibes_cancel();
    prv_stop_sound();
  }
  /* A failed registration can be retried by a later native refresh/tick.
   * Retaining s_pending prevents retries from replaying the immediate pulse. */
  prv_schedule();
}

void timer_alarm_feedback_deinit(void) {
  timer_alarm_feedback_update(0);
#ifdef PBL_PLATFORM_EMERY
  speaker_set_finish_callback(NULL,NULL);
#endif
}
