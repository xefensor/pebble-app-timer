#include "../src/c/timer_ui_motion.h"
#include <assert.h>
#include <stdio.h>

static void selection(TimerMotionRect from,TimerMotionRect to,
                      uint32_t stretch,uint32_t settle) {
  TimerMotionRect first=timer_motion_selection(from,to,0,stretch,settle);
  assert(first.x==from.x&&first.y==from.y&&first.w==from.w&&first.h==from.h);
  for(uint32_t ms=0;ms<=stretch+settle;++ms) {
    const TimerMotionRect r=timer_motion_selection(from,to,ms,stretch,settle);
    assert(r.x>=0&&r.y>=32&&r.w>0&&r.h>0);
    assert(r.x+r.w<=166&&r.y+r.h<=215);
  }
  const TimerMotionRect last=timer_motion_selection(from,to,UINT32_MAX,stretch,settle);
  assert(last.x==to.x&&last.y==to.y&&last.w==to.w&&last.h==to.h);
}

int main(void) {
  selection((TimerMotionRect){0,40,166,54},(TimerMotionRect){0,96,166,54},100,250);
  selection((TimerMotionRect){0,152,166,54},(TimerMotionRect){0,96,166,54},100,250);
  selection((TimerMotionRect){0,58,166,41},(TimerMotionRect){0,101,166,41},100,250);
  selection((TimerMotionRect){0,143,166,32},(TimerMotionRect){0,179,166,32},100,250);
  selection((TimerMotionRect){58,104,50,45},(TimerMotionRect){111,104,50,45},107,179);
  selection((TimerMotionRect){58,104,50,45},(TimerMotionRect){5,104,50,45},107,179);
  /* A reverse press continues from the currently stretched edges. */
  selection((TimerMotionRect){0,56,166,95},(TimerMotionRect){0,40,166,54},100,250);
  for(uint32_t ms=0;ms<400;++ms)assert(timer_motion_bump(ms)>=0&&timer_motion_bump(ms)<=3);
  assert(!timer_motion_bump(0)&&timer_motion_bump(107)==3&&!timer_motion_bump(321));
  assert(!timer_motion_bump(UINT32_MAX));
  assert(timer_motion_ease(5,0,0,144)==5&&!timer_motion_ease(5,0,144,144));
  puts("timer UI motion: elastic row/field bounds, finite endpoints, bounded value and action feedback passed");
}
