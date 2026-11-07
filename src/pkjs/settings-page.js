'use strict';

var settingsStyle = require('pebble-xoce-ui/settings-style');

function behavior(initial) {
  'use strict';
  var code=String(typeof navigator!=='undefined'&&navigator.language||'en').toLowerCase().split(/[_.@-]/)[0];
  if(code==='nb'||code==='nn')code='no';
  if(initial.languages.indexOf(code)<0)code='en';
  var words=initial.words[code];
  function t(key){var phrase=initial.phone[key];return words[phrase]||phrase;}
  document.documentElement.lang=code;
  Array.prototype.forEach.call(document.querySelectorAll('[data-t]'),function(el){el.textContent=t(el.getAttribute('data-t'));});
  var field=document.getElementById('theme'),form=document.getElementById('settings');
  var save=document.getElementById('save'),discard=document.getElementById('discard'),retry=document.getElementById('retry'),status=document.getElementById('status'),actions=document.getElementById('actions');
  var loaded=false,repairing=false,uncertain=false,saved='',phase='loading',operation=0,timer=null,bridgeReady=false,pkjsReady=false,layoutPending=false,lastHeight='',lastFlow=null;
  function normalize(value){return value&&value.version===1&&Object.keys(value).length===2&&['light','dark'].indexOf(value.theme)!==-1?{version:1,theme:value.theme}:null;}
  function value(){return normalize({version:1,theme:field.value});}
  function edited(){return loaded&&JSON.stringify(value())!==saved;}
  function dirty(){return loaded&&(repairing||uncertain||edited());}
  function layout(){
    if(layoutPending)return;layoutPending=true;
    (window.requestAnimationFrame||function(fn){return setTimeout(fn,0);})(function(){
      layoutPending=false;
      var viewport=window.visualViewport&&window.visualViewport.height||window.innerHeight,height=actions.getBoundingClientRect().height;
      var flow=viewport<560||height>viewport*.28;
      if(flow!==lastFlow){actions.setAttribute('data-flow',String(flow));lastFlow=flow;}
      var size=flow?'0px':Math.ceil(height)+'px';
      if(size!==lastHeight){document.documentElement.style.setProperty('--actions-height',size);lastHeight=size;}
    });
  }
  function say(key,error){status.textContent=t(key);status.className=error?'status error':'status';layout();}
  function controls(){field.options[0].textContent=t(phase==='load-error'?'notLoaded':'loadingChoice');field.disabled=!loaded||phase==='saving';save.disabled=!loaded||phase==='saving'||!dirty()||!value();save.textContent=t(phase==='saving'?'saving':'save');discard.hidden=!loaded;discard.disabled=phase==='saving'||!uncertain&&!edited();discard.textContent=t(uncertain?'reload':'discard');retry.hidden=phase!=='load-error';}
  function apply(chosen,repair){field.value=chosen.theme;loaded=true;repairing=!!repair;uncertain=false;saved=JSON.stringify(chosen);phase='ready';controls();say(repairing?'repair':'none',repairing);}
  function stop(){if(timer!==null)clearTimeout(timer);timer=null;}
  function request(payload,complete,failed,target){
    var current=++operation;stop();
    timer=setTimeout(function(){if(current===operation){operation++;timer=null;failed();}},6000);
    function finish(callback,response){if(current!==operation)return;operation++;stop();callback(response);}
    try{
      var pending=Pebble.sendMessage(target||'plugin',payload);
      if(!pending||typeof pending.then!=='function'){finish(failed);return;}
      pending.then(function(response){finish(complete,response);},function(){finish(failed);});
    }catch(error){finish(failed);}
  }
  function loadFailed(){loaded=false;phase='load-error';controls();say('loadFailed',true);}
  function load(){
    var readback=uncertain;loaded=false;phase='loading';field.value='';controls();say('loading');
    if(!bridgeReady){stop();timer=setTimeout(loadFailed,6000);return;}
    request({type:'getSettings'},function(response){var chosen=response&&response.ok===true&&normalize(response.settings);if(!chosen){loadFailed();return;}apply(chosen,response.needsRepair);if(readback&&!repairing)say('reloaded');},loadFailed);
  }
  field.onchange=function(){controls();say(uncertain?'uncertain':edited()?'dirty':repairing?'repair':'none',uncertain||repairing);};
  retry.onclick=load;
  discard.onclick=function(){if(loaded&&uncertain)load();else if(loaded)apply(JSON.parse(saved),repairing);};
  function saveFailed(){uncertain=true;phase='ready';controls();say('uncertain',true);}
  function settingsTarget(complete,failed){
    if(typeof Pebble.targets!=='function'){complete(pkjsReady?'pkjs':'plugin');return;}
    var settled=false,timeout=setTimeout(function(){finish(failed);},6000);
    function finish(callback,target){if(settled)return;settled=true;clearTimeout(timeout);callback(target);}
    try{Promise.resolve(Pebble.targets()).then(function(targets){if(!Array.isArray(targets)){finish(failed);return;}finish(complete,targets.some(function(target){return target&&target.id==='pkjs';})?'pkjs':'plugin');},function(){finish(failed);});}catch(error){finish(failed);}
  }
  form.onsubmit=function(event){
    if(event)event.preventDefault();if(save.disabled)return;
    var selected=value();if(!selected)return;
    phase='saving';controls();say('saving');var attempts=0;
    function confirmed(chosen){apply(chosen,false);say('saved');}
    function write(target){
      if(++attempts>3){saveFailed();return;}
      request({type:'saveSettings',settings:selected},function(response){
        var accepted=response&&response.ok===true&&normalize(response.settings);
        if(!accepted||JSON.stringify(accepted)!==JSON.stringify(selected)){
          if(response&&response.error==='invalid_settings'){phase='ready';controls();say('failed',true);}else saveFailed();return;
        }
        // Android can restore a separate PKJS storage snapshot. The live writer
        // must own the final save before its teardown replaces native storage.
        if(target==='plugin')settingsTarget(function(current){if(current==='pkjs')write(current);else confirmed(accepted);},saveFailed);
        else confirmed(accepted);
      },function(){if(target!=='pkjs'){saveFailed();return;}settingsTarget(function(current){if(current==='plugin')write(current);else saveFailed();},saveFailed);},target);
    }
    settingsTarget(write,saveFailed);
  };
  save.onclick=function(event){if(event)event.preventDefault();form.onsubmit();};
  window.addEventListener('resize',layout);if(window.visualViewport)window.visualViewport.addEventListener('resize',layout);if(typeof ResizeObserver!=='undefined')new ResizeObserver(layout).observe(actions);
  if(typeof Pebble!=='undefined'&&typeof Pebble.addEventListener==='function')Pebble.addEventListener('ready',function(event){if(event&&event.target==='pkjs'){pkjsReady=true;return;}if(event&&event.target&&event.target!=='plugin')return;if(bridgeReady)return;bridgeReady=true;load();});
  load();
}
function render(){return [
  '<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>Timer</title><style id="xoce-settings-style">',
  settingsStyle,
  '</style></head><body><main><header><h1>Timer</h1><p data-t="intro">Appearance for Timer on your watch.</p></header><form id="settings"><section class="card"><h2 data-t="appearance">Appearance</h2><label for="theme" data-t="theme">Watch appearance</label><select id="theme" aria-describedby="theme-help" disabled><option value="" disabled hidden selected data-t="loadingChoice">Loading saved choice…</option><option value="light" data-t="light">Light</option><option value="dark" data-t="dark">Dark</option></select><p id="theme-help" class="hint" data-t="hint">The watch appearance is independent of this phone page.</p></section></form><details class="card"><summary data-t="help">Setup &amp; help</summary><p data-t="helpText">Create, pause, reset and manage timers on the watch. These settings change appearance only.</p><p data-t="delivery">Saved appearance applies when Timer is open and connected. Timer keeps its last confirmed appearance on startup.</p></details></main><footer id="actions"><div class="footer-inner"><p id="status" class="status" role="status" aria-live="polite" data-t="loading">Loading settings…</p><div class="actions"><button id="retry" type="button" hidden data-t="retry">Retry loading</button><button id="discard" type="button" disabled data-t="discard">Discard</button><button id="save" type="submit" form="settings" disabled data-t="save">Save</button></div></div></footer><script>(',behavior.toString(),')(',JSON.stringify(require('../common/translations.json')).replace(/</g,'\\u003c'),');</script></body></html>'
].join('\n');}
module.exports={config:render};
