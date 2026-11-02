/* Emulator-only external Wakeup owner. Never part of the shipping Timer app.
 * A distinct UUID creates the SDK's cross-app 60-second Wakeup exclusion.
 * Select cancels only this fixture's Wakeups, so the native Timer can recover. */
#include <pebble.h>

static Window *s_window;
static TextLayer *s_text;

static void prv_clear(ClickRecognizerRef recognizer, void *context) {
  (void)recognizer;
  (void)context;
  wakeup_cancel_all();
  text_layer_set_text(s_text, "Conflict cleared\nNo fixture Wakeup\nBack to leave");
}

static void prv_clicks(void *context) {
  (void)context;
  window_single_click_subscribe(BUTTON_ID_SELECT, prv_clear);
}

int main(void) {
  wakeup_cancel_all();
  const WakeupId wakeup = wakeup_schedule(time(NULL) + 600, 1, false);
  s_window = window_create();
  window_set_background_color(s_window, GColorWhite);
  window_set_click_config_provider(s_window, prv_clicks);
  s_text = text_layer_create(GRect(5, 48, 190, 145));
  text_layer_set_font(s_text, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  text_layer_set_text_alignment(s_text, GTextAlignmentCenter);
  text_layer_set_text(s_text, wakeup >= 0 ? "External Wakeup\n10 minutes ahead\nSelect to clear" : "Fixture scheduling failed");
  layer_add_child(window_get_root_layer(s_window), text_layer_get_layer(s_text));
  window_stack_push(s_window, false);
  app_event_loop();
  /* Keep the Wakeup while Timer is active. Explicit Select owns cleanup. */
  text_layer_destroy(s_text);
  window_destroy(s_window);
}
