'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm');
const page=require('../src/pkjs/settings-page'),data=require('../src/common/translations.json');
function boot(locale,storage){
 const elements={},events={},timers=new Map(),html=page.config();let sequence=0;
 for(const id of ['theme','text-size','delivery-retry','settings','save','discard','retry','status','actions'])elements[id]={id,value:'',options:[{}],style:{setProperty(){}},setAttribute(){},getBoundingClientRect(){return {height:100};}};
 const root={style:{setProperty(){}}};
 vm.runInNewContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],{
  navigator:{language:locale},document:{documentElement:root,getElementById:id=>elements[id],querySelectorAll:()=>[]},
  window:{innerHeight:800,requestAnimationFrame(){},addEventListener(){}},setTimeout(fn){timers.set(++sequence,fn);return sequence;},clearTimeout(id){timers.delete(id);},Promise,
  Pebble:{addEventListener:(name,fn)=>{events[name]=fn;},targets:async()=>[{id:'plugin'}],sendMessage:async(target,message)=>{assert.equal(target,'plugin');if(message.type==='saveUiPreferences'){storage.saved=JSON.parse(JSON.stringify(message.settings));storage.applied=false;}return {ok:true,settings:storage.saved,saved:true,delivery:{applied:!!storage.applied,pending:!storage.applied}};}}
 });
 events.ready({target:'plugin'});
 return {elements,root,tick(){const entry=timers.entries().next().value;assert.ok(entry);timers.delete(entry[0]);entry[1]();},flush:async()=>{for(let i=0;i<20;++i)await Promise.resolve();}};
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
