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
  const TimerMotionRect pairs[][2]={
    {{0,46,166,80},{0,134,166,80}},
    {{0,134,166,80},{0,46,166,80}},
    {{0,68,166,38},{0,112,166,38}},
    {{0,160,166,30},{0,196,166,30}}
  };
  for(unsigned n=0;n<sizeof(pairs)/sizeof(pairs[0]);++n) {
    const TimerMotionRect from=pairs[n][0],to=pairs[n][1];
    int previous=from.y;
    for(uint32_t ms=0;ms<=TIMER_MENU_MOTION_MS;++ms) {
      const TimerMotionRect r=timer_motion_menu(from,to,ms);
      assert(r.x==to.x&&r.w==to.w&&r.h==to.h);
      assert(r.y>=(from.y<to.y?from.y:to.y)&&r.y<=(from.y>to.y?from.y:to.y));
      assert(to.y>from.y?r.y>=previous:r.y<=previous);
      previous=r.y;
    }
    assert(previous==to.y);
    assert(timer_motion_menu(from,to,UINT32_MAX).y==to.y);
  }
  /* Rapid reverse resumes a rigid visible position, never stretched edges. */
  const TimerMotionRect mid=timer_motion_menu(pairs[0][0],pairs[0][1],40);
  assert(mid.h==80);
  assert(timer_motion_menu(mid,pairs[0][0],0).y==mid.y);
  assert(timer_motion_menu(mid,pairs[0][0],120).y==46);
  selection((TimerMotionRect){58,96,50,52},(TimerMotionRect){111,96,50,52},107,179);
  selection((TimerMotionRect){58,96,50,52},(TimerMotionRect){5,96,50,52},107,179);
  for(uint32_t ms=0;ms<400;++ms)assert(timer_motion_bump(ms)>=0&&timer_motion_bump(ms)<=3);
  assert(!timer_motion_bump(0)&&timer_motion_bump(107)==3&&!timer_motion_bump(321));
  assert(!timer_motion_bump(UINT32_MAX));
  assert(timer_motion_ease(5,0,0,144)==5&&!timer_motion_ease(5,0,144,144));
  puts("timer UI motion: rigid120ms choices, rapid reverse, elastic editor bounds and finite feedback passed");
}
