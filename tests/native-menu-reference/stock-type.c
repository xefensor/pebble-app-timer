/* Independent SDK bitmap-font specimen. No production type/draw helpers. */
#include <pebble.h>
static const char *header_key,*title_key,*footer_key;
static void draw(Layer *layer,GContext *ctx) {
  (void)layer;graphics_context_set_text_color(ctx,GColorBlack);
  graphics_draw_text(ctx,"Paused",fonts_get_system_font(header_key),GRect(8,0,80,32),GTextOverflowModeWordWrap,GTextAlignmentLeft,NULL);
  graphics_draw_text(ctx,"10:00",fonts_get_system_font(title_key),GRect(8,53,143,60),GTextOverflowModeWordWrap,GTextAlignmentLeft,NULL);
  graphics_draw_text(ctx,"Elapsed 00:00",fonts_get_system_font(footer_key),GRect(8,198,150,30),GTextOverflowModeWordWrap,GTextAlignmentCenter,NULL);
}
int main(void) {
  const PreferredContentSize size=preferred_content_size();
  const WatchInfoVersion v=watch_info_get_firmware_version();
  header_key=size<=PreferredContentSizeMedium?FONT_KEY_GOTHIC_18_BOLD:size==PreferredContentSizeLarge?FONT_KEY_GOTHIC_24_BOLD:FONT_KEY_GOTHIC_28_BOLD;
  title_key=size<PreferredContentSizeExtraLarge?FONT_KEY_GOTHIC_24_BOLD:
      v.major>4||(v.major==4&&v.minor>=44)?FONT_KEY_GOTHIC_28_BOLD:FONT_KEY_GOTHIC_28;
  footer_key=size<PreferredContentSizeExtraLarge?FONT_KEY_GOTHIC_18:FONT_KEY_GOTHIC_24;
  Window *window=window_create();Layer *layer=layer_create(GRect(0,0,200,228));
  layer_set_update_proc(layer,draw);layer_add_child(window_get_root_layer(window),layer);
  window_stack_push(window,false);app_event_loop();layer_destroy(layer);window_destroy(window);
}
