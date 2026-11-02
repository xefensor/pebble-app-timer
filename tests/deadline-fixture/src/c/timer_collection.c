#include "timer_collection.h"

#include <limits.h>
#include <string.h>

static uint32_t saturating_add(uint32_t value, uint32_t delta) {
  return value > UINT32_MAX - delta ? UINT32_MAX : value + delta;
}

static bool slot_ready(const TimerCollection *collection, unsigned slot) {
  return collection && slot < TIMER_COLLECTION_CAPACITY &&
      collection->revision < UINT32_MAX;
}

static bool commit(TimerCollection *collection, unsigned slot,
                   const ActivityTimerModel *model) {
  if (!activity_timer_valid(model) || model->revision <= collection->revision) return false;
  collection->slots[slot] = *model;
  collection->revision = model->revision;
  return true;
}

void timer_collection_init(TimerCollection *collection, uint32_t seed, uint32_t now) {
  memset(collection, 0, sizeof(*collection));
  collection->magic = TIMER_COLLECTION_MAGIC;
  collection->version = TIMER_COLLECTION_VERSION;
  collection->seed = seed ? seed : 1u;
  collection->revision = 1;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    uint32_t origin = (uint32_t)(((uint64_t)collection->seed - 1u + i) % UINT32_MAX) + 1u;
    activity_timer_init(&collection->slots[i], origin, now);
  }
}

bool timer_collection_migrate(TimerCollection *collection,
                              const ActivityTimerModel *legacy, uint32_t now) {
  if (!collection || !activity_timer_valid(legacy)) return false;
  timer_collection_init(collection, legacy->origin, now);
  collection->slots[0] = *legacy;
  collection->revision = legacy->revision;
  if (legacy->state != ACTIVITY_TIMER_CANCELLED) collection->visible_mask = 1u;
  return true;
}

bool timer_collection_valid(const TimerCollection *collection) {
  if (!collection || collection->magic != TIMER_COLLECTION_MAGIC ||
      collection->version != TIMER_COLLECTION_VERSION || !collection->seed ||
      !collection->revision || (collection->visible_mask >> TIMER_COLLECTION_CAPACITY)) return false;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    const ActivityTimerModel *timer = &collection->slots[i];
    if (!activity_timer_valid(timer) || timer->revision > collection->revision) return false;
    const bool visible = (collection->visible_mask & (1u << i)) != 0;
    if (visible != (timer->state != ACTIVITY_TIMER_CANCELLED)) return false;
    for (unsigned j = 0; j < i; ++j)
      if (collection->slots[j].origin == timer->origin) return false;
  }
  return true;
}

unsigned timer_collection_count(const TimerCollection *collection) {
  unsigned count = 0;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i)
    if (timer_collection_visible(collection, i)) ++count;
  return count;
}

bool timer_collection_visible(const TimerCollection *collection, unsigned slot) {
  return collection && slot < TIMER_COLLECTION_CAPACITY &&
      (collection->visible_mask & (1u << slot)) != 0;
}

int timer_collection_visible_slot(const TimerCollection *collection, unsigned row) {
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    if (!timer_collection_visible(collection, i)) continue;
    if (!row--) return (int)i;
  }
  return -1;
}

int timer_collection_create(TimerCollection *collection, uint32_t seconds, uint32_t now) {
  if (!collection || collection->revision == UINT32_MAX) return -1;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    if (timer_collection_visible(collection, i)) continue;
    ActivityTimerModel next = collection->slots[i];
    next.revision = collection->revision;
    if (!activity_timer_start(&next, seconds, now) || !commit(collection, i, &next)) return -1;
    collection->visible_mask |= 1u << i;
    return (int)i;
  }
  return -1;
}

bool timer_collection_edit(TimerCollection *collection, unsigned slot,
                            uint32_t seconds, uint32_t now) {
  if (!slot_ready(collection, slot) || !timer_collection_visible(collection, slot) ||
      seconds < ACTIVITY_TIMER_MIN_SECONDS || seconds > ACTIVITY_TIMER_MAX_SECONDS) return false;
  ActivityTimerModel next = collection->slots[slot];
  next.revision = collection->revision;
  if (next.state == ACTIVITY_TIMER_FINISHED) {
    if (!activity_timer_start(&next, seconds, now)) return false;
  } else {
    if (next.state != ACTIVITY_TIMER_RUNNING && next.state != ACTIVITY_TIMER_PAUSED) return false;
    if (next.state == ACTIVITY_TIMER_RUNNING && now > UINT32_MAX - seconds) return false;
    ++next.revision;
    next.configured_seconds = seconds;
    next.observed_at = now;
    if (next.state == ACTIVITY_TIMER_RUNNING) {
      next.deadline = now + seconds;
      next.remaining_seconds = 0;
      next.valid_until = saturating_add(next.deadline, ACTIVITY_TIMER_TERMINAL_SECONDS);
    } else {
      next.deadline = 0;
      next.remaining_seconds = seconds;
      next.valid_until = saturating_add(now, ACTIVITY_TIMER_MAX_SECONDS);
    }
  }
  return commit(collection, slot, &next);
}

bool timer_collection_pause(TimerCollection *collection, unsigned slot, uint32_t now) {
  if (!slot_ready(collection, slot) || !timer_collection_visible(collection, slot)) return false;
  ActivityTimerModel next = collection->slots[slot];
  next.revision = collection->revision;
  return activity_timer_pause(&next, now) && commit(collection, slot, &next);
}

bool timer_collection_resume(TimerCollection *collection, unsigned slot, uint32_t now) {
  if (!slot_ready(collection, slot) || !timer_collection_visible(collection, slot)) return false;
  ActivityTimerModel next = collection->slots[slot];
  next.revision = collection->revision;
  return activity_timer_resume(&next, now) && commit(collection, slot, &next);
}

bool timer_collection_delete(TimerCollection *collection, unsigned slot, uint32_t now) {
  if (!slot_ready(collection, slot) || !timer_collection_visible(collection, slot)) return false;
  ActivityTimerModel next = collection->slots[slot];
  next.revision = collection->revision;
  if (next.state == ACTIVITY_TIMER_FINISHED) {
    ++next.revision;
    next.state = ACTIVITY_TIMER_CANCELLED;
    next.observed_at = now;
    next.valid_until = saturating_add(now, ACTIVITY_TIMER_TERMINAL_SECONDS);
  } else if (!activity_timer_cancel(&next, now)) return false;
  if (!commit(collection, slot, &next)) return false;
  collection->visible_mask &= ~(1u << slot);
  return true;
}

bool timer_collection_snooze(TimerCollection *collection, unsigned slot, uint32_t now) {
  if (!slot_ready(collection, slot) || !timer_collection_visible(collection, slot) ||
      collection->slots[slot].state != ACTIVITY_TIMER_FINISHED) return false;
  ActivityTimerModel next = collection->slots[slot];
  next.revision = collection->revision;
  return activity_timer_start(&next, 60, now) && commit(collection, slot, &next);
}

uint8_t timer_collection_finish_due(TimerCollection *collection, uint32_t now) {
  uint8_t changed = 0;
  if (!collection) return 0;
  for (unsigned i = 0; i < TIMER_COLLECTION_CAPACITY; ++i) {
    if (!slot_ready(collection, i) || !timer_collection_visible(collection, i)) continue;
    ActivityTimerModel next = collection->slots[i];
    next.revision = collection->revision;
    if (activity_timer_finish_if_due(&next, now) && commit(collection, i, &next))
      changed |= (uint8_t)(1u << i);
  }
  return changed;
}

bool timer_collection_reconfirm(TimerCollection *collection, unsigned slot, uint32_t now) {
  if (!slot_ready(collection, slot)) return false;
  ActivityTimerModel next = collection->slots[slot];
  next.revision = collection->revision;
  return activity_timer_reconfirm(&next, now) && commit(collection, slot, &next);
}

bool timer_collection_raise_floor(TimerCollection *collection, uint32_t floor) {
  if (!collection || floor == UINT32_MAX) return false;
  if (floor > collection->revision) collection->revision = floor;
  return true;
}
