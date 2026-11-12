#pragma once

#include <pebble.h>
#include "timer_ui_state.h"

void timer_ui_draw(GContext *context, const TimerUiView *view);
int timer_ui_issue_height(const TimerUiView *view);
int timer_ui_editor_top(const TimerUiView *view);
GRect timer_ui_editor_field_rect(const TimerUiView *view,unsigned field);
int16_t timer_ui_context_height(const TimerUiView *view);
void timer_ui_draw_context(GContext *ctx,const TimerUiView *view);
int16_t timer_ui_action_row_height(const TimerUiView *view,unsigned row);
void timer_ui_draw_action_row(GContext *ctx,const Layer *cell,const TimerUiView *view,unsigned row);
int16_t timer_ui_row_height(const TimerUiView *view, unsigned slot);
void timer_ui_draw_row(GContext *ctx, const Layer *cell, const TimerUiView *view,
                       unsigned slot);

#ifdef PBL_ROUND
unsigned timer_ui_context_copy(const TimerUiView *,char lines[3][96]);
int timer_ui_round_header_height(const TimerUiView *view);
int16_t timer_ui_new_row_height(const TimerUiView *view);
void timer_ui_draw_new_row(GContext *,const Layer *,const TimerUiView *);
void timer_ui_draw_rail(GContext *,const TimerUiView *);
#endif
