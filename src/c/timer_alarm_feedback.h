#pragma once

#include <stdint.h>

/* Mirror the native pending-alarm mask after a committed mutation (or retained
 * local alarm on save failure). This owns feedback only, never timer state or
 * Wakeup scheduling. Repeated nonzero updates do not restart the cadence. */
void timer_alarm_feedback_update(uint8_t pending_mask);

/* Cancel the repeat callback and any in-flight alarm vibration on app exit. */
void timer_alarm_feedback_deinit(void);
