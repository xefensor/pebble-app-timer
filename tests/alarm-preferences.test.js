'use strict';
const test=require('node:test'),assert=require('node:assert/strict');
const prefs=require('../src/pkjs/alarm-preferences');
const light={version:1,sound:true};
const large={version:1,sound:false};
function setup(map=new Map(),transport=true){
 const sent=[],timers=new Map();let id=0,failWrite=false;
 const storage={getItem:key=>map.get(key)??null,setItem(key,value){if(failWrite)throw Error('disk');map.set(key,value);}};
 const controller=prefs.create({storage,send:transport?(message,fail,ok)=>sent.push({message,fail,ok}):undefined,later(fn){timers.set(++id,fn);return id;},cancel(id){timers.delete(id);}});
 return {controller,map,sent,timers,fail(value){failWrite=value;},request(type,settings){let result;controller.handle({type,settings},value=>result=value);return result;},ack(index=sent.length-1,edit){const bytes=sent[index].message.TIMER_ALARM_PREFERENCES.slice();if(edit)edit(bytes);return controller.receive(bytes);},tick(){const entry=timers.entries().next().value;assert.ok(entry);timers.delete(entry[0]);entry[1]();}};
}
test('phone save survives reopen; only exact persisted watch receipt marks applied',()=>{
 const offline=setup(new Map(),false),saved=offline.request('saveAlarmPreferences',large);
 assert.equal(saved.saved,true);assert.equal(saved.delivery.pending,true);assert.equal(offline.sent.length,0);
 const live=setup(offline.map);assert.deepEqual(live.request('getAlarmPreferences').settings,large);
 live.controller.reconcile(true);assert.equal(live.sent.length,1);
 live.sent[0].ok();assert.equal(live.request('getAlarmPreferences').delivery.applied,false);
 live.ack(0,b=>b[4]++);assert.equal(live.request('getAlarmPreferences').delivery.applied,false);
 live.ack(0,b=>b[8]=1);assert.equal(live.request('getAlarmPreferences').delivery.applied,false);
 assert.equal(live.ack(),true);assert.equal(live.timers.size,0);
 assert.equal(setup(live.map,false).request('getAlarmPreferences').delivery.applied,true);
});
test('new choices supersede old acknowledgments and retry at most three times',()=>{
 const s=setup();s.request('saveAlarmPreferences',light);const old=s.sent[0].message.TIMER_ALARM_PREFERENCES;
 s.request('saveAlarmPreferences',large);s.controller.receive(old);
 assert.equal(s.request('getAlarmPreferences').delivery.applied,false);
 s.sent[1].fail();s.tick();s.sent[2].ok();s.tick();s.tick();s.sent[3].fail();
 assert.equal(s.sent.length,4);assert.equal(s.timers.size,0);
 assert.equal(s.request('getAlarmPreferences').delivery.pending,true);
 s.request('retryAlarmPreferences');assert.equal(s.sent.length,5);
});
test('fresh watch state invalidates an earlier receipt even if pending metadata cannot be written',()=>{
 const s=setup();s.request('saveAlarmPreferences',large);s.ack();s.fail(true);
 const hello=prefs.encode(light,0);hello[3]=1;s.controller.receive(hello);
 assert.equal(s.request('getAlarmPreferences').delivery.applied,false);
 assert.equal(s.sent.length,1,'metadata failure must not send a reusable revision');
 assert.equal(s.ack(),false,'an old receipt cannot undo known watch mismatch');
 s.fail(false);s.request('retryAlarmPreferences');s.ack();
 assert.equal(s.request('getAlarmPreferences').delivery.applied,true);
});
test('save failure cannot become applied; retry and reopening retain desired choices',()=>{
 const s=setup();s.request('saveAlarmPreferences',large);s.ack(0,b=>b[2]=1);
 assert.equal(s.request('getAlarmPreferences').delivery.error,'watch_save_unconfirmed');
 assert.equal(setup(s.map,false).request('getAlarmPreferences').delivery.pending,true);
 s.request('retryAlarmPreferences');s.ack();assert.equal(s.request('getAlarmPreferences').delivery.applied,true);
});
test('corrupt phone store requires explicit repair; invalid choices and unconfirmed writes fail closed',()=>{
 const s=setup(new Map([[prefs.key,'broken']]));
 assert.equal(s.request('getAlarmPreferences').needsRepair,true);assert.equal(s.controller.reconcile(true),false);
 assert.equal(s.map.get(prefs.key),'broken');
 assert.equal(s.request('saveAlarmPreferences',{...large,extra:true}).ok,false);
 s.fail(true);assert.equal(s.request('saveAlarmPreferences',large).ok,false);assert.equal(s.sent.length,0);
 s.fail(false);assert.equal(s.request('saveAlarmPreferences',large).saved,true);
});

test('upgrades default to sound, speaker capability is reported separately and storage is independent',()=>{
 const s=setup(new Map([['timer-ui-preferences-v1','untouched'],['timer-owner','untouched']]));
 assert.deepEqual(s.request('getAlarmPreferences').settings,light);
 assert.equal(s.request('getAlarmPreferences').supported,null);
 s.request('saveAlarmPreferences',large);s.ack(0,b=>b[3]=2);
 assert.equal(s.request('getAlarmPreferences').supported,true);
 assert.equal(s.request('getAlarmPreferences').delivery.applied,true);
 assert.equal(s.map.get('timer-ui-preferences-v1'),'untouched');assert.equal(s.map.get('timer-owner'),'untouched');
 const r=setup(s.map,false);assert.equal(r.request('getAlarmPreferences').settings.sound,false);
});
test('wire rejects invalid sound, length and reserved flags',()=>{
 const b=prefs.encode(light,0xffffffff);assert.equal(b.length,12);assert.equal(prefs.decode(b).revision,0xffffffff);
 for(const edit of [b=>b.pop(),b=>b[0]=2,b=>b[1]=2,b=>b[2]=2,b=>b[3]=4,b=>b[8]=2,b=>b[9]=256]){const bad=b.slice();edit(bad);assert.equal(prefs.decode(bad),null);}
});
