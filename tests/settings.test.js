'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),fs=require('node:fs'),vm=require('node:vm');
const {createRequire}=require('node:module');
const settings=require('../src/pkjs/settings');
const dark={version:1,theme:'dark'},light={version:1,theme:'light'};
function stores(initial=light){
 const native=new Map([[settings.key,JSON.stringify(initial)]]),dom=new Map(native);
 return {native,dom,
  plugin:{getItem:k=>native.get(k)??null,setItem:(k,v)=>native.set(k,v)},
  pkjs:{getItem:k=>dom.get(k)??null,setItem(k,v){dom.set(k,v);native.set(k,v);}},
  teardown(){native.clear();for(const [k,v]of dom)native.set(k,v);}};
}
function context(kind,storage){
 const file=require.resolve('../src/'+(kind==='plugin'?'plugin':'pkjs')+'/index'),localRequire=createRequire(file);
 const events={},messages=[],timers=new Map();let config,sequence=0;
 const Pebble={addEventListener(name,fn){events[name]=fn;},registerSourceHandler(){},registerConfigHandler(fn){config=fn;},sendAppMessage(message,ok){messages.push(message);if(ok)ok();}};
 vm.runInNewContext(fs.readFileSync(file,'utf8'),{Pebble,localStorage:storage,require:localRequire,setTimeout(fn){timers.set(++sequence,fn);return sequence;},clearTimeout(id){timers.delete(id);}});
 return {events,messages,receive(message){let value;const respond=r=>{value=JSON.parse(JSON.stringify(r));};if(kind==='plugin')config(message,respond);else events.configmessage({data:message,respond});return value;}};
}
test('appearance config preserves a live Android writer through close/restart without touching timer snapshot keys',()=>{
 const data=stores(),plugin=context('plugin',data.plugin),runtime=context('pkjs',data.pkjs);
 assert.deepEqual(runtime.receive({type:'saveSettings',settings:dark}),{ok:true,settings:dark});
 assert.deepEqual(settings.load(data.pkjs),dark);assert.deepEqual(plugin.receive({type:'getSettings'}).settings,dark);
 assert.deepEqual(runtime.messages,[{TIMER_APPEARANCE:1,TIMER_APPEARANCE_VERSION:1}]);
 assert.equal([...data.native.keys()].length,1);
 data.teardown();assert.deepEqual(context('plugin',data.plugin).receive({type:'getSettings'}).settings,dark);
 const reopened=context('pkjs',data.pkjs);reopened.events.ready();assert.equal(reopened.messages[0].TIMER_APPEARANCE,1);assert.equal(reopened.messages[1].ACTIVITY_REQUEST,1);
});
test('closed settings plugin changes only isolated appearance configuration',()=>{
 const data=stores(),plugin=context('plugin',data.plugin);
 assert.deepEqual(plugin.receive({type:'save_settings',settings:dark}),{ok:true,settings:dark});
 assert.deepEqual(plugin.receive(' {"type":"getSettings"} ').settings,dark);
 assert.equal(plugin.messages.length,0);
 for(const chosen of [{},null,{version:1,theme:'other'},{...dark,extra:true}])assert.equal(plugin.receive({type:'saveSettings',settings:chosen}).ok,false);
 assert.deepEqual(settings.load(data.plugin),dark);
});
test('load exception stays unavailable, corrupt saved value requires explicit repair, and unconfirmed writes never report Saved',()=>{
 const broken=context('plugin',{getItem(){throw Error('unavailable');},setItem(){throw Error('unavailable');}});
 assert.equal(broken.receive({type:'getSettings'}).ok,false);
 assert.equal(broken.receive({type:'saveSettings',settings:dark}).ok,false);
 const data=stores();data.native.set(settings.key,'broken');const corrupt=context('plugin',data.plugin);
 assert.deepEqual(corrupt.receive({type:'getSettings'}),{ok:true,settings:light,needsRepair:true});
 assert.equal(data.native.get(settings.key),'broken');
 const unchanged=context('plugin',{getItem(){return JSON.stringify(light);},setItem(){}});
 assert.deepEqual(unchanged.receive({type:'saveSettings',settings:dark}),{ok:false,error:'save_unconfirmed'});
});
