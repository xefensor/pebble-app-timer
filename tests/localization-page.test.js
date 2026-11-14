'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm');
const page=require('../src/pkjs/settings-page'),data=require('../src/common/translations.json');
function boot(locale,storage,options={}){
 const elements={},events={},timers=new Map(),html=page.config();let sequence=0;
 for(const id of ['theme','text-size','delivery-retry','settings','save','discard','retry','status','actions','alarm-sound','alarm-delivery-retry','alarm-settings','alarm-save','alarm-discard','alarm-retry','alarm-status','alarm-actions','alarm-controls','appearance-follow'])elements[id]={id,value:'',options:[{}],style:{setProperty(){}},setAttribute(){},getBoundingClientRect(){return {height:100};}};
 const root={style:{setProperty(){}}},targetRequests=[],requests=[];
 for(const element of Object.values(elements)){let value='';element.textWrites=0;Object.defineProperty(element,'textContent',{get(){return value;},set(next){value=next;element.textWrites++;}});}
 vm.runInNewContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],{
  navigator:{language:locale},document:{documentElement:root,getElementById:id=>elements[id],querySelectorAll:()=>[]},
  window:{innerHeight:800,requestAnimationFrame(){},addEventListener(){}},setTimeout(fn){timers.set(++sequence,fn);return sequence;},clearTimeout(id){timers.delete(id);},Promise,
  Pebble:{addEventListener:(name,fn)=>{(events[name]||(events[name]=[])).push(fn);},targets:()=>options.deferTargets?new Promise(resolve=>targetRequests.push(resolve)):Promise.resolve([{id:'plugin'}]),sendMessage:async(target,message)=>{requests.push(message.type);assert.equal(target,'plugin');if(/AlarmPreferences$/.test(message.type)){if(message.type==='saveAlarmPreferences')storage.alarm=JSON.parse(JSON.stringify(message.settings));return {ok:true,settings:storage.alarm||{version:1,sound:true},saved:true,supported:true,delivery:{applied:true,pending:false}};}if(message.type==='saveUiPreferences'){storage.saved=JSON.parse(JSON.stringify(message.settings));storage.applied=false;}return {ok:true,settings:storage.saved,saved:true,delivery:{applied:!!storage.applied,pending:!storage.applied}};}}
 });
 events.ready.forEach(fn=>fn({target:'plugin'}));
 return {elements,root,targetRequests,requests,tick(){const entry=timers.entries().next().value;assert.ok(entry);timers.delete(entry[0]);entry[1]();},flush:async()=>{for(let i=0;i<20;++i)await Promise.resolve();}};
}
for(const locale of [...data.languages,'nb-NO','nn_NO','DE_de','zz-ZZ'])test('Timer localized save/reopen and watch confirmation: '+locale,async()=>{
 const storage={saved:{version:2,textSize:'follow',appearance:'light',lastExplicit:'light'},applied:true},ui=boot(locale,storage);await ui.flush();
 const language=locale==='DE_de'?'de':locale.startsWith('n')&&locale!=='nl'&&locale!=='no'?'no':locale==='zz-ZZ'?'en':locale;
 assert.equal(ui.root.lang,language);assert.equal(ui.elements.save.disabled,true);
 ui.elements.theme.value='dark';ui.elements.theme.onchange();ui.elements['text-size'].value='large';ui.elements['text-size'].onchange();ui.elements.save.onclick();await ui.flush();
 assert.deepEqual(storage.saved,{version:2,textSize:'large',appearance:'dark',lastExplicit:'dark'});
 assert.equal(ui.elements.status.textContent,data.words[language][data.phone.pending]);
 assert.equal(ui.elements['delivery-retry'].hidden,false);
 const fresh=boot(locale,storage);await fresh.flush();assert.equal(fresh.elements.theme.value,'dark');assert.equal(fresh.elements['text-size'].value,'large');assert.equal(fresh.elements.save.disabled,true);
 storage.applied=true;fresh.tick();await fresh.flush();
 assert.equal(fresh.elements.status.textContent,data.words[language][data.phone.applied]);
 assert.equal(fresh.elements['delivery-retry'].hidden,true);
});
test('pending polling preserves an unsaved draft and Follow retains the last explicit appearance',async()=>{
 const storage={saved:{version:2,textSize:'large',appearance:'dark',lastExplicit:'dark'}},ui=boot('en',storage);await ui.flush();
 ui.elements.theme.value='follow';ui.elements.theme.onchange();assert.equal(ui.elements.status.textContent,data.phone.dirty);
 assert.equal(ui.elements['delivery-retry'].hidden,true);
 ui.elements.save.onclick();await ui.flush();assert.deepEqual(storage.saved,{version:2,textSize:'large',appearance:'follow',lastExplicit:'dark'});
});

for(const section of ['display','alarm'])test('late '+section+' poll target discovery cannot interrupt Save or resume after it',async()=>{
 const options={},storage={saved:{version:2,textSize:'follow',appearance:'light',lastExplicit:'light'},applied:true},ui=boot('en',storage,options);await ui.flush();
 options.deferTargets=true;
 // The two controllers poll independently. Trigger the selected controller explicitly.
 ui.elements[section==='display'?'delivery-retry':'alarm-delivery-retry'].onclick();await ui.flush();
 assert.equal(ui.targetRequests.length,1);
 const field=ui.elements[section==='display'?'theme':'alarm-sound'];field.value=section==='display'?'dark':'off';field.onchange();
 const save=ui.elements[section==='display'?'save':'alarm-save'];save.onclick();await ui.flush();assert.equal(ui.targetRequests.length,2);
 const readType=section==='display'?'getUiPreferences':'getAlarmPreferences',before=ui.requests.filter(type=>type===readType).length;
 // Resolve Save discovery first, including the plugin-writer handoff check.
 options.deferTargets=false;ui.targetRequests[1]([{id:'plugin'}]);await ui.flush();
 ui.targetRequests[0]([{id:'plugin'}]);await ui.flush();
 assert.equal(ui.requests.filter(type=>type===readType).length,before,'stale poll must never dispatch after edit/Save');
 assert.equal(save.disabled,true);assert.equal(field.disabled,false);
 assert.notEqual(ui.elements[section==='display'?'status':'alarm-status'].textContent,data.phone.saving);
});
test('unchanged delivery polls do not rewrite live status',async()=>{
 const storage={saved:{version:2,textSize:'follow',appearance:'follow',lastExplicit:'dark'},applied:true},ui=boot('en',storage);await ui.flush();
 assert.equal(ui.elements.theme.value,'follow');assert.equal(ui.elements['appearance-follow'].disabled,true);
 assert.equal(ui.elements['appearance-follow'].textContent,data.phone.follow+' — '+data.phone.dark);
 const writes=ui.elements.status.textWrites;ui.tick();await ui.flush();assert.equal(ui.elements.status.textWrites,writes);
 ui.elements['text-size'].value='large';ui.elements['text-size'].onchange();ui.elements.save.onclick();await ui.flush();
 assert.equal(storage.saved.appearance,'follow','legacy follow remains persisted until an explicit appearance change');
});
