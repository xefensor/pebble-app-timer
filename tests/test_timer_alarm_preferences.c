#include <pebble.h>
#include <assert.h>
#include <string.h>
#include "../src/c/timer_alarm_preferences.h"
static uint8_t stored[12];static bool exists,fail_write,corrupt_read;static int size;static unsigned writes;
bool persist_exists(uint32_t key){assert(key==2107);return exists;}
int persist_get_size(uint32_t key){assert(key==2107);return size;}
int persist_read_data(uint32_t key,void *data,size_t n){assert(key==2107&&n==12);if(!exists)return -1;memcpy(data,stored,n);if(corrupt_read)((uint8_t*)data)[8]^=1;return (int)n;}
int persist_write_data(uint32_t key,const void *data,size_t n){assert(key==2107&&n==12);++writes;if(fail_write)return -1;memcpy(stored,data,n);size=n;exists=true;return (int)n;}
int main(void){
 TimerAlarmPreferences v,decoded;uint8_t bytes[12],status,flags;
 assert(timer_alarm_preferences_load(&v)&&v.sound&&v.revision==0); /* New install and upgrade. */
 v=(TimerAlarmPreferences){7,false};assert(timer_alarm_preferences_save(&v));
 assert(timer_alarm_preferences_load(&decoded)&&!decoded.sound&&decoded.revision==7);
 unsigned before=writes;assert(timer_alarm_preferences_save(&v)&&writes==before);
 timer_alarm_preferences_encode(&v,0,3,bytes);assert(timer_alarm_preferences_decode(bytes,12,&decoded,&status,&flags)&&flags==3&&!status);
 assert(!timer_alarm_preferences_decode(bytes,11,&decoded,NULL,NULL));
 for(unsigned i=0;i<4;++i){uint8_t copy[12];memcpy(copy,bytes,12);copy[i]=99;assert(!timer_alarm_preferences_decode(copy,12,&decoded,NULL,NULL));}
 bytes[8]=2;assert(!timer_alarm_preferences_decode(bytes,12,&decoded,NULL,NULL));
 v.sound=true;v.revision=8;fail_write=true;assert(!timer_alarm_preferences_save(&v));fail_write=false;
 assert(timer_alarm_preferences_load(&decoded)&&!decoded.sound&&decoded.revision==7);
 corrupt_read=true;assert(!timer_alarm_preferences_save(&v));corrupt_read=false;
 assert(timer_alarm_preferences_save(&v)&&timer_alarm_preferences_load(&decoded)&&decoded.sound&&decoded.revision==8);
 stored[0]=99;assert(!timer_alarm_preferences_load(&decoded)&&!decoded.sound); /* Damaged record must not re-enable sound. */
 v.revision=0;assert(!timer_alarm_preferences_save(&v));return 0;
}
