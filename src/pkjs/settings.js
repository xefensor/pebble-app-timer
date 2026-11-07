'use strict';

var KEY = 'timer-appearance-settings-v1';
function defaults() { return {version: 1, theme: 'light'}; }
function normalize(value) {
  if (!value || typeof value !== 'object' || Array.isArray(value) || value.version !== 1 ||
      Object.keys(value).some(function(key) { return ['version', 'theme'].indexOf(key) === -1; }) ||
      ['light', 'dark'].indexOf(value.theme) === -1) return null;
  return {version: 1, theme: value.theme};
}
function load(storage) {
  try {
    var encoded=storage.getItem(KEY);
    return encoded===null||encoded===undefined ? defaults() : normalize(JSON.parse(encoded));
  } catch (error) { return null; }
}
function save(storage,value) {
  var settings=normalize(value);
  if(!settings)return false;
  try {
    var encoded=JSON.stringify(settings);
    storage.setItem(KEY,encoded);
    return storage.getItem(KEY)===encoded;
  } catch(error) {return false;}
}
function handle(storage,message,respond,saved) {
  try {
    if(typeof message==='string')message=JSON.parse(message);
    if(!message||message.type==='getSettings'||message.type==='load_settings') {
      var encoded=storage.getItem(KEY),current;
      if(encoded===null||encoded===undefined)current=defaults();
      else { try { current=normalize(JSON.parse(encoded)); } catch(error) { current=null; } }
      respond({ok:true,settings:current||defaults(),needsRepair:!current});
    } else if(message.type==='saveSettings'||message.type==='save_settings') {
      var chosen=normalize(message.settings);
      if(!chosen){respond({ok:false,error:'invalid_settings'});return;}
      if(!save(storage,chosen)){respond({ok:false,error:'save_unconfirmed'});return;}
      if(saved)saved(chosen);
      respond({ok:true,settings:chosen});
    } else respond({ok:false,error:'unknown_request'});
  } catch(error){respond({ok:false,error:'save_unconfirmed'});}
}
function message(value) {
  var settings=normalize(value);
  return settings?{TIMER_APPEARANCE:settings.theme==='dark'?1:0,TIMER_APPEARANCE_VERSION:1}:null;
}
module.exports={key:KEY,defaults:defaults,normalize:normalize,load:load,save:save,handle:handle,message:message};
