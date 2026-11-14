'use strict';
var KEY='timer-alarm-preferences-v1';
function normalize(v){return v&&v.version===1&&Object.keys(v).sort().join(',')==='sound,version'&&typeof v.sound==='boolean'?{version:1,sound:v.sound}:null;}
function defaults(){return {version:1,sound:true};}
function equal(a,b){return JSON.stringify(a)===JSON.stringify(b);}
function put(b,at,n){for(var i=0;i<4;i++)b[at+i]=(n>>>(8*i))&255;}
function u32(b,at){return (b[at]|b[at+1]<<8|b[at+2]<<16|b[at+3]<<24)>>>0;}
function encode(v,revision){var b=[1,1,0,0];put(b,4,revision);put(b,8,v.sound?1:0);return b;}
function decode(input){
 if(!input||input.length!==12)return null;var b=Array.prototype.slice.call(input);
 if(!b.every(function(n){return Number.isInteger(n)&&n>=0&&n<=255;})||b[0]!==1||b[1]!==1||b[2]>1||(b[3]&~3)||u32(b,8)>1)return null;
 return {settings:{version:1,sound:!!u32(b,8)},revision:u32(b,4),status:b[2],hello:!!(b[3]&1),supported:!!(b[3]&2)};
}
function matches(frame,record){var r=decode(frame);return !!r&&!r.status&&r.revision===record.revision&&equal(r.settings,record.settings);}
function next(n){return ((n+1)>>>0)||1;}
function isRequest(m){return m&&['getAlarmPreferences','saveAlarmPreferences','retryAlarmPreferences'].indexOf(m.type)>=0;}
function create(options){
  var storage=options.storage,send=options.send,later=options.later||function(fn,ms){return setTimeout(fn,ms);},cancel=options.cancel||function(id){clearTimeout(id);};
  var generation=0,timer=null,attempts=0,watchMismatch=false,invalidatedRevision=null,revisionPending=false;
  function stop(){if(timer!==null)cancel(timer);timer=null;}
  function read(){
    var raw=storage.getItem(KEY),record,stored=raw!==null&&raw!==undefined;
    if(!stored)return {healthy:true,stored:false,record:{version:1,settings:defaults(),revision:1,applied:null,error:null,supported:null}};
    try{record=JSON.parse(raw);}catch(error){record=null;}
    if(!record||record.version!==1||!normalize(record.settings)||!Number.isInteger(record.revision)||record.revision<1||record.revision>0xffffffff||
        record.supported!==null&&typeof record.supported!=='boolean'||record.applied!==null&&!decode(record.applied)||
        record.error!==null&&typeof record.error!=='string')
      return {healthy:false,stored:false,record:{version:1,settings:defaults(),revision:1,applied:null,error:null,supported:null}};
    record.settings=normalize(record.settings);
    return {healthy:true,stored:stored,record:record};
  }
  function commit(record){
    try{var encoded=JSON.stringify(record);storage.setItem(KEY,encoded);return storage.getItem(KEY)===encoded;}catch(error){return false;}
  }
  function response(state){
    var record=state.record,applied=state.healthy&&!watchMismatch&&matches(record.applied,record);
    return {ok:true,settings:record.settings,needsRepair:!state.healthy,saved:state.stored,
      supported:record.supported,delivery:{applied:applied,pending:!applied,error:record.error}};
  }
  function retry(g){
    if(g!==generation||attempts>=3||!send)return;
    stop();timer=later(function(){timer=null;if(g===generation)deliver(g);},500*attempts);
  }
  function deliver(g){
    if(g!==generation||attempts>=3||!send)return;
    var state;try{state=read();}catch(error){return;}
    if(!state.healthy||!state.stored||revisionPending||!watchMismatch&&matches(state.record.applied,state.record))return;
    ++attempts;
    var message={TIMER_ALARM_PREFERENCES:encode(state.record.settings,state.record.revision)};
    send(message,function(){retry(g);},function(){
      if(g!==generation)return;
      var latest;try{latest=read();}catch(error){return;}
      if(latest.healthy&&!watchMismatch&&matches(latest.record.applied,latest.record))return;
      stop();timer=later(function(){timer=null;retry(g);},1500);
    });
  }
  function reconcile(force){
    if(force){watchMismatch=true;revisionPending=true;++generation;stop();}
    var state;try{state=read();}catch(error){return false;}
    if(!state.healthy)return false;
    if(force||revisionPending||state.record.revision===invalidatedRevision){
      watchMismatch=true;invalidatedRevision=state.record.revision;
      state.record.revision=next(state.record.revision);state.record.applied=null;state.record.error=null;
      if(!commit(state.record)){revisionPending=true;return false;}
      revisionPending=false;
    }else if(!state.stored&&!commit(state.record))return false;
    ++generation;attempts=0;stop();deliver(generation);return true;
  }
  function save(value){
    var selected=normalize(value);if(!selected)return {ok:false,error:'invalid_settings'};
    var state;try{state=read();}catch(error){return {ok:false,error:'save_unconfirmed'};}
    var record=state.record;
    if(!state.healthy||!equal(record.settings,selected)){
      record={version:1,settings:selected,revision:state.healthy?next(record.revision):1,
        applied:null,error:null,supported:record.supported};
    }
    if(!commit(record))return {ok:false,error:'save_unconfirmed'};
    reconcile(false);
    try{return response(read());}catch(error){return {ok:false,error:'save_unconfirmed'};}
  }
  function receive(input){
    var received=decode(input),state;if(!received)return false;
    try{state=read();}catch(error){return false;}
    if(!state.healthy)return false;
    state.record.supported=received.supported;
    if(!revisionPending&&state.record.revision!==invalidatedRevision&&received.revision===state.record.revision&&equal(received.settings,state.record.settings)){
      if(received.status===0){
        state.record.applied=encode(received.settings,received.revision);state.record.error=null;
        if(!commit(state.record)){watchMismatch=true;return false;}
        watchMismatch=false;invalidatedRevision=null;
        stop();return true;
      }
      watchMismatch=true;
      state.record.applied=null;state.record.error='watch_save_unconfirmed';commit(state.record);return false;
    }
    if(received.hello){
      watchMismatch=true;revisionPending=true;invalidatedRevision=state.record.revision;++generation;stop();
      // A failed metadata write cannot erase the observed native mismatch.
      commit(state.record);reconcile(false);
    }
    return false;
  }
  function handle(message,respond){
    try{
      if(typeof message==='string')message=JSON.parse(message);
      if(message.type==='getAlarmPreferences')respond(response(read()));
      else if(message.type==='saveAlarmPreferences')respond(save(message.settings));
      else if(message.type==='retryAlarmPreferences'){
        if(!reconcile(true)){respond({ok:false,error:'save_unconfirmed'});return;}
        respond(response(read()));
      }else respond({ok:false,error:'unknown_request'});
    }catch(error){respond({ok:false,error:'save_unconfirmed'});}
  }
  return {handle:handle,receive:receive,reconcile:reconcile};
}
module.exports={key:KEY,normalize:normalize,encode:encode,decode:decode,isRequest:isRequest,create:create};
