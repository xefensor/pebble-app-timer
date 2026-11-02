#include "../src/c/timer_glance_plan.h"
#include <assert.h>
#include <string.h>

int main(void) {
  TimerCollection collection;
  TimerGlanceSlice slice;
  timer_collection_init(&collection, 417, 1000);
  assert(!timer_glance_next(&collection, 1000, &slice));
  const int later = timer_collection_create(&collection, 600, 1000);
  const int first = timer_collection_create(&collection, 60, 1000);
  const int equal = timer_collection_create(&collection, 60, 1000);
  assert(later >= 0 && first >= 0 && equal >= 0);
  assert(timer_glance_next(&collection, 1000, &slice));
  assert(slice.deadline == 1060 && strstr(slice.subtitle, "time_until(1060)"));
  assert(strstr(slice.subtitle, "/ 3 running"));
  assert(strstr(slice.subtitle, ">=1H:'%fH:%0M:%0S','%fM:%0S'"));
  assert(strlen(slice.subtitle) < 150);
  assert(timer_glance_next(&collection, slice.deadline, &slice));
  assert(slice.deadline == 1600 && !strstr(slice.subtitle, "running"));
  assert(!timer_glance_next(&collection, slice.deadline, &slice));
  assert(timer_collection_pause(&collection, (unsigned)later, 1020));
  assert(timer_glance_next(&collection, 1020, &slice) && slice.deadline == 1060);
  assert(!timer_glance_next(&collection, 1060, &slice));
  assert(timer_collection_finish_due(&collection, 1060));
  assert(!timer_glance_next(&collection, 1060, &slice));
  assert(timer_collection_acknowledge(&collection, (unsigned)first));
  assert(!timer_glance_next(&collection, 1060, &slice));
  assert(timer_collection_resume(&collection, (unsigned)later, 1100));
  assert(timer_glance_next(&collection, 1100, &slice) && slice.deadline == 1680);
  assert(timer_collection_delete(&collection, (unsigned)later, 1101));
  assert(!timer_glance_next(&collection, 1101, &slice));
  assert(!timer_glance_next(NULL, 1000, &slice));
  assert(!timer_glance_next(&collection, 1000, NULL));
  return 0;
}
