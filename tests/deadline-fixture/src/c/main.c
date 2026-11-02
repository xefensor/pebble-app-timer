/* Emulator-only persisted deadline fixture; never part of the shipping app.
 * Model/collection files beside this fixture are exact copies of app sources. */
#include <pebble.h>
#include "timer_collection.h"
#ifndef DEADLINE_OFFSET
#define DEADLINE_OFFSET 0
#endif
typedef struct { uint32_t magic, version, generation, bytes, checksum, alarms; } SavedHeader;
static Window *window;
static TextLayer *label;
int main(void) {
  wakeup_cancel_all();
  persist_delete(2100); persist_delete(2101); persist_delete(2120); persist_delete(2121);
  for (uint32_t key=2130; key<=2145; ++key) persist_delete(key);
  TimerCollection collection;
  const uint32_t now=(uint32_t)time(NULL);
  timer_collection_init(&collection,now,now);
  bool saved=timer_collection_create(&collection,20,now)==0 &&
      timer_collection_create(&collection,20+DEADLINE_OFFSET,now)==1 &&
      timer_collection_valid(&collection);
  const uint8_t *bytes=(const uint8_t *)&collection;
  uint32_t checksum=UINT32_C(2166136261);
  for (size_t i=0;i<sizeof(collection);++i) checksum=(checksum^bytes[i])*UINT32_C(16777619);
  for (size_t offset=0,chunk=0;offset<sizeof(collection);offset+=200,++chunk) {
    size_t count=sizeof(collection)-offset<200?sizeof(collection)-offset:200;
    saved=saved && persist_write_data(2130+chunk,bytes+offset,count)==(int)count;
  }
  const SavedHeader header={TIMER_COLLECTION_MAGIC,TIMER_COLLECTION_VERSION,1,sizeof(collection),checksum,0};
  saved=saved && persist_write_data(2120,&header,sizeof(header))==(int)sizeof(header);
  saved=saved && persist_write_int(2103,1)>=0;
  window=window_create();window_set_background_color(window,GColorWhite);
  label=text_layer_create(GRect(5,55,190,120));
  text_layer_set_font(label,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(label,GTextAlignmentCenter);
  text_layer_set_text(label,saved?"Deadline fixture\n2 running timers":"Fixture write failed");
  layer_add_child(window_get_root_layer(window),text_layer_get_layer(label));
  window_stack_push(window,false);app_event_loop();text_layer_destroy(label);window_destroy(window);
}
