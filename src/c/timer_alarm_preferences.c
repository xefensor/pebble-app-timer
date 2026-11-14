#include <pebble.h>
#include <string.h>
#include "timer_alarm_preferences.h"
#define ALARM_PREFERENCES_KEY 2107
static uint32_t get32(const uint8_t *b){return (uint32_t)b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;}
static void put32(uint8_t *b,uint32_t n){for(unsigned i=0;i<4;++i)b[i]=(uint8_t)(n>>(8*i));}
void timer_alarm_preferences_encode(const TimerAlarmPreferences *v,uint8_t status,uint8_t flags,uint8_t b[TIMER_ALARM_PREFERENCES_BYTES]){
 memset(b,0,TIMER_ALARM_PREFERENCES_BYTES);b[0]=1;b[1]=1;b[2]=status;b[3]=flags;put32(b+4,v->revision);put32(b+8,v->sound);
}
bool timer_alarm_preferences_decode(const uint8_t *b,size_t n,TimerAlarmPreferences *v,uint8_t *status,uint8_t *flags){
 if(!b||!v||n!=TIMER_ALARM_PREFERENCES_BYTES||b[0]!=1||b[1]!=1||b[2]>1||(b[3]&~3)||get32(b+8)>1)return false;
 *v=(TimerAlarmPreferences){get32(b+4),get32(b+8)!=0};if(status)*status=b[2];if(flags)*flags=b[3];return true;
}
bool timer_alarm_preferences_load(TimerAlarmPreferences *v){
 *v=(TimerAlarmPreferences){0,true};if(!persist_exists(ALARM_PREFERENCES_KEY))return true;
 uint8_t b[TIMER_ALARM_PREFERENCES_BYTES],status,flags;TimerAlarmPreferences saved;
 if(persist_get_size(ALARM_PREFERENCES_KEY)!=(int)sizeof(b)||persist_read_data(ALARM_PREFERENCES_KEY,b,sizeof(b))!=(int)sizeof(b)||
   !timer_alarm_preferences_decode(b,sizeof(b),&saved,&status,&flags)||status||flags||!saved.revision){v->sound=false;return false;}
 *v=saved;return true;
}
bool timer_alarm_preferences_save(const TimerAlarmPreferences *v){
 if(!v||!v->revision)return false;
 uint8_t b[TIMER_ALARM_PREFERENCES_BYTES],check[TIMER_ALARM_PREFERENCES_BYTES];timer_alarm_preferences_encode(v,0,0,b);
 bool same=persist_read_data(ALARM_PREFERENCES_KEY,check,sizeof(check))==(int)sizeof(check)&&!memcmp(b,check,sizeof(b));
 if(!same&&persist_write_data(ALARM_PREFERENCES_KEY,b,sizeof(b))!=(int)sizeof(b))return false;
 return persist_get_size(ALARM_PREFERENCES_KEY)==(int)sizeof(b)&&persist_read_data(ALARM_PREFERENCES_KEY,check,sizeof(check))==(int)sizeof(check)&&!memcmp(b,check,sizeof(b));
}
