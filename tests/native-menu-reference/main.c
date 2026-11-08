/* Emery-only comparison fixture. Uses the production model and row renderer.
 * This separate UUID never reads/writes Timer storage or owns alarms. */
#include <pebble.h>
#include "timer_ui.h"
#include "timer_fonts.h"
static Window *s_window,*s_detail;
static MenuLayer *s_menu;
static Layer *s_chrome,*s_detail_canvas;
static TimerCollection s_collection;
static TimerUiView s_view;
static uint16_t rows(MenuLayer *m,uint16_t section,void *c) {
  (void)m;(void)section;(void)c;return 8;
}
#if REFERENCE_MODE != 0
static int16_t height(MenuLayer *m,MenuIndex *i,void *c) {
  (void)m;(void)c;return timer_ui_row_height(&s_view,i->row);
}
#endif
static void draw_row(GContext *ctx,const Layer *cell,MenuIndex *i,void *c) {
  (void)c;
#if REFERENCE_MODE == 0
  (void)i;menu_cell_basic_draw(ctx,cell,"10:00","Paused",NULL);
#else
  timer_ui_draw_row(ctx,cell,&s_view,i->row);
#endif
}
static void draw(Layer *layer,GContext *ctx) {
  TimerUiView view=s_view;view.screen=layer==s_chrome?SCREEN_LIST:SCREEN_DETAIL;
  view.now=(uint32_t)time(NULL);timer_ui_draw(ctx,&view);
}
static void changed(MenuLayer *m,MenuIndex next,MenuIndex old,void *c) {
  (void)m;(void)old;(void)c;s_view.menu=next.row+1;layer_mark_dirty(s_chrome);
}
static void select_row(MenuLayer *m,MenuIndex *i,void *c) {
  (void)m;(void)c;s_view.selected=i->row;window_stack_push(s_detail,true);
}
int main(void) {
  timer_fonts_init();const uint32_t now=(uint32_t)time(NULL);
  timer_collection_init(&s_collection,42,now);
  for(unsigned i=0;i<8;++i){timer_collection_create(&s_collection,600,now);timer_collection_pause(&s_collection,i,now);}
  const PreferredContentSize size=preferred_content_size();
  s_view=(TimerUiView){.collection=&s_collection,.screen=SCREEN_LIST,.menu=1,
    .text_size=size<=PreferredContentSizeMedium?0:size==PreferredContentSizeLarge?1:2};
  s_window=window_create();s_detail=window_create();
  s_chrome=layer_create(GRect(0,0,200,228));layer_set_update_proc(s_chrome,draw);
  layer_add_child(window_get_root_layer(s_window),s_chrome);
  s_detail_canvas=layer_create(GRect(0,0,200,228));layer_set_update_proc(s_detail_canvas,draw);
  layer_add_child(window_get_root_layer(s_detail),s_detail_canvas);
  s_menu=menu_layer_create(GRect(0,50,166,178));
  menu_layer_set_callbacks(s_menu,NULL,(MenuLayerCallbacks){.get_num_rows=rows,
#if REFERENCE_MODE != 0
    .get_cell_height=height,
#endif
    .draw_row=draw_row,.selection_changed=changed,.select_click=select_row});
  menu_layer_set_click_config_onto_window(s_menu,s_window);
  menu_layer_set_center_focused(s_menu,REFERENCE_MODE==1);
  layer_add_child(window_get_root_layer(s_window),menu_layer_get_layer(s_menu));
  window_stack_push(s_window,false);app_event_loop();
  menu_layer_destroy(s_menu);layer_destroy(s_detail_canvas);window_destroy(s_detail);
  layer_destroy(s_chrome);window_destroy(s_window);timer_fonts_deinit();
}
