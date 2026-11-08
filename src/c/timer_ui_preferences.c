#include <pebble.h>
#include <string.h>
#include "timer_appearance.h"
#include "timer_ui_preferences.h"

#define UI_PREFERENCES_KEY 2106

static uint32_t prv_u32(const uint8_t *at) {
  return (uint32_t)at[0]|(uint32_t)at[1]<<8|(uint32_t)at[2]<<16|(uint32_t)at[3]<<24;
}
static void prv_put_u32(uint8_t *at,uint32_t value) {
  for(unsigned i=0;i<4;++i)at[i]=(uint8_t)(value>>(8*i));
}
TimerUiPreferences timer_ui_preferences_defaults(bool legacy_dark) {
  const uint8_t appearance=legacy_dark?TIMER_UI_DARK:TIMER_UI_LIGHT;
  return (TimerUiPreferences){0,0,appearance,appearance};
}
bool timer_ui_preferences_valid(const TimerUiPreferences *value) {
  return value&&value->text_size<=3&&value->appearance<=TIMER_UI_DARK&&
      value->last_explicit>=TIMER_UI_LIGHT&&value->last_explicit<=TIMER_UI_DARK&&
      (value->appearance==TIMER_UI_FOLLOW||value->appearance==value->last_explicit);
}
void timer_ui_preferences_encode(const TimerUiPreferences *value,uint8_t status,
    uint8_t flags,uint8_t bytes[TIMER_UI_PREFERENCES_BYTES]) {
  memset(bytes,0,TIMER_UI_PREFERENCES_BYTES);
  bytes[0]=1;bytes[1]=3;bytes[2]=status;bytes[3]=flags;
  prv_put_u32(bytes+4,value->revision);prv_put_u32(bytes+8,value->text_size);
  prv_put_u32(bytes+12,value->appearance);prv_put_u32(bytes+16,value->last_explicit);
}
bool timer_ui_preferences_decode(const uint8_t *bytes,size_t length,
    TimerUiPreferences *value,uint8_t *status,uint8_t *flags) {
  if(!bytes||!value||length!=TIMER_UI_PREFERENCES_BYTES||bytes[0]!=1||bytes[1]!=3||
      bytes[2]>TIMER_UI_SAVE_FAILED||(bytes[3]&~TIMER_UI_HELLO)||
      prv_u32(bytes+8)>3||prv_u32(bytes+12)>2||prv_u32(bytes+16)>2)return false;
  const TimerUiPreferences decoded={prv_u32(bytes+4),(uint8_t)prv_u32(bytes+8),
      (uint8_t)prv_u32(bytes+12),(uint8_t)prv_u32(bytes+16)};
  if(!timer_ui_preferences_valid(&decoded))return false;
  *value=decoded;
  if(status)*status=bytes[2];
  if(flags)*flags=bytes[3];
  return true;
}
bool timer_ui_preferences_dark(const TimerUiPreferences *value) {
  return (value->appearance==TIMER_UI_FOLLOW?value->last_explicit:value->appearance)==TIMER_UI_DARK;
}
bool timer_ui_preferences_load(TimerUiPreferences *value) {
  *value=timer_ui_preferences_defaults(timer_appearance_load());
  if(!persist_exists(UI_PREFERENCES_KEY))return true;
  uint8_t bytes[TIMER_UI_PREFERENCES_BYTES],status,flags;
  TimerUiPreferences stored;
  if(persist_get_size(UI_PREFERENCES_KEY)!=(int)sizeof(bytes)||
      persist_read_data(UI_PREFERENCES_KEY,bytes,sizeof(bytes))!=(int)sizeof(bytes)||
      !timer_ui_preferences_decode(bytes,sizeof(bytes),&stored,&status,&flags)||
      status||flags||!stored.revision)return false;
  *value=stored;return true;
}
bool timer_ui_preferences_save(const TimerUiPreferences *value) {
  if(!timer_ui_preferences_valid(value)||!value->revision)return false;
  uint8_t bytes[TIMER_UI_PREFERENCES_BYTES],readback[TIMER_UI_PREFERENCES_BYTES];
  timer_ui_preferences_encode(value,TIMER_UI_APPLIED,0,bytes);
  const bool same=persist_exists(UI_PREFERENCES_KEY)&&
      persist_read_data(UI_PREFERENCES_KEY,readback,sizeof(readback))==(int)sizeof(readback)&&
      !memcmp(readback,bytes,sizeof(bytes));
  if(!same&&persist_write_data(UI_PREFERENCES_KEY,bytes,sizeof(bytes))!=(int)sizeof(bytes))return false;
  if(persist_get_size(UI_PREFERENCES_KEY)!=(int)sizeof(bytes)||
      persist_read_data(UI_PREFERENCES_KEY,readback,sizeof(readback))!=(int)sizeof(readback)||
      memcmp(readback,bytes,sizeof(bytes)))return false;
  /* The verified blob is authoritative. Keep the older colour-only build's
   * fallback current without coupling this commit to any timer bank. */
  (void)timer_appearance_save(timer_ui_preferences_dark(value));
  return true;
}
