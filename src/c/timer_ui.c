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

static void header(GContext *ctx, const TimerUiView *view, const char *title,
                    GColor ink) {
  char wall_clock[8];
  const time_t timestamp=(time_t)view->now;
  const struct tm *local=localtime(&timestamp);
  const bool twenty_four=clock_is_24h_style();
  if(local) {
    strftime(wall_clock,sizeof(wall_clock),twenty_four?"%H:%M":"%I:%M",local);
    if(!twenty_four&&wall_clock[0]=='0')
      memmove(wall_clock,wall_clock+1,strlen(wall_clock));
  } else clock_copy_time_string(wall_clock,sizeof(wall_clock));
  timer_fonts_text(ctx,title,18,8,25,90,ink,GTextAlignmentLeft);
  timer_fonts_text(ctx,wall_clock,18,102,25,56,ink,GTextAlignmentRight);
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

static GPoint glass_point(int x, int y, int center, int pivot,
                           int scale, int angle) {
  const TimerGlassPoint point=timer_hourglass_transform(x,y,scale,angle);
  return GPoint(center+point.x,pivot+point.y);
}

static void glass_rect(GContext *ctx, int left, int top, int width, int height,
                        int center, int pivot, int scale, int angle) {
  GPoint points[]={glass_point(left,top,center,pivot,scale,angle),
    glass_point(left+width,top,center,pivot,scale,angle),
    glass_point(left+width,top+height,center,pivot,scale,angle),
    glass_point(left,top+height,center,pivot,scale,angle)};
  GPath path={.num_points=4,.points=points};
  gpath_draw_filled(ctx,&path);
}

static void glass_sand(GContext *ctx, const uint8_t rows[TIMER_SAND_ROWS],
                        int top, int center, int pivot, int scale, int angle) {
  /* Fill the whole chamber silhouette at once. Stock GPath scan conversion
   * can discard one-pixel-wide vertical polygons, so drawing each sand row
   * separately loses its fill when the glass reaches the paused 90deg pose.
   * The same conserved row widths form this single rigid, closed contour. */
  GPoint points[TIMER_SAND_ROWS*4];
  unsigned count=0;
  for(unsigned row=0;row<TIMER_SAND_ROWS;++row) {
    if(!rows[row])continue;
    const int left=-(int)rows[row]/2;
    points[count++]=glass_point(left,top+(int)row,center,pivot,scale,angle);
    points[count++]=glass_point(left,top+(int)row+1,center,pivot,scale,angle);
  }
  for(unsigned n=TIMER_SAND_ROWS;n>0;--n) {
    const unsigned row=n-1;
    if(!rows[row])continue;
    const int right=-(int)rows[row]/2+rows[row];
    points[count++]=glass_point(right,top+(int)row+1,center,pivot,scale,angle);
    points[count++]=glass_point(right,top+(int)row,center,pivot,scale,angle);
  }
  if(count) {
    GPath path={.num_points=count,.points=points};
    gpath_draw_filled(ctx,&path);
  }
}

static void hourglass(GContext *ctx, uint32_t remaining, uint32_t total,
                      int center, int pivot, int scale, int angle, GColor sand,
                      GColor outline, bool running, uint32_t now) {
  TimerSandFill fill;
  timer_sand_fill(remaining,total,&fill);
  graphics_context_set_fill_color(ctx,sand);
  glass_sand(ctx,fill.upper,4,center,pivot,scale,angle);
  glass_sand(ctx,fill.lower,44,center,pivot,scale,angle);
  if(running&&remaining&&angle==0) {
    /* This intentionally bold stream remains legible on the actual watch.
     * Only the two grain positions alternate on the existing second tick;
     * conserved chamber fill still comes exclusively from remaining time. */
    glass_rect(ctx,-2,36,4,12,center,pivot,scale,angle);
    const int phase=now&1u;
    glass_rect(ctx,-3,phase?54:50,6,4,center,pivot,scale,angle);
    glass_rect(ctx,-3,phase?64:60,6,4,center,pivot,scale,angle);
  }
  GPoint points[sizeof(hourglass_outline)/sizeof(hourglass_outline[0])];
  for(unsigned i=0;i<sizeof(points)/sizeof(points[0]);++i)
    points[i]=glass_point(hourglass_outline[i].x,hourglass_outline[i].y,
        center,pivot,scale,angle);
  GPath path={.num_points=sizeof(points)/sizeof(points[0]),.points=points};
  graphics_context_set_stroke_color(ctx,outline);
  graphics_context_set_stroke_width(ctx,scale<75?2:3);
  gpath_draw_outline_open(ctx,&path);
  graphics_draw_line(ctx,glass_point(-29,0,center,pivot,scale,angle),
      glass_point(29,0,center,pivot,scale,angle));
  graphics_draw_line(ctx,glass_point(-29,79,center,pivot,scale,angle),
      glass_point(29,79,center,pivot,scale,angle));
  graphics_context_set_stroke_width(ctx,1);
}

static void main_screen(GContext *ctx, const TimerUiView *view) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  const bool paused=timer->state==ACTIVITY_TIMER_PAUSED;
  const bool finished=timer->state==ACTIVITY_TIMER_FINISHED;
  const bool ready=activity_timer_ready(timer);
  const uint32_t left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
  bar(ctx,ICON_EDIT,paused||finished||ready?ICON_PLAY:ICON_PAUSE,ICON_MORE,GColorBlack);
  const char *heading=paused?"Paused":ready?"Ready":finished?"Finished":"Timer";
  header(ctx,view,heading,GColorBlack);
  graphics_context_set_stroke_color(ctx,GColorLightGray);segment(ctx,8,32,158,32);
  char clock[20],elapsed[20],support[40];
  clock_text(left,clock,sizeof(clock));
  timer_fonts_digits(ctx,clock,UI_CENTER,86,left>=3600?36:58,GColorBlack,GColorWhite);
  const int pose=timer_hourglass_pose(ready?TIMER_GLASS_READY:
      paused?TIMER_GLASS_PAUSED:TIMER_GLASS_RUNNING);
  const int angle=view->hourglass_transition?view->hourglass_angle:pose;
  hourglass(ctx,left,ready?timer->repeat_seconds:timer->configured_seconds,
      UI_CENTER,view->issue?137:145,view->issue?85:100,angle,paused?AMBER:BLUE,GColorBlack,
      timer->state==ACTIVITY_TIMER_RUNNING,view->now);
  if(ready)snprintf(support,sizeof(support),"Select to start");
  else {
    clock_text(activity_timer_elapsed(timer,view->now),elapsed,sizeof(elapsed));
    snprintf(support,sizeof(support),"Elapsed %s",elapsed);
  }
  text(ctx,support,18,220,GColorDarkGray);
}

static void list_screen(GContext *ctx, const TimerUiView *view) {
  const TimerListWindow list=timer_ui_list_window(view);
  const unsigned count=list.count,menu=list.menu;
  const bool full=!timer_collection_can_create(view->collection);
  const bool replacing=!full&&count==TIMER_COLLECTION_CAPACITY;
  const unsigned first_menu=full&&count?1:0;
  bar(ctx,menu>first_menu?ICON_UP:ICON_NONE,menu?ICON_NEXT:full?ICON_NONE:ICON_PLUS,
      menu<count?ICON_DOWN:ICON_NONE,GColorBlack);
  if(full)rect(ctx,GRect(0,0,UI_WIDTH,37),AMBER);
  else if(!menu)rect(ctx,GRect(0,0,UI_WIDTH,37),BLUE);
  header(ctx,view,full?"Timers full":menu?"Timers":"New timer",GColorBlack);
  graphics_context_set_stroke_color(ctx,GColorLightGray);
  segment(ctx,9,36,157,36);
  if(!count) {
    hourglass(ctx,1,1,UI_CENTER,107,70,180,BLUE,GColorBlack,false,view->now);
    text(ctx,"Start a timer",23,180,GColorBlack);
  } else {
    const unsigned rows=list.rows,first=list.first;
    for(unsigned row=first;row<count&&row<first+rows;++row) {
      const unsigned slot=list.slots[row];
      const ActivityTimerModel *timer=&view->collection->slots[slot];
      const bool ready=activity_timer_ready(timer);
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
      int detail_point=18;
      if(timer_fonts_text_width(detail,detail_point)>128)detail_point=16;
      if(timer_fonts_text_width(detail,detail_point)>128)detail_point=15;
      timer_fonts_text(ctx,detail,detail_point,13,y+49,128,
          selected?GColorLightGray:GColorDarkGray,GTextAlignmentLeft);
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
  text(ctx,replacing&&!menu?"Reuses oldest ready":position,
       18,223,GColorDarkGray);
}

static void editor_screen(GContext *ctx, const TimerUiView *view) {
  static const char *labels[]={"HRS","MIN","SEC"};
  const uint32_t total=view->edit[0]*3600+view->edit[1]*60+view->edit[2];
  const bool locked=view->edit[0]==24&&view->edit_field>0;
  const bool invalid=total<ACTIVITY_TIMER_MIN_SECONDS;
  bar(ctx,locked?ICON_NONE:ICON_PLUS,
      view->edit_field<2?ICON_NEXT:invalid?ICON_NONE:ICON_CHECK,
      locked?ICON_NONE:ICON_MINUS,GColorBlack);
  const bool reuse=view->editing_new&&timer_collection_count(view->collection)==TIMER_COLLECTION_CAPACITY;
  header(ctx,view,view->editing_new?(reuse?"Replace":"New timer"):
       view->editing_ready?"Saved timer":"Remaining",GColorBlack);
  hourglass(ctx,1,1,UI_CENTER,65,50,180,BLUE,GColorBlack,false,view->now);
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
    timer_fonts_text(ctx,labels[field],18,x,170,50,selected?GColorBlack:GColorDarkGray,GTextAlignmentCenter);
  }
  if(view->edit[0]==24)text(ctx,"24 hours max",18,188,GColorDarkGray);
  else if(reuse)text(ctx,"Reuses oldest ready",18,188,GColorDarkGray);
  else text(ctx,view->edit_field==0?"Back to cancel":
      view->edit_field==1?"Back for hours":"Back for minutes",18,188,GColorDarkGray);
  text(ctx,invalid?"At least 5 seconds":
       view->edit_field<2?"Select: next":view->editing_new?"Select: start":"Select: save",
       18,216,GColorBlack);
}

static void alarm_screen(GContext *ctx, const TimerUiView *view) {
  rect(ctx,GRect(0,0,200,228),GColorBlack);
  bar(ctx,ICON_SNOOZE,ICON_CHECK,ICON_NONE,GColorDarkGray);
  const bool issue=view->issue&&view->issue[0];
  header(ctx,view,"Time’s up",GColorWhite);
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  char duration[20];clock_text(timer->repeat_seconds,duration,sizeof(duration));
  timer_fonts_digits(ctx,duration,UI_CENTER,issue?65:77,
      timer->repeat_seconds>=3600?(issue?28:32):(issue?36:47),GColorWhite,GColorBlack);
  hourglass(ctx,0,1,UI_CENTER,issue?114:136,issue?85:100,0,BLUE,GColorWhite,false,view->now);
  text(ctx,"Select: Done",18,issue?167:200,GColorWhite);
  text(ctx,"Up: Snooze 1 min",18,issue?183:218,GColorWhite);
}

static void options_screen(GContext *ctx, const TimerUiView *view) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  char duration[20],reset[32];
  duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
  snprintf(reset,sizeof(reset),"Reset %s",duration);
  const char *options[]={activity_timer_ready(timer)?
      "Change duration":"Change remaining",reset,"Delete timer"};
  bar(ctx,view->option?ICON_UP:ICON_NONE,ICON_CHECK,view->option<2?ICON_DOWN:ICON_NONE,GColorBlack);
  const unsigned count=timer_collection_count(view->collection);
  unsigned position=1;
  for(unsigned row=0;row<count;++row)
    if(timer_collection_visible_slot(view->collection,row)==(int)view->selected)position=row+1;
  char heading[32],context[40];
  if(count>1)snprintf(heading,sizeof(heading),"Timer %u/%u",position,count);
  else snprintf(heading,sizeof(heading),"Options");
  snprintf(context,sizeof(context),"%s · %s",duration,
      timer->state==ACTIVITY_TIMER_RUNNING?"Running":timer->state==ACTIVITY_TIMER_PAUSED?"Paused":
      activity_timer_ready(timer)?"Ready":"Time’s up");
  header(ctx,view,heading,GColorBlack);
  text(ctx,context,18,46,GColorDarkGray);
  graphics_context_set_stroke_color(ctx,GColorLightGray);segment(ctx,10,49,156,49);
  for(unsigned row=0;row<3;++row) {
    const int y=58+(int)row*43;
    const bool selected=view->option==row;
    if(selected)rect(ctx,GRect(0,y,UI_WIDTH,41),GColorBlack);
    timer_fonts_text(ctx,options[row],19,13,y+28,149,selected?GColorWhite:GColorBlack,GTextAlignmentLeft);
  }
  text(ctx,view->status&&view->status[0]?view->status:
      activity_timer_ready(timer)?"Back to timer":"Back to keep timing",18,220,GColorDarkGray);
}

static void confirm_screen(GContext *ctx, const TimerUiView *view) {
  const bool issue=view->issue&&view->issue[0];
  bar(ctx,view->confirm_choice?ICON_UP:ICON_NONE,ICON_CHECK,
      view->confirm_choice?ICON_NONE:ICON_DOWN,GColorBlack);
  header(ctx,view,"Delete",GColorBlack);
  graphics_context_set_stroke_color(ctx,GColorLightGray);segment(ctx,8,32,158,32);
  if(!issue)icon(ctx,ICON_TRASH,UI_CENTER,55,GColorBlack);
  text(ctx,"Delete timer?",23,issue?103:99,GColorBlack);
  text(ctx,"Cannot be undone",18,issue?127:123,GColorDarkGray);
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
    rect(ctx,GRect(0,confirm?32:187,UI_WIDTH,confirm?35:41),AMBER);
    text(ctx,view->issue,14,confirm?46:202,GColorBlack);
    text(ctx,"Keep Timer open",14,confirm?62:220,GColorBlack);
  }
}
