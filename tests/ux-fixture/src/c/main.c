/* Emulator-only original V1 bank fixture, never shipping code. Its own clock
 * only establishes saved deadlines. The installed Timer owns real Wakeups. */
#include <pebble.h>
#include <stdint.h>
#include <string.h>
#ifndef UX_FIXTURE_MODE
#define UX_FIXTURE_MODE 0
#endif
typedef struct {
  uint32_t magic, version, origin, run, revision, state, configured_seconds;
  uint32_t remaining_seconds, deadline, observed_at, valid_until;
} ModelV1;
typedef struct {
  uint32_t magic, version, seed, revision, visible_mask;
  ModelV1 slots[8];
} CollectionV1;
typedef struct { uint32_t magic, version, generation, bytes, checksum, alarms; } Header;
static Window *window;
static TextLayer *label;
int main(void) {
  wakeup_cancel_all();
  persist_delete(2100); persist_delete(2101); persist_delete(2120); persist_delete(2121);
  for (uint32_t key=2130;key<=2145;++key) persist_delete(key);
  CollectionV1 collection;
  memset(&collection,0,sizeof(collection));
  const uint32_t now=(uint32_t)time(NULL);
  collection.magic=UINT32_C(0x54434f4c); collection.version=1;
  collection.seed=UINT32_C(0x6b49d300); collection.revision=30;
  uint32_t alarms=0;
  for (unsigned i=0;i<8;++i) {
    ModelV1 *timer=&collection.slots[i];
    timer->magic=UINT32_C(0x4154494d);timer->version=1;
    timer->origin=collection.seed+i;timer->revision=20+i;timer->state=4;
    timer->configured_seconds=300;timer->observed_at=now;timer->valid_until=now+86400;
    if (UX_FIXTURE_MODE==2 || (UX_FIXTURE_MODE==1 && i<2)) {
      timer->state=3;timer->run=1;
      timer->configured_seconds=300+60*i;
      timer->observed_at=now-100-i;timer->valid_until=timer->observed_at+60;
      collection.visible_mask|=1u<<i;
      if (UX_FIXTURE_MODE==1 && !i) {
        alarms=1;timer->observed_at=now-10;timer->valid_until=now+50;
      }
    } else if ((UX_FIXTURE_MODE==1 && i==2) ||
               ((UX_FIXTURE_MODE==3 || UX_FIXTURE_MODE==4) && i<2)) {
      const uint32_t duration=UX_FIXTURE_MODE==1?600:20+(UX_FIXTURE_MODE==3?5*i:0);
      timer->state=1;timer->run=1;timer->configured_seconds=duration;
      timer->deadline=now+duration;timer->valid_until=timer->deadline+60;
      collection.visible_mask|=1u<<i;
    }
  }
  const uint8_t *bytes=(const uint8_t *)&collection;
  uint32_t checksum=UINT32_C(2166136261)^alarms;
  for (size_t i=0;i<sizeof(collection);++i) checksum=(checksum^bytes[i])*UINT32_C(16777619);
  bool saved=true;
  for (size_t offset=0,chunk=0;offset<sizeof(collection);offset+=200,++chunk) {
    const size_t count=sizeof(collection)-offset<200?sizeof(collection)-offset:200;
    saved=saved && persist_write_data(2130+chunk,bytes+offset,count)==(int)count;
  }
  const Header header={UINT32_C(0x54434f4c),1,1,sizeof(collection),checksum,alarms};
  saved=saved && persist_write_data(2120,&header,sizeof(header))==(int)sizeof(header);
  saved=saved && persist_write_int(2103,1)>=0;
  window=window_create();window_set_background_color(window,GColorWhite);
  label=text_layer_create(GRect(5,55,190,120));
  text_layer_set_font(label,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(label,GTextAlignmentCenter);
  static char caption[80];snprintf(caption,sizeof(caption),saved?"V1 UX fixture %u\n372-byte bank":"Fixture write failed",UX_FIXTURE_MODE);
  text_layer_set_text(label,caption);
  layer_add_child(window_get_root_layer(window),text_layer_get_layer(label));
  window_stack_push(window,false);app_event_loop();text_layer_destroy(label);window_destroy(window);
}
