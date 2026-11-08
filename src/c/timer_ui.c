#include "timer_ui.h"
#include "localization.h"
#include "timer_native_type.h"
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
static GColor ui_muted(void) { return ui_fg(); }
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

static unsigned size_index(const TimerUiView *view) {
  return view->text_size>2?2:view->text_size;
}
static void header(GContext *ctx,const TimerUiView *view,const char *title,GColor ink) {
  char wall_clock[8];const time_t timestamp=(time_t)view->now;const struct tm *local=localtime(&timestamp);
  const bool twenty_four=clock_is_24h_style();
  if(local){strftime(wall_clock,sizeof(wall_clock),twenty_four?"%H:%M":"%I:%M",local);if(!twenty_four&&wall_clock[0]=='0')memmove(wall_clock,wall_clock+1,strlen(wall_clock));}
  else clock_copy_time_string(wall_clock,sizeof(wall_clock));
  const unsigned size=size_index(view);
  const int clock_width=timer_native_type_measure(wall_clock,size,TIMER_TYPE_HEADER,2048).w;
  const int clock_x=UI_WIDTH-XOCE_UI_INSET-clock_width;
  /* Same built-in Header font/size as PebbleOS. Reserve the complete clock
   * first, then let native ellipsis protect it from a long translated label. */
  timer_native_type_draw(ctx,app_heading(title),size,TIMER_TYPE_HEADER,
      GRect(XOCE_UI_INSET,0,clock_x-2*XOCE_UI_INSET,32),ink,GTextAlignmentLeft,GTextOverflowModeTrailingEllipsis);
  timer_native_type_draw(ctx,wall_clock,size,TIMER_TYPE_HEADER,
      GRect(clock_x,0,clock_width,32),ink,GTextAlignmentRight,GTextOverflowModeWordWrap);
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

int timer_ui_issue_height(const TimerUiView *view) {
  if(!view->issue||!view->issue[0])return 0;
  const char *message=!strcmp(view->issue,"Changes not saved")?"Not saved":view->issue;
  const unsigned size=size_index(view);
  const int height=timer_native_type_measure(app_text(message),size,TIMER_TYPE_FOOTER,150).h+
      timer_native_type_measure(app_text(view->appearance_issue?"Retry phone app":"Keep app open"),size,TIMER_TYPE_FOOTER,150).h+8;
  return height<54?54:height;
}
static void main_screen(GContext *ctx,const TimerUiView *view) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  const bool paused=timer->state==ACTIVITY_TIMER_PAUSED,finished=timer->state==ACTIVITY_TIMER_FINISHED,ready=activity_timer_ready(timer);
  const uint32_t left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
  bar(ctx,view,XOCE_ICON_EDIT,paused||finished||ready?XOCE_ICON_PLAY:XOCE_ICON_PAUSE,XOCE_ICON_MORE,ui_fg());
  header(ctx,view,paused?"Paused":ready?"Ready":finished?"Ended":"Timer",ui_fg());
  char clock[20],support[64];
  const unsigned size=size_index(view);
  const bool long_time=left>=3600;
  if(long_time) {
    char hours[12];snprintf(hours,sizeof(hours),"%lu",(unsigned long)(left/3600));
    timer_native_type_countdown(ctx,hours,GRect(22,34,92,44),ui_fg());
    timer_native_type_draw(ctx,"h",size,TIMER_TYPE_FOOTER,GRect(116,42,38,32),
        ui_fg(),GTextAlignmentLeft,GTextOverflowModeWordWrap);
    clock_text(left%3600,clock,sizeof(clock));
    timer_native_type_countdown(ctx,clock,GRect(8,78,150,46),ui_fg());
  } else {
    clock_text(left,clock,sizeof(clock));
    timer_native_type_countdown(ctx,clock,GRect(8,38,150,56),ui_fg());
  }
  int bottom=228-timer_ui_issue_height(view);
  if(!view->issue) {
    if(ready)snprintf(support,sizeof(support),"%s",app_text("Select to start"));
    else {clock_text(activity_timer_elapsed(timer,view->now),clock,sizeof(clock));snprintf(support,sizeof(support),app_text("Elapsed %s"),clock);}
    const int measured=timer_native_type_measure(support,size,TIMER_TYPE_FOOTER,150).h+4;
    const int height=measured<30?30:measured;bottom=228-height;
    timer_native_type_draw(ctx,support,size,TIMER_TYPE_FOOTER,GRect(8,bottom+2,150,height-2),
        ui_muted(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
  }
  const int available=bottom-(long_time?124:94)-8;
  const int desired=view->issue?(long_time?34:70):(long_time?60:90);
  const int glass_size=desired<available?desired:available;
  const int glass_y=bottom-4-glass_size/2;
  const int pose=timer_hourglass_pose(ready?TIMER_GLASS_READY:paused?TIMER_GLASS_PAUSED:TIMER_GLASS_RUNNING);
  hourglass(ctx,left,ready?timer->repeat_seconds:timer->configured_seconds,UI_CENTER,glass_y,glass_size,
      view->hourglass_transition?view->hourglass_angle:pose,paused?AMBER:BLUE,ui_fg(),timer->state==ACTIVITY_TIMER_RUNNING,view->now);
}

/* One layout is used for both MenuLayer measurement and paint. No selected
 * index is consulted for animation colors: the SDK may draw the same cell in
 * both highlight states in an intermediate frame. */
typedef struct {
  char clock[20],saved[64],support[128];
  const char *state;
  bool joined;
  GSize clock_size,state_size,saved_size;
  int pad,gap,height;
  uint32_t left,total;
} RowLayout;
static RowLayout row_layout(const TimerUiView *view,unsigned slot) {
  const ActivityTimerModel *timer=&view->collection->slots[slot];
  const bool ready=activity_timer_ready(timer);
  const unsigned size=size_index(view);
  RowLayout row={.pad=3,.gap=2};
  row.left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
  row.total=ready?timer->repeat_seconds:timer->configured_seconds;
  clock_text(row.left,row.clock,sizeof(row.clock));
  row.state=app_text(timer->state==ACTIVITY_TIMER_RUNNING?"Running":timer->state==ACTIVITY_TIMER_PAUSED?"Paused":ready?"Ready":"Time's up");
  row.clock_size=timer_native_type_measure(row.clock,size,TIMER_TYPE_MENU_TITLE,143);
  row.state_size=timer_native_type_measure(row.state,size,TIMER_TYPE_MENU_SUBTITLE,143);
  row.height=2*row.pad+row.clock_size.h+row.gap+row.state_size.h;
  if(!ready) {
    char duration[24];duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
    snprintf(row.saved,sizeof(row.saved),app_text("Set %s"),duration);
    snprintf(row.support,sizeof(row.support),"%s · %s",row.state,row.saved);
    const GSize joined=timer_native_type_measure(row.support,size,TIMER_TYPE_MENU_SUBTITLE,2048);
    /* Keep both pieces of information on one line only when the native font
     * really fits. Density comes from reflow, never from shrinking its type. */
    if(joined.w<=143) {
      row.joined=true;row.height+=joined.h-row.state_size.h;row.state_size=joined;
    } else {
      row.saved_size=timer_native_type_measure(row.saved,size,TIMER_TYPE_MENU_SUBTITLE,143);
      row.height+=row.gap+row.saved_size.h;
    }
  }
  return row;
}
int16_t timer_ui_row_height(const TimerUiView *view,unsigned slot) {
  return (int16_t)row_layout(view,slot).height;
}
void timer_ui_draw_row(GContext *ctx,const Layer *cell,const TimerUiView *view,unsigned slot) {
  const RowLayout row=row_layout(view,slot);
  const unsigned size=size_index(view);
  const bool selected=view->menu&&menu_cell_layer_is_highlighted(cell);
  const GColor background=selected?(view->dark?GColorWhite:GColorBlack):(view->dark?GColorBlack:GColorWhite);
  const GColor ink=selected?(view->dark?GColorBlack:GColorWhite):(view->dark?GColorWhite:GColorBlack);
  rect(ctx,layer_get_bounds(cell),background);
  int top=row.pad;
  timer_native_type_draw(ctx,row.clock,size,TIMER_TYPE_MENU_TITLE,GRect(8,top,143,row.clock_size.h),
      ink,GTextAlignmentLeft,GTextOverflowModeWordWrap);
  top+=row.clock_size.h+row.gap;
  timer_native_type_draw(ctx,row.joined?row.support:row.state,size,TIMER_TYPE_MENU_SUBTITLE,GRect(8,top,143,row.state_size.h),
      ink,GTextAlignmentLeft,GTextOverflowModeWordWrap);
  if(row.saved[0]&&!row.joined) {
    top+=row.state_size.h+row.gap;
    timer_native_type_draw(ctx,row.saved,size,TIMER_TYPE_MENU_SUBTITLE,GRect(8,top,143,row.saved_size.h),
        ink,GTextAlignmentLeft,GTextOverflowModeWordWrap);
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
    timer_native_type_draw(ctx,app_text("Start a timer"),size_index(view),TIMER_TYPE_MENU_TITLE,
        GRect(8,165,150,50),ui_fg(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
    return;
  }
  /* Production rows, clipping, selection and scrolling belong to MenuLayer.
   * The app canvas owns only the header, warnings and full-height action rail. */
  if(replacing&&!menu&&!view->issue)timer_native_type_draw(ctx,app_text("Oldest ready"),size_index(view),
      TIMER_TYPE_FOOTER,GRect(8,194,150,34),ui_muted(),GTextAlignmentCenter,GTextOverflowModeWordWrap);

}

typedef struct {
  const char *back,*select;
  int top,box,unit_height,back_height,select_height;
} EditorLayout;
static EditorLayout editor_layout(const TimerUiView *view) {
  const bool reuse=view->editing_new&&timer_collection_count(view->collection)==TIMER_COLLECTION_CAPACITY;
  const bool invalid=view->edit[0]*3600+view->edit[1]*60+view->edit[2]<ACTIVITY_TIMER_MIN_SECONDS;
  EditorLayout layout={.box=52};const unsigned size=size_index(view);
  layout.back=app_text(view->edit[0]==24?"24 hours max":reuse?"Oldest ready":view->edit_field==0?"Back to cancel":view->edit_field==1?"Back for hours":"Back for minutes");
  layout.select=app_text(invalid?"Min: 5 seconds":view->edit_field<2?"Select: next":view->editing_new?"Select: start":"Select: save");
  static const char *units[]={"HRS","MIN","SEC"};
  for(unsigned field=0;field<3;++field) {
    const int height=timer_native_type_measure(app_text(units[field]),size,TIMER_TYPE_FOOTER,50).h;
    if(height>layout.unit_height)layout.unit_height=height;
  }
  layout.back_height=timer_native_type_measure(layout.back,size,TIMER_TYPE_FOOTER,150).h;
  layout.select_height=timer_native_type_measure(layout.select,size,TIMER_TYPE_FOOTER,150).h;
  const int minimum=36+timer_ui_issue_height(view);
  const int tail=2+layout.unit_height+4+layout.back_height+4+layout.select_height+4;
  if(minimum+layout.box+tail>228) {
    const int number=timer_native_type_measure("24",size,TIMER_TYPE_MENU_TITLE,50).h+12;
    layout.box=228-minimum-tail;if(layout.box<number)layout.box=number;
  }
  layout.top=228-layout.box-tail;
  if(layout.top>96)layout.top=96;
  if(layout.top<minimum)layout.top=minimum;
  return layout;
}
int timer_ui_editor_top(const TimerUiView *view) {return editor_layout(view).top;}
GRect timer_ui_editor_field_rect(const TimerUiView *view,unsigned field) {
  const EditorLayout layout=editor_layout(view);
  return GRect(5+(int)field*53,layout.top,50,layout.box);
}
static void editor_screen(GContext *ctx,const TimerUiView *view) {
  static const char *units[]={"HRS","MIN","SEC"};
  const uint32_t total=view->edit[0]*3600+view->edit[1]*60+view->edit[2];
  const bool locked=view->edit[0]==24&&view->edit_field>0,invalid=total<ACTIVITY_TIMER_MIN_SECONDS;
  bar(ctx,view,locked?XOCE_ICON_NONE:XOCE_ICON_PLUS,view->edit_field<2?XOCE_ICON_NEXT:invalid?XOCE_ICON_NONE:XOCE_ICON_CHECK,
      locked?XOCE_ICON_NONE:XOCE_ICON_MINUS,ui_fg());
  const bool reuse=view->editing_new&&timer_collection_count(view->collection)==TIMER_COLLECTION_CAPACITY;
  header(ctx,view,view->editing_new?(reuse?"Replace":"New"):view->editing_ready?"Saved":"Edit",ui_fg());
  const EditorLayout layout=editor_layout(view);
  if(!view->issue&&layout.top>=84)hourglass(ctx,1,1,UI_CENTER,60,34,180,BLUE,ui_fg(),false,view->now);
  const unsigned size=size_index(view);const int top=layout.top;
  for(unsigned field=0;field<3;++field) {
    const int x=5+(int)field*53;const bool selected=view->edit_field==field;
    const GColor bg=selected?(locked?ui_line():invalid&&field==2?AMBER:ui_fg()):ui_bg();
    const GColor ink=selected?(locked?ui_muted():invalid&&field==2?GColorBlack:ui_bg()):ui_fg();
    const TimerValueMotion motion=selected?view->editor_motion:(TimerValueMotion){0};
    if(selected&&!view->selection_transition)rect(ctx,GRect(x,top+motion.box_y,50,layout.box+motion.box_growth),bg);
    const uint32_t shown=motion.previous_value?view->editor_previous_value:view->edit[field];
    char value[4];snprintf(value,sizeof(value),"%02lu",(unsigned long)shown);
    const int h=timer_native_type_measure(value,size,TIMER_TYPE_MENU_TITLE,50).h;
    timer_native_type_draw(ctx,value,size,TIMER_TYPE_MENU_TITLE,GRect(x,top+(layout.box-h)/2+motion.text_y,50,h),
        view->selection_transition?ui_fg():ink,GTextAlignmentCenter,GTextOverflowModeWordWrap);
    timer_native_type_draw(ctx,app_text(units[field]),size,TIMER_TYPE_FOOTER,GRect(x,top+layout.box+2,50,layout.unit_height),
        selected?ui_fg():ui_muted(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
  }
  moving_selection(ctx,view);
  const int help=top+layout.box+2+layout.unit_height+4;
  timer_native_type_draw(ctx,layout.back,size,TIMER_TYPE_FOOTER,GRect(8,help,150,layout.back_height),ui_muted(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
  timer_native_type_draw(ctx,layout.select,size,TIMER_TYPE_FOOTER,GRect(8,help+layout.back_height+4,150,layout.select_height),ui_fg(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
}

static void alarm_screen(GContext *ctx,const TimerUiView *view) {
  rect(ctx,GRect(0,0,200,228),GColorBlack);
  bar(ctx,view,XOCE_ICON_SNOOZE,XOCE_ICON_CHECK,XOCE_ICON_NONE,GColorDarkGray);header(ctx,view,"Alarm",GColorWhite);
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];char duration[20];clock_text(timer->repeat_seconds,duration,sizeof(duration));
  const unsigned size=size_index(view);const int bottom=228-timer_ui_issue_height(view);
  timer_native_type_countdown(ctx,duration,GRect(8,38,150,46),GColorWhite);
  const int first=timer_native_type_measure(app_text("Select: Done"),size,TIMER_TYPE_FOOTER,150).h;
  const int second=timer_native_type_measure(app_text("Up: Snooze 1m"),size,TIMER_TYPE_FOOTER,150).h;
  const int text_top=bottom-first-second-8,space=text_top-88,glass=space>80?80:space;
  if(glass>12)hourglass(ctx,0,1,UI_CENTER,86+glass/2,glass,0,BLUE,GColorWhite,false,view->now);
  timer_native_type_draw(ctx,app_text("Select: Done"),size,TIMER_TYPE_FOOTER,GRect(8,text_top,150,first),GColorWhite,GTextAlignmentCenter,GTextOverflowModeWordWrap);
  timer_native_type_draw(ctx,app_text("Up: Snooze 1m"),size,TIMER_TYPE_FOOTER,GRect(8,text_top+first+4,150,second),GColorWhite,GTextAlignmentCenter,GTextOverflowModeWordWrap);
}

static const char *action_label(const TimerUiView *view,unsigned row,char text[64]) {
  if(view->screen==SCREEN_CONFIRM)return app_text(row?"Delete timer":"Keep timer");
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];
  if(!row)return app_text(activity_timer_ready(timer)?"Edit duration":"Edit remaining");
  if(row==2)return app_text("Delete timer");
  char duration[24];duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
  snprintf(text,64,app_text("Reset %s"),duration);return text;
}
int16_t timer_ui_action_row_height(const TimerUiView *view,unsigned row) {
  char text[64];const int height=timer_native_type_measure(action_label(view,row,text),size_index(view),TIMER_TYPE_MENU_TITLE,150).h+12;
  return height<36?36:height;
}
void timer_ui_draw_action_row(GContext *ctx,const Layer *cell,const TimerUiView *view,unsigned row) {
  const bool selected=menu_cell_layer_is_highlighted(cell);
  const GColor bg=selected?(view->dark?GColorWhite:GColorBlack):(view->dark?GColorBlack:GColorWhite);
  const GColor fg=selected?(view->dark?GColorBlack:GColorWhite):(view->dark?GColorWhite:GColorBlack);
  const GRect bounds=layer_get_bounds(cell);char text[64];const char *label=action_label(view,row,text);
  const unsigned size=size_index(view);const int height=timer_native_type_measure(label,size,TIMER_TYPE_MENU_TITLE,150).h;
  rect(ctx,bounds,bg);timer_native_type_draw(ctx,label,size,TIMER_TYPE_MENU_TITLE,
      GRect(8,(bounds.size.h-height)/2,150,height),fg,GTextAlignmentLeft,GTextOverflowModeWordWrap);
}
static int context_line(GContext *ctx,const TimerUiView *view,const char *text,int top,TimerNativeTypeRole role) {
  const int height=timer_native_type_measure(text,size_index(view),role,150).h;
  if(ctx)timer_native_type_draw(ctx,text,size_index(view),role,GRect(8,top,150,height),
      ui_muted(),GTextAlignmentLeft,GTextOverflowModeWordWrap);
  return top+height+4;
}
static int issue_panel(GContext *ctx,const TimerUiView *view,int top) {
  const int height=timer_ui_issue_height(view);if(!height)return top;
  const char *message=app_text(!strcmp(view->issue,"Changes not saved")?"Not saved":view->issue);
  const unsigned size=size_index(view);const int first=timer_native_type_measure(message,size,TIMER_TYPE_FOOTER,150).h;
  if(ctx) {
    rect(ctx,GRect(0,top,UI_WIDTH,height),AMBER);
    timer_native_type_draw(ctx,message,size,TIMER_TYPE_FOOTER,GRect(8,top+2,150,first),GColorBlack,GTextAlignmentCenter,GTextOverflowModeWordWrap);
    timer_native_type_draw(ctx,app_text(view->appearance_issue?"Retry phone app":"Keep app open"),size,TIMER_TYPE_FOOTER,
        GRect(8,top+first+4,150,height-first-4),GColorBlack,GTextAlignmentCenter,GTextOverflowModeWordWrap);
  }
  return top+height;
}
static int action_context(GContext *ctx,const TimerUiView *view) {
  s_dark=view->dark;int top=issue_panel(ctx,view,0)+4;
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];char duration[24],text[96];
  duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
  if(view->screen==SCREEN_CONFIRM) {
    top=context_line(ctx,view,app_text("Delete timer?"),top,TIMER_TYPE_MENU_SUBTITLE);
    snprintf(text,sizeof(text),app_text("%s timer"),duration);
    top=context_line(ctx,view,text,top,TIMER_TYPE_FOOTER);
    top=context_line(ctx,view,app_text("Cannot undo"),top,TIMER_TYPE_FOOTER);
  } else {
    snprintf(text,sizeof(text),"%s · %s",duration,app_text(timer->state==ACTIVITY_TIMER_RUNNING?"Running":
        timer->state==ACTIVITY_TIMER_PAUSED?"Paused":activity_timer_ready(timer)?"Ready":"Time's up"));
    top=context_line(ctx,view,text,top,TIMER_TYPE_MENU_SUBTITLE);
    const char *status=view->status&&view->status[0]?view->status:"Back to timer";
    if(!strcmp(status,"Phone disconnected"))status="Phone offline";
    else if(!strcmp(status,"Sharing unavailable"))status="Sharing failed";
    else if(!strcmp(status,"Shared with phone"))status="Phone received";
    top=context_line(ctx,view,app_text(status),top,TIMER_TYPE_FOOTER);
  }
  return top+4;
}
int16_t timer_ui_context_height(const TimerUiView *view) {return (int16_t)action_context(NULL,view);}
void timer_ui_draw_context(GContext *ctx,const TimerUiView *view) {action_context(ctx,view);}
static void action_screen(GContext *ctx,const TimerUiView *view) {
  const bool confirm=view->screen==SCREEN_CONFIRM;
  const unsigned row=confirm?view->confirm_choice:view->option,maximum=confirm?1:2;
  bar(ctx,view,row||view->context_above?XOCE_ICON_UP:XOCE_ICON_NONE,XOCE_ICON_CHECK,
      row<maximum?XOCE_ICON_DOWN:XOCE_ICON_NONE,ui_fg());
  const unsigned count=timer_collection_count(view->collection);
  char heading[32];snprintf(heading,sizeof(heading),count>1?"%u/%u":confirm?"Delete":"Options",
      timer_ui_menu_for_slot(view->collection,view->selected),count);
  header(ctx,view,heading,ui_fg());
}

void timer_ui_draw(GContext *ctx,const TimerUiView *view) {
  s_dark=view&&view->dark;rect(ctx,GRect(0,0,200,228),ui_bg());if(!view||!view->collection)return;
  switch(view->screen){case SCREEN_LIST:list_screen(ctx,view);break;case SCREEN_EDITOR:editor_screen(ctx,view);break;case SCREEN_ALARM:alarm_screen(ctx,view);break;case SCREEN_OPTIONS:case SCREEN_CONFIRM:action_screen(ctx,view);break;default:main_screen(ctx,view);break;}
  if(view->issue&&view->issue[0]&&view->screen!=SCREEN_OPTIONS&&view->screen!=SCREEN_CONFIRM) {
    const int top=view->screen==SCREEN_EDITOR?32:228-timer_ui_issue_height(view);
    issue_panel(ctx,view,top);
  }
}
