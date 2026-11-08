#pragma once

#include <pebble.h>
#include "timer_ui_state.h"

void timer_ui_draw(GContext *context, const TimerUiView *view);
int16_t timer_ui_row_height(const TimerUiView *view, unsigned slot);
void timer_ui_draw_row(GContext *ctx, const Layer *cell, const TimerUiView *view,
                       unsigned slot);
