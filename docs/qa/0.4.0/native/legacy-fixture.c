/* Emulator-only legacy persistence fixture; never part of the shipping app. */
#include <pebble.h>
#include <stdint.h>
#include <string.h>
typedef struct { uint32_t magic,version,origin,run,revision,state,configured_seconds,remaining_seconds,deadline,observed_at,valid_until; } LegacyTimer;
static Window *window;
static TextLayer *label;
int main(void) {
  wakeup_cancel_all();
  persist_delete(2101);persist_delete(2103);persist_delete(2120);persist_delete(2121);
  for (uint32_t key=2130;key<=2145;++key)persist_delete(key);
  LegacyTimer timer={UINT32_C(0x4154494d),1,UINT32_C(0x6b49d200),1,20,2,300,298,0,(uint32_t)time(NULL),(uint32_t)time(NULL)+86400};
  bool saved=persist_write_data(2100,&timer,sizeof(timer))==(int)sizeof(timer);
  window=window_create();window_set_background_color(window,GColorWhite);
  label=text_layer_create(GRect(5,55,190,120));
  text_layer_set_font(label,fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(label,GTextAlignmentCenter);
  text_layer_set_text(label,saved?"Legacy fixture\n4:58 paused":"Fixture write failed");
  layer_add_child(window_get_root_layer(window),text_layer_get_layer(label));
  window_stack_push(window,false);app_event_loop();text_layer_destroy(label);window_destroy(window);
}
