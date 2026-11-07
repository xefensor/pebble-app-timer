'use strict';
// Actual optimized PBW HTML/handlers in Chromium with a simulated SDK bridge
// and separate Android DOM/native storage. No real phone or watch is used.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {execFileSync,spawn}=require('node:child_process');
const root=path.resolve(__dirname,'..'),version=require('../package.json').version;
const artifact=path.join(root,'dist/Timer-'+version+'.pbw'),out=path.join(process.env.PEBBLE_QA_OUTPUT_ROOT||path.join(root,'build/qa'),'settings');fs.mkdirSync(out,{recursive:true});
const files=JSON.parse(execFileSync('python3',['-c','import sys,json,zipfile\nwith zipfile.ZipFile(sys.argv[1]) as z: print(json.dumps({n:z.read(n).decode() for n in ["config.html","plugin.js","pebble-js-app.js"]}))',artifact],{encoding:'utf8'}));
assert.equal(files['config.html'],fs.readFileSync(path.join(root,'config.html'),'utf8'));
const sha=v=>crypto.createHash('sha256').update(v).digest('hex'),scratch=fs.mkdtempSync(path.join(os.tmpdir(),'timer-settings-browser-')),profile=path.join(scratch,'profile');
const delay=ms=>new Promise(r=>setTimeout(r,ms)),checks=[],captures=[];let browser,socket,send,evaluate;
function sdk(seed,faults,pluginCode,pkjsCode,persistenceKey,locale){
 if(locale)Object.defineProperty(navigator,'language',{value:locale,configurable:true});
 const copy=v=>JSON.parse(JSON.stringify(v)),previous=localStorage.getItem(persistenceKey);
 const state=window.__qa={store:previous?JSON.parse(previous):copy(seed),dom:null,active:false,requests:[],appMessages:[],faults};
 const persist=()=>localStorage.setItem(persistenceKey,JSON.stringify(state.store));
 const storage={getItem:k=>Object.prototype.hasOwnProperty.call(state.store,k)?state.store[k]:null,setItem(k,v){state.store[k]=String(v);persist();}};
 let config,events={};
 new Function('Pebble','localStorage',pluginCode)({registerSourceHandler(){},registerConfigHandler(fn){config=fn;}},storage);
 state.start=function(){state.dom=copy(state.store);events={};new Function('Pebble','localStorage',pkjsCode)({addEventListener(n,fn){events[n]=fn;},sendAppMessage(message,ok){state.appMessages.push(copy(message));if(ok)ok();}},{getItem:k=>Object.prototype.hasOwnProperty.call(state.dom,k)?state.dom[k]:null,setItem(k,v){state.dom[k]=String(v);state.store[k]=String(v);persist();}});state.active=true;};
 state.teardown=function(){state.store=copy(state.dom);state.active=false;events={};persist();};
 if(faults.active)state.start();
 window.Pebble={targets(){return Promise.resolve(state.active?[{id:'plugin'},{id:'pkjs'}]:[{id:'plugin'}]);},addEventListener(name,fn){if(name==='ready')document.addEventListener('DOMContentLoaded',()=>{fn({target:'plugin'});if(state.active)fn({target:'pkjs'});});},sendMessage(target,message){state.requests.push({...copy(message),target});return new Promise((resolve,reject)=>{
  if(message.type==='getSettings'&&faults.load){faults.load--;reject(Error('load unavailable'));return;}
  if(message.type==='saveSettings'&&faults.save){faults.save--;resolve({ok:false,error:'save_unconfirmed'});return;}
  if(message.type==='saveSettings'&&target==='pkjs'&&faults.pkjsReject){faults.pkjsReject--;if(faults.gone)state.teardown();reject(Error('target disappeared'));return;}
  if(message.type==='saveSettings'&&faults.mismatch){faults.mismatch--;resolve({ok:true,settings:{version:1,theme:message.settings.theme==='dark'?'light':'dark'}});return;}
  const dispatch=respond=>{if(target==='plugin')config(message,value=>{if(message.type==='saveSettings'&&faults.appear){faults.appear--;state.start();}respond(value);});else if(state.active)events.configmessage({data:message,respond});else reject(Error('no PKJS'));};
  if(message.type==='saveSettings'&&faults.lost){faults.lost--;dispatch(()=>reject(Error('lost acknowledgment')));return;}
  setTimeout(()=>dispatch(resolve),faults.delay||0);
 });}};
}
function fixture(seed={},faults={},locale='en-US'){
 const key='timer-config-qa-'+crypto.randomBytes(8).toString('hex');
 const args=[seed,faults,files['plugin.js'],files['pebble-js-app.js'],key,locale].map(v=>JSON.stringify(v).replace(/<\/script/gi,'<\\/script'));
 const html=files['config.html'].replace('<script>','<script>('+sdk.toString()+')('+args.join(',')+');</script><script>');
 const page=path.join(scratch,key+'.html');fs.writeFileSync(page,html);return 'file://'+page;
}
async function launch(){
 fs.mkdirSync(profile,{recursive:true});const portFile=path.join(profile,'DevToolsActivePort');fs.rmSync(portFile,{force:true});
 const log=fs.openSync(path.join(out,'chromium.log'),'a');browser=spawn(process.env.CHROMIUM_BINARY||'/usr/bin/chromium',['--headless=new','--no-sandbox','--disable-gpu','--disable-dev-shm-usage','--disable-background-networking','--no-first-run','--remote-debugging-port=0','--user-data-dir='+profile,'about:blank'],{stdio:['ignore',log,log]});fs.closeSync(log);
 for(let i=0;i<100&&!fs.existsSync(portFile);i++)await delay(50);assert.ok(fs.existsSync(portFile));
 const port=fs.readFileSync(portFile,'utf8').split('\n')[0],tabs=await(await fetch('http://127.0.0.1:'+port+'/json')).json();socket=new WebSocket(tabs.find(t=>t.type==='page').webSocketDebuggerUrl);
 await new Promise((resolve,reject)=>{socket.onopen=resolve;socket.onerror=reject;});
 const pending=new Map();let sequence=0;socket.onmessage=event=>{const message=JSON.parse(event.data);if(message.id){const request=pending.get(message.id);pending.delete(message.id);message.error?request.reject(message.error):request.resolve(message.result);}};
 send=(method,params={})=>new Promise((resolve,reject)=>{const id=++sequence;pending.set(id,{resolve,reject});socket.send(JSON.stringify({id,method,params}));});
 evaluate=async expression=>{const r=await send('Runtime.evaluate',{expression,returnByValue:true,awaitPromise:true});assert.equal(r.exceptionDetails,undefined,JSON.stringify(r.exceptionDetails));return r.result.value;};
 await send('Page.enable');await send('Runtime.enable');
}
async function stop(){if(!browser)return;try{await send('Browser.close');}catch{}for(let i=0;i<80&&browser.exitCode===null;i++)await delay(25);if(browser.exitCode===null)browser.kill('SIGTERM');try{socket.close();}catch{}}
async function wait(expression){for(let i=0;i<150;i++){if(await evaluate(expression))return;await delay(30);}throw Error('Timed out: '+expression);}
async function nav(url){await send('Page.navigate',{url});await wait("document.readyState==='complete'&&!document.getElementById('theme').disabled");}
async function change(value){await evaluate('(()=>{const f=document.getElementById("theme");f.value='+JSON.stringify(value)+';f.dispatchEvent(new Event("change",{bubbles:true}));})()');}
async function save(){await evaluate('document.getElementById("save").click()');}
async function saved(){await wait('document.getElementById("status").textContent==="✓ Saved"');}
async function capture(name){const r=await send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});const bytes=Buffer.from(r.data,'base64');fs.writeFileSync(path.join(out,name),bytes);captures.push({file:name,sha256:sha(bytes)});}
async function run(){
 await launch();const key=require('../src/pkjs/settings').key,light={version:1,theme:'light'},dark={version:1,theme:'dark'};
 for(const width of [320,360])for(const theme of ['light','dark']){
  await send('Emulation.setDeviceMetricsOverride',{width,height:800,deviceScaleFactor:1,mobile:true});await send('Emulation.setEmulatedMedia',{features:[{name:'prefers-color-scheme',value:theme}]});
  await nav(fixture());assert.equal(await evaluate('document.getElementById("save").disabled'),true);assert.equal(await evaluate('document.activeElement.tagName'),'BODY');assert.deepEqual(await evaluate('Object.keys(__qa.store)'),[]);
  const layout=await evaluate('({width:innerWidth,scroll:document.documentElement.scrollWidth,targets:Array.from(document.querySelectorAll("button,select,summary")).filter(x=>x.getClientRects().length).map(x=>x.getBoundingClientRect().height),selects:document.querySelectorAll("select").length})');
  assert.ok(layout.scroll<=width);assert.equal(layout.selects,1);assert.ok(layout.targets.every(h=>h>=44));await capture('settings-'+width+'-'+theme+'.png');
 }
 checks.push('one Appearance selector only; no writes/default saves or autofocus;320/360px light/dark;44px targets;no overflow');
 const route=fixture({[key]:JSON.stringify(light)},{active:true});await nav(route);await change('dark');assert.equal(await evaluate('document.getElementById("status").textContent'),'Unsaved changes');
 await evaluate('__qa.faults.delay=150');await save();await wait('document.getElementById("status").textContent==="Saving…"');await saved();
 assert.equal(await evaluate('location.href'),route);assert.equal(await evaluate('document.getElementById("save").disabled'),true);assert.equal(await evaluate('__qa.requests.filter(m=>m.type==="saveSettings")[0].target'),'pkjs');
 assert.deepEqual(JSON.parse(await evaluate('__qa.store['+JSON.stringify(key)+']')),dark);assert.ok(await evaluate('__qa.appMessages.every(m=>m.TIMER_APPEARANCE_VERSION===1&&Object.keys(m).length===2)'));
 await evaluate('__qa.teardown()');await send('Page.reload');await wait('!document.getElementById("theme").disabled');assert.equal(await evaluate('document.getElementById("theme").value'),'dark');
 await stop();await launch();await nav(route);assert.equal(await evaluate('document.getElementById("theme").value'),'dark');
 checks.push('active PKJS owns exact confirmed Save;page stays open;Android teardown,reload and full browser restart preserve appearance;appearance transport never acts on timers');
 await change('light');await evaluate('__qa.faults.lost=1');await save();await wait('document.getElementById("status").textContent.includes("not confirmed")');assert.equal(await evaluate('document.getElementById("theme").value'),'light');assert.equal(await evaluate('document.getElementById("discard").textContent'),'Reload saved');
 await evaluate('document.getElementById("discard").click()');await wait('document.getElementById("status").textContent==="Saved settings reloaded."');assert.equal(await evaluate('document.getElementById("theme").value'),'light');
 checks.push('lost acknowledgment stays uncertain;explicit Reload saved reads actual stored value without falsely claiming rollback');
 for(const faults of [{appear:1},{active:true,pkjsReject:1,gone:true},{save:1},{mismatch:1}]){
  await nav(fixture({[key]:JSON.stringify(light)},faults));await change('dark');await save();
  if(faults.save||faults.mismatch){await wait('document.getElementById("status").textContent.includes("not confirmed")');assert.equal(await evaluate('document.getElementById("theme").value'),'dark');}
  else{await saved();assert.deepEqual(JSON.parse(await evaluate('__qa.store['+JSON.stringify(key)+']')),dark);if(await evaluate('__qa.active'))await evaluate('__qa.teardown()');assert.deepEqual(JSON.parse(await evaluate('__qa.store['+JSON.stringify(key)+']')),dark);}
 }
 checks.push('newly started PKJS synchronizes plugin save;confirmed disappearance permits fallback;failed/mismatched acknowledgments retain draft');
 const failed=fixture({[key]:JSON.stringify(dark)},{load:1});await send('Page.navigate',{url:failed});await wait('!document.getElementById("retry").hidden');assert.equal(await evaluate('document.getElementById("theme").value'),'');assert.equal(await evaluate('document.getElementById("save").disabled'),true);await evaluate('document.getElementById("retry").click()');await wait('!document.getElementById("theme").disabled');assert.equal(await evaluate('document.getElementById("theme").value'),'dark');
 await nav(fixture({[key]:'broken'}));assert.equal(await evaluate('__qa.store['+JSON.stringify(key)+']'),'broken');assert.equal(await evaluate('document.getElementById("save").disabled'),false);await save();await saved();
 checks.push('load failure retains unknown disabled choice;Retry reads saved state;corrupt record requires explicit repair');
 await send('Emulation.setDeviceMetricsOverride',{width:320,height:800,deviceScaleFactor:1,mobile:true});await nav(fixture({}, {},'cs-CZ'));assert.equal(await evaluate('document.documentElement.lang'),'cs');assert.equal(await evaluate('document.getElementById("save").textContent'),'Uložit');await change('dark');await save();await wait('document.getElementById("status").textContent==="✓ Uloženo"');await capture('settings-320-czech-saved.png');
 await send('Emulation.setDeviceMetricsOverride',{width:320,height:360,deviceScaleFactor:1,mobile:true});await wait('document.getElementById("actions").getAttribute("data-flow")==="true"');await evaluate('(()=>{const style=document.createElement("style");style.textContent="body,button,select,summary,label{font-size:32px!important}h1{font-size:56px!important}h2{font-size:40px!important}.hint,.status,details>p{font-size:28px!important}";document.head.appendChild(style);})()');await capture('settings-320-short-czech.png');
 assert.ok(await evaluate('document.documentElement.scrollWidth<=innerWidth'),'Double text must not overflow the page');
 checks.push('EN/CS Save states and controls;short viewport moves action area into document flow');
 const report={result:'PASS',version,scope:'Actual packaged optimized config HTML and handlers in Chromium, simulated SDK and independent Android stores;not real-phone/device verification',pbwSha256:sha(fs.readFileSync(artifact)),configSha256:sha(files['config.html']),checks,captures};fs.writeFileSync(path.join(out,'report.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({result:'PASS',checks:checks.length,captures:captures.length,path:path.join(out,'report.json')}));
}
run().catch(error=>{console.error(error);process.exitCode=1;}).finally(async()=>{await stop();fs.rmSync(scratch,{recursive:true,force:true});});
