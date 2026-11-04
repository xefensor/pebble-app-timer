#include "../src/c/timer_hourglass.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned row_sum(const uint8_t *rows) {
  unsigned sum=0;
  for(unsigned row=0;row<TIMER_SAND_ROWS;++row)sum+=rows[row];
  return sum;
}

static void verify_fill(uint32_t remaining, uint32_t total, TimerSandFill *fill) {
  timer_sand_fill(remaining,total,fill);
  assert(row_sum(fill->upper)==fill->upper_pixels);
  assert(row_sum(fill->lower)==fill->lower_pixels);
  assert(fill->upper_pixels+fill->lower_pixels==timer_sand_capacity());
  for(unsigned row=0;row<TIMER_SAND_ROWS;++row) {
    assert(fill->upper[row]<=timer_sand_width[row]);
    assert(fill->lower[row]<=timer_sand_width[TIMER_SAND_ROWS-1-row]);
    /* Sand always settles at the base of each chamber. A row cannot float
     * over an unfilled row below it, including at fractional boundaries. */
    if(row+1<TIMER_SAND_ROWS) {
      if(fill->upper[row])assert(fill->upper[row+1]==timer_sand_width[row+1]);
      if(fill->lower[row])assert(fill->lower[row+1]==timer_sand_width[TIMER_SAND_ROWS-2-row]);
    }
  }
}

static void countdown(uint32_t total) {
  TimerSandFill current,previous;
  verify_fill(total,total,&previous);
  assert(previous.upper_pixels==timer_sand_capacity()&&previous.lower_pixels==0);
  for(uint32_t remaining=total;remaining>0;--remaining) {
    verify_fill(remaining-1,total,&current);
    assert(current.upper_pixels<=previous.upper_pixels);
    assert(current.lower_pixels>=previous.lower_pixels);
    for(unsigned row=0;row<TIMER_SAND_ROWS;++row) {
      assert(current.upper[row]<=previous.upper[row]);
      assert(current.lower[row]>=previous.lower[row]);
    }
    previous=current;
  }
  assert(previous.upper_pixels==0&&previous.lower_pixels==timer_sand_capacity());
}

static void orientation(void) {
  assert(timer_hourglass_pose(TIMER_GLASS_READY)==180);
  assert(timer_hourglass_pose(TIMER_GLASS_RUNNING)==0);
  assert(timer_hourglass_pose(TIMER_GLASS_PAUSED)==90);
  TimerGlassPoint point=timer_hourglass_transform(17,13,100,0);
  assert(point.x==17&&point.y==-27);
  point=timer_hourglass_transform(17,13,100,90);
  assert(point.x==27&&point.y==17);
  point=timer_hourglass_transform(17,13,100,180);
  assert(point.x==-17&&point.y==27);
  const int scales[]={50,70,85,100};
  const int xs[]={-29,0,29},ys[]={0,40,79};
  for(unsigned scale=0;scale<sizeof(scales)/sizeof(scales[0]);++scale) {
    for(int angle=0;angle<=180;++angle) {
      point=timer_hourglass_transform(0,40,scales[scale],angle);
      assert(!point.x&&!point.y);
      for(unsigned x=0;x<sizeof(xs)/sizeof(xs[0]);++x) {
        for(unsigned y=0;y<sizeof(ys)/sizeof(ys[0]);++y) {
          point=timer_hourglass_transform(xs[x],ys[y],scales[scale],angle);
          /* The farthest canonical corner is under 50px from the pivot.
           * Rotation must fit the same bounded square at every frame. */
          const int bound=(50*scales[scale]+99)/100;
          assert(abs(point.x)<=bound&&abs(point.y)<=bound);
          if(angle) {
            TimerGlassPoint previous=timer_hourglass_transform(xs[x],ys[y],scales[scale],angle-1);
            assert(abs(point.x-previous.x)<=3&&abs(point.y-previous.y)<=3);
          }
        }
      }
      /* A rigid rotation must retain handedness and area, within integer
       * raster rounding. This catches axis flips or a collapsed midpoint. */
      TimerGlassPoint xaxis=timer_hourglass_transform(20,40,scales[scale],angle);
      TimerGlassPoint yaxis=timer_hourglass_transform(0,60,scales[scale],angle);
      const int area=xaxis.x*yaxis.y-xaxis.y*yaxis.x;
      const int expected=400*scales[scale]*scales[scale]/10000;
      assert(area>0&&abs(area-expected)<=expected/4+4);
    }
  }
  /* A rapid second press starts from the displayed intermediate orientation. */
  const int endpoints[][2]={{180,0},{0,90},{90,0},{0,180},{180,90},{90,180},
      {73,0},{31,90},{149,0}};
  for(unsigned run=0;run<sizeof(endpoints)/sizeof(endpoints[0]);++run) {
    const int from=endpoints[run][0],to=endpoints[run][1];
    int previous=from;
    for(uint32_t elapsed=0;elapsed<=300;++elapsed) {
      const int angle=timer_hourglass_angle(from,to,elapsed,300);
      assert(angle>=(from<to?from:to)&&angle<=(from>to?from:to));
      assert(from<to?angle>=previous:angle<=previous);
      if(!elapsed)assert(angle==from);
      if(elapsed==300)assert(angle==to);
      previous=angle;
    }
    assert(timer_hourglass_angle(from,to,0,0)==to);
    assert(timer_hourglass_angle(from,to,UINT32_MAX,300)==to);
  }
}

int main(void) {
  assert(timer_sand_capacity()==1073);
  for(unsigned row=0;row<TIMER_SAND_ROWS;++row) {
    assert(timer_sand_width[row]>=5&&timer_sand_width[row]<=49);
    assert(timer_sand_width[row]&1u);
    if(row)assert(timer_sand_width[row]<=timer_sand_width[row-1]);
  }
  countdown(5);
  countdown(600);
  countdown(86400);
  TimerSandFill fill;
  verify_fill(UINT32_MAX,UINT32_MAX,&fill);
  assert(fill.upper_pixels==timer_sand_capacity());
  verify_fill(UINT32_MAX,1,&fill);
  assert(fill.upper_pixels==timer_sand_capacity());
  verify_fill(0,0,&fill);
  assert(fill.lower_pixels==timer_sand_capacity());
  orientation();
  puts("timer UI: conserved sand, monotonic 24h progress, rigid state orientations and bounded finite easing passed");
  return 0;
}
