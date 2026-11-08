#include "timer_ui.h"
#include "localization.h"
#include "timer_fonts.h"
#include "timer_hourglass.h"
#include <pebble-xoce-ui/pebble-xoce-ui.h>

#include <stdio.h>
#include <string.h>

#define UI_WIDTH (200 - ACTION_BAR_WIDTH)
#define UI_CENTER (UI_WIDTH / 2)
#define BLUE GColorPictonBlue
#define AMBER GColorChromeYellow

/* The event-loop renderer applies one semantic palette to a complete frame. */
static bool s_dark;
static GColor ui_bg(void) { return s_dark ? GColorBlack : GColorWhite; }
static GColor ui_fg(void) { return s_dark ? GColorWhite : GColorBlack; }
static GColor ui_muted(void) { return s_dark ? GColorLightGray : GColorDarkGray; }
static GColor ui_line(void) { return s_dark ? GColorDarkGray : GColorLightGray; }

static void rect(GContext *ctx, GRect frame, GColor color) {
  graphics_context_set_fill_color(ctx, color);
  graphics_fill_rect(ctx, frame, 0, GCornerNone);
}

/* Only moving selection frames need direct pixels. Invert neutral ink and
 * surface together, so a travelling highlight never hides labels.
 * Keep the blue progress / amber warning semantics intact. Emery is 8-bit;
 * focus loss cancels motion before any covering-window transition. */
static void moving_selection(GContext *ctx, const TimerUiView *view) {
  if (!view->selection_transition) return;
  const TimerMotionRect r=view->selection_rect;
  GBitmap *frame=graphics_capture_frame_buffer_format(ctx,GBitmapFormat8Bit);
  if (!frame) return;
  const int top=r.y<0?0:r.y,bottom=r.y+r.h>228?228:r.y+r.h;
  for (int y=top;y<bottom;++y) {
    const GBitmapDataRowInfo row=gbitmap_get_data_row_info(frame,y);
    const int first=r.x<row.min_x?row.min_x:r.x;
    int last=r.x+r.w-1>row.max_x?row.max_x:r.x+r.w-1;
    if(last>=UI_WIDTH)last=UI_WIDTH-1;
    for(int x=first;x<=last;++x) {
      const uint8_t c=row.data[x],red=(c>>4)&3,green=(c>>2)&3,blue=c&3;
      if(red==green&&green==blue)row.data[x]=0xc0|((~c)&0x3f);
    }
  }
  graphics_release_frame_buffer(ctx,frame);
}

static void label(GContext *ctx,const char *value,int point,GRect bounds,GColor ink,GTextAlignment align) {
  timer_fonts_text_box(ctx,app_text(value),point,bounds,ink,align);
}
static void centered_label(GContext *ctx,const char *value,int point,int top,int height,GColor ink) {
  label(ctx,value,point,GRect(XOCE_UI_INSET,top,UI_WIDTH-2*XOCE_UI_INSET,height),ink,GTextAlignmentCenter);
}
static void footer(GContext *ctx,const char *value,int point) {
  rect(ctx,GRect(0,198,UI_WIDTH,30),ui_bg());
  label(ctx,value,point,GRect(XOCE_UI_INSET,198,UI_WIDTH-2*XOCE_UI_INSET,30),ui_muted(),GTextAlignmentCenter);
}
static unsigned size_index(const TimerUiView *view) {
  return view->text_size>2?2:view->text_size;
}
static void header(GContext *ctx,const TimerUiView *view,const char *title,GColor ink) {
  char wall_clock[8];const time_t timestamp=(time_t)view->now;const struct tm *local=localtime(&timestamp);
  const bool twenty_four=clock_is_24h_style();
  if(local){strftime(wall_clock,sizeof(wall_clock),twenty_four?"%H:%M":"%I:%M",local);if(!twenty_four&&wall_clock[0]=='0')memmove(wall_clock,wall_clock+1,strlen(wall_clock));}
  else clock_copy_time_string(wall_clock,sizeof(wall_clock));
  const int point=22+2*(int)size_index(view);
  const int clock_width=timer_fonts_text_width(wall_clock,point);
  const int clock_x=UI_WIDTH-XOCE_UI_INSET-clock_width;
  /* Allocate the full measured clock first. Only the translated label fits or
   * truncates; the clock never wraps or loses its last digit. */
  timer_fonts_text_box(ctx,app_heading(title),point,
      GRect(XOCE_UI_INSET,0,clock_x-2*XOCE_UI_INSET,32),ink,GTextAlignmentLeft);
  timer_fonts_text_box(ctx,wall_clock,point,GRect(clock_x,0,clock_width,32),ink,GTextAlignmentRight);
  rect(ctx,GRect(0,31,UI_WIDTH,1),ui_line());
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

static void icon(GContext *ctx, XoceUiIcon which, int x, int y, GColor color) {
  xoce_ui_draw_icon(ctx,which,GPoint(x,y),XOCE_UI_RAIL_ICON_SIZE,color);
}

static void bar(GContext *ctx, const TimerUiView *view, XoceUiIcon up,
                XoceUiIcon select, XoceUiIcon down, GColor background) {
  const XoceUiIcon icons[]={up,select,down};
  const GColor ink=background.argb==GColorWhite.argb?GColorBlack:GColorWhite;
  xoce_ui_draw_action_bar(ctx,GRect(UI_WIDTH,0,ACTION_BAR_WIDTH,228),icons,
                          view->action_offset,background,ink);
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

static void main_screen(GContext *ctx,const TimerUiView *view) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  const bool paused=timer->state==ACTIVITY_TIMER_PAUSED,finished=timer->state==ACTIVITY_TIMER_FINISHED,ready=activity_timer_ready(timer);
  const uint32_t left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
  bar(ctx,view,XOCE_ICON_EDIT,paused||finished||ready?XOCE_ICON_PLAY:XOCE_ICON_PAUSE,XOCE_ICON_MORE,ui_fg());
  header(ctx,view,paused?"Paused":ready?"Ready":finished?"Ended":"Timer",ui_fg());
  char clock[20],support[40];
  const unsigned size=size_index(view);
  const int support_point=18+2*(int)size;
  const bool long_time=left>=3600;
  if(long_time) {
    char hours[8];snprintf(hours,sizeof(hours),"%lu",(unsigned long)(left/3600));
    const int point=view->issue?36:34+4*(int)size,width=(timer_font_digits_width_q8(hours,point)+128)/256;
    const char *unit="h";const int unit_width=timer_fonts_text_width(unit,support_point),first=(UI_WIDTH-width-8-unit_width)/2;
    timer_fonts_digits_box(ctx,hours,point,GRect(first,38,width,36),ui_fg(),ui_bg());
    label(ctx,unit,support_point,GRect(first+width+8,38,unit_width,36),ui_fg(),GTextAlignmentLeft);
    clock_text(left%3600,clock,sizeof(clock));
    timer_fonts_digits_box(ctx,clock,view->issue?40:40+4*(int)size,GRect(8,78,150,46),ui_fg(),ui_bg());
  } else {
    clock_text(left,clock,sizeof(clock));
    timer_fonts_digits_box(ctx,clock,46+6*(int)size,GRect(8,38,150,56),ui_fg(),ui_bg());
  }
  const int pose=timer_hourglass_pose(ready?TIMER_GLASS_READY:paused?TIMER_GLASS_PAUSED:TIMER_GLASS_RUNNING);
  const int angle=view->hourglass_transition?view->hourglass_angle:pose;
  const int glass_size=view->issue?(long_time?34:70):(long_time?60:90);
  const int glass_y=view->issue?(long_time?148:132):(long_time?160:144);
  hourglass(ctx,left,ready?timer->repeat_seconds:timer->configured_seconds,UI_CENTER,glass_y,glass_size,angle,paused?AMBER:BLUE,ui_fg(),timer->state==ACTIVITY_TIMER_RUNNING,view->now);
  if(!view->issue) {
    if(ready)snprintf(support,sizeof(support),"Select to start");
    else {clock_text(activity_timer_elapsed(timer,view->now),clock,sizeof(clock));snprintf(support,sizeof(support),app_text("Elapsed %s"),clock);}
    footer(ctx,support,support_point);
  }
}

/* One layout is used for both MenuLayer measurement and paint. No selected
 * index is consulted for animation colors: the SDK may draw the same cell in
 * both highlight states in an intermediate frame. */
typedef struct {
  char lines[4][64];
  int heights[4];
  unsigned count;
} RowText;
typedef struct {
  char clock[20];
  RowText text;
  int digit_point, secondary_point, pad, gap, height;
  uint32_t left, total;
} RowLayout;

static void row_text(RowText *out,const char *text,int point,int width) {
  /* Bounded UTF-8 wrapping, including an overlong word. Never shorten a saved
   * duration or state to force a fixed row height. */
  while(*text&&out->count<4) {
    char *line=out->lines[out->count];unsigned length=0,last_space=0;
    while(text[length]&&length<sizeof(out->lines[0])-5) {
      unsigned next=length+1;
      while(((unsigned char)text[next]&0xc0)==0x80)++next;
      memcpy(line,text,next);line[next]=0;
      if(timer_fonts_text_width(line,point)>width&&length)break;
      if(text[length]==' ')last_space=length;
      length=next;
    }
    if(text[length]&&last_space)length=last_space;
    memcpy(line,text,length);line[length]=0;
    out->heights[out->count]=timer_fonts_text_height(line,point);
    ++out->count;text+=length;while(*text==' ')++text;
  }
}
static RowLayout row_layout(const TimerUiView *view,unsigned slot) {
  const ActivityTimerModel *timer=&view->collection->slots[slot];
  const bool ready=activity_timer_ready(timer);
  const unsigned size=size_index(view);
  static const int digits[]={22,30,36};
  RowLayout row={.secondary_point=18+2*(int)size,.digit_point=digits[size],
      .pad=3+(int)size,.gap=1+(int)size};
  row.left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
  row.total=ready?timer->repeat_seconds:timer->configured_seconds;
  clock_text(row.left,row.clock,sizeof(row.clock));
  while(timer_font_digits_width_q8(row.clock,row.digit_point)>137*256&&row.digit_point>18)--row.digit_point;
  const char *state=timer->state==ACTIVITY_TIMER_RUNNING?"Running":timer->state==ACTIVITY_TIMER_PAUSED?"Paused":ready?"Ready":"Time's up";
  row_text(&row.text,app_text(state),row.secondary_point,143);
  if(!ready) {
    char duration[24],saved[64];duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
    snprintf(saved,sizeof(saved),app_text("Set %s"),duration);
    row_text(&row.text,saved,row.secondary_point,143);
  }
  row.height=2*row.pad+row.digit_point;
  for(unsigned i=0;i<row.text.count;++i)row.height+=row.gap+row.text.heights[i];
  return row;
}
int16_t timer_ui_row_height(const TimerUiView *view,unsigned slot) {
  return (int16_t)row_layout(view,slot).height;
}
void timer_ui_draw_row(GContext *ctx,const Layer *cell,const TimerUiView *view,unsigned slot) {
  const RowLayout row=row_layout(view,slot);
  const bool selected=view->menu&&menu_cell_layer_is_highlighted(cell);
  const GColor background=selected?(view->dark?GColorWhite:GColorBlack):(view->dark?GColorBlack:GColorWhite);
  const GColor ink=selected?(view->dark?GColorBlack:GColorWhite):(view->dark?GColorWhite:GColorBlack);
  rect(ctx,layer_get_bounds(cell),background);
  timer_fonts_digits_box(ctx,row.clock,row.digit_point,GRect(8,row.pad,137,row.digit_point),ink,background);
  int top=row.pad+row.digit_point;
  for(unsigned i=0;i<row.text.count;++i) {
    top+=row.gap;
    timer_fonts_text_top(ctx,row.text.lines[i],row.secondary_point,8,top,143,ink,GTextAlignmentLeft);
    top+=row.text.heights[i];
  }
  const unsigned fill=row.total?(unsigned)(((uint64_t)row.left*28+row.total/2)/row.total):0;
  const unsigned bounded=fill>28?28:fill;
  rect(ctx,GRect(151,row.pad+2,4,28),GColorDarkGray);
  if(bounded)rect(ctx,GRect(151,row.pad+30-bounded,4,bounded),
      view->collection->slots[slot].state==ACTIVITY_TIMER_PAUSED?AMBER:BLUE);
}

static void list_screen(GContext *ctx,const TimerUiView *view) {
  const unsigned count=timer_collection_count(view->collection),menu=view->menu;
  const bool full=!timer_collection_can_create(view->collection),replacing=!full&&count==TIMER_COLLECTION_CAPACITY;
  const unsigned first_menu=full&&count?1:0;
  bar(ctx,view,menu>first_menu?XOCE_ICON_UP:XOCE_ICON_NONE,menu?XOCE_ICON_NEXT:full?XOCE_ICON_NONE:XOCE_ICON_PLUS,menu<count?XOCE_ICON_DOWN:XOCE_ICON_NONE,ui_fg());
  char heading[32];
  if(menu)snprintf(heading,sizeof(heading),"%u/%u",menu,count);
  else snprintf(heading,sizeof(heading),replacing?"Replace":"New");
  header(ctx,view,heading,ui_fg());
  if(!count) {
    hourglass(ctx,1,1,UI_CENTER,110,78,180,BLUE,ui_fg(),false,view->now);
    centered_label(ctx,"Start a timer",24,165,32,ui_fg());
    return;
  }
  /* Production rows, clipping, selection and scrolling belong to MenuLayer.
   * The app canvas owns only the header, warnings and full-height action rail. */
  if(replacing&&!menu&&!view->issue)centered_label(ctx,"Oldest ready",22,194,34,ui_muted());

}

static void editor_screen(GContext *ctx,const TimerUiView *view) {
  static const char *units[]={"HRS","MIN","SEC"};
  const uint32_t total=view->edit[0]*3600+view->edit[1]*60+view->edit[2];
  const bool locked=view->edit[0]==24&&view->edit_field>0,invalid=total<ACTIVITY_TIMER_MIN_SECONDS;
  bar(ctx,view,locked?XOCE_ICON_NONE:XOCE_ICON_PLUS,view->edit_field<2?XOCE_ICON_NEXT:invalid?XOCE_ICON_NONE:XOCE_ICON_CHECK,locked?XOCE_ICON_NONE:XOCE_ICON_MINUS,ui_fg());
  const bool reuse=view->editing_new&&timer_collection_count(view->collection)==TIMER_COLLECTION_CAPACITY;
  header(ctx,view,view->editing_new?(reuse?"Replace":"New"):view->editing_ready?"Saved":"Edit",ui_fg());
  if(!view->issue)hourglass(ctx,1,1,UI_CENTER,65,44,180,BLUE,ui_fg(),false,view->now);
  for(unsigned field=0;field<3;++field) {
    const int x=5+(int)field*53;const bool selected=view->edit_field==field;
    const GColor bg=selected?(locked?ui_line():invalid&&field==2?AMBER:ui_fg()):ui_bg();
    const GColor ink=selected?(locked?ui_muted():invalid&&field==2?GColorBlack:ui_bg()):ui_fg();
    const TimerValueMotion motion=selected?view->editor_motion:(TimerValueMotion){0};
    if(selected&&!view->selection_transition){graphics_context_set_fill_color(ctx,bg);graphics_fill_rect(ctx,GRect(x,96+motion.box_y,50,52+motion.box_growth),3,GCornersAll);}
    const uint32_t shown=motion.previous_value?view->editor_previous_value:view->edit[field];
    char value[4];snprintf(value,sizeof(value),"%02lu",(unsigned long)shown);
    timer_fonts_digits_box(ctx,value,36,GRect(x,96+motion.text_y,50,52),view->selection_transition?ui_fg():ink,view->selection_transition?ui_bg():bg);
    label(ctx,units[field],22,GRect(x,154,50,24),selected?ui_fg():ui_muted(),GTextAlignmentCenter);
  }
  moving_selection(ctx,view);
  const char *back=view->edit[0]==24?"24 hours max":reuse?"Oldest ready":view->edit_field==0?"Back to cancel":view->edit_field==1?"Back for hours":"Back for minutes";
  centered_label(ctx,back,22,176,22,ui_muted());
  const char *select=invalid?"Min: 5 seconds":view->edit_field<2?"Select: next":view->editing_new?"Select: start":"Select: save";
  footer(ctx,select,22);
}

static void alarm_screen(GContext *ctx,const TimerUiView *view) {
  rect(ctx,GRect(0,0,200,228),GColorBlack);bar(ctx,view,XOCE_ICON_SNOOZE,XOCE_ICON_CHECK,XOCE_ICON_NONE,GColorDarkGray);header(ctx,view,"Alarm",GColorWhite);
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];char duration[20];clock_text(timer->repeat_seconds,duration,sizeof(duration));
  const bool issue=view->issue&&view->issue[0];
  timer_fonts_digits_box(ctx,duration,issue?30:timer->repeat_seconds>=3600?32:47,GRect(8,38,150,issue?29:48),GColorWhite,GColorBlack);
  hourglass(ctx,0,1,UI_CENTER,issue?95:128,issue?46:80,0,BLUE,GColorWhite,false,view->now);
  centered_label(ctx,"Select: Done",22,issue?122:176,issue?26:24,GColorWhite);
  centered_label(ctx,"Up: Snooze 1m",22,issue?148:202,26,GColorWhite);
}

static void options_screen(GContext *ctx,const TimerUiView *view) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];char duration[20],reset[32];
  duration_text(timer->repeat_seconds,duration,sizeof(duration),false);snprintf(reset,sizeof(reset),app_text("Reset %s"),duration);
  const char *options[]={activity_timer_ready(timer)?"Edit duration":"Edit remaining",reset,"Delete timer"};
  bar(ctx,view,view->option?XOCE_ICON_UP:XOCE_ICON_NONE,XOCE_ICON_CHECK,view->option<2?XOCE_ICON_DOWN:XOCE_ICON_NONE,ui_fg());
  const unsigned count=timer_collection_count(view->collection);unsigned position=1;
  for(unsigned row=0;row<count;++row)if(timer_collection_visible_slot(view->collection,row)==(int)view->selected)position=row+1;
  char heading[32],context[40];snprintf(heading,sizeof(heading),count>1?"%u/%u":"Options",position,count);
  snprintf(context,sizeof(context),"%s · %s",duration,timer->state==ACTIVITY_TIMER_RUNNING?app_text("Running"):timer->state==ACTIVITY_TIMER_PAUSED?app_text("Paused"):activity_timer_ready(timer)?app_text("Ready"):app_text("Time's up"));
  if(timer_fonts_text_width(context,22)>154)snprintf(context,sizeof(context),"%s %s",duration,timer->state==ACTIVITY_TIMER_RUNNING?app_text("Running"):timer->state==ACTIVITY_TIMER_PAUSED?app_text("Paused"):activity_timer_ready(timer)?app_text("Ready"):app_text("Done"));
  header(ctx,view,heading,ui_fg());if(!view->issue)centered_label(ctx,context,22,36,26,ui_muted());
  for(unsigned row=0;row<3;++row){const int y=timer_ui_option_top(view)+(int)row*TIMER_OPTION_STRIDE;const bool selected=!view->selection_transition&&view->option==row;
    if(selected)rect(ctx,GRect(0,y,UI_WIDTH,TIMER_OPTION_HEIGHT),ui_fg());
    label(ctx,options[row],24,GRect(XOCE_UI_INSET,y,UI_WIDTH-2*XOCE_UI_INSET,TIMER_OPTION_HEIGHT),selected?ui_bg():ui_fg(),GTextAlignmentLeft);
  }
  moving_selection(ctx,view);
  const char *status=view->status&&view->status[0]?view->status:activity_timer_ready(timer)?"Back to timer":"Still timing";
  if(!strcmp(status,"Phone disconnected"))status="Phone offline";
  else if(!strcmp(status,"Sharing unavailable"))status="Sharing failed";
  else if(!strcmp(status,"Shared with phone"))status="Phone received";
  if(!view->issue)footer(ctx,status,22);
}

static void confirm_screen(GContext *ctx,const TimerUiView *view) {
  const bool issue=view->issue&&view->issue[0];const ActivityTimerModel *timer=&view->collection->slots[view->selected];char duration[24],target[64];
  duration_text(timer->repeat_seconds,duration,sizeof(duration),false);const unsigned count=timer_collection_count(view->collection);unsigned position=1;
  for(unsigned row=0;row<count;++row)if(timer_collection_visible_slot(view->collection,row)==(int)view->selected)position=row+1;
  snprintf(target,sizeof(target),app_text("%s timer"),duration);char heading[32];snprintf(heading,sizeof(heading),count>1?"%u/%u":"Delete",position,count);
  bar(ctx,view,view->confirm_choice?XOCE_ICON_UP:XOCE_ICON_NONE,XOCE_ICON_CHECK,view->confirm_choice?XOCE_ICON_NONE:XOCE_ICON_DOWN,ui_fg());header(ctx,view,heading,ui_fg());
  if(!issue)icon(ctx,XOCE_ICON_TRASH,UI_CENTER,54,ui_fg());
  centered_label(ctx,"Delete timer?",24,issue?90:77,26,ui_fg());
  centered_label(ctx,target,22,issue?116:108,24,ui_fg());
  centered_label(ctx,"Cannot undo",22,140,18,ui_muted());
  const bool keep=!view->selection_transition&&!view->confirm_choice,remove=!view->selection_transition&&view->confirm_choice;
  if(keep)rect(ctx,GRect(0,TIMER_CONFIRM_TOP,UI_WIDTH,TIMER_CONFIRM_HEIGHT),ui_fg());
  label(ctx,"Keep timer",XOCE_UI_BODY_POINT,GRect(XOCE_UI_INSET,TIMER_CONFIRM_TOP,UI_WIDTH-2*XOCE_UI_INSET,TIMER_CONFIRM_HEIGHT),keep?ui_bg():ui_fg(),GTextAlignmentLeft);
  if(remove)rect(ctx,GRect(0,TIMER_CONFIRM_TOP+TIMER_CONFIRM_STRIDE,UI_WIDTH,TIMER_CONFIRM_HEIGHT),ui_fg());
  label(ctx,"Delete timer",XOCE_UI_BODY_POINT,GRect(XOCE_UI_INSET,TIMER_CONFIRM_TOP+TIMER_CONFIRM_STRIDE,UI_WIDTH-2*XOCE_UI_INSET,TIMER_CONFIRM_HEIGHT),remove?ui_bg():ui_fg(),GTextAlignmentLeft);
  moving_selection(ctx,view);
}

void timer_ui_draw(GContext *ctx,const TimerUiView *view) {
  s_dark=view&&view->dark;rect(ctx,GRect(0,0,200,228),ui_bg());if(!view||!view->collection)return;
  switch(view->screen){case SCREEN_LIST:list_screen(ctx,view);break;case SCREEN_EDITOR:editor_screen(ctx,view);break;case SCREEN_ALARM:alarm_screen(ctx,view);break;case SCREEN_OPTIONS:options_screen(ctx,view);break;case SCREEN_CONFIRM:confirm_screen(ctx,view);break;default:main_screen(ctx,view);break;}
  if(view->issue&&view->issue[0]) {
    const bool top_issue=view->screen==SCREEN_CONFIRM||view->screen==SCREEN_OPTIONS||view->screen==SCREEN_EDITOR;const int top=top_issue?32:174;
    rect(ctx,GRect(0,top,UI_WIDTH,54),AMBER);
    const char *message=!strcmp(view->issue,"Changes not saved")?"Not saved":view->issue;
    centered_label(ctx,message,22,top+3,23,GColorBlack);
    centered_label(ctx,view->appearance_issue?"Retry phone app":"Keep app open",22,top+28,23,GColorBlack);
  }
}
