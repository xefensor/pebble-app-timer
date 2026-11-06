#include <pebble.h>
#include "../src/c/timer_appearance.h"
#include <assert.h>
#include <stdio.h>

static bool exists,fail_write,unconfirmed;
static int32_t stored;
static unsigned writes;

bool persist_exists(uint32_t key) { assert(key==2105);return exists; }
int32_t persist_read_int(uint32_t key) { assert(key==2105);return unconfirmed?99:stored; }
int persist_write_int(uint32_t key,int32_t value) {
  assert(key==2105);++writes;
  if(fail_write)return -1;
  exists=true;stored=value;return sizeof(value);
}

int main(void) {
  assert(!timer_appearance_load());
  exists=true;stored=99;assert(!timer_appearance_load());
  assert(timer_appearance_save(true));assert(timer_appearance_load());
  const unsigned saved=writes;
  assert(timer_appearance_save(true)&&writes==saved);
  fail_write=true;
  assert(!timer_appearance_save(false));assert(timer_appearance_load());
  fail_write=false;
  assert(timer_appearance_save(false));assert(!timer_appearance_load());
  unconfirmed=true;
  assert(!timer_appearance_save(true));
  unconfirmed=false;
  assert(timer_appearance_load());
  puts("timer appearance: isolated key, default/reopen, unchanged-write suppression, failures and readback checks passed");
  return 0;
}
