"""Run the production row planner with controlled measured glyph widths.
Actual Pebble fonts and pixels are covered separately by emulator captures.
"""
from pathlib import Path
import os,subprocess,tempfile,unittest
ROOT=Path(__file__).resolve().parents[1]
class CompactRows(unittest.TestCase):
    def test_primary_and_supporting_pairs_at_the_width_boundary(self):
        source=(ROOT/'src/c/timer_ui.c').read_text()
        start=source.index('typedef struct {\n  char clock[20],saved[64],support[128];')
        planner=source[start:source.index('int16_t timer_ui_row_height',start)]
        prelude=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef struct {int w,h;} GSize;
typedef struct {int state;uint32_t repeat_seconds,configured_seconds;} ActivityTimerModel;
typedef struct {ActivityTimerModel slots[8];} Collection;
typedef struct {Collection *collection;uint32_t now;} TimerUiView;
enum {ACTIVITY_TIMER_RUNNING=1,ACTIVITY_TIMER_PAUSED=2,TIMER_TYPE_MENU_TITLE=0,TIMER_TYPE_MENU_SUBTITLE=1};
static int clock_width=60,state_width=76,support_width=142;
static bool activity_timer_ready(const ActivityTimerModel *t){return t->state==0;}
static unsigned size_index(const TimerUiView *v){(void)v;return 0;}
static uint32_t activity_timer_remaining(const ActivityTimerModel *t,uint32_t n){(void)t;(void)n;return 299;}
static const char *app_text(const char *s){return s;}
static void clock_text(uint32_t n,char *out,size_t cap){(void)n;snprintf(out,cap,"04:59");}
static void duration_text(uint32_t n,char *out,size_t cap,bool title){(void)n;(void)title;snprintf(out,cap,"5 min");}
static GSize timer_native_type_measure(const char *s,unsigned size,unsigned role,int available){
 (void)size;int width=role==TIMER_TYPE_MENU_TITLE?clock_width:strstr(s," · ")?support_width:strncmp(s,"Set",3)?state_width:50;
 return (GSize){width,((width+available-1)/available)*(role==TIMER_TYPE_MENU_TITLE?28:24)};
}
'''
        cases=r'''
int main(void){
 Collection c={0};c.slots[0]=(ActivityTimerModel){ACTIVITY_TIMER_PAUSED,300,300};TimerUiView v={&c,0};
 for(state_width=76;state_width<=78;++state_width){
  RowLayout r=row_layout(&v,0);assert(r.clock_inline==(state_width<=77));
  assert(!strcmp(r.clock,"04:59")&&!strcmp(r.saved,"Set 5 min"));
  assert(r.height==60);
 }
 state_width=120; // Countdown/state stack; supporting pair then uses its own measured boundary.
 for(support_width=142;support_width<=144;++support_width){
  RowLayout r=row_layout(&v,0);assert(!r.clock_inline);assert(r.joined==(support_width<=143));
  assert(r.height==(r.joined?60:86));
 }
 c.slots[0].state=0;state_width=50;RowLayout ready=row_layout(&v,0);
 assert(ready.clock_inline&&!ready.saved[0]&&ready.height==34);
 c.slots[0].state=ACTIVITY_TIMER_RUNNING;clock_width=180;
 RowLayout long_value=row_layout(&v,0);assert(!long_value.clock_inline&&long_value.clock_size.h==56);
 return 0;
}
'''
        with tempfile.TemporaryDirectory()as directory:
            path=Path(directory);(path/'rows.c').write_text(prelude+planner+cases)
            subprocess.run([os.environ.get('CC','cc'),'-std=c99','-Wall','-Wextra','-Werror',str(path/'rows.c'),'-o',str(path/'rows')],check=True)
            subprocess.run([str(path/'rows')],check=True)
