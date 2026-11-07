'use strict';
const test=require('node:test'),assert=require('node:assert/strict'),vm=require('node:vm');
const page=require('../src/pkjs/settings-page'),data=require('../src/common/translations.json');
function boot(locale,storage){
 const elements={},events={},html=page.config();
 for(const id of ['theme','settings','save','discard','retry','status','actions'])elements[id]={id,value:'',options:[{}],style:{setProperty(){}},setAttribute(){},getBoundingClientRect(){return {height:100};}};
 const root={style:{setProperty(){}}};
 vm.runInNewContext(html.match(/<script>([\s\S]*?)<\/script>/)[1],{
  navigator:{language:locale},document:{documentElement:root,getElementById:id=>elements[id],querySelectorAll:()=>[]},
  window:{innerHeight:800,requestAnimationFrame(){},addEventListener(){}},setTimeout(){return 1;},clearTimeout(){},Promise,
  Pebble:{addEventListener:(name,fn)=>{events[name]=fn;},targets:async()=>[{id:'plugin'}],sendMessage:async(target,message)=>{assert.equal(target,'plugin');if(message.type==='saveSettings')storage.saved=JSON.parse(JSON.stringify(message.settings));return {ok:true,settings:storage.saved};}}
 });
 events.ready({target:'plugin'});
 return {elements,root,flush:async()=>{for(let i=0;i<12;++i)await Promise.resolve();}};
}
for(const locale of [...data.languages,'nb-NO','nn_NO','DE_de','zz-ZZ'])test('Timer localized save/reopen: '+locale,async()=>{
 const storage={saved:{version:1,theme:'light'}},ui=boot(locale,storage);await ui.flush();
 const language=locale==='DE_de'?'de':locale.startsWith('n')&&locale!=='nl'&&locale!=='no'?'no':locale==='zz-ZZ'?'en':locale;
 assert.equal(ui.root.lang,language);assert.equal(ui.elements.save.disabled,true);
 ui.elements.theme.value='dark';ui.elements.theme.onchange();ui.elements.save.onclick();await ui.flush();
 assert.deepEqual(storage.saved,{version:1,theme:'dark'});assert.equal(ui.elements.status.textContent,data.words[language]['✓ Saved']);
 const fresh=boot(locale,storage);await fresh.flush();assert.equal(fresh.elements.theme.value,'dark');assert.equal(fresh.elements.save.disabled,true);
});
