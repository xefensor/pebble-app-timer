#!/usr/bin/env node
'use strict';

// Functional artifact check, not a real phone or hardware test. Execute the
// packaged scripts with their host surfaces and no external Node/WebView APIs.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const directory = process.argv[2];
if (!directory || process.argv.length !== 3) {
  throw new Error('Usage: verify-timer-bundle.js EXTRACTED_PBW_DIRECTORY');
}
const root = path.resolve(directory);
const appinfo = JSON.parse(fs.readFileSync(path.join(root, 'appinfo.json'), 'utf8'));
const keys = appinfo.messageKeys;
assert.ok(keys && typeof keys === 'object', 'PBW has no symbolic message-key map');
assert.deepEqual(appinfo.appKeys, keys, 'PBW numeric and symbolic key maps differ');
assert.equal(appinfo.uuid, '50e9ef12-a5ab-4369-a3b5-3447bfba5151');
assert.equal(appinfo.manifestVersion, 2);
assert.equal(appinfo.watchapp.watchface, false);
assert.equal(keys.ACTIVITY_SLOT, 1021, 'Timer slot key changed');
assert.equal(keys.ACTIVITY_TOTAL_DURATION, 1022, 'Timer total-duration key changed');
assert.equal(keys.ACTIVITY_SHOW_TERMINAL, 1023, 'Timer result-visibility key changed');
assert.equal(appinfo.plugin.sources.filter(source => source.category === 'activity').length, 1,
  'PBW must declare one activity category covering simple and compatibility source items');
assert.deepEqual(appinfo.plugin.sources.find(source => source.category === 'activity').items,
  ['summary', 'live', 'session', 'card']);

assert.equal(keys.TIMER_APPEARANCE,1024,'Appearance key changed');
assert.equal(keys.TIMER_APPEARANCE_VERSION,1025,'Appearance version key changed');
assert.equal(appinfo.configPage,'config.html');
assert.ok(fs.readFileSync(path.join(root,'config.html'),'utf8').includes('id="theme"'));

const reverse = {};
for (const [name, value] of Object.entries(keys)) {
  assert.ok(Number.isInteger(value) && value >= 0 && value <= 4294967295,
    `Invalid message-key number: ${name}`);
  assert.equal(reverse[value], undefined, `Duplicate numeric message key: ${value}`);
  reverse[value] = name;
}
function nativeDictionary(symbolic) {
  return Object.fromEntries(Object.entries(symbolic).map(([name, value]) => {
    assert.ok(Object.hasOwn(keys, name), `Undeclared message key: ${name}`);
    assert.notEqual(value, undefined, `Undefined message value: ${name}`);
    return [keys[name], value];
  }));
}
function pkjsDictionary(numeric) {
  return Object.fromEntries(Object.entries(numeric).map(([number, value]) => {
    assert.ok(Object.hasOwn(reverse, number), `Unknown native message key: ${number}`);
    return [reverse[number], value];
  }));
}

const NOW_SECONDS = 1800000000;
const NOW_MS = NOW_SECONDS * 1000;
class FixedDate extends Date {
  constructor(...arguments_) {
    super(...(arguments_.length ? arguments_ : [NOW_MS]));
  }
  static now() { return NOW_MS; }
}
const saved = new Map();
const storage = {
  getItem(key) { return saved.has(key) ? saved.get(key) : null; },
  setItem(key, value) { saved.set(key, String(value)); },
  removeItem(key) { saved.delete(key); }
};
const run = 'packaged-timer-check:1';
const revision = 7;
const ticket = 123;
const observedAt = NOW_SECONDS - 20;
const deadline = NOW_SECONDS + 600;
const validUntil = NOW_SECONDS + 660;

function verifyPkjs() {
  const events = {};
  const outgoing = [];
  const pebble = {
    addEventListener(name, handler) { events[name] = handler; },
    removeEventListener(name) { delete events[name]; },
    sendAppMessage(message, success) {
      // This is the appinfo mapping used at the mobile AppMessage boundary.
      outgoing.push(nativeDictionary(message));
      if (success) success();
    }
  };
  const context = vm.createContext({
    Pebble: pebble, localStorage: storage, Date: FixedDate, console,setTimeout(){return 1;},clearTimeout(){},
    __events: events
  });
  vm.runInContext(fs.readFileSync(path.join(root, 'pebble-js-app.js'), 'utf8'), context,
    {filename: 'packaged-pebble-js-app.js', timeout: 1000});
  assert.equal(typeof events.ready, 'function', 'Packaged PKJS has no ready handler');
  assert.equal(typeof events.appmessage, 'function', 'Packaged PKJS has no message handler');
  vm.runInContext('__events.ready()', context, {timeout: 1000});
  assert.equal(outgoing.length, 2, 'PKJS ready reconciles appearance and requests the native snapshot');
  const ui=pkjsDictionary(outgoing.shift());
  assert.equal(ui.TIMER_APPEARANCE,0);assert.equal(ui.TIMER_APPEARANCE_VERSION,1);
  assert.deepEqual(Array.from(ui.TIMER_UI_PREFERENCES),[1,3,0,0,1,0,0,0,0,0,0,0,1,0,0,0,1,0,0,0]);
  const request = pkjsDictionary(outgoing.shift());
  assert.equal(request.ACTIVITY_REQUEST, 1);
  assert.equal(request.ACTIVITY_LAST_REVISION, 0);

  const native = nativeDictionary({
    ACTIVITY_SCHEMA_VERSION: 1,
    ACTIVITY_SLOT: 0,
    ACTIVITY_RUN: run,
    ACTIVITY_REVISION: revision,
    ACTIVITY_STATE: 'active',
    ACTIVITY_KIND: 'timer',
    ACTIVITY_CLOCK: 'deadline',
    ACTIVITY_NAME: 'Timer',
    ACTIVITY_MAIN: '',
    ACTIVITY_FOOTER: 'Timer',
    ACTIVITY_OBSERVED_AT: observedAt,
    ACTIVITY_VALID_UNTIL: validUntil,
    ACTIVITY_DEADLINE: deadline,
    ACTIVITY_DURATION: 0,
    ACTIVITY_CLOCK_AT: 0,
    ACTIVITY_TOTAL_DURATION: 1200,
    ACTIVITY_PRIVATE: 0,
    ACTIVITY_PUBLISH_SESSION: ticket
  });
  context.__incoming = {payload: pkjsDictionary(native)};
  vm.runInContext('__events.appmessage(__incoming)', context, {timeout: 1000});
  assert.equal(outgoing.length, 1, 'Saving one snapshot must produce one receipt');
  const receipt = pkjsDictionary(outgoing.shift());
  assert.equal(receipt.ACTIVITY_ACCEPTED_RUN, run);
  assert.equal(receipt.ACTIVITY_ACCEPTED_REVISION, revision);
  assert.equal(receipt.ACTIVITY_PUBLISH_SESSION, ticket);
  assert.equal(receipt.ACTIVITY_ERROR, undefined);
  assert.ok(saved.size, 'Receipt arrived without any shared phone-side state');
  for (const [id, nextRevision] of [['second-slot:1', revision + 1], ['second-slot:2', revision + 2]]) {
    context.__incoming = {payload: {...pkjsDictionary(native), ACTIVITY_SLOT:7,
      ACTIVITY_RUN:id, ACTIVITY_REVISION:nextRevision}};
    vm.runInContext('__events.appmessage(__incoming)', context, {timeout:1000});
    assert.equal(outgoing.length, 1);
    const accepted = pkjsDictionary(outgoing.shift());
    assert.equal(accepted.ACTIVITY_ACCEPTED_RUN, id);
    assert.equal(accepted.ACTIVITY_ACCEPTED_REVISION, nextRevision);
    assert.equal(accepted.ACTIVITY_PUBLISH_SESSION, ticket);
    assert.equal(accepted.ACTIVITY_ERROR, undefined);
  }
  return function publish(overrides) {
    context.__incoming = {payload: {...pkjsDictionary(native), ...overrides}};
    vm.runInContext('__events.appmessage(__incoming)', context, {timeout:1000});
    assert.equal(outgoing.length, 1);
    return pkjsDictionary(outgoing.shift());
  };
}
const publishNative = verifyPkjs();

// The PKJS execution above is no longer used. The provider must work in a new
// context with only the saved strings, not variables from the watchapp's JS.
const source = appinfo.plugin && appinfo.plugin.sources.find(declaration =>
  declaration.category === 'activity' && declaration.items.includes('session'));
assert.ok(source, 'PBW does not declare activity/session');
assert.equal(appinfo.plugin.script, 'plugin.js');
let handler;
let response;
let sourceError;
const context = vm.createContext({
  Pebble: {registerSourceHandler(callback) { handler = callback; }},
  localStorage: storage, Date: FixedDate, console
});
vm.runInContext(fs.readFileSync(path.join(root, appinfo.plugin.script), 'utf8'), context,
  {filename: 'packaged-plugin.js', timeout: 1000});
assert.equal(typeof handler, 'function', 'Standalone plugin did not register its source');
context.__handler = handler;
context.__request = {category: 'activity', item: 'session'};
context.__respond = {
  data(value) { response = value; },
  error(code) { sourceError = code; }
};
vm.runInContext('__handler(__request, __respond)', context, {timeout: 1000});
assert.equal(sourceError, undefined);
assert.ok(response && response.validUntilMs > NOW_MS, 'Plugin did not return available source data');
assert.equal(response.instances.length, 2);
assert.equal(response.instances[0].instanceId, run);
assert.equal(response.instances[1].instanceId, 'second-slot:2', 'An obsolete offline run remained visible');
assert.equal(response.instances[1].properties.revision.numericValue.value, revision + 2);
const properties = response.instances[0].properties;
assert.equal(properties.schema_version.numericValue.value, 1);
assert.equal(properties.revision.numericValue.value, revision);
assert.equal(properties.state.shortText.text, 'active');
assert.equal(properties.clock.shortText.text, 'deadline');
assert.equal(properties.observed_at.timestamp.value, observedAt);
assert.equal(properties.deadline.timestamp.value, deadline);
assert.equal(properties.valid_until.timestamp.value, validUntil);
assert.equal(properties.private.boolean.value, false);
assert.equal(properties.unconfirmed.boolean.value, true);
for (const [name, value] of Object.entries(properties)) {
  for (const shape of Object.keys(value)) {
    assert.ok(source.properties[name] && source.properties[name].includes(shape),
      `Plugin emitted an undeclared property/shape: ${name}/${shape}`);
  }
}

const cardSource = appinfo.plugin.sources.find(declaration =>
  declaration.category === 'activity' && declaration.items.includes('card'));
assert.ok(cardSource, 'PBW does not declare activity/card');
response = undefined;
sourceError = undefined;
context.__request = {category:'activity', item:'card'};
vm.runInContext('__handler(__request, __respond)', context, {timeout:1000});
assert.equal(sourceError, undefined);
assert.equal(response.instances.length, 2);
assert.equal(response.instances[0].instanceId, run);
assert.equal(response.instances[1].instanceId, 'second-slot:2');
const card = response.instances[0].properties;
assert.equal(card.schema_version.numericValue.value, 2);
assert.equal(card.state.shortText.text, 'active');
assert.equal(card.main_type.shortText.text, 'clock');
assert.equal(card.main_clock.shortText.text, 'deadline');
assert.equal(card.main_running.boolean.value, true);
assert.equal(card.main_base.numericValue.value - (NOW_SECONDS-card.main_anchor.timestamp.value), 600);
assert.equal(card.main_label.longText.text, card.caption.longText.text);
assert.equal(card.support_type, undefined, 'Timer must expose its compact projection');
assert.equal(card.unconfirmed.boolean.value, true);
for (const [name, value] of Object.entries(card)) {
  for (const shape of Object.keys(value)) {
    assert.ok(cardSource.properties[name] && cardSource.properties[name].includes(shape),
      `Card emitted an undeclared property/shape: ${name}/${shape}`);
  }
}
const liveSource=appinfo.plugin.sources.find(declaration =>
  declaration.category==='activity'&&declaration.items.includes('live'));
assert.ok(liveSource,'PBW does not declare activity/live');
response=undefined;sourceError=undefined;context.__request={category:'activity',item:'live'};
vm.runInContext('__handler(__request, __respond)',context,{timeout:1000});
assert.equal(sourceError,undefined);
assert.equal(response.instances.length,2);
assert.equal(response.instances[0].instanceId,run);
assert.equal(response.instances[1].instanceId,'second-slot:2');
const live=response.instances[0].properties;
assert.equal(live.schema_version.numericValue.value,3);
assert.equal(live.app_name.longText.text,'Timer');
assert.equal(live.observed_at.timestamp.value,observedAt);
assert.equal(live.valid_until.timestamp.value,validUntil);
assert.equal(live.unconfirmed.boolean.value,true);
const content=JSON.parse(live.content.longText.text);
assert.equal(content.clock,'countdown');
assert.equal(content.deadline,deadline);
assert.equal(content.label,undefined,'Simple Timer should not repeat a duration or clock label');
for(const field of ['main_label','support_type','detail','meter_kind'])
  assert.equal(live[field],undefined,'Simple source leaked an advanced display field: '+field);
for(const [name,value] of Object.entries(live))for(const shape of Object.keys(value))
  assert.ok(liveSource.properties[name]&&liveSource.properties[name].includes(shape),
    'Simple activity emitted an undeclared property/shape: '+name+'/'+shape);
context.__request={category:'activity',item:'card'};
const completed = {ACTIVITY_REVISION:revision+3, ACTIVITY_STATE:'completed',
  ACTIVITY_CLOCK:'none', ACTIVITY_DEADLINE:0, ACTIVITY_OBSERVED_AT:NOW_SECONDS,
  ACTIVITY_VALID_UNTIL:NOW_SECONDS+60, ACTIVITY_SHOW_TERMINAL:1};
assert.equal(publishNative(completed).ACTIVITY_ACCEPTED_REVISION, revision+3);
vm.runInContext('__handler(__request, __respond)', context, {timeout:1000});
assert.equal(response.instances[0].properties.show_terminal.boolean.value, true);
assert.equal(publishNative({...completed, ACTIVITY_REVISION:revision+4,
  ACTIVITY_SHOW_TERMINAL:0}).ACTIVITY_ACCEPTED_REVISION, revision+4);
vm.runInContext('__handler(__request, __respond)', context, {timeout:1000});
assert.equal(response.instances[0].properties.state.shortText.text, 'completed');
assert.equal(response.instances[0].properties.show_terminal.boolean.value, false);
assert.ok(publishNative(completed).ACTIVITY_ERROR, 'Stale completion resurrected an acknowledged result');
context.__request={category:'activity',item:'live'};
vm.runInContext('__handler(__request, __respond)',context,{timeout:1000});
assert.equal(response.instances[0].properties.state.shortText.text,'completed');
assert.equal(response.instances[0].properties.show_terminal.boolean.value,false);
assert.equal(response.instances[0].properties.observed_at.timestamp.value,NOW_SECONDS);
const cold = vm.createContext({Pebble:{registerSourceHandler(callback){handler=callback;}},
  localStorage:storage, Date:FixedDate, console});
vm.runInContext(fs.readFileSync(path.join(root, appinfo.plugin.script), 'utf8'), cold,
  {filename:'cold-acknowledged-plugin.js',timeout:1000});
cold.__handler=handler;cold.__request={category:'activity',item:'live'};
cold.__respond={data(value){response=value;},error(code){throw new Error(code);}};
vm.runInContext('__handler(__request, __respond)',cold,{timeout:1000});
assert.equal(response.instances[0].properties.show_terminal.boolean.value, false);
assert.equal(response.instances[0].properties.schema_version.numericValue.value,3);
cold.__request={category:'activity',item:'card'};
vm.runInContext('__handler(__request, __respond)',cold,{timeout:1000});
assert.equal(response.instances[0].properties.show_terminal.boolean.value,false);
async function verifySummary() {
  const declaration = appinfo.plugin.sources.find(item =>
    item.category === 'activity' && item.items.includes('summary'));
  assert.ok(declaration, 'PBW does not declare activity/summary');
  cold.__request = {category:'activity', item:'summary'};
  response = undefined;
  await vm.runInContext('__handler(__request, __respond)', cold, {timeout:1000});
  assert.ok(response, 'Cold summary source did not reply');
  assert.equal(response.instances[0].instanceId, '@snapshot');
  assert.equal(response.instances[0].properties.activity_count.numericValue.value, 1,
    'Acknowledged first slot must be omitted while the other timer remains');
  assert.equal(response.instances[1].instanceId, 'second-slot:2');
  const properties = response.instances[1].properties;
  assert.equal(properties.schema_version.numericValue.value, 4);
  assert.equal(properties.revision.numericValue.value, revision + 2);
  assert.equal(properties.observed_at.timestamp.value, observedAt);
  assert.equal(properties.valid_until.timestamp.value, validUntil);
  assert.equal(properties.unconfirmed.boolean.value, true);
  const descriptor = JSON.parse(properties.content.longText.text);
  assert.equal(descriptor.type, 'standard');
  assert.equal(descriptor.headline.mode, 'countdown');
  assert.equal(descriptor.headline.deadline, deadline);
  assert.equal(descriptor.detail, undefined);
  assert.deepEqual(JSON.parse(properties.open_app.longText.text), {watchAppUuid:appinfo.uuid});
  assert.equal(properties.actions, undefined, 'No invented native Timer controls');
  for (const instance of response.instances) {
    for (const [name, shapes] of Object.entries(instance.properties)) {
      for (const shape of Object.keys(shapes)) {
        assert.ok(declaration.properties[name] && declaration.properties[name].includes(shape),
          'Summary emitted an undeclared property/shape: ' + name + '/' + shape);
      }
    }
  }
  const sequence = response.instances[0].properties.snapshot_sequence.numericValue.value;
  await vm.runInContext('__handler(__request, __respond)', cold, {timeout:1000});
  assert.ok(response.instances[0].properties.snapshot_sequence.numericValue.value > sequence);
  assert.equal(response.instances[1].properties.revision.numericValue.value, revision + 2);
  await verifyReset();
  console.log('Packaged Timer PKJS, numeric keys, schema 4 owner snapshots/navigation, compatibility sources, multi-slot replacement, acknowledgement, Reset withdrawal/new-run Start and cold reload passed.');
}

async function verifyReset() {
  async function readCold(item) {
    let callback,answer;
    const isolated=vm.createContext({Pebble:{registerSourceHandler(handler){callback=handler;}},
      localStorage:storage,Date:FixedDate,console});
    vm.runInContext(fs.readFileSync(path.join(root,appinfo.plugin.script),'utf8'),isolated,{timeout:1000});
    isolated.__handler=callback;isolated.__request={category:'activity',item};
    isolated.__respond={data(value){answer=value;},error(code){throw Error(code);}};
    await vm.runInContext('__handler(__request,__respond)',isolated,{timeout:1000});
    assert.ok(answer,'Cold reset source did not reply: '+item);return answer.instances;
  }
  for(const [index,state] of ['active','paused'].entries()) {
    const id='packaged-reset-'+state+':1',firstRevision=revision+5+index*4;
    const active={ACTIVITY_SLOT:0,ACTIVITY_RUN:id,ACTIVITY_REVISION:firstRevision,ACTIVITY_STATE:state,
      ACTIVITY_CLOCK:'deadline',ACTIVITY_OBSERVED_AT:NOW_SECONDS,ACTIVITY_VALID_UNTIL:NOW_SECONDS+660,
      ACTIVITY_DEADLINE:state==='active'?NOW_SECONDS+600:0,ACTIVITY_DURATION:state==='paused'?600:0,
      ACTIVITY_TOTAL_DURATION:600,ACTIVITY_SHOW_TERMINAL:0};
    assert.equal(publishNative(active).ACTIVITY_ACCEPTED_REVISION,firstRevision);
    const before=await readCold('summary');assert.equal(before[0].properties.activity_count.numericValue.value,2);
    const reset={...active,ACTIVITY_REVISION:firstRevision+1,ACTIVITY_STATE:'cancelled',ACTIVITY_CLOCK:'none',
      ACTIVITY_DEADLINE:0,ACTIVITY_DURATION:0};
    assert.equal(publishNative(reset).ACTIVITY_ACCEPTED_REVISION,firstRevision+1);
    const after=await readCold('summary');
    assert.equal(after[0].properties.activity_count.numericValue.value,1);
    assert.deepEqual(Array.from(after.slice(1),instance=>instance.instanceId),['second-slot:2']);
    assert.ok(after[0].properties.snapshot_sequence.numericValue.value>before[0].properties.snapshot_sequence.numericValue.value);
    for(const item of ['live','card','session']) {
      const instances=await readCold(item),ready=instances.find(instance=>instance.instanceId===id).properties;
      assert.equal(ready.state.shortText.text,'cancelled');
      assert.equal(ready.observed_at.timestamp.value,NOW_SECONDS);assert.equal(ready.valid_until.timestamp.value,NOW_SECONDS+660);
      if(item!=='session')assert.equal(ready.show_terminal.boolean.value,false);
      const sibling=instances.find(instance=>instance.instanceId==='second-slot:2').properties;
      assert.equal(sibling.state.shortText.text,'active');assert.equal(sibling.revision.numericValue.value,revision+2);
      assert.equal(sibling.observed_at.timestamp.value,observedAt);assert.equal(sibling.valid_until.timestamp.value,validUntil);
    }
    assert.equal(publishNative(active).ACTIVITY_ERROR,'STALE_REVISION');
    assert.equal(publishNative({...active,ACTIVITY_REVISION:firstRevision+2}).ACTIVITY_ERROR,'TERMINAL_RUN');
    const started={...active,ACTIVITY_RUN:'packaged-reset-'+state+':2',ACTIVITY_REVISION:firstRevision+2,
      ACTIVITY_STATE:'active',ACTIVITY_DEADLINE:NOW_SECONDS+600,ACTIVITY_DURATION:0};
    assert.equal(publishNative(started).ACTIVITY_ACCEPTED_REVISION,firstRevision+2);
    const resumed=await readCold('summary');
    assert.deepEqual(Array.from(resumed.slice(1),instance=>instance.instanceId),[started.ACTIVITY_RUN,'second-slot:2']);
    assert.equal(JSON.parse(resumed[1].properties.content.longText.text).headline.deadline,NOW_SECONDS+600);
  }
}
verifySummary().catch(error => { console.error(error); process.exitCode = 1; });

// Execute both actual optimized writers with separate Android DOM/native stores.
function verifyAppearance() {
  const key='timer-appearance-settings-v1',native=new Map(),dom=new Map(),events={},sent=[];
  const nativeStorage={getItem:k=>native.get(k)??null,setItem:(k,v)=>native.set(k,v)};
  const liveStorage={getItem:k=>dom.get(k)??null,setItem(k,v){dom.set(k,v);native.set(k,v);}};
  let config;
  vm.runInNewContext(fs.readFileSync(path.join(root,'plugin.js'),'utf8'),{
    Pebble:{registerSourceHandler(){},registerConfigHandler(fn){config=fn;}},localStorage:nativeStorage,Date:FixedDate});
  vm.runInNewContext(fs.readFileSync(path.join(root,'pebble-js-app.js'),'utf8'),{
    Pebble:{addEventListener(name,fn){events[name]=fn;},sendAppMessage(message,ok){sent.push(message);ok();}},
    localStorage:liveStorage,Date:FixedDate,setTimeout(){throw Error('Unexpected retry');},clearTimeout(){}});
  const selected={version:1,theme:'dark'};
  let receipt;events.configmessage({data:{type:'saveSettings',settings:selected},respond(value){receipt=JSON.parse(JSON.stringify(value));}});
  assert.deepEqual(receipt,{ok:true,settings:selected});
  assert.equal(native.get(key),JSON.stringify(selected));assert.equal(dom.get(key),native.get(key));
  assert.equal(sent.length,1);assert.deepEqual(JSON.parse(JSON.stringify(sent[0])),{TIMER_APPEARANCE:1,TIMER_APPEARANCE_VERSION:1});
  native.clear();for(const [k,v]of dom)native.set(k,v);
  config({type:'getSettings'},value=>receipt=JSON.parse(JSON.stringify(value)));
  assert.deepEqual(receipt,{ok:true,settings:selected,needsRepair:false});
  assert.equal(native.size,1,'Appearance writes must not create timer snapshots');
  console.log('Packaged Timer settings: exact active-writer confirmation, Android teardown and independent appearance transport passed');
}
verifyAppearance();
