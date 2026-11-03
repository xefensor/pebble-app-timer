#include "timer_ui.h"
#include "timer_fonts.h"
#include "timer_hourglass.h"

#include <stdio.h>
#include <string.h>

#define UI_WIDTH 166
#define UI_CENTER 83
#define BLUE GColorPictonBlue
#define AMBER GColorChromeYellow

typedef enum {
  ICON_NONE, ICON_EDIT, ICON_PAUSE, ICON_PLAY, ICON_MORE, ICON_PLUS,
  ICON_MINUS, ICON_NEXT, ICON_CHECK, ICON_SNOOZE, ICON_UP, ICON_DOWN,
  ICON_TRASH
} Icon;

static void rect(GContext *ctx, GRect frame, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, frame, 0, GCornerNone);
}

static void segment(GContext *ctx, int x1, int y1, int x2, int y2) {
  graphics_draw_line(ctx, GPoint(x1, y1), GPoint(x2, y2));
}

static void text(GContext *ctx, const char *value, int point, int baseline,
                 GColor ink) {
  timer_fonts_text(ctx, value, point, 4, baseline, UI_WIDTH - 8,
                   ink, GTextAlignmentCenter);
}

static void clock_text(uint32_t seconds, char *buffer, size_t capacity) {
  if (seconds >= 3600)
    snprintf(buffer, capacity, "%lu:%02lu:%02lu", (unsigned long)(seconds / 3600),
             (unsigned long)((seconds / 60) % 60), (unsigned long)(seconds % 60));
  else
    snprintf(buffer, capacity, "%02lu:%02lu", (unsigned long)(seconds / 60),
             (unsigned long)(seconds % 60));
}

static void duration_text(uint32_t seconds, char *buffer, size_t capacity,
                          bool title) {
  if (!(seconds % 3600))
    snprintf(buffer, capacity, title ? "%lu HOUR TIMER" : "%lu h",
             (unsigned long)(seconds / 3600));
  else if (!(seconds % 60))
    snprintf(buffer, capacity, title ? "%lu MIN TIMER" : "%lu min",
             (unsigned long)(seconds / 60));
  else {
    char duration[16];
    clock_text(seconds, duration, sizeof(duration));
    snprintf(buffer, capacity, title ? "%s TIMER" : "%s", duration);
  }
}

static int digit_width(const char *value, int point) {
  int units = 0;
  for (; *value; ++value) units += *value == ':' ? 25 : 58;
  return (units * point + 50) / 100;
}

static void icon(GContext *ctx, Icon which, int x, int y, GColor color) {
  graphics_context_set_stroke_color(ctx, color);
  graphics_context_set_fill_color(ctx, color);
  graphics_context_set_stroke_width(ctx, 2);
  switch (which) {
    case ICON_EDIT: {
      GPoint points[] = {{x-7,y+6},{x+4,y-5},{x+8,y-1},{x-3,y+10},{x-8,y+11}};
      GPath path = {.num_points=5,.points=points};
      gpath_draw_outline(ctx, &path);
      segment(ctx,x+2,y-3,x+6,y+1);
      break;
    }
    case ICON_PAUSE:
      rect(ctx,GRect(x-7,y-7,4,14),color);
      rect(ctx,GRect(x+3,y-7,4,14),color);
      break;
    case ICON_PLAY: {
      GPoint points[] = {{x-5,y-8},{x+8,y},{x-5,y+8}};
      GPath path = {.num_points=3,.points=points};
      gpath_draw_filled(ctx,&path);
      break;
    }
    case ICON_MORE:
      for (int offset=-7;offset<=7;offset+=7) graphics_fill_circle(ctx,GPoint(x+offset,y),2);
      break;
    case ICON_PLUS:
      segment(ctx,x,y-7,x,y+7);
      /* fall through */
    case ICON_MINUS:
      segment(ctx,x-7,y,x+7,y);
      break;
    case ICON_NEXT:
      segment(ctx,x-7,y,x+7,y);segment(ctx,x+1,y-6,x+7,y);segment(ctx,x+1,y+6,x+7,y);
      break;
    case ICON_CHECK:
      segment(ctx,x-8,y,x-2,y+6);segment(ctx,x-2,y+6,x+9,y-7);
      break;
    case ICON_SNOOZE:
      graphics_draw_circle(ctx,GPoint(x,y),8);
      segment(ctx,x-8,y-8,x-8,y-1);segment(ctx,x-8,y-1,x-1,y-1);
      segment(ctx,x,y-5,x,y);segment(ctx,x,y,x+4,y+2);
      break;
    case ICON_UP:
      segment(ctx,x-6,y+3,x,y-3);segment(ctx,x,y-3,x+6,y+3);
      break;
    case ICON_DOWN:
      segment(ctx,x-6,y-3,x,y+3);segment(ctx,x,y+3,x+6,y-3);
      break;
    case ICON_TRASH:
      segment(ctx,x-7,y-5,x+7,y-5);segment(ctx,x-5,y-4,x-5,y+8);
      segment(ctx,x-5,y+8,x+5,y+8);segment(ctx,x+5,y+8,x+5,y-4);
      segment(ctx,x-2,y-8,x+2,y-8);segment(ctx,x-2,y-1,x-2,y+5);
      segment(ctx,x+2,y-1,x+2,y+5);
      break;
    default: break;
  }
  graphics_context_set_stroke_width(ctx,1);
}

static void bar(GContext *ctx, Icon up, Icon select, Icon down, GColor background) {
  rect(ctx,GRect(UI_WIDTH,0,34,228),background);
  icon(ctx,up,183,44,GColorWhite);
  icon(ctx,select,183,114,GColorWhite);
  icon(ctx,down,183,184,GColorWhite);
}

static int scaled(int value, int percent) {
  return value < 0 ? -((-value*percent+50)/100) : (value*percent+50)/100;
}

typedef struct { int8_t x,y; } OutlinePoint;
static const OutlinePoint hourglass_outline[] = {
  {-27,1},
  {-27,5},
  {-26,9},
  {-26,13},
  {-25,16},
  {-23,19},
  {-21,22},
  {-19,24},
  {-17,27},
  {-14,30},
  {-11,32},
  {-7,35},
  {-3,38},
  {-2,39},
  {-2,40},
  {-3,41},
  {-7,44},
  {-11,47},
  {-14,49},
  {-17,52},
  {-19,55},
  {-21,57},
  {-23,60},
  {-25,63},
  {-26,66},
  {-26,70},
  {-27,74},
  {-27,78},
  {27,78},
  {27,74},
  {26,70},
  {26,66},
  {25,63},
  {23,60},
  {21,57},
  {19,55},
  {17,52},
  {14,49},
  {11,47},
  {7,44},
  {3,41},
  {2,40},
  {2,39},
  {3,38},
  {7,35},
  {11,32},
  {14,30},
  {17,27},
  {19,24},
  {21,22},
  {23,19},
  {25,16},
  {26,13},
  {26,9},
  {27,5},
  {27,1}
};

static void sand_row(GContext *ctx, int center, int top, unsigned row,
                     unsigned pixels, int scale) {
  if (!pixels) return;
  const int left = center + scaled(-(int)pixels/2,scale);
  const int right = center + scaled((int)pixels-(int)pixels/2,scale);
  const int y = top + scaled((int)row,scale);
  const int next = top + scaled((int)row+1,scale);
  graphics_fill_rect(ctx,GRect(left,y,right>left?right-left:1,next>y?next-y:1),0,GCornerNone);
}

static void hourglass(GContext *ctx, uint32_t remaining, uint32_t total,
                      int center, int top, int scale, GColor sand,
                      GColor outline, bool running, uint32_t now) {
  TimerSandFill fill;
  timer_sand_fill(remaining,total,&fill);
  graphics_context_set_fill_color(ctx,sand);
  for (unsigned row=0;row<TIMER_SAND_ROWS;++row) {
    sand_row(ctx,center,top+scaled(4,scale),row,fill.upper[row],scale);
    sand_row(ctx,center,top+scaled(44,scale),row,fill.lower[row],scale);
  }
  if (running && remaining) {
    rect(ctx,GRect(center-scaled(1,scale),top+scaled(36,scale),
                  scaled(2,scale),scaled(7,scale)),sand);
    const int phase = now & 1u;
    rect(ctx,GRect(center-scaled(1,scale),top+scaled(phase?47:51,scale),
                  scaled(2,scale),scaled(3,scale)),sand);
    rect(ctx,GRect(center-scaled(1,scale),top+scaled(phase?55:59,scale),
                  scaled(2,scale),scaled(2,scale)),sand);
  }
  GPoint points[sizeof(hourglass_outline)/sizeof(hourglass_outline[0])];
  for (unsigned i=0;i<sizeof(points)/sizeof(points[0]);++i)
    points[i]=GPoint(center+scaled(hourglass_outline[i].x,scale),
                     top+scaled(hourglass_outline[i].y,scale));
  GPath path={.num_points=sizeof(points)/sizeof(points[0]),.points=points};
  graphics_context_set_stroke_color(ctx,outline);
  graphics_context_set_stroke_width(ctx,scale<75?1:3);
  gpath_draw_outline_open(ctx,&path);
  segment(ctx,center-scaled(29,scale),top,center+scaled(29,scale),top);
  segment(ctx,center-scaled(29,scale),top+scaled(79,scale),
          center+scaled(29,scale),top+scaled(79,scale));
  graphics_context_set_stroke_width(ctx,1);
}

static void main_screen(GContext *ctx, const TimerUiView *view) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  const bool paused=timer->state==ACTIVITY_TIMER_PAUSED;
  const bool finished=timer->state==ACTIVITY_TIMER_FINISHED;
  const bool ready=finished&&timer->acknowledged;
  const uint32_t left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
  const int offset=paused||finished?8:0;
  bar(ctx,ICON_EDIT,paused||finished?ICON_PLAY:ICON_PAUSE,ICON_MORE,GColorBlack);
  if(paused||finished) {
    rect(ctx,GRect(0,0,UI_WIDTH,27),paused?AMBER:BLUE);
    text(ctx,paused?"PAUSED":ready?"READY":"FINISHED",14,19,GColorBlack);
  }
  char label[32],clock[20],elapsed[20],support[36];
  duration_text(timer->repeat_seconds,label,sizeof(label),true);
  text(ctx,label,15,paused||finished?46:23,GColorBlack);
  clock_text(left,clock,sizeof(clock));
  timer_fonts_digits(ctx,clock,UI_CENTER,82+offset,left>=3600?36:58,GColorBlack,GColorWhite);
  hourglass(ctx,left,ready?timer->repeat_seconds:timer->configured_seconds,UI_CENTER,99+offset,100,
            paused?AMBER:BLUE,GColorBlack,!paused&&timer->state==ACTIVITY_TIMER_RUNNING,view->now);
  clock_text(activity_timer_elapsed(timer,view->now),elapsed,sizeof(elapsed));
  snprintf(support,sizeof(support),"Elapsed %s",elapsed);
  text(ctx,ready?"Select to repeat":support,16,211,GColorBlack);
}

static void list_screen(GContext *ctx, const TimerUiView *view) {
  const unsigned count=timer_collection_count(view->collection);
  const unsigned menu=view->menu>count?count:view->menu;
  const bool full=!timer_collection_can_create(view->collection);
  const bool replacing=!full&&count==TIMER_COLLECTION_CAPACITY;
  const unsigned first_menu=full&&count?1:0;
  bar(ctx,menu>first_menu?ICON_UP:ICON_NONE,menu?ICON_NEXT:full?ICON_NONE:ICON_PLUS,
      menu<count?ICON_DOWN:ICON_NONE,GColorBlack);
  if(full)rect(ctx,GRect(0,0,UI_WIDTH,37),AMBER);
  else if(!menu)rect(ctx,GRect(0,0,UI_WIDTH,37),BLUE);
  timer_fonts_text(ctx,full?"TIMERS FULL":menu?"TIMERS":"NEW TIMER",16,12,24,144,GColorBlack,GTextAlignmentLeft);
  if(!full)icon(ctx,ICON_PLUS,147,18,GColorBlack);
  graphics_context_set_stroke_color(ctx,GColorLightGray);
  segment(ctx,9,36,157,36);
  if(!count) {
    hourglass(ctx,1,1,UI_CENTER,77,70,BLUE,GColorBlack,false,view->now);
    text(ctx,"Start a timer",18,178,GColorBlack);
  } else {
    const unsigned rows=view->issue&&view->issue[0]?2:3;
    unsigned first=menu>1?menu-2:0;
    if(first+rows>count)first=count>rows?count-rows:0;
    for(unsigned row=first;row<count&&row<first+rows;++row) {
      const int slot=timer_collection_visible_slot(view->collection,row);
      if(slot<0)continue;
      const ActivityTimerModel *timer=&view->collection->slots[slot];
      const bool ready=timer->state==ACTIVITY_TIMER_FINISHED&&timer->acknowledged;
      const uint32_t left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
      const bool selected=menu==row+1;
      const GColor foreground=selected?GColorWhite:GColorBlack;
      const GColor background=selected?GColorBlack:GColorWhite;
      const int y=40+(int)(row-first)*56;
      rect(ctx,GRect(0,y,UI_WIDTH,54),background);
      char clock[20],duration[24],detail[52];
      clock_text(left,clock,sizeof(clock));
      const int point=left>=3600?24:30;
      timer_fonts_digits(ctx,clock,13+(digit_width(clock,point)+1)/2,y+29,point,foreground,background);
      duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
      snprintf(detail,sizeof(detail),"%s · %s",timer->state==ACTIVITY_TIMER_RUNNING?"Running":
               timer->state==ACTIVITY_TIMER_PAUSED?"Paused":ready?"Ready":"Time’s up",duration);
      timer_fonts_text(ctx,detail,14,13,y+47,126,selected?GColorLightGray:GColorDarkGray,GTextAlignmentLeft);
      const uint32_t total=ready?timer->repeat_seconds:timer->configured_seconds;
      const unsigned fill=total?
        (unsigned)(((uint64_t)left*29+total/2)/total):0;
      const unsigned bounded=fill>29?29:fill;
      rect(ctx,GRect(145,y+13,4,29),selected?GColorDarkGray:GColorLightGray);
      if(bounded)rect(ctx,GRect(145,y+13+29-bounded,4,bounded),timer->state==ACTIVITY_TIMER_PAUSED?AMBER:BLUE);
    }
    if(count>rows) {
      const unsigned height=rows*56-5;
      const unsigned thumb=(height*rows+count/2)/count;
      const unsigned offset=first*(height-thumb)/(count-rows);
      rect(ctx,GRect(159,43,2,height),GColorLightGray);
      rect(ctx,GRect(159,43+offset,2,thumb),GColorBlack);
    }
  }
  char position[32];
  if(menu)snprintf(position,sizeof(position),"Timer %u of %u",menu,count);
  else if(count)snprintf(position,sizeof(position),"%u of 8 saved",count);
  else snprintf(position,sizeof(position),"Select to create");
  text(ctx,replacing&&!menu?"Replaces oldest ready":position,
       replacing&&!menu?14:16,222,GColorDarkGray);
}

static void editor_screen(GContext *ctx, const TimerUiView *view) {
  static const char *labels[]={"HOURS","MIN","SEC"};
  const uint32_t total=view->edit[0]*3600+view->edit[1]*60+view->edit[2];
  const bool locked=view->edit[0]==24&&view->edit_field>0;
  const bool invalid=total<ACTIVITY_TIMER_MIN_SECONDS;
  bar(ctx,locked?ICON_NONE:ICON_PLUS,
      view->edit_field<2?ICON_NEXT:invalid?ICON_NONE:ICON_CHECK,
      locked?ICON_NONE:ICON_MINUS,GColorBlack);
  const bool reuse=view->editing_new&&timer_collection_count(view->collection)==TIMER_COLLECTION_CAPACITY;
  text(ctx,view->editing_new?(reuse?"REPLACE SAVED":"NEW TIMER"):
       view->editing_ready?"EDIT SAVED TIMER":"CHANGE REMAINING",15,24,GColorBlack);
  hourglass(ctx,total?1:0,1,UI_CENTER,43,50,BLUE,GColorBlack,false,view->now);
  for(unsigned field=0;field<3;++field) {
    const int x=5+(int)field*53;
    const bool selected=view->edit_field==field;
    const GColor background=selected?(locked?GColorLightGray:invalid&&field==2?AMBER:BLUE):GColorWhite;
    if(selected) {
      graphics_context_set_fill_color(ctx,background);
      graphics_fill_rect(ctx,GRect(x,104,50,45),3,GCornersAll);
    }
    char value[4];snprintf(value,sizeof(value),"%02lu",(unsigned long)view->edit[field]);
    timer_fonts_digits(ctx,value,x+25,138,36,GColorBlack,background);
    timer_fonts_text(ctx,labels[field],14,x,168,50,selected?GColorBlack:GColorDarkGray,GTextAlignmentCenter);
  }
  if(view->edit[0]==24)text(ctx,"24 hours max",14,186,GColorDarkGray);
  else if(reuse)text(ctx,"Replaces oldest ready",14,186,GColorDarkGray);
  else text(ctx,view->edit_field==0?"Back to cancel":
      view->edit_field==1?"Back for hours":"Back for minutes",14,186,GColorDarkGray);
  text(ctx,invalid?"Set at least 5 seconds":
       view->edit_field<2?"Select for next field":view->editing_new?"Select to start":"Select to save",
       16,206,GColorBlack);
}

static void alarm_screen(GContext *ctx, const TimerUiView *view) {
  rect(ctx,GRect(0,0,200,228),GColorBlack);
  bar(ctx,ICON_SNOOZE,ICON_CHECK,ICON_NONE,GColorDarkGray);
  const bool issue=view->issue&&view->issue[0];
  text(ctx,"TIME’S UP",issue?19:23,issue?24:28,GColorWhite);
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  char duration[20];clock_text(timer->repeat_seconds,duration,sizeof(duration));
  timer_fonts_digits(ctx,duration,UI_CENTER,issue?65:77,
      timer->repeat_seconds>=3600?(issue?28:32):(issue?36:47),GColorWhite,GColorBlack);
  hourglass(ctx,0,1,UI_CENTER,issue?80:96,issue?85:100,BLUE,GColorWhite,false,view->now);
  text(ctx,"Select: Done",15,issue?167:200,GColorWhite);
  text(ctx,"Up: Snooze 1 min",14,issue?183:217,GColorWhite);
}

static void options_screen(GContext *ctx, const TimerUiView *view) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  char duration[20],restart[32];
  duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
  snprintf(restart,sizeof(restart),"Restart %s",duration);
  const char *options[]={timer->state==ACTIVITY_TIMER_FINISHED&&timer->acknowledged?
      "Change duration":"Change remaining",restart,"Delete timer"};
  bar(ctx,view->option?ICON_UP:ICON_NONE,ICON_CHECK,view->option<2?ICON_DOWN:ICON_NONE,GColorBlack);
  const unsigned count=timer_collection_count(view->collection);
  unsigned position=1;
  for(unsigned row=0;row<count;++row)
    if(timer_collection_visible_slot(view->collection,row)==(int)view->selected)position=row+1;
  char heading[32],context[40];
  if(count>1)snprintf(heading,sizeof(heading),"TIMER %u OF %u",position,count);
  else snprintf(heading,sizeof(heading),"TIMER OPTIONS");
  snprintf(context,sizeof(context),"%s · %s",duration,
      timer->state==ACTIVITY_TIMER_RUNNING?"Running":timer->state==ACTIVITY_TIMER_PAUSED?"Paused":
      timer->acknowledged?"Ready":"Time’s up");
  text(ctx,heading,16,24,GColorBlack);
  text(ctx,context,14,43,GColorDarkGray);
  graphics_context_set_stroke_color(ctx,GColorLightGray);segment(ctx,10,49,156,49);
  for(unsigned row=0;row<3;++row) {
    const int y=58+(int)row*43;
    const bool selected=view->option==row;
    if(selected)rect(ctx,GRect(0,y,UI_WIDTH,41),GColorBlack);
    timer_fonts_text(ctx,options[row],19,13,y+28,149,selected?GColorWhite:GColorBlack,GTextAlignmentLeft);
  }
  text(ctx,view->status&&view->status[0]?view->status:"Back to keep timing",14,214,GColorDarkGray);
}

static void confirm_screen(GContext *ctx, const TimerUiView *view) {
  const bool issue=view->issue&&view->issue[0];
  bar(ctx,view->confirm_choice?ICON_UP:ICON_NONE,ICON_CHECK,
      view->confirm_choice?ICON_NONE:ICON_DOWN,GColorBlack);
  icon(ctx,ICON_TRASH,UI_CENTER,issue?55:43,GColorBlack);
  text(ctx,"Delete timer?",23,issue?95:89,GColorBlack);
  text(ctx,"This cannot be undone.",15,issue?119:113,GColorDarkGray);
  rect(ctx,GRect(0,141,UI_WIDTH,32),view->confirm_choice?GColorWhite:GColorBlack);
  text(ctx,"Keep timer",18,163,view->confirm_choice?GColorBlack:GColorWhite);
  if(view->confirm_choice)rect(ctx,GRect(0,177,UI_WIDTH,32),AMBER);
  text(ctx,"Delete timer",18,199,GColorBlack);
}

void timer_ui_draw(GContext *ctx, const TimerUiView *view) {
  rect(ctx,GRect(0,0,200,228),GColorWhite);
  if(!view||!view->collection)return;
  switch(view->screen) {
    case SCREEN_LIST:list_screen(ctx,view);break;
    case SCREEN_EDITOR:editor_screen(ctx,view);break;
    case SCREEN_ALARM:alarm_screen(ctx,view);break;
    case SCREEN_OPTIONS:options_screen(ctx,view);break;
    case SCREEN_CONFIRM:confirm_screen(ctx,view);break;
    default:main_screen(ctx,view);break;
  }
  if(view->issue&&view->issue[0]) {
    const bool confirm=view->screen==SCREEN_CONFIRM;
    rect(ctx,GRect(0,confirm?0:187,UI_WIDTH,confirm?35:41),AMBER);
    text(ctx,view->issue,14,confirm?14:202,GColorBlack);
    text(ctx,"Keep Timer open",14,confirm?30:220,GColorBlack);
  }
}

bool timer_ui_needs_tick(const TimerUiView *view) {
  if(!view||!view->collection)return false;
  if(view->screen==SCREEN_DETAIL&&view->selected<TIMER_COLLECTION_CAPACITY)
    return view->collection->slots[view->selected].state==ACTIVITY_TIMER_RUNNING;
  if(view->screen==SCREEN_LIST)
    for(unsigned i=0;i<TIMER_COLLECTION_CAPACITY;++i)
      if(timer_collection_visible(view->collection,i)&&
         view->collection->slots[i].state==ACTIVITY_TIMER_RUNNING)return true;
  return false;
}
