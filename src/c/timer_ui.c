#include "timer_ui.h"
#include "localization.h"
#include "timer_native_type.h"
#include "timer_hourglass.h"
#include <pebble-xoce-ui/pebble-xoce-ui.h>

#include <stdio.h>
#include <string.h>

#include "app_geometry.h"
#define UI_WIDTH (APP_SCREEN_W - ACTION_BAR_WIDTH)
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
  TimerMotionRect r=view->selection_rect;
#ifdef PBL_ROUND
  /* SelectionLayer keeps a circle while its width overshoots the field. */
  r.y+=(r.h-r.w)/2;r.h=r.w;
#endif
  GBitmap *frame=graphics_capture_frame_buffer_format(ctx,
      PBL_IF_COLOR_ELSE(GBitmapFormat8Bit,GBitmapFormat1Bit));
  if (!frame) return;
  const int top=r.y<0?0:r.y,bottom=r.y+r.h>APP_SCREEN_H?APP_SCREEN_H:r.y+r.h;
  for (int y=top;y<bottom;++y) {
    const GBitmapDataRowInfo row=gbitmap_get_data_row_info(frame,y+APP_SCREEN_Y);
    const int left=r.x+APP_SCREEN_X,right=r.x+r.w-1+APP_SCREEN_X;
    const int first=left<row.min_x?row.min_x:left;
    int last=right>row.max_x?row.max_x:right;
    if(last>=UI_WIDTH+APP_SCREEN_X)last=UI_WIDTH+APP_SCREEN_X-1;
    for(int x=first;x<=last;++x) {
#ifdef PBL_ROUND
      const int dx=2*(x-r.x)-r.w,dy=2*(y-r.y)-r.h;
      if(dx*dx+dy*dy>r.w*r.w)continue;
#endif
#ifdef PBL_COLOR
      const uint8_t c=row.data[x],red=(c>>4)&3,green=(c>>2)&3,blue=c&3;
      if(red==green&&green==blue)row.data[x]=0xc0|((~c)&0x3f);
#else
      row.data[x/8]^=(uint8_t)(1u<<(x%8));
#endif
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
#ifdef PBL_ROUND
  const XoceUiHeaderLayout layout=xoce_ui_round_header_layout(APP_SCREEN_FRAME,UI_WIDTH,size,true);
  timer_native_type_draw(ctx,wall_clock,size,TIMER_TYPE_HEADER,layout.clock,ink,GTextAlignmentCenter,GTextOverflowModeWordWrap);
  if(view->screen!=SCREEN_OPTIONS&&view->screen!=SCREEN_CONFIRM)timer_native_type_draw(ctx,app_heading(title),size,TIMER_TYPE_HEADER,layout.label,ink,GTextAlignmentCenter,GTextOverflowModeTrailingEllipsis);
  return;
#endif
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

static GRect content_band(int top,int height) {
  GRect box=GRect(8,top,UI_WIDTH-16,height);
#ifdef PBL_ROUND
  box=xoce_ui_circle_band(APP_SCREEN_FRAME,box,6);
#endif
  return box;
}
static GRect bounded_band(int top,int height,int width) {
  GRect box=content_band(top,height);
  if(APP_CENTER_FOCUSED&&box.size.w>width){box.origin.x+=(box.size.w-width)/2;box.size.w=width;}
  return box;
}
#ifdef PBL_ROUND
static int footer_height_at(const char *text,unsigned size,int top) {
  int height=timer_native_type_measure(text,size,TIMER_TYPE_FOOTER,UI_WIDTH-16).h;
  for(unsigned i=0;i<5;++i) {
    const GRect band=content_band(top,height);if(band.size.w<20)return 0;
    const int next=timer_native_type_measure(text,size,TIMER_TYPE_FOOTER,band.size.w).h;
    if(next==height)return height;
    height=next;
  }
  return 0;
}
#endif
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

#ifdef PBL_ROUND
static XoceUiIcon s_rail_icons[SCREEN_CONFIRM+1][3];
static GColor s_rail_background[SCREEN_CONFIRM+1];
void timer_ui_draw_rail(GContext *ctx,const TimerUiView *view) {
  const GColor bg=s_rail_background[view->screen];
  xoce_ui_draw_action_bar(ctx,GRect(UI_WIDTH,0,ACTION_BAR_WIDTH,APP_SCREEN_H),s_rail_icons[view->screen],view->action_offset,bg,bg.argb==GColorWhite.argb?GColorBlack:GColorWhite);
}
int timer_ui_round_header_height(const TimerUiView *view) {
  const XoceUiHeaderLayout layout=xoce_ui_round_header_layout(APP_SCREEN_FRAME,UI_WIDTH,size_index(view),true);
  return view->screen==SCREEN_OPTIONS||view->screen==SCREEN_CONFIRM?layout.label.origin.y+4:layout.height;
}
#endif
static void bar(GContext *ctx, const TimerUiView *view, XoceUiIcon up,
                XoceUiIcon select, XoceUiIcon down, GColor background) {
  const XoceUiIcon icons[]={up,select,down};
#ifdef PBL_ROUND
  memcpy(s_rail_icons[view->screen],icons,sizeof(icons));s_rail_background[view->screen]=background;
#endif
  const GColor ink=background.argb==GColorWhite.argb?GColorBlack:GColorWhite;
  xoce_ui_draw_action_bar(ctx,GRect(UI_WIDTH,0,ACTION_BAR_WIDTH,APP_SCREEN_H),icons,
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
  const int height=timer_native_type_measure(app_text(message),size,TIMER_TYPE_FOOTER,APP_CENTER_FOCUSED?160:(UI_WIDTH-16)).h+
      timer_native_type_measure(app_text(view->appearance_issue?"Retry phone app":"Keep app open"),size,TIMER_TYPE_FOOTER,APP_CENTER_FOCUSED?160:(UI_WIDTH-16)).h+8;
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
#ifdef PBL_ROUND
  {
  clock_text(left,clock,sizeof(clock));
  timer_native_type_countdown(ctx,clock,content_band(78,52),ui_fg());
  int support_top=218,support_height=0;
  if(!view->issue) {
    if(ready)snprintf(support,sizeof(support),"%s",app_text("Select to start"));
    else {clock_text(activity_timer_elapsed(timer,view->now),clock,sizeof(clock));snprintf(support,sizeof(support),app_text("Elapsed %s"),clock);}
    for(;support_top>150;support_top-=2) {
      support_height=footer_height_at(support,size,support_top);
      if(support_height&&support_top+support_height<=240)break;
    }
  }
  const int bottom=view->issue?224-timer_ui_issue_height(view):support_top-4;
  const int glass=bottom-136>64?64:bottom-136;
  if(glass>14)hourglass(ctx,left,ready?timer->repeat_seconds:timer->configured_seconds,
      UI_CENTER+4,136+glass/2,glass,view->hourglass_transition?view->hourglass_angle:
      timer_hourglass_pose(ready?TIMER_GLASS_READY:paused?TIMER_GLASS_PAUSED:TIMER_GLASS_RUNNING),
      paused?AMBER:BLUE,ui_fg(),timer->state==ACTIVITY_TIMER_RUNNING,view->now);
  if(!view->issue)timer_native_type_draw(ctx,support,size,TIMER_TYPE_FOOTER,
      content_band(support_top,support_height),ui_fg(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
  return;
  }
#endif
  const bool long_time=left>=3600;
  if(long_time) {
    char hours[12];snprintf(hours,sizeof(hours),"%lu",(unsigned long)(left/3600));
    const bool narrow=UI_WIDTH<166;
    timer_native_type_countdown(ctx,hours,GRect(narrow?8:22,34,narrow?UI_WIDTH-46:92,44),ui_fg());
    timer_native_type_draw(ctx,"h",size,TIMER_TYPE_FOOTER,GRect(narrow?UI_WIDTH-38:116,42,30,32),
        ui_fg(),GTextAlignmentLeft,GTextOverflowModeWordWrap);
    clock_text(left%3600,clock,sizeof(clock));
    timer_native_type_countdown(ctx,clock,GRect(8,78,(UI_WIDTH-16),46),ui_fg());
  } else {
    clock_text(left,clock,sizeof(clock));
    timer_native_type_countdown(ctx,clock,GRect(8,38,(UI_WIDTH-16),56),ui_fg());
  }
  int bottom=APP_SCREEN_H-timer_ui_issue_height(view);
  if(!view->issue) {
    if(ready)snprintf(support,sizeof(support),"%s",app_text("Select to start"));
    else {clock_text(activity_timer_elapsed(timer,view->now),clock,sizeof(clock));snprintf(support,sizeof(support),app_text("Elapsed %s"),clock);}
    const int measured=timer_native_type_measure(support,size,TIMER_TYPE_FOOTER,(UI_WIDTH-16)).h+4;
    const int height=measured<30?30:measured;bottom=APP_SCREEN_H-height;
    timer_native_type_draw(ctx,support,size,TIMER_TYPE_FOOTER,GRect(8,bottom+2,(UI_WIDTH-16),height-2),
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
  bool joined,clock_inline;
  GSize clock_size,state_size,saved_size;
  int first_height;
  int pad,gap,height;
  uint32_t left;
} RowLayout;
static RowLayout row_layout(const TimerUiView *view,unsigned slot) {
  const ActivityTimerModel *timer=&view->collection->slots[slot];
  const bool ready=activity_timer_ready(timer);
  const unsigned size=size_index(view);
  const int width=UI_WIDTH-(APP_CENTER_FOCUSED?36:23);
  RowLayout row={.pad=3,.gap=2};
  row.left=ready?timer->repeat_seconds:activity_timer_remaining(timer,view->now);
  clock_text(row.left,row.clock,sizeof(row.clock));
  row.state=app_text(timer->state==ACTIVITY_TIMER_RUNNING?"Running":timer->state==ACTIVITY_TIMER_PAUSED?"Paused":ready?"Ready":"Time's up");
  row.clock_size=timer_native_type_measure(row.clock,size,TIMER_TYPE_MENU_TITLE,2048);
  row.state_size=timer_native_type_measure(row.state,size,TIMER_TYPE_MENU_SUBTITLE,2048);
  row.clock_inline=row.clock_size.w+6+row.state_size.w<=width;
  if(!row.clock_inline){
    row.clock_size=timer_native_type_measure(row.clock,size,TIMER_TYPE_MENU_TITLE,width);
    row.state_size=timer_native_type_measure(row.state,size,TIMER_TYPE_MENU_SUBTITLE,width);
  }
  row.first_height=row.clock_size.h;
  if(row.clock_inline&&row.state_size.h>row.first_height)row.first_height=row.state_size.h;
  row.height=2*row.pad+row.first_height+(row.clock_inline?0:row.gap+row.state_size.h);
  if(!ready) {
    char duration[24];duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
    snprintf(row.saved,sizeof(row.saved),app_text("Set %s"),duration);
    snprintf(row.support,sizeof(row.support),"%s · %s",row.state,row.saved);
    const GSize joined=timer_native_type_measure(row.support,size,TIMER_TYPE_MENU_SUBTITLE,2048);
    /* Keep both pieces of information on one line only when the native font
     * really fits. Density comes from reflow, never from shrinking its type. */
    if(!row.clock_inline&&joined.w<=width) {
      row.joined=true;row.height+=joined.h-row.state_size.h;row.state_size=joined;
    } else {
      row.saved_size=timer_native_type_measure(row.saved,size,TIMER_TYPE_MENU_SUBTITLE,width);
      row.height+=row.gap+row.saved_size.h;
    }
  }
#ifdef PBL_ROUND
  row.clock_inline=false;row.joined=false;
  row.clock_size=timer_native_type_measure(row.clock,size,TIMER_TYPE_MENU_TITLE,width);
  row.state_size=timer_native_type_measure(row.state,size,TIMER_TYPE_MENU_SUBTITLE,width);
  row.saved_size=timer_native_type_measure(row.saved,size,TIMER_TYPE_MENU_SUBTITLE,width);
  row.height=8+row.clock_size.h+row.state_size.h+(row.saved[0]?row.saved_size.h:0);
#endif
  return row;
}
int16_t timer_ui_row_height(const TimerUiView *view,unsigned slot) {
  return (int16_t)row_layout(view,slot).height;
}
void timer_ui_draw_row(GContext *ctx,const Layer *cell,const TimerUiView *view,unsigned slot) {
  const RowLayout row=row_layout(view,slot);
  const int width=UI_WIDTH-23;
  const unsigned size=size_index(view);
  const bool selected=(APP_CENTER_FOCUSED||view->menu)&&menu_cell_layer_is_highlighted(cell);
  const GColor background=selected?(view->dark?GColorWhite:GColorBlack):(view->dark?GColorBlack:GColorWhite);
  const GColor ink=selected?(view->dark?GColorBlack:GColorWhite):(view->dark?GColorWhite:GColorBlack);
  rect(ctx,layer_get_bounds(cell),background);
#ifdef PBL_ROUND
  const GRect cell_bounds=layer_get_bounds(cell);
  int y=selected?4:(cell_bounds.size.h-row.clock_size.h)/2;
  const char *lines[]={row.clock,row.state,row.saved};
  for(unsigned i=0;i<(selected?3u:1u);++i) {
    if(!lines[i][0])continue;
    const TimerNativeTypeRole role=i?TIMER_TYPE_MENU_SUBTITLE:TIMER_TYPE_MENU_TITLE;
    const int h=timer_native_type_measure(lines[i],size,role,UI_WIDTH-36).h;
    GPoint at=layer_convert_point_to_screen(cell,GPoint(8,y));
    GRect band=xoce_ui_circle_band(APP_SCREEN_FRAME,GRect(at.x,at.y,UI_WIDTH-36,h),6);
    band.origin.x-=at.x-8;band.origin.y=y;
    timer_native_type_draw(ctx,lines[i],size,role,band,ink,GTextAlignmentCenter,GTextOverflowModeTrailingEllipsis);
    y+=h;
  }
  return;
#endif
  int top=row.pad;
  timer_native_type_draw(ctx,row.clock,size,TIMER_TYPE_MENU_TITLE,
      GRect(8,top+(row.first_height-row.clock_size.h)/2,row.clock_inline?row.clock_size.w:width,row.clock_size.h),
      ink,GTextAlignmentLeft,GTextOverflowModeWordWrap);
  if(row.clock_inline){
    timer_native_type_draw(ctx,row.state,size,TIMER_TYPE_MENU_SUBTITLE,
        GRect(8+width-row.state_size.w,top+(row.first_height-row.state_size.h)/2,row.state_size.w,row.state_size.h),
        ink,GTextAlignmentRight,GTextOverflowModeWordWrap);
    top+=row.first_height;
  }else{
    top+=row.first_height+row.gap;
    timer_native_type_draw(ctx,row.joined?row.support:row.state,size,TIMER_TYPE_MENU_SUBTITLE,GRect(8,top,width,row.state_size.h),
        ink,GTextAlignmentLeft,GTextOverflowModeWordWrap);
    top+=row.state_size.h;
  }
  if(row.saved[0]&&!row.joined) {
    top+=row.gap;
    timer_native_type_draw(ctx,row.saved,size,TIMER_TYPE_MENU_SUBTITLE,GRect(8,top,width,row.saved_size.h),
        ink,GTextAlignmentLeft,GTextOverflowModeWordWrap);
  }
}

#ifdef PBL_ROUND
int16_t timer_ui_new_row_height(const TimerUiView *view) {
  return timer_native_type_measure(app_text("Replace"),size_index(view),TIMER_TYPE_MENU_TITLE,184).h+16;
}
void timer_ui_draw_new_row(GContext *ctx,const Layer *cell,const TimerUiView *view) {
  const bool selected=menu_cell_layer_is_highlighted(cell);
  const GColor bg=selected?ui_fg():ui_bg(),ink=selected?ui_bg():ui_fg();
  rect(ctx,layer_get_bounds(cell),bg);
  const char *label=app_heading(timer_collection_count(view->collection)==TIMER_COLLECTION_CAPACITY?"Replace":"New");
  const unsigned size=size_index(view);const GSize text=timer_native_type_measure(label,size,TIMER_TYPE_MENU_TITLE,184);
  const int y=(layer_get_bounds(cell).size.h-text.h)/2;
  const int x=(UI_WIDTH-20-text.w-26)/2;
  xoce_ui_draw_icon(ctx,XOCE_ICON_PLUS,GPoint(x+9,y+text.h/2),18,ink);
  timer_native_type_draw(ctx,label,size,TIMER_TYPE_MENU_TITLE,GRect(x+26,y,text.w,text.h),ink,GTextAlignmentLeft,GTextOverflowModeWordWrap);
}
#endif
static void list_screen(GContext *ctx,const TimerUiView *view) {
  const unsigned count=timer_collection_count(view->collection),menu=view->menu;
  const bool full=!timer_collection_can_create(view->collection),replacing=!full&&count==TIMER_COLLECTION_CAPACITY;
  const unsigned first_menu=full&&count?1:0;
  bar(ctx,view,menu>first_menu?XOCE_ICON_UP:XOCE_ICON_NONE,menu?XOCE_ICON_NEXT:full?XOCE_ICON_NONE:XOCE_ICON_PLUS,menu<count?XOCE_ICON_DOWN:XOCE_ICON_NONE,ui_fg());
  char heading[32];
  if(menu)snprintf(heading,sizeof(heading),"%u/%u",menu,count);
  else snprintf(heading,sizeof(heading),replacing?"Replace":"New");
  if(!menu&&!APP_CENTER_FOCUSED)rect(ctx,GRect(0,0,UI_WIDTH,XOCE_UI_HEADER_HEIGHT),ui_fg());
  header(ctx,view,heading,APP_CENTER_FOCUSED?ui_fg():menu?ui_fg():ui_bg());
  if(!count) {
    if(!APP_CENTER_FOCUSED)hourglass(ctx,1,1,UI_CENTER,110,78,180,BLUE,ui_fg(),false,view->now);
    timer_native_type_draw(ctx,app_text("Start a timer"),size_index(view),TIMER_TYPE_MENU_TITLE,
        content_band(APP_SCREEN_H-63,APP_CENTER_FOCUSED?36:50),ui_fg(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
    return;
  }
  /* Production rows, clipping, selection and scrolling belong to MenuLayer.
   * The app canvas owns only the header, warnings and full-height action rail. */
  if(replacing&&!menu&&!view->issue) {
    int height=34;
#ifdef PBL_ROUND
    height=footer_height_at(app_text("Oldest ready"),size_index(view),APP_REPLACE_TOP);
#endif
    timer_native_type_draw(ctx,app_text("Oldest ready"),size_index(view),TIMER_TYPE_FOOTER,
        content_band(APP_REPLACE_TOP,height),ui_muted(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
  }

}

typedef struct {
  const char *back,*select;
  int top,box,unit_height,back_height,select_height,field_width,field_step,left;
} EditorLayout;
#define EDITOR_FIELD_W ((UI_WIDTH-(APP_CENTER_FOCUSED?40:16))/3)
#define EDITOR_FIELD_STEP (EDITOR_FIELD_W+(APP_CENTER_FOCUSED?8:3))
#define EDITOR_LEFT (APP_CENTER_FOCUSED?14:5)
static EditorLayout editor_layout(const TimerUiView *view) {
  const bool reuse=view->editing_new&&timer_collection_count(view->collection)==TIMER_COLLECTION_CAPACITY;
  const bool invalid=view->edit[0]*3600+view->edit[1]*60+view->edit[2]<ACTIVITY_TIMER_MIN_SECONDS;
  EditorLayout layout={.box=52,.field_width=EDITOR_FIELD_W,.field_step=EDITOR_FIELD_STEP,.left=EDITOR_LEFT};const unsigned size=size_index(view);
  layout.back=app_text(view->edit[0]==24?"24 hours max":reuse?"Oldest ready":view->edit_field==0?"Back to cancel":view->edit_field==1?"Back for hours":"Back for minutes");
  layout.select=app_text(invalid?"Min: 5 seconds":view->edit_field<2?"Select: next":view->editing_new?"Select: start":"Select: save");
  static const char *units[]={"HRS","MIN","SEC"};
  for(unsigned field=0;field<3;++field) {
    const int height=timer_native_type_measure(app_text(units[field]),size,TIMER_TYPE_FOOTER,EDITOR_FIELD_W).h;
    if(height>layout.unit_height)layout.unit_height=height;
  }
  layout.back_height=timer_native_type_measure(layout.back,size,TIMER_TYPE_FOOTER,(UI_WIDTH-16)).h;
  layout.select_height=timer_native_type_measure(layout.select,size,TIMER_TYPE_FOOTER,(UI_WIDTH-16)).h;
  const int minimum=36+timer_ui_issue_height(view);
  const int tail=2+layout.unit_height+4+layout.back_height+4+layout.select_height+4;
  if(minimum+layout.box+tail>APP_SCREEN_H) {
    const int number=timer_native_type_measure("24",size,TIMER_TYPE_MENU_TITLE,50).h+12;
    layout.box=APP_SCREEN_H-minimum-tail;if(layout.box<number)layout.box=number;
  }
  layout.top=APP_SCREEN_H-layout.box-tail;
  if(layout.top>96)layout.top=96;
  if(layout.top<minimum)layout.top=minimum;
#ifdef PBL_ROUND
  layout.top=106;layout.box=EDITOR_FIELD_W;
  if(view->issue) {
    layout.field_width=50;layout.field_step=58;layout.left=(UI_WIDTH-166)/2;
    layout.box=50;layout.top=APP_HEADER_HEIGHT+timer_ui_issue_height(view)+4;
  } else {
    for(;layout.top>APP_HEADER_HEIGHT+8;layout.top-=2) {
      const int help=layout.top+layout.box+2+layout.unit_height+4;
      const int back=footer_height_at(layout.back,size,help);
      const int select=back?footer_height_at(layout.select,size,help+back+4):0;
      if(back&&select&&help+back+4+select<=240) {
        layout.back_height=back;layout.select_height=select;break;
      }
    }
  }
#endif
#if defined(ACTIVITY_TIMER_DIAGNOSTICS) && ACTIVITY_TIMER_DIAGNOSTICS
  APP_LOG(APP_LOG_LEVEL_INFO,"Editor geometry h=%d top=%d box=%d unit=%d back=%d select=%d",
      APP_SCREEN_H,layout.top,layout.box,layout.unit_height,layout.back_height,layout.select_height);
#endif
  return layout;
}
int timer_ui_editor_top(const TimerUiView *view) {return editor_layout(view).top;}
GRect timer_ui_editor_field_rect(const TimerUiView *view,unsigned field) {
  const EditorLayout layout=editor_layout(view);
  return GRect(layout.left+(int)field*layout.field_step,layout.top,layout.field_width,layout.box);
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
  if(!APP_CENTER_FOCUSED&&!view->issue&&layout.top>=84)hourglass(ctx,1,1,UI_CENTER,60,34,180,BLUE,ui_fg(),false,view->now);
  const unsigned size=size_index(view);const int top=layout.top;
  for(unsigned field=0;field<3;++field) {
    const int x=layout.left+(int)field*layout.field_step;const int field_width=layout.field_width;const bool selected=view->edit_field==field;
    const GColor bg=selected?(locked?ui_line():invalid&&field==2?AMBER:ui_fg()):ui_bg();
    const GColor ink=selected?(locked?ui_muted():invalid&&field==2?GColorBlack:ui_bg()):ui_fg();
    const TimerValueMotion motion=selected?view->editor_motion:(TimerValueMotion){0};
    if(selected&&!view->selection_transition) {
#ifdef PBL_ROUND
      graphics_context_set_fill_color(ctx,bg);
      graphics_fill_circle(ctx,GPoint(x+field_width/2,top+layout.box/2+motion.box_y/2),
          (field_width+motion.box_growth)/2);
#else
      rect(ctx,GRect(x,top+motion.box_y,field_width,layout.box+motion.box_growth),bg);
#endif
    }
    const uint32_t shown=motion.previous_value?view->editor_previous_value:view->edit[field];
    char value[4];snprintf(value,sizeof(value),"%02lu",(unsigned long)shown);
    const int h=timer_native_type_measure(value,size,TIMER_TYPE_MENU_TITLE,field_width).h;
    timer_native_type_draw(ctx,value,size,TIMER_TYPE_MENU_TITLE,GRect(x,top+(APP_CENTER_FOCUSED&&view->issue?2:(layout.box-h)/2)+motion.text_y,field_width,h),
        view->selection_transition?ui_fg():ink,GTextAlignmentCenter,GTextOverflowModeWordWrap);
    timer_native_type_draw(ctx,app_text(units[field]),size,TIMER_TYPE_FOOTER,GRect(x,top+layout.box+(APP_CENTER_FOCUSED&&view->issue?-32:2),field_width,layout.unit_height),
        APP_CENTER_FOCUSED&&view->issue&&selected&&!view->selection_transition?ink:selected?ui_fg():ui_muted(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
  }
  moving_selection(ctx,view);
  if(APP_CENTER_FOCUSED&&view->issue)return;
  const int help=top+layout.box+2+layout.unit_height+4;
  timer_native_type_draw(ctx,layout.back,size,TIMER_TYPE_FOOTER,content_band(help,layout.back_height),ui_muted(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
  timer_native_type_draw(ctx,layout.select,size,TIMER_TYPE_FOOTER,content_band(help+layout.back_height+4,layout.select_height),ui_fg(),GTextAlignmentCenter,GTextOverflowModeWordWrap);
}

static void alarm_screen(GContext *ctx,const TimerUiView *view) {
  rect(ctx,GRect(0,0,APP_SCREEN_W,APP_SCREEN_H),GColorBlack);
  bar(ctx,view,XOCE_ICON_SNOOZE,XOCE_ICON_CHECK,XOCE_ICON_NONE,GColorDarkGray);header(ctx,view,"Alarm",GColorWhite);
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];char duration[20];clock_text(timer->repeat_seconds,duration,sizeof(duration));
  const unsigned size=size_index(view);const int bottom=(APP_CENTER_FOCUSED?224:APP_SCREEN_H)-timer_ui_issue_height(view);
  timer_native_type_countdown(ctx,duration,content_band(APP_CENTER_FOCUSED?76:38,46),GColorWhite);
  if(APP_CENTER_FOCUSED&&view->issue)return;
  const int first=timer_native_type_measure(app_text("Select: Done"),size,TIMER_TYPE_FOOTER,APP_CENTER_FOCUSED?144:(UI_WIDTH-16)).h;
  const int second=timer_native_type_measure(app_text("Up: Snooze 1m"),size,TIMER_TYPE_FOOTER,APP_CENTER_FOCUSED?144:(UI_WIDTH-16)).h;
  const int text_top=bottom-first-second-(APP_CENTER_FOCUSED?26:8),space=text_top-(APP_CENTER_FOCUSED?128:88),glass=space>80?80:space;
  if(glass>12)hourglass(ctx,0,1,UI_CENTER,(APP_CENTER_FOCUSED?126:86)+glass/2,glass,0,BLUE,GColorWhite,false,view->now);
  timer_native_type_draw(ctx,app_text("Select: Done"),size,TIMER_TYPE_FOOTER,bounded_band(text_top,first,144),GColorWhite,GTextAlignmentCenter,GTextOverflowModeWordWrap);
  timer_native_type_draw(ctx,app_text("Up: Snooze 1m"),size,TIMER_TYPE_FOOTER,bounded_band(text_top+first+4,second,144),GColorWhite,GTextAlignmentCenter,GTextOverflowModeWordWrap);
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
  char text[64];const int height=timer_native_type_measure(action_label(view,row,text),size_index(view),TIMER_TYPE_MENU_TITLE,APP_CENTER_FOCUSED?184:(UI_WIDTH-16)).h+12;
  return height<36?36:height;
}
void timer_ui_draw_action_row(GContext *ctx,const Layer *cell,const TimerUiView *view,unsigned row) {
  const bool selected=menu_cell_layer_is_highlighted(cell);
  const GColor bg=selected?(view->dark?GColorWhite:GColorBlack):(view->dark?GColorBlack:GColorWhite);
  const GColor fg=selected?(view->dark?GColorBlack:GColorWhite):(view->dark?GColorWhite:GColorBlack);
  const GRect bounds=layer_get_bounds(cell);char text[64];const char *label=action_label(view,row,text);
  const unsigned size=size_index(view);const int height=timer_native_type_measure(label,size,TIMER_TYPE_MENU_TITLE,APP_CENTER_FOCUSED?184:(UI_WIDTH-16)).h;
  rect(ctx,bounds,bg);
#ifdef PBL_ROUND
  const int top=(bounds.size.h-height)/2;
  const GPoint at=layer_convert_point_to_screen(cell,GPoint(8,top));
  GRect band=xoce_ui_circle_band(APP_SCREEN_FRAME,GRect(at.x,at.y,UI_WIDTH-36,height),6);
  band.origin.x-=at.x-8;band.origin.y=top;
  if(band.size.w>184){band.origin.x+=(band.size.w-184)/2;band.size.w=184;}
  timer_native_type_draw(ctx,label,size,TIMER_TYPE_MENU_TITLE,band,fg,GTextAlignmentCenter,selected?GTextOverflowModeWordWrap:GTextOverflowModeTrailingEllipsis);
  return;
#endif
  timer_native_type_draw(ctx,label,size,TIMER_TYPE_MENU_TITLE,
      GRect(8,(bounds.size.h-height)/2,(UI_WIDTH-16),height),fg,GTextAlignmentLeft,GTextOverflowModeWordWrap);
}
static int context_line(GContext *ctx,const TimerUiView *view,const char *text,int top,TimerNativeTypeRole role) {
#ifdef PBL_ROUND
  GTextAttributes *attributes=graphics_text_attributes_create();
  if(attributes){
    graphics_text_attributes_enable_screen_text_flow(attributes,8);
    graphics_text_attributes_enable_paging(attributes,GPoint(0,APP_HEADER_HEIGHT),GRect(0,APP_HEADER_HEIGHT,UI_WIDTH,APP_SCREEN_H-APP_HEADER_HEIGHT));
  }
  const GFont font=fonts_get_system_font(timer_native_type_key(size_index(view),role,true));
  GRect box=GRect(8,top,UI_WIDTH-16,2048);
  const int flowed=graphics_text_layout_get_content_size_with_attributes(text,font,box,GTextOverflowModeWordWrap,GTextAlignmentLeft,attributes).h;
  if(ctx){box.size.h=flowed;graphics_context_set_text_color(ctx,ui_muted());graphics_draw_text(ctx,text,font,box,GTextOverflowModeWordWrap,GTextAlignmentLeft,attributes);}
  if(attributes)graphics_text_attributes_destroy(attributes);
  return top+flowed+4;
#endif
  const int height=timer_native_type_measure(text,size_index(view),role,(UI_WIDTH-16)).h;
  if(ctx)timer_native_type_draw(ctx,text,size_index(view),role,GRect(8,top,(UI_WIDTH-16),height),
      ui_muted(),GTextAlignmentLeft,GTextOverflowModeWordWrap);
  return top+height+4;
}
static GRect warning_band(int top,int height,int offset) {
  GRect band=content_band(top+offset,height);band.origin.y-=offset;
  if(APP_CENTER_FOCUSED&&band.size.w>160){band.origin.x+=(band.size.w-160)/2;band.size.w=160;}
  return band;
}
static int issue_panel(GContext *ctx,const TimerUiView *view,int top,int offset) {
  const int height=timer_ui_issue_height(view);if(!height)return top;
  const char *message=app_text(!strcmp(view->issue,"Changes not saved")?"Not saved":view->issue);
  const unsigned size=size_index(view);const int first=timer_native_type_measure(message,size,TIMER_TYPE_FOOTER,APP_CENTER_FOCUSED?160:(UI_WIDTH-16)).h;
  if(ctx) {
    rect(ctx,GRect(0,top,APP_CENTER_FOCUSED?APP_SCREEN_W:UI_WIDTH,height),AMBER);
    timer_native_type_draw(ctx,message,size,TIMER_TYPE_FOOTER,warning_band(top+2,first,offset),GColorBlack,GTextAlignmentCenter,GTextOverflowModeWordWrap);
    timer_native_type_draw(ctx,app_text(view->appearance_issue?"Retry phone app":"Keep app open"),size,TIMER_TYPE_FOOTER,
        warning_band(top+first+4,height-first-4,offset),GColorBlack,GTextAlignmentCenter,GTextOverflowModeWordWrap);
  }
  return top+height;
}
static int action_context(GContext *ctx,const TimerUiView *view) {
  s_dark=view->dark;int top=issue_panel(ctx,view,0,APP_CENTER_FOCUSED?APP_HEADER_HEIGHT:0)+4;
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
#ifdef PBL_ROUND
unsigned timer_ui_context_copy(const TimerUiView *view,char lines[3][96]) {
  const ActivityTimerModel *timer=&view->collection->slots[view->selected];char duration[24];
  duration_text(timer->repeat_seconds,duration,sizeof(duration),false);
  if(view->screen==SCREEN_CONFIRM) {
    snprintf(lines[0],96,"%s",app_text("Delete timer?"));
    snprintf(lines[1],96,app_text("%s timer"),duration);
    snprintf(lines[2],96,"%s",app_text("Cannot undo"));return 3;
  }
  snprintf(lines[0],96,"%s · %s",duration,app_text(timer->state==ACTIVITY_TIMER_RUNNING?"Running":timer->state==ACTIVITY_TIMER_PAUSED?"Paused":activity_timer_ready(timer)?"Ready":"Time's up"));
  const char *status=view->status&&view->status[0]?view->status:"Back to timer";
  if(!strcmp(status,"Phone disconnected"))status="Phone offline";
  else if(!strcmp(status,"Sharing unavailable"))status="Sharing failed";
  else if(!strcmp(status,"Shared with phone"))status="Phone received";
  snprintf(lines[1],96,"%s",app_text(status));return 2;
}
#endif
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
#ifdef PBL_ROUND
  if(view->issue&&view->issue[0])issue_panel(ctx,view,224-timer_ui_issue_height(view),0);
#endif
}

void timer_ui_draw(GContext *ctx,const TimerUiView *view) {
  s_dark=view&&view->dark;rect(ctx,GRect(0,0,APP_SCREEN_W,APP_SCREEN_H),ui_bg());if(!view||!view->collection)return;
  switch(view->screen){case SCREEN_LIST:list_screen(ctx,view);break;case SCREEN_EDITOR:editor_screen(ctx,view);break;case SCREEN_ALARM:alarm_screen(ctx,view);break;case SCREEN_OPTIONS:case SCREEN_CONFIRM:action_screen(ctx,view);break;default:main_screen(ctx,view);break;}
  if(view->issue&&view->issue[0]&&view->screen!=SCREEN_OPTIONS&&view->screen!=SCREEN_CONFIRM) {
    const int top=view->screen==SCREEN_EDITOR?APP_HEADER_HEIGHT:(APP_CENTER_FOCUSED?224:APP_SCREEN_H)-timer_ui_issue_height(view);
    issue_panel(ctx,view,top,0);
  }
}
