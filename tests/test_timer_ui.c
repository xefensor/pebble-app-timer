#include "../src/c/timer_hourglass.h"

#include <assert.h>
#include <limits.h>
#include <stdio.h>

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
  puts("timer UI: bounded hourglass silhouette, exact conserved sand area and monotonic 24h progress passed");
  return 0;
}
