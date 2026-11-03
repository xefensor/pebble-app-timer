#pragma once

#include "timer_model.h"

#define TIMER_COLLECTION_MAGIC UINT32_C(0x54434f4c)
#define TIMER_COLLECTION_VERSION 2u
#define TIMER_COLLECTION_CAPACITY 8u

/* RAM representation. Persist the header and individual models in separate
 * records: the complete collection exceeds Pebble's per-key storage limit. */
typedef struct {
  uint32_t magic;
  uint32_t version;
  uint32_t seed;
  uint32_t revision;
  uint32_t visible_mask;
  ActivityTimerModel slots[TIMER_COLLECTION_CAPACITY];
} TimerCollection;

void timer_collection_init(TimerCollection *collection, uint32_t seed, uint32_t now);
bool timer_collection_migrate(TimerCollection *collection,
                              const ActivityTimerModel *legacy, uint32_t now);
bool timer_collection_valid(const TimerCollection *collection);
bool timer_collection_decode(TimerCollection *collection, const void *bytes,
                              size_t length, uint8_t alarms);
unsigned timer_collection_count(const TimerCollection *collection);
/* Fill the bounded stable display order once for multi-row callers. */
unsigned timer_collection_visible_slots(const TimerCollection *collection,
                                        unsigned ordered[TIMER_COLLECTION_CAPACITY]);
int timer_collection_visible_slot(const TimerCollection *collection, unsigned row);
bool timer_collection_visible(const TimerCollection *collection, unsigned slot);
bool timer_collection_can_create(const TimerCollection *collection);

/* Returns a stable slot index, or -1 on capacity/validation/revision failure.
 * Reusing a hidden slot starts a new run; providers replace the preceding run
 * atomically by its separate stable ACTIVITY_SLOT field. */
int timer_collection_create(TimerCollection *collection, uint32_t seconds, uint32_t now);
bool timer_collection_edit(TimerCollection *collection, unsigned slot,
                            uint32_t seconds, uint32_t now);
/* Restart this stable slot as a new run with its configured duration. */
bool timer_collection_restart(TimerCollection *collection, unsigned slot, uint32_t now);
bool timer_collection_pause(TimerCollection *collection, unsigned slot, uint32_t now);
bool timer_collection_resume(TimerCollection *collection, unsigned slot, uint32_t now);
bool timer_collection_delete(TimerCollection *collection, unsigned slot, uint32_t now);
bool timer_collection_snooze(TimerCollection *collection, unsigned slot, uint32_t now);
bool timer_collection_acknowledge(TimerCollection *collection, unsigned slot);
uint8_t timer_collection_finish_due(TimerCollection *collection, uint32_t now);
bool timer_collection_has_due(const TimerCollection *collection, uint32_t now);
bool timer_collection_reconfirm(TimerCollection *collection, unsigned slot, uint32_t now);
/* Raise the collection-wide phone revision floor before reconfirming/sending
 * slots. Raising it alone does not alter an owner's observation timestamp. */
bool timer_collection_raise_floor(TimerCollection *collection, uint32_t floor);
