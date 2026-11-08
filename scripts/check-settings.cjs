'use strict';
// Actual optimized PBW HTML/handlers in Chromium with a simulated SDK bridge
// and separate Android DOM/native storage. No real phone or watch is used.
const fs=require('node:fs'),path=require('node:path'),os=require('node:os'),assert=require('node:assert/strict'),crypto=require('node:crypto');
const {execFileSync,spawn}=require('node:child_process');
const root=path.resolve(__dirname,'..'),version=require('../package.json').version;
const artifact=path.resolve(process.env.PEBBLE_PBW||path.join(root,'build/candidate/Timer-'+version+'.pbw')),out=path.join(process.env.PEBBLE_QA_OUTPUT_ROOT||path.join(root,'build/qa'),'settings');fs.mkdirSync(out,{recursive:true});
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
 state.confirm=function(){const message=state.appMessages.filter(m=>m.TIMER_UI_PREFERENCES).at(-1);if(message)events.appmessage({payload:{TIMER_UI_PREFERENCES_RESULT:message.TIMER_UI_PREFERENCES.slice()}});};
 state.resetWatch=function(){events.appmessage({payload:{TIMER_UI_PREFERENCES_RESULT:[1,3,0,1,0,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0]}});};
 state.teardown=function(){state.store=copy(state.dom);state.active=false;events={};persist();};
 if(faults.active)state.start();
 window.Pebble={targets(){return Promise.resolve(state.active?[{id:'plugin'},{id:'pkjs'}]:[{id:'plugin'}]);},addEventListener(name,fn){if(name==='ready')document.addEventListener('DOMContentLoaded',()=>{fn({target:'plugin'});if(state.active)fn({target:'pkjs'});});},sendMessage(target,message){state.requests.push({...copy(message),target});return new Promise((resolve,reject)=>{
  if(message.type==='getUiPreferences'&&faults.load){faults.load--;reject(Error('load unavailable'));return;}
  if(message.type==='saveUiPreferences'&&faults.save){faults.save--;resolve({ok:false,error:'save_unconfirmed'});return;}
  if(message.type==='saveUiPreferences'&&target==='pkjs'&&faults.pkjsReject){faults.pkjsReject--;if(faults.gone)state.teardown();reject(Error('target disappeared'));return;}
  if(message.type==='saveUiPreferences'&&faults.mismatch){faults.mismatch--;resolve({ok:true,settings:{...message.settings,textSize:message.settings.textSize==='large'?'small':'large'}});return;}
  const dispatch=respond=>{if(target==='plugin')config(message,value=>{if(message.type==='saveUiPreferences'&&faults.appear){faults.appear--;state.start();}respond(value);});else if(state.active)events.configmessage({data:message,respond});else reject(Error('no PKJS'));};
  if(message.type==='saveUiPreferences'&&faults.lost){faults.lost--;dispatch(()=>reject(Error('lost acknowledgment')));return;}
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
async function wait(expression){for(let i=0;i<300;i++){if(await evaluate(expression))return;await delay(30);}throw Error('Timed out: '+expression);}
async function nav(url){await send('Page.navigate',{url});await wait("document.readyState==='complete'&&!document.getElementById('theme').disabled");}
async function change(value){await evaluate('(()=>{const f=document.getElementById("theme");f.value='+JSON.stringify(value)+';f.dispatchEvent(new Event("change",{bubbles:true}));})()');}
async function save(){await evaluate('document.getElementById("save").click()');}
async function saved(){await wait('document.getElementById("status").textContent==="Saved on phone · Pending on watch"');}
async function capture(name){const r=await send('Page.captureScreenshot',{format:'png',captureBeyondViewport:false});const bytes=Buffer.from(r.data,'base64');fs.writeFileSync(path.join(out,name),bytes);captures.push({file:name,sha256:sha(bytes)});}
async function run(){
 await launch();const key=require('../src/pkjs/settings').key,uiKey=require('../src/pkjs/ui-preferences').key,light={version:1,theme:'light'};
 const desired={version:2,textSize:'large',appearance:'dark',lastExplicit:'dark'};
 for(const width of [320,360])for(const theme of ['light','dark']){
  await send('Emulation.setDeviceMetricsOverride',{width,height:800,deviceScaleFactor:1,mobile:true});await send('Emulation.setEmulatedMedia',{features:[{name:'prefers-color-scheme',value:theme}]});
  await nav(fixture());assert.equal(await evaluate('document.getElementById("save").disabled'),false);assert.equal(await evaluate('document.activeElement.tagName'),'BODY');assert.deepEqual(await evaluate('Object.keys(__qa.store)'),[]);
  const layout=await evaluate('({scroll:document.documentElement.scrollWidth,targets:Array.from(document.querySelectorAll("button,select,summary")).filter(x=>x.getClientRects().length).map(x=>x.getBoundingClientRect().height),selects:document.querySelectorAll("select").length})');
  assert.ok(layout.scroll<=width);assert.equal(layout.selects,2);assert.ok(layout.targets.every(h=>h>=44));await capture('settings-'+width+'-'+theme+'.png');
 }
 checks.push('two independent selectors;explicit fresh-default Save;no writes/autofocus on load;320/360px light/dark;44px targets;no overflow');
 const route=fixture({[key]:JSON.stringify(light)},{active:true});await nav(route);await change('dark');
 await evaluate('(()=>{const field=document.getElementById("text-size");field.value="large";field.dispatchEvent(new Event("change"));})()');
 await evaluate('__qa.faults.delay=150');await save();await wait('document.getElementById("status").textContent==="Saving…"');await saved();
 assert.equal(await evaluate('location.href'),route);assert.equal(await evaluate('__qa.requests.filter(m=>m.type==="saveUiPreferences")[0].target'),'pkjs');
 assert.deepEqual(JSON.parse(await evaluate('__qa.store['+JSON.stringify(uiKey)+']')).settings,desired);
 assert.ok(await evaluate('__qa.appMessages.every(m=>m.TIMER_UI_PREFERENCES&&m.TIMER_APPEARANCE_VERSION===1)'));
 await capture('settings-pending.png');await evaluate('__qa.confirm()');await wait('document.getElementById("status").textContent==="Applied on watch"');await capture('settings-applied.png');
 await evaluate('__qa.resetWatch()');await saved();await evaluate('__qa.confirm()');await wait('document.getElementById("status").textContent==="Applied on watch"');
 await evaluate('__qa.teardown()');await send('Page.reload');await wait('!document.getElementById("theme").disabled');assert.equal(await evaluate('document.getElementById("theme").value'),'dark');
 await stop();await launch();await nav(route);assert.equal(await evaluate('document.getElementById("text-size").value'),'large');
 checks.push('actual optimized PKJS/plugin persist exact choices;transport success stays pending;exact watch application receipt changes status;fresh mismatched watch state returns the page to pending;Android teardown/page reopen/browser restart retain choices');
 await change('light');await evaluate('__qa.faults.lost=1');await save();await wait('document.getElementById("status").textContent.includes("not confirmed")');assert.equal(await evaluate('document.getElementById("theme").value'),'light');
 await evaluate('document.getElementById("discard").click()');await saved();assert.equal(await evaluate('document.getElementById("theme").value'),'light');
 checks.push('lost phone response retains draft;explicit reload reads saved choice as pending');
 for(const faults of [{appear:1},{active:true,pkjsReject:1,gone:true},{save:1},{mismatch:1}]){
  await nav(fixture({[key]:JSON.stringify(light)},faults));await change('dark');await save();
  if(faults.save||faults.mismatch){await wait('document.getElementById("status").textContent.includes("not confirmed")');assert.equal(await evaluate('document.getElementById("theme").value'),'dark');}
  else{await saved();assert.equal(JSON.parse(await evaluate('__qa.store['+JSON.stringify(uiKey)+']')).settings.appearance,'dark');if(await evaluate('__qa.active'))await evaluate('__qa.teardown()');assert.equal(JSON.parse(await evaluate('__qa.store['+JSON.stringify(uiKey)+']')).settings.appearance,'dark');}
 }
 checks.push('live-writer handoff survives PKJS appearing/disappearing;failed/mismatched responses do not claim saved');
 const failed=fixture({[key]:JSON.stringify(light)},{load:1});await send('Page.navigate',{url:failed});await wait('!document.getElementById("retry").hidden');assert.equal(await evaluate('document.getElementById("theme").value'),'');assert.equal(await evaluate('document.getElementById("save").disabled'),true);await evaluate('document.getElementById("retry").click()');await wait('!document.getElementById("theme").disabled');
 await nav(fixture({[uiKey]:'broken'}));assert.equal(await evaluate('__qa.store['+JSON.stringify(uiKey)+']'),'broken');await save();await saved();
 checks.push('load failure leaves unknown choice disabled;Retry loads;corrupt record repaired only by explicit Save');
 await nav(fixture({[key]:JSON.stringify(light)},{active:true}));await change('dark');await save();await saved();await evaluate('(()=>{const f=document.getElementById("text-size");f.value="small";f.dispatchEvent(new Event("change"));__qa.confirm();})()');await delay(2200);
 assert.equal(await evaluate('document.getElementById("text-size").value'),'small');assert.equal(await evaluate('document.getElementById("status").textContent'),'Unsaved changes');
 checks.push('late watch confirmation/polling leaves an unsaved text-size draft intact');
 await send('Emulation.setDeviceMetricsOverride',{width:320,height:800,deviceScaleFactor:1,mobile:true});await nav(fixture({}, {},'cs-CZ'));assert.equal(await evaluate('document.documentElement.lang'),'cs');await change('dark');await save();await wait('document.getElementById("status").textContent==="Uloženo v telefonu · Čeká na hodinky"');await capture('settings-320-czech-pending.png');
 await send('Emulation.setDeviceMetricsOverride',{width:320,height:360,deviceScaleFactor:1,mobile:true});await wait('document.getElementById("actions").getAttribute("data-flow")==="true"');await evaluate('(()=>{const style=document.createElement("style");style.textContent="body,button,select,summary,label{font-size:32px!important}h1{font-size:56px!important}h2{font-size:40px!important}.hint,.status,details>p{font-size:28px!important}";document.head.appendChild(style);})()');await capture('settings-320-short-czech.png');assert.ok(await evaluate('document.documentElement.scrollWidth<=innerWidth'));
 checks.push('localized pending status;short viewport and enlarged text keep controls reachable without horizontal overflow');
 const report={result:'PASS',version,scope:'Exact packaged optimized config HTML and handlers in Chromium;simulated SDK, watch receipts and independent Android stores;not real-phone/device verification',pbwSha256:sha(fs.readFileSync(artifact)),configSha256:sha(files['config.html']),checks,captures};fs.writeFileSync(path.join(out,'report.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify({result:'PASS',checks:checks.length,captures:captures.length,path:path.join(out,'report.json')}));
}
run().catch(error=>{console.error(error);process.exitCode=1;}).finally(async()=>{await stop();fs.rmSync(scratch,{recursive:true,force:true});});
