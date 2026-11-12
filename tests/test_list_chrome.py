"""Exercise production list/header paint without requiring the Pebble SDK."""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]

class ListChrome(unittest.TestCase):
    def test_header_focus_and_rows_have_one_highlight_and_no_progress_bar(self):
        source=(ROOT/'src/c/timer_ui.c').read_text()
        def function(signature):
            start=source.index(signature)
            return source[start:source.index('\n}',start)+2]
        prelude=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int GContext,Layer,GColor;
typedef struct {int w,h;} GSize;
typedef struct {int x,y,w,h;} Rect;
#define GRect(x,y,w,h) ((Rect){x,y,w,h})
enum {UI_WIDTH=166,UI_CENTER=83,XOCE_UI_HEADER_HEIGHT=32,TIMER_COLLECTION_CAPACITY=8,
 GColorBlack=0,GColorWhite=255,GColorDarkGray=85,BLUE=170,AMBER=171,
 XOCE_ICON_UP,XOCE_ICON_NONE,XOCE_ICON_NEXT,XOCE_ICON_PLUS,XOCE_ICON_DOWN,
 TIMER_TYPE_MENU_TITLE,TIMER_TYPE_MENU_SUBTITLE,TIMER_TYPE_FOOTER,
 GTextAlignmentLeft,GTextAlignmentRight,GTextAlignmentCenter,GTextOverflowModeWordWrap,
 ACTIVITY_TIMER_PAUSED=2};
typedef struct {unsigned state;} Model;
typedef struct {Model slots[8];unsigned count;bool create;} Collection;
typedef struct {Collection *collection;unsigned menu,now;bool dark;const char *issue;} TimerUiView;
typedef struct {
 char clock[20],saved[64],support[128];const char *state;bool joined,clock_inline;
 GSize clock_size,state_size,saved_size;int first_height,pad,gap,height;uint32_t left,total;
} RowLayout;
static bool s_dark;
static unsigned rectangles;static Rect painted;static GColor color,header_ink;
static char heading[32];
static GColor ui_fg(void){return s_dark?255:0;}
static GColor ui_bg(void){return s_dark?0:255;}
static GColor ui_muted(void){return ui_fg();}
static unsigned size_index(const TimerUiView *v){(void)v;return 0;}
static unsigned timer_collection_count(const Collection *c){return c->count;}
static bool timer_collection_can_create(const Collection *c){return c->create;}
static const char *app_text(const char *s){return s;}
static void rect(GContext *c,Rect r,GColor ink){(void)c;rectangles++;painted=r;color=ink;}
static void bar(GContext *c,...){(void)c;}
static void hourglass(GContext *c,...){(void)c;}
static void timer_native_type_draw(GContext *c,const char *text,...){(void)c;(void)text;}
static void header(GContext *c,const TimerUiView *v,const char *title,GColor ink){
 (void)c;(void)v;header_ink=ink;snprintf(heading,sizeof(heading),"%s",title);
}
static bool menu_cell_layer_is_highlighted(const Layer *l){(void)l;return true;}
static Rect layer_get_bounds(const Layer *l){(void)l;return GRect(0,0,166,60);}
static RowLayout row_layout(const TimerUiView *v,unsigned slot){
 (void)v;(void)slot;return (RowLayout){.clock="05:00",.saved="Set 5 min",.state="Paused",
 .clock_inline=true,.clock_size={60,28},.state_size={50,24},.saved_size={70,24},
 .first_height=28,.pad=3,.gap=2,.height=60,.left=150,.total=300};
}
'''
        cases=r'''
int main(void){
 Collection c={.count=1,.create=true};TimerUiView v={.collection=&c};
 for(unsigned dark=0;dark<2;++dark)for(unsigned count=0;count<=8;count+=count?7:1){
  s_dark=v.dark=dark;c.count=count;
  for(unsigned menu=0;menu<=1;++menu){
   v.menu=menu;rectangles=0;list_screen(NULL,&v);
   assert(header_ink==(menu?ui_fg():ui_bg()));
   if(!menu){
    assert(rectangles==1&&painted.x==0&&painted.y==0&&painted.w==166&&painted.h==32);
    assert(color==ui_fg()&&!strcmp(heading,count==8?"Replace":"New"));
   }else assert(!rectangles);
   for(unsigned state=1;state<=2;++state){
    c.slots[0].state=state;rectangles=0;timer_ui_draw_row(NULL,NULL,&v,0);
    assert(rectangles==1); // Only the row background, with no progress track/fill.
    assert(painted.w==166&&painted.h==60);
    assert(color==(menu?ui_fg():ui_bg())); // A remembered native row cannot claim header focus.
   }
  }
 }
 return 0;
}
'''
        with tempfile.TemporaryDirectory(prefix='timer-list-chrome-')as directory:
            path=Path(directory);src=path/'chrome.c';binary=path/'chrome'
            src.write_text(prelude+function('void timer_ui_draw_row(')+
                function('static void list_screen(')+cases)
            subprocess.run([os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Werror','-pedantic',
                str(src),'-o',str(binary)],check=True)
            subprocess.run([str(binary)],check=True)
