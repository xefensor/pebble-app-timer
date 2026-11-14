#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
enum { TIMER_ALARM_PREFERENCES_BYTES=12, TIMER_ALARM_HELLO=1, TIMER_ALARM_SPEAKER=2 };
typedef struct { uint32_t revision; bool sound; } TimerAlarmPreferences;
void timer_alarm_preferences_encode(const TimerAlarmPreferences *,uint8_t status,uint8_t flags,uint8_t bytes[TIMER_ALARM_PREFERENCES_BYTES]);
bool timer_alarm_preferences_decode(const uint8_t *,size_t,TimerAlarmPreferences *,uint8_t *,uint8_t *);
bool timer_alarm_preferences_load(TimerAlarmPreferences *);
bool timer_alarm_preferences_save(const TimerAlarmPreferences *);
