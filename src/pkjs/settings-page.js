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
  var field=document.getElementById('theme'),size=document.getElementById('text-size'),deliveryRetry=document.getElementById('delivery-retry'),form=document.getElementById('settings');
  var save=document.getElementById('save'),discard=document.getElementById('discard'),retry=document.getElementById('retry'),status=document.getElementById('status'),actions=document.getElementById('actions');
  var loaded=false,repairing=false,uncertain=false,saved='',phase='loading',operation=0,timer=null,bridgeReady=false,pkjsReady=false,layoutPending=false,lastHeight='',lastFlow=null,poll=null,lastDelivery=null,lastExplicit='light';
  function normalize(value){return value&&value.version===2&&Object.keys(value).length===4&&['follow','small','medium','large'].indexOf(value.textSize)>=0&&['follow','light','dark'].indexOf(value.appearance)>=0&&['light','dark'].indexOf(value.lastExplicit)>=0&&(value.appearance==='follow'||value.appearance===value.lastExplicit)?{version:2,textSize:value.textSize,appearance:value.appearance,lastExplicit:value.lastExplicit}:null;}
  function value(){return normalize({version:2,textSize:size.value,appearance:field.value,lastExplicit:lastExplicit});}
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
  function controls(){
    [field,size].forEach(function(select){select.options[0].textContent=t(phase==='load-error'?'notLoaded':'loadingChoice');select.disabled=!loaded||phase==='saving';});
    save.disabled=!loaded||phase==='saving'||!dirty()||!value();save.textContent=t(phase==='saving'?'saving':'save');
    discard.hidden=!loaded;discard.disabled=phase==='saving'||!uncertain&&!edited();discard.textContent=t(uncertain?'reload':'discard');retry.hidden=phase!=='load-error';
    deliveryRetry.hidden=!loaded||!lastDelivery||lastDelivery.applied||dirty();deliveryRetry.disabled=phase!=='ready';
  }
  function stopPoll(){if(poll!==null)clearTimeout(poll);poll=null;}
  function showDelivery(){
    controls();say(lastDelivery&&lastDelivery.applied?'applied':lastDelivery&&lastDelivery.error?'watchFailed':'pending',!!(lastDelivery&&lastDelivery.error));
    stopPoll();if(lastDelivery&&!dirty())poll=setTimeout(function(){poll=null;checkDelivery(false);},lastDelivery.applied?5000:2000);
  }
  function checkDelivery(force){
    if(!loaded||phase!=='ready'||dirty())return;
    settingsTarget(function(target){
      if(!loaded||phase!=='ready'||dirty())return;
      request({type:force?'retryUiPreferences':'getUiPreferences'},function(response){
        if(!dirty()&&response&&response.ok&&JSON.stringify(normalize(response.settings))===saved){lastDelivery=response.delivery;showDelivery();}else if(!dirty())showDelivery();
      },function(){if(!dirty())showDelivery();},target);
    },function(){if(!dirty())showDelivery();});
  }
  function apply(chosen,repair,response){
    field.value=chosen.appearance;size.value=chosen.textSize;lastExplicit=chosen.lastExplicit;loaded=true;repairing=!!repair||!!(response&&response.saved===false);uncertain=false;saved=JSON.stringify(chosen);phase='ready';lastDelivery=response&&response.delivery||null;controls();
    if(repair)say('repair',true);else if(repairing)say('dirty');else showDelivery();
  }
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
    var readback=uncertain;stopPoll();loaded=false;phase='loading';field.value='';size.value='';controls();say('loading');
    if(!bridgeReady){stop();timer=setTimeout(loadFailed,6000);return;}
    settingsTarget(function(target){request({type:'getUiPreferences'},function(response){var chosen=response&&response.ok===true&&normalize(response.settings);if(!chosen){loadFailed();return;}apply(chosen,response.needsRepair,response);if(readback&&!repairing&&lastDelivery&&lastDelivery.applied)say('applied');},loadFailed,target);},loadFailed);
  }
  field.onchange=size.onchange=function(){if(field.value!=='follow')lastExplicit=field.value;stopPoll();controls();if(!dirty())showDelivery();else say(uncertain?'uncertain':edited()?'dirty':'repair',uncertain||repairing);};
  deliveryRetry.onclick=function(){stopPoll();checkDelivery(true);};
  retry.onclick=load;
  discard.onclick=function(){if(loaded&&uncertain)load();else if(loaded)apply(JSON.parse(saved),repairing,{delivery:lastDelivery});};
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
    stopPoll();phase='saving';controls();say('saving');var attempts=0;
    function confirmed(chosen,response){apply(chosen,false,response);}
    function write(target){
      if(++attempts>3){saveFailed();return;}
      request({type:'saveUiPreferences',settings:selected},function(response){
        var accepted=response&&response.ok===true&&normalize(response.settings);
        if(!accepted||JSON.stringify(accepted)!==JSON.stringify(selected)){
          if(response&&response.error==='invalid_settings'){phase='ready';controls();say('failed',true);}else saveFailed();return;
        }
        // Android can restore a separate PKJS storage snapshot. The live writer
        // must own the final save before its teardown replaces native storage.
        if(target==='plugin')settingsTarget(function(current){if(current==='pkjs')write(current);else confirmed(accepted,response);},saveFailed);
        else confirmed(accepted,response);
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
  '</style></head><body><main><header><h1>Timer</h1><p data-t="intro">Text size and appearance for Timer on your watch.</p></header><form id="settings"><section class="card"><h2 data-t="display">Watch display</h2><label for="text-size" data-t="textSize">Text size</label><select id="text-size" disabled><option value="" disabled hidden selected data-t="loadingChoice">Loading saved choice…</option><option value="follow" data-t="follow">Follow Pebble</option><option value="small" data-t="small">Small</option><option value="medium" data-t="medium">Medium</option><option value="large" data-t="large">Large</option></select><label for="theme" data-t="theme">Watch appearance</label><select id="theme" aria-describedby="theme-help" disabled><option value="" disabled hidden selected data-t="loadingChoice">Loading saved choice…</option><option value="follow" data-t="follow">Follow Pebble</option><option value="light" data-t="light">Light</option><option value="dark" data-t="dark">Dark</option></select><p id="theme-help" class="hint" data-t="appearanceUnavailable">Follow Pebble appearance is unavailable on this build. It uses your last Light or Dark choice.</p></section></form><details class="card"><summary data-t="help">Setup &amp; help</summary><p data-t="helpText">Create, pause, reset and manage timers on the watch. These settings change appearance only.</p><p data-t="confirmedDelivery">Settings saved on your phone stay pending until Timer is open, connected and confirms them. Timers and alarms are unchanged.</p></details></main><footer id="actions"><div class="footer-inner"><p id="status" class="status" role="status" aria-live="polite" data-t="loading">Loading settings…</p><div class="actions"><button id="delivery-retry" type="button" hidden data-t="retryWatch">Retry watch delivery</button><button id="retry" type="button" hidden data-t="retry">Retry loading</button><button id="discard" type="button" disabled data-t="discard">Discard</button><button id="save" type="submit" form="settings" disabled data-t="save">Save</button></div></div></footer><script>(',behavior.toString(),')(',JSON.stringify(require('../common/translations.json')).replace(/</g,'\\u003c'),');</script></body></html>'
].join('\n');}
module.exports={config:render};
