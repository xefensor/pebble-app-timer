#pragma once

#include "timer_collection.h"

/* Publish native deadlines to the launcher. PebbleOS owns subtitle ticking;
 * this does not run a worker or change Timer's saved state/Wakeup alarms. */
void timer_glance_update(const TimerCollection *collection, uint32_t now);

/* Flush a coalesced change before ordinary exit. The callback is synchronous
 * when no flush is needed, otherwise bounded by a one-second app timer. */
void timer_glance_request_exit(void (*on_ready)(void));
void timer_glance_deinit(void);
