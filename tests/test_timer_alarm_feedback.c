#include <pebble.h>
#include "../src/c/timer_alarm_feedback.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

struct AppTimer {
  uint64_t due;
  AppTimerCallback callback;
  void *data;
  bool active;
};

static struct AppTimer s_timers[4];
static uint64_t s_now, s_motor_until;
static unsigned s_pulses, s_cancels, s_registrations, s_peak_timers;
static bool s_quiet, s_fail_timer;

#ifdef PBL_PLATFORM_EMERY
static unsigned s_beeps,s_stops;static bool s_muted,s_busy,s_sound_failure;
static SpeakerFinishedCallback s_finished;static void *s_finish_context;
void speaker_set_finish_callback(SpeakerFinishedCallback callback,void *context){s_finished=callback;s_finish_context=context;}
bool speaker_is_muted(void){return s_muted;}
SpeakerStatus speaker_get_status(void){return s_busy?SpeakerStatusPlaying:SpeakerStatusIdle;}
bool speaker_play_notes(const SpeakerNote *notes,uint32_t count,uint8_t volume){
 assert(count==3&&volume==70&&!s_muted&&!s_busy&&!s_quiet);
 assert(notes[0].midi_note==93&&notes[0].waveform==SpeakerWaveformSquare&&notes[0].duration_ms==150);
 assert(notes[1].midi_note==0&&notes[1].duration_ms==100);
 assert(notes[2].midi_note==93&&notes[2].duration_ms==150);
 ++s_beeps;return !s_sound_failure;
}
void speaker_stop(void){++s_stops;if(s_finished)s_finished(SpeakerFinishReasonStopped,s_finish_context);}
static void sound_tests(void);
#endif

static unsigned active_timers(void) {
  unsigned count = 0;
  for (unsigned i = 0; i < 4; ++i) if (s_timers[i].active) ++count;
  return count;
}

AppTimer *app_timer_register(uint32_t delay, AppTimerCallback callback, void *data) {
  ++s_registrations;
  if (s_fail_timer) return NULL;
  assert(delay == 3000u);
  for (unsigned i = 0; i < 4; ++i) if (!s_timers[i].active) {
    s_timers[i] = (struct AppTimer){s_now + delay, callback, data, true};
    const unsigned active = active_timers();
    if (active > s_peak_timers) s_peak_timers = active;
    assert(active <= 1);
    return &s_timers[i];
  }
  assert(!"Unexpected alarm callback accumulation");
  return NULL;
}

bool app_timer_cancel(AppTimer *timer) {
  assert(timer);
  const bool active = timer->active;
  timer->active = false;
  return active;
}

bool quiet_time_is_active(void) { return s_quiet; }
void vibes_double_pulse(void) {
  assert(!s_quiet);
  assert(s_motor_until <= s_now); /* No stacked patterns in this scheduler. */
  ++s_pulses;
  s_motor_until = s_now + 500u;
}
void vibes_cancel(void) { ++s_cancels; s_motor_until = s_now; }

static void advance(uint32_t elapsed) {
  const uint64_t target = s_now + elapsed;
  unsigned callbacks = 0;
  for (;;) {
    int chosen = -1;
    for (unsigned i = 0; i < 4; ++i)
      if (s_timers[i].active && s_timers[i].due <= target &&
          (chosen < 0 || s_timers[i].due < s_timers[chosen].due)) chosen = (int)i;
    if (chosen < 0) break;
    assert(++callbacks < 1000);
    s_now = s_timers[chosen].due;
    const AppTimerCallback callback = s_timers[chosen].callback;
    void *data = s_timers[chosen].data;
    s_timers[chosen].active = false;
    callback(data);
  }
  s_now = target;
}

static void reset(void) {
  timer_alarm_feedback_deinit();
  memset(s_timers, 0, sizeof(s_timers));
  s_now = s_motor_until = 0;
  s_pulses = s_cancels = s_registrations = s_peak_timers = 0;
  s_quiet = s_fail_timer = false;
  timer_alarm_feedback_set_sound(true);
#ifdef PBL_PLATFORM_EMERY
  s_beeps=s_stops=0;s_muted=s_busy=s_sound_failure=false;
#endif
}

static void repeat_and_cancel(void) {
  reset();
  timer_alarm_feedback_update(0);
  assert(!s_pulses && !s_cancels && !s_registrations);
  timer_alarm_feedback_update(1);
  assert(s_pulses == 1 && active_timers() == 1 && s_motor_until > s_now);
  for (unsigned i = 0; i < 30; ++i) timer_alarm_feedback_update(1);
  assert(s_pulses == 1 && s_registrations == 1);
  advance(2999);
  assert(s_pulses == 1);
  advance(1);
  assert(s_pulses == 2 && s_motor_until > s_now && active_timers() == 1);
  timer_alarm_feedback_update(0); /* Last verified Done or Snooze. */
  assert(s_motor_until == s_now && !active_timers() && s_cancels == 1);
  advance(12000);
  assert(s_pulses == 2);
  timer_alarm_feedback_update(0);
  timer_alarm_feedback_deinit();
  assert(s_cancels == 1); /* Idle updates must not cancel unrelated feedback. */
}

static void multiple_pending_and_failed_acknowledgement(void) {
  reset();
  timer_alarm_feedback_update(1);
  advance(1000);
  timer_alarm_feedback_update(3); /* Another timer finishes. */
  timer_alarm_feedback_update(2); /* First acknowledged, another still pending. */
  assert(s_pulses == 1 && s_registrations == 1 && !s_cancels);
  advance(2000);
  assert(s_pulses == 2);
  timer_alarm_feedback_update(2); /* Failed save retains the true pending mask. */
  advance(3000);
  assert(s_pulses == 3 && s_peak_timers == 1);
  timer_alarm_feedback_update(0);
  assert(!active_timers() && s_cancels == 1);
}

static void quiet_time_suppresses_and_resumes(void) {
  reset();
  s_quiet = true;
  timer_alarm_feedback_update(1);
  assert(!s_pulses && s_cancels == 1 && active_timers() == 1);
  advance(6000);
  assert(!s_pulses && active_timers() == 1);
  s_quiet = false;
  advance(3000);
  assert(s_pulses == 1 && s_motor_until > s_now);
  s_quiet = true;
  timer_alarm_feedback_update(1); /* An explicit update cancels in-flight work. */
  assert(s_motor_until == s_now);
  advance(3000); /* Callback independently checks Quiet Time too. */
  assert(s_pulses == 1);
  s_quiet = false;
  advance(3000);
  assert(s_pulses == 2);
  timer_alarm_feedback_update(0);
}

static void registration_failure_and_reopen(void) {
  reset();
  s_fail_timer = true;
  timer_alarm_feedback_update(1);
  assert(s_pulses == 1 && !active_timers());
  for (unsigned i = 0; i < 3; ++i) timer_alarm_feedback_update(1);
  assert(s_pulses == 1 && s_registrations == 4);
  s_fail_timer = false;
  timer_alarm_feedback_update(1);
  assert(s_pulses == 1 && active_timers() == 1);
  advance(3000);
  assert(s_pulses == 2);
  s_fail_timer = true;
  advance(3000); /* Failure after a successful repeat also remains recoverable. */
  assert(s_pulses == 3 && !active_timers());
  s_fail_timer = false;
  timer_alarm_feedback_update(1);
  assert(s_pulses == 3 && active_timers() == 1);
  timer_alarm_feedback_deinit();
  assert(!active_timers() && s_motor_until == s_now);
  advance(6000);
  assert(s_pulses == 3);
  timer_alarm_feedback_update(1); /* Reopened with an unacknowledged native alarm. */
  assert(s_pulses == 4 && active_timers() == 1);
  timer_alarm_feedback_deinit();
}

#ifdef PBL_PLATFORM_EMERY
static void sound_tests(void){
 reset();timer_alarm_feedback_update(1);assert(s_beeps==1);
 timer_alarm_feedback_update(3);timer_alarm_feedback_update(2);assert(s_beeps==1&&!s_stops);
 assert(s_finished);s_finished(SpeakerFinishReasonDone,s_finish_context);
 advance(3000);assert(s_beeps==2);timer_alarm_feedback_update(0);assert(s_stops==1);
 reset();s_busy=true;timer_alarm_feedback_update(1);assert(!s_beeps&&s_pulses==1);
 timer_alarm_feedback_update(0);assert(!s_stops); /* Never cancel another owner's sound. */
 reset();s_muted=true;timer_alarm_feedback_update(1);assert(!s_beeps&&s_pulses==1);
 s_muted=false;advance(3000);assert(s_beeps==1);
 timer_alarm_feedback_set_sound(false);assert(s_stops==1);advance(3000);assert(s_beeps==1&&s_pulses==3);
 timer_alarm_feedback_set_sound(true);advance(3000);assert(s_beeps==2);
 s_quiet=true;timer_alarm_feedback_update(1);assert(s_stops==2);
 s_quiet=false;s_finished(SpeakerFinishReasonPreempted,s_finish_context);s_busy=true;advance(3000);assert(s_beeps==2);
 timer_alarm_feedback_deinit();assert(s_stops==2&&s_finished==NULL);
 reset();s_sound_failure=true;timer_alarm_feedback_update(1);assert(s_beeps==1&&s_pulses==1);
 advance(3000);assert(s_beeps==2&&s_pulses==2);timer_alarm_feedback_deinit();assert(!s_stops);
}
#endif
int main(void) {
  repeat_and_cancel();
  multiple_pending_and_failed_acknowledgement();
  quiet_time_suppresses_and_resumes();
  registration_failure_and_reopen();
#ifdef PBL_PLATFORM_EMERY
  sound_tests();
#endif
  puts("timer alarm feedback: repeated native pulses, Quiet Time, pending masks, cancellation and timer failure recovery passed");
  return 0;
}
