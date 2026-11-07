#include "../src/c/timer_ui_motion.h"
#include <assert.h>
#include <stdio.h>

static void selection(TimerMotionRect from,TimerMotionRect to) {
  TimerMotionRect first=timer_motion_field(from,to,0);
  assert(first.x==from.x&&first.y==from.y&&first.w==from.w&&first.h==from.h);
  for(uint32_t ms=0;ms<=TIMER_FIELD_SLIDE_MS+TIMER_FIELD_SETTLE_MS;++ms) {
    const TimerMotionRect r=timer_motion_field(from,to,ms);
    assert(r.x>=0&&r.y>=32&&r.w>0&&r.h>0);
    assert(r.x+r.w<=166&&r.y+r.h<=215);
    /* Only the three-pixel field gap may grow. Adjacent fields stay legible. */
    assert(r.w>=50&&r.w<=53&&r.h==52&&r.y==96);
  }
  const TimerMotionRect edge=timer_motion_field(from,to,TIMER_FIELD_SLIDE_MS);
  assert(edge.w==53&&edge.x==to.x-(to.x<from.x?3:0));
  const TimerMotionRect last=timer_motion_field(from,to,UINT32_MAX);
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
  selection((TimerMotionRect){58,96,50,52},(TimerMotionRect){111,96,50,52});
  selection((TimerMotionRect){58,96,50,52},(TimerMotionRect){5,96,50,52});
  const TimerMotionRect accelerating=timer_motion_field(
      (TimerMotionRect){58,96,50,52},(TimerMotionRect){111,96,50,52},53);
  assert(accelerating.x>58&&accelerating.x<84); /* Native EaseIn first stage. */
  for(int direction=-1;direction<=1;direction+=2) {
    for(uint32_t ms=0;ms<400;++ms) {
      const TimerValueMotion value=timer_motion_value(ms,direction);
      assert(value.text_y*direction>=0&&value.text_y*direction<=10);
      assert(value.box_growth<=6&&value.box_y<=0);
      /* Extension is exclusively toward the pressed edge, never sideways. */
      assert(direction<0?value.box_y==-(int)value.box_growth:value.box_y==0);
      assert(96+value.box_y>=90&&96+value.box_y+52+value.box_growth<=154);
      assert(value.previous_value==(ms<TIMER_VALUE_TEXT_MS));
      if(ms<TIMER_VALUE_TEXT_MS)assert(value.box_growth==0);
    }
    const TimerValueMotion edge=timer_motion_value(107,direction);
    assert(edge.box_growth==6&&edge.text_y==direction*3&&!edge.previous_value);
    const TimerValueMotion last=timer_motion_value(UINT32_MAX,direction);
    assert(!last.text_y&&!last.box_y&&!last.box_growth&&!last.previous_value);
  }
  assert(timer_motion_ease(5,0,0,144)==5&&!timer_motion_ease(5,0,144,144));
  puts("timer UI motion: rigid120ms choices, native field slide and directional value bounds passed");
}
