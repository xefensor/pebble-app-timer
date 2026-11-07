#include <pebble.h>
#include "timer_appearance.h"

/* 2100–2103 own native state/publication; 2104 is AppGlance. */
#define APPEARANCE_KEY 2105
#define LIGHT 1
#define DARK 2

bool timer_appearance_load(void) {
  return persist_exists(APPEARANCE_KEY) && persist_read_int(APPEARANCE_KEY)==DARK;
}

bool timer_appearance_save(bool dark) {
  const int32_t value=dark?DARK:LIGHT;
  if(persist_exists(APPEARANCE_KEY)&&persist_read_int(APPEARANCE_KEY)==value)return true;
  return persist_write_int(APPEARANCE_KEY,value)==(int)sizeof(value)&&
      persist_exists(APPEARANCE_KEY)&&persist_read_int(APPEARANCE_KEY)==value;
}
