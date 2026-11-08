#include <pebble.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../src/c/timer_ui_preferences.h"

static uint8_t stored[TIMER_UI_PREFERENCES_BYTES];
static bool blob_exists,legacy_exists,fail_write,fail_read,wrong_readback;
static int legacy=2,stored_size=TIMER_UI_PREFERENCES_BYTES;
static unsigned blob_writes;
bool persist_exists(uint32_t key){assert(key==2105||key==2106);return key==2105?legacy_exists:blob_exists;}
int persist_get_size(uint32_t key){assert(key==2106);return stored_size;}
int32_t persist_read_int(uint32_t key){assert(key==2105);return legacy;}
int persist_write_int(uint32_t key,int32_t value){assert(key==2105);legacy_exists=true;legacy=value;return sizeof(value);}
int persist_read_data(uint32_t key,void *data,size_t size){
 assert(key==2106&&size==sizeof(stored));if(fail_read||!blob_exists)return -1;
 memcpy(data,stored,size);if(wrong_readback)((uint8_t *)data)[8]^=1;return (int)size;
}
int persist_write_data(uint32_t key,const void *data,size_t size){
 assert(key==2106&&size==sizeof(stored));++blob_writes;if(fail_write)return -1;
 memcpy(stored,data,size);blob_exists=true;return (int)size;
}
int main(void){
 TimerUiPreferences prefs;assert(timer_ui_preferences_load(&prefs)&&prefs.revision==0&&!timer_ui_preferences_dark(&prefs));
 legacy_exists=true;assert(timer_ui_preferences_load(&prefs)&&timer_ui_preferences_dark(&prefs)&&prefs.text_size==0);
 prefs=(TimerUiPreferences){0x12345678,3,0,2};
 uint8_t bytes[TIMER_UI_PREFERENCES_BYTES],status,flags;
 timer_ui_preferences_encode(&prefs,0,0,bytes);
 assert(bytes[0]==1&&bytes[1]==3&&bytes[4]==0x78&&bytes[7]==0x12&&bytes[8]==3&&bytes[12]==0&&bytes[16]==2);
 TimerUiPreferences decoded;assert(timer_ui_preferences_decode(bytes,sizeof(bytes),&decoded,&status,&flags)&&!status&&!flags);
 assert(timer_ui_preferences_save(&prefs));unsigned writes=blob_writes;
 assert(timer_ui_preferences_save(&prefs)&&writes==blob_writes);
 legacy=1;assert(timer_ui_preferences_load(&decoded)&&decoded.appearance==0&&timer_ui_preferences_dark(&decoded));
 prefs.text_size=1;fail_write=true;assert(!timer_ui_preferences_save(&prefs));fail_write=false;
 assert(timer_ui_preferences_load(&decoded)&&decoded.text_size==3);
 wrong_readback=true;assert(!timer_ui_preferences_save(&prefs));wrong_readback=false;
 fail_read=true;assert(!timer_ui_preferences_save(&prefs));fail_read=false;
 assert(timer_ui_preferences_save(&prefs)&&timer_ui_preferences_load(&decoded)&&decoded.text_size==1);
 for(unsigned i=0;i<4;++i){uint8_t changed[sizeof(bytes)];memcpy(changed,bytes,sizeof(bytes));changed[i]=99;assert(!timer_ui_preferences_decode(changed,sizeof(changed),&decoded,NULL,NULL));}
 assert(!timer_ui_preferences_decode(bytes,sizeof(bytes)-1,&decoded,NULL,NULL));
 bytes[8]=4;assert(!timer_ui_preferences_decode(bytes,sizeof(bytes),&decoded,NULL,NULL));
 prefs.appearance=1;prefs.last_explicit=2;assert(!timer_ui_preferences_save(&prefs));
 prefs.appearance=0;prefs.revision=0;assert(!timer_ui_preferences_save(&prefs));
 stored[0]=99;assert(!timer_ui_preferences_load(&decoded));
 assert(decoded.revision==0&&decoded.text_size==0);
 puts("UI prefs: exact wire, legacy migration, retained Follow fallback, verified commit/reopen and isolated failure paths passed");
 return 0;
}
