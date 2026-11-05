'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const helpers = require('pebble-activities/publisher');
const library = require('pebble-activities');
const uuid = '50e9ef12-a5ab-4369-a3b5-3447bfba5151';
const now = 1800000000;

function storage(failing) {
  const values = new Map();
  return {
    getItem(key) { return values.has(key) ? values.get(key) : null; },
    setItem(key, value) {
      if (failing) throw new Error('test storage failure');
      values.set(key, String(value));
    }
  };
}

function runtime(file, shared, at = now, options = {}) {
  const events = {}, sent = [];
  let source;
  const pebble = {
    addEventListener(name, fn) { events[name] = fn; },
    sendAppMessage(message, success, failure) {
      sent.push(JSON.parse(JSON.stringify(message)));
      if (options.send) return options.send(message, success, failure);
      success();
    },
    registerSourceHandler(fn) { source = fn; }
  };
  const api = {
    createPublisher(options) { return helpers.createPublisher(options); },
    createProvider(options) { return helpers.createProvider({...options, now: () => at * 1000}); },
    readSnapshots(options) { return helpers.readSnapshots(options); }
  };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '..', file), 'utf8'), {
    Pebble: pebble, localStorage: shared,
    setTimeout: options.later, clearTimeout: options.cancel,
    require(name) {
      if (name === 'pebble-activities/summary') {
        return {...library.summary, defineSnapshotProvider(options) {
          return library.summary.defineSnapshotProvider({...options, now: () => at * 1000});
        }};
      }
      assert.equal(name, 'pebble-activities/publisher');
      return api;
    }
  }, {filename: file});
  return {events, sent, source};
}

async function readSummary(shared, at = now) {
  const plugin = runtime('src/plugin/index.js', shared, at);
  let answer, error;
  await plugin.source({category:'activity', item:'summary'}, {
    data(value) { answer = value; }, error(code) { error = code; }
  });
  assert.equal(error, undefined);
  assert.ok(answer);
  return answer;
}

test('schema 4 cold source preserves owner clock and exposes only app navigation', async () => {
  const shared = storage();
  runtime('src/pkjs/index.js', shared).events.appmessage({payload:snapshot()});
  const first = await readSummary(shared);
  assert.equal(first.instances[0].instanceId, '@snapshot');
  assert.equal(first.instances[0].properties.activity_count.numericValue.value, 1);
  const p = first.instances[1].properties;
  assert.equal(p.schema_version.numericValue.value, 4);
  assert.equal(p.revision.numericValue.value, 4);
  assert.equal(p.observed_at.timestamp.value, now);
  assert.equal(p.valid_until.timestamp.value, now + 660);
  assert.equal(p.unconfirmed.boolean.value, true);
  const content = JSON.parse(p.content.longText.text);
  assert.equal(content.type, 'standard');
  assert.deepEqual(content.headline, {type:'clock', mode:'countdown', deadline:now + 600});
  assert.equal(content.icon, 'timer');
  assert.deepEqual(JSON.parse(p.open_app.longText.text), {watchAppUuid:uuid});
  assert.equal(p.actions, undefined, 'Timer has no native command route');
  const second = await readSummary(shared, now + 1);
  assert.ok(second.instances[0].properties.snapshot_sequence.numericValue.value >
    first.instances[0].properties.snapshot_sequence.numericValue.value);
  assert.equal(second.instances[1].properties.revision.numericValue.value, 4);
  assert.equal(second.instances[1].properties.observed_at.timestamp.value, now);
});

test('schema 4 empty withdrawal after expiry keeps a newer complete-response header', async () => {
  const shared = storage();
  runtime('src/pkjs/index.js', shared).events.appmessage({payload:snapshot()});
  const active = await readSummary(shared);
  const expired = await readSummary(shared, now + 661);
  assert.deepEqual(Array.from(expired.instances, x => x.instanceId), ['@snapshot']);
  assert.equal(expired.instances[0].properties.activity_count.numericValue.value, 0);
  assert.ok(expired.instances[0].properties.snapshot_sequence.numericValue.value >
    active.instances[0].properties.snapshot_sequence.numericValue.value);
});

test('summary omits exact native duration labels and preserves meaningful instance names', async () => {
  for (const label of ['1 min timer', '2 h timer', '00:05 timer', '1:00:05 timer', '23:59:59 timer']) {
    const shared = storage();
    runtime('src/pkjs/index.js', shared).events.appmessage({payload:snapshot({
      ACTIVITY_NAME:label, ACTIVITY_FOOTER:label, ACTIVITY_TOTAL_DURATION:61
    })});
    const result = await readSummary(shared);
    const properties = result.instances[1].properties;
    assert.equal(JSON.parse(properties.content.longText.text).name, undefined, label);
    assert.equal(properties.app_name.longText.text, 'Timer');
  }
  for (const label of ['Tea', 'Pasta', '60 min timer', '1:99:05 timer']) {
    const shared = storage();
    runtime('src/pkjs/index.js', shared).events.appmessage({payload:snapshot({ACTIVITY_NAME:label,ACTIVITY_FOOTER:label})});
    const result = await readSummary(shared);
    assert.equal(JSON.parse(result.instances[1].properties.content.longText.text).name,label);
  }
});

test('schema 4 pause expiry and completion acknowledgement survive cold plugin requests', async () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_STATE:'paused', ACTIVITY_DEADLINE:0,
    ACTIVITY_DURATION:480, ACTIVITY_VALID_UNTIL:now + 300})});
  const paused = await readSummary(shared, now + 299);
  assert.deepEqual(JSON.parse(paused.instances[1].properties.content.longText.text).headline,
    {type:'clock', mode:'countdown', seconds:480});
  assert.equal((await readSummary(shared, now + 300)).instances.length, 1);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_REVISION:5, ACTIVITY_STATE:'completed',
    ACTIVITY_CLOCK:'none', ACTIVITY_DEADLINE:0, ACTIVITY_SHOW_TERMINAL:1,
    ACTIVITY_VALID_UNTIL:now + 60})});
  const completed = await readSummary(shared);
  assert.equal(JSON.parse(completed.instances[1].properties.content.longText.text).result, true);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_REVISION:6, ACTIVITY_STATE:'completed',
    ACTIVITY_CLOCK:'none', ACTIVITY_DEADLINE:0, ACTIVITY_SHOW_TERMINAL:0,
    ACTIVITY_VALID_UNTIL:now + 60})});
  assert.equal((await readSummary(shared)).instances.length, 1);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_REVISION:5})});
  assert.equal(pkjs.sent.at(-1).ACTIVITY_ERROR, 'STALE_REVISION');
  assert.equal((await readSummary(shared)).instances.length, 1);
});

function snapshot(changes) {
  return {
    ACTIVITY_SCHEMA_VERSION: 1,
    ACTIVITY_RUN: '6b49d200:00000001',
    ACTIVITY_REVISION: 4,
    ACTIVITY_STATE: 'active',
    ACTIVITY_KIND: 'timer',
    ACTIVITY_CLOCK: 'deadline',
    ACTIVITY_NAME: 'Timer',
    ACTIVITY_MAIN: '',
    ACTIVITY_FOOTER: 'Timer',
    ACTIVITY_OBSERVED_AT: now,
    ACTIVITY_VALID_UNTIL: now + 660,
    ACTIVITY_DEADLINE: now + 600,
    ACTIVITY_DURATION: 0,
    ACTIVITY_CLOCK_AT: 0,
    ACTIVITY_PRIVATE: 0,
    ACTIVITY_PUBLISH_SESSION: 7,
    ...changes
  };
}

test('native payload is saved by PKJS and read by a separate cold plugin runtime', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.ready();
  assert.deepEqual(pkjs.sent.shift(), {ACTIVITY_REQUEST: 1, ACTIVITY_LAST_REVISION: 0});
  pkjs.events.appmessage({payload: snapshot()});
  assert.equal(pkjs.sent[0].ACTIVITY_ACCEPTED_RUN, '6b49d200:00000001');
  assert.equal(pkjs.sent[0].ACTIVITY_ACCEPTED_REVISION, 4);
  assert.equal(pkjs.sent[0].ACTIVITY_PUBLISH_SESSION, 7);

  // This is a new plugin context after the timer's PKJS context has gone away.
  const plugin = runtime('src/plugin/index.js', shared);
  let answer;
  plugin.source({category: 'activity', item: 'session'}, {data(value) { answer = value; }});
  assert.equal(answer.instances.length, 1);
  const p = answer.instances[0].properties;
  assert.equal(p.deadline.timestamp.value, now + 600);
  assert.equal(p.observed_at.timestamp.value, now);
  assert.equal(p.valid_until.timestamp.value, now + 660);
  assert.equal(p.unconfirmed.boolean.value, true);

  const restart = runtime('src/pkjs/index.js', shared);
  restart.events.ready();
  assert.equal(restart.sent[0].ACTIVITY_LAST_REVISION, 4);
});

test('pause and cancel update the saved run without deriving completion on the phone', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.appmessage({payload: snapshot()});
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_REVISION: 5, ACTIVITY_STATE: 'paused',
    ACTIVITY_DEADLINE: 0, ACTIVITY_DURATION: 480})});
  let source = helpers.createProvider({storage: shared, pluginUuid: uuid, now: () => (now + 120) * 1000}).source({});
  assert.equal(source.instances[0].properties.duration.numericValue.value, 480);
  assert.equal(source.instances[0].properties.state.shortText.text, 'paused');
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_REVISION: 6, ACTIVITY_STATE: 'cancelled',
    ACTIVITY_CLOCK: 'none', ACTIVITY_DEADLINE: 0})});
  source = helpers.createProvider({storage: shared, pluginUuid: uuid, now: () => (now + 120) * 1000}).source({});
  assert.equal(source.instances[0].properties.state.shortText.text, 'cancelled');
  // An older queued running snapshot must not overwrite that cancellation.
  pkjs.events.appmessage({payload: snapshot()});
  assert.equal(pkjs.sent.at(-1).ACTIVITY_ERROR, 'STALE_REVISION');
  assert.equal(pkjs.sent.at(-1).ACTIVITY_RUN, '6b49d200:00000001');
  assert.equal(pkjs.sent.at(-1).ACTIVITY_REVISION, 4);
});

test('storage failure never emits a successful application receipt', () => {
  const pkjs = runtime('src/pkjs/index.js', storage(true));
  pkjs.events.appmessage({payload: snapshot()});
  assert.equal(pkjs.sent.length, 1);
  assert.equal(pkjs.sent[0].ACTIVITY_ERROR, 'STORAGE_FAILED');
  assert.equal(pkjs.sent[0].ACTIVITY_ACCEPTED_REVISION, undefined);
});

test('polling does not renew the original native lease', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.appmessage({payload: snapshot()});
  const provider = helpers.createProvider({storage: shared, pluginUuid: uuid, now: () => (now + 661) * 1000});
  assert.deepEqual(provider.source({}).instances, []);
});

test('timer slots survive cold reads and replace old offline runs independently', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_SLOT:0})});
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_SLOT:7, ACTIVITY_RUN:'timer-eight:1',
    ACTIVITY_REVISION:5, ACTIVITY_DEADLINE:now + 120})});
  const read = () => {
    let answer;
    runtime('src/plugin/index.js', shared).source({category:'activity', item:'session'},
      {data(value) { answer = value; }});
    return answer.instances;
  };
  assert.deepEqual(Array.from(read(), item => item.instanceId), ['6b49d200:00000001', 'timer-eight:1']);
  // Reusing a slot while disconnected must replace its previous run without
  // requiring an intermediate cancellation to have reached the phone.
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_SLOT:7, ACTIVITY_RUN:'timer-eight:2',
    ACTIVITY_REVISION:6, ACTIVITY_DEADLINE:now + 180})});
  assert.deepEqual(Array.from(read(), item => item.instanceId), ['6b49d200:00000001', 'timer-eight:2']);
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_SLOT:0, ACTIVITY_REVISION:7,
    ACTIVITY_STATE:'paused', ACTIVITY_DEADLINE:0, ACTIVITY_DURATION:321})});
  assert.equal(read()[0].properties.duration.numericValue.value, 321);
  assert.equal(read()[1].properties.deadline.timestamp.value, now + 180);
  const restart = runtime('src/pkjs/index.js', shared);
  restart.events.ready();
  assert.equal(restart.sent[0].ACTIVITY_LAST_REVISION, 7);
});

test('invalid native slot cannot acknowledge or replace another timer', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_SLOT:0})});
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_SLOT:8, ACTIVITY_REVISION:5})});
  assert.ok(pkjs.sent.at(-1).ACTIVITY_ERROR);
  assert.equal(pkjs.sent.at(-1).ACTIVITY_ACCEPTED_REVISION, undefined);
  const provider = helpers.createProvider({storage:shared, pluginUuid:uuid, now:() => now * 1000});
  assert.equal(provider.source({}).instances.length, 1);
  assert.equal(provider.source({}).instances[0].properties.revision.numericValue.value, 4);
});

test('one activity manifest declaration covers summaries and every compatibility source', () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'package.json'), 'utf8'));
  assert.equal(manifest.pebble.messageKeys.ACTIVITY_SLOT, 1021);
  assert.equal(manifest.pebble.messageKeys.ACTIVITY_TOTAL_DURATION, 1022);
  assert.equal(manifest.pebble.messageKeys.ACTIVITY_SHOW_TERMINAL, 1023);
  const sources = manifest.pebble.plugin.sources.filter(source => source.category === 'activity');
  assert.equal(sources.length, 1, 'Each source category must be declared once');
  const card = sources[0];
  assert.deepEqual(card.items, ['summary', 'live', 'session', 'card']);
  for (const [property, shapes] of Object.entries(library.activity.SOURCE_PROPERTIES)) {
    assert.deepEqual(card.properties[property], shapes,
      'Manifest must declare every simple source shape: ' + property);
  }
  for (const [property, shapes] of Object.entries(library.summary.SOURCE_PROPERTIES)) {
    assert.deepEqual(card.properties[property], shapes,
      'Manifest must declare every summary source shape: ' + property);
  }
  assert.deepEqual(card.properties.deadline, ['timestamp']);
  assert.deepEqual(card.properties.duration, ['numericValue']);
  assert.deepEqual(card.properties.clock_at, ['timestamp']);
  assert.deepEqual(card.properties.main_base, ['numericValue']);
  assert.deepEqual(card.properties.support_anchor, ['timestamp']);
});

test('cold simple Timer activity supplies one countdown without mandatory display labels or elapsed detail', () => {
  const shared = storage();
  runtime('src/pkjs/index.js', shared).events.appmessage({payload:snapshot({ACTIVITY_TOTAL_DURATION:900})});
  function read(at) {
    let answer;
    runtime('src/plugin/index.js',shared,at).source({category:'activity',item:'live'},
      {data(value){answer=value;},error(code){assert.fail(code);}});
    return answer;
  }
  const first=read(now),later=read(now+120),properties=first.instances[0].properties;
  assert.equal(properties.schema_version.numericValue.value,3);
  assert.equal(properties.app_name.longText.text,'Timer');
  assert.equal(properties.main_label,undefined);
  assert.equal(properties.support_type,undefined);
  assert.equal(properties.meter_kind,undefined);
  assert.equal(properties.detail,undefined);
  const model=library.activity.sourceCard(first.instances[0],uuid);
  assert.ok(model);
  assert.equal(model.content.clock,'countdown');
  assert.equal(model.content.deadline,now+600);
  assert.equal(model.detail,null);
  assert.equal(model.observedAt,now);
  assert.equal(model.validUntil,now+660);
  assert.equal(model.unconfirmed,true);
  assert.equal(model.revision,4);
  assert.deepEqual(later.instances,first.instances,'Polling must not change any source observation, deadline or revision');
  const view=library.activity.toView(model);
  assert.equal(view.main.type,'clock');
  assert.equal(view.main.baseSeconds,600);
  assert.equal(view.main.anchorAt,now);
  assert.equal(view.support,null);
  assert.equal(view.meter,null);
  const declaration=JSON.parse(fs.readFileSync(path.join(__dirname,'..','package.json'),'utf8')).pebble.plugin.sources[0];
  for(const [name,value] of Object.entries(properties))for(const shape of Object.keys(value))
    assert.ok(declaration.properties[name].includes(shape),'Undeclared simple source shape: '+name+'/'+shape);
});

test('simple Timer keeps a paused countdown fixed through the original five-minute window', () => {
  const shared=storage(),pkjs=runtime('src/pkjs/index.js',shared);
  const paused=snapshot({ACTIVITY_REVISION:5,ACTIVITY_STATE:'paused',ACTIVITY_DEADLINE:0,
    ACTIVITY_DURATION:480,ACTIVITY_TOTAL_DURATION:600,ACTIVITY_VALID_UNTIL:now+300});
  pkjs.events.appmessage({payload:paused});
  function read(at){
    let answer;runtime('src/plugin/index.js',shared,at).source({category:'activity',item:'live'},
      {data(value){answer=value;},error(code){assert.fail(code);}});return answer.instances;
  }
  const before=read(now+299),model=library.activity.sourceCard(before[0],uuid);
  assert.equal(model.state,'paused');
  assert.equal(model.content.clock,'countdown');
  assert.equal(model.content.seconds,480);
  assert.equal(model.observedAt,now);
  assert.equal(model.validUntil,now+300);
  assert.equal(read(now+300).length,0);
  pkjs.events.appmessage({payload:{...paused,ACTIVITY_REVISION:6}});
  assert.equal(read(now+3600).length,0,'Native reconfirmation cannot renew pause visibility');
  assert.equal(helpers.createPublisher({storage:shared,pluginUuid:uuid,maxRecords:8}).lastRevision(),6);
});

test('simple Timer completion keeps the acknowledged hidden tombstone through cold reads', () => {
  const shared=storage(),pkjs=runtime('src/pkjs/index.js',shared);
  const completed=snapshot({ACTIVITY_STATE:'completed',ACTIVITY_CLOCK:'none',ACTIVITY_DEADLINE:0,
    ACTIVITY_OBSERVED_AT:now,ACTIVITY_VALID_UNTIL:now+60,ACTIVITY_SHOW_TERMINAL:1});
  pkjs.events.appmessage({payload:completed});
  function read(at){
    let answer;runtime('src/plugin/index.js',shared,at).source({category:'activity',item:'live'},
      {data(value){answer=value;},error(code){assert.fail(code);}});return answer.instances;
  }
  assert.equal(read(now+1)[0].properties.show_terminal.boolean.value,true);
  pkjs.events.appmessage({payload:{...completed,ACTIVITY_REVISION:5,ACTIVITY_SHOW_TERMINAL:0}});
  const hidden=read(now+2)[0];
  assert.equal(hidden.properties.state.shortText.text,'completed');
  assert.equal(hidden.properties.show_terminal.boolean.value,false);
  assert.equal(hidden.properties.observed_at.timestamp.value,now);
  const model=library.activity.sourceCard(hidden,uuid);
  assert.equal(model.showTerminal,false);
  assert.equal(model.revision,5);
  pkjs.events.appmessage({payload:completed});
  assert.equal(pkjs.sent.at(-1).ACTIVITY_ERROR,'STALE_REVISION');
  assert.equal(read(now+61).length,0);
});

test('cold Timer card is compact while detailed providers retain elapsed from the same observation', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.appmessage({payload: snapshot({ACTIVITY_TOTAL_DURATION: 900})});
  let answer;
  runtime('src/plugin/index.js', shared).source({category:'activity', item:'card'}, {data(value) { answer = value; }});
  assert.equal(answer.instances.length, 1);
  const p = answer.instances[0].properties;
  assert.equal(p.schema_version.numericValue.value, 2);
  assert.equal(p.main_type.shortText.text, 'clock');
  assert.equal(p.main_clock.shortText.text, 'deadline');
  assert.equal(p.main_base.numericValue.value, 600);
  assert.equal(p.main_anchor.timestamp.value, now);
  assert.equal(p.main_running.boolean.value, true);
  assert.equal(p.main_label.longText.text, 'Timer');
  assert.equal(p.caption.longText.text, 'Timer');
  assert.equal(p.support_type, undefined);
  assert.equal(p.unconfirmed.boolean.value, true);
  const later = helpers.createProvider({storage:shared,pluginUuid:uuid,now:() => (now+120)*1000}).source({category:'activity',item:'card'}).instances[0].properties;
  assert.equal(later.support_type.shortText.text, 'clock');
  assert.equal(later.support_clock.shortText.text, 'elapsed');
  assert.equal(later.support_running.boolean.value, true);
  assert.equal(later.observed_at.timestamp.value, now);
  assert.equal(later.support_observed_at.timestamp.value, now);
  assert.equal(later.support_valid_until.timestamp.value, now+660);
  assert.equal(later.support_base.numericValue.value, 300);
  assert.equal(later.support_anchor.timestamp.value, now);
});

test('paused card clocks stay fixed and a changed duration is a new source revision', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_TOTAL_DURATION:600})});
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_REVISION:5,ACTIVITY_STATE:'paused',
    ACTIVITY_OBSERVED_AT:now+120,ACTIVITY_DEADLINE:0,ACTIVITY_DURATION:480,ACTIVITY_TOTAL_DURATION:600})});
  let provider = helpers.createProvider({storage:shared,pluginUuid:uuid,now:() => (now+300)*1000});
  let p = provider.source({category:'activity',item:'card'}).instances[0].properties;
  assert.equal(p.main_base.numericValue.value, 480);
  assert.equal(p.main_running.boolean.value, false);
  assert.equal(p.support_base.numericValue.value, 120);
  assert.equal(p.support_running.boolean.value, false);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_REVISION:6,ACTIVITY_STATE:'paused',
    ACTIVITY_OBSERVED_AT:now+300,ACTIVITY_DEADLINE:0,ACTIVITY_DURATION:300,ACTIVITY_TOTAL_DURATION:300})});
  p = provider.source({category:'activity',item:'card'}).instances[0].properties;
  assert.equal(p.support_base.numericValue.value, 0);
  assert.equal(p.revision.numericValue.value, 6);
});

test('old native snapshots still work without inventing an elapsed value', () => {
  const shared = storage();
  runtime('src/pkjs/index.js',shared).events.appmessage({payload:snapshot()});
  const p=helpers.createProvider({storage:shared,pluginUuid:uuid,now:() => now*1000})
    .source({category:'activity',item:'card'}).instances[0].properties;
  assert.equal(p.schema_version.numericValue.value,2);
  assert.equal(p.main_base.numericValue.value,600);
  assert.equal(p.support_type,undefined);
});

test('conflicting duration metadata cannot mutate an accepted revision', () => {
  const shared=storage();
  const pkjs=runtime('src/pkjs/index.js',shared);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_TOTAL_DURATION:900})});
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_TOTAL_DURATION:1200})});
  assert.ok(pkjs.sent.at(-1).ACTIVITY_ERROR);
  assert.equal(pkjs.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,undefined);
  const p=helpers.createProvider({storage:shared,pluginUuid:uuid,now:() => now*1000})
    .source({category:'activity',item:'card'}).instances[0].properties;
  assert.equal(p.support_base.numericValue.value,300);
});

test('only native completion can retain a bounded result card', () => {
  const shared=storage();
  const pkjs=runtime('src/pkjs/index.js',shared);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_TOTAL_DURATION:600})});
  let provider=helpers.createProvider({storage:shared,pluginUuid:uuid,now:() => (now+600)*1000});
  let p=provider.source({category:'activity',item:'card'}).instances[0].properties;
  assert.equal(p.state.shortText.text,'active'); /* Phone clock zero cannot complete it. */
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_REVISION:5,ACTIVITY_STATE:'completed',ACTIVITY_CLOCK:'none',
    ACTIVITY_DEADLINE:0,ACTIVITY_OBSERVED_AT:now+600,ACTIVITY_VALID_UNTIL:now+660,ACTIVITY_TOTAL_DURATION:600})});
  p=provider.source({category:'activity',item:'card'}).instances[0].properties;
  assert.equal(p.state.shortText.text,'completed');
  assert.equal(p.show_terminal.boolean.value,true);
  assert.equal(p.main_retain.boolean.value,true); /* A cached result is still last-reported. */
  assert.equal(p.valid_until.timestamp.value,now+660);
  provider=helpers.createProvider({storage:shared,pluginUuid:uuid,now:() => (now+661)*1000});
  assert.equal(provider.source({category:'activity',item:'card'}).instances.length,0);
});

test('both source schemas hide an expired pause without losing its saved snapshot', () => {
  const shared = storage();
  const pkjs = runtime('src/pkjs/index.js', shared);
  const paused = snapshot({ACTIVITY_STATE:'paused', ACTIVITY_DEADLINE:0,
    ACTIVITY_DURATION:480, ACTIVITY_TOTAL_DURATION:600, ACTIVITY_VALID_UNTIL:now+300});
  pkjs.events.appmessage({payload:paused});
  function read(item, at) {
    let answer;
    runtime('src/plugin/index.js',shared,at).source({category:'activity',item},
      {data(value) { answer=value; }});
    return answer.instances;
  }
  for (const item of ['session','card']) {
    const visible = read(item,now+299.999);
    assert.equal(visible.length,1);
    assert.equal(visible[0].properties.observed_at.timestamp.value,now);
    assert.equal(visible[0].properties.valid_until.timestamp.value,now+300);
    if (item==='card') {
      assert.equal(visible[0].properties.main_valid_until.timestamp.value,now+300);
      assert.equal(visible[0].properties.support_type,undefined);
    }
    assert.equal(read(item,now+300).length,0);
    assert.equal(read(item,now+3600).length,0);
  }
  // Native reconfirm advances the revision without renewing the pause lease.
  pkjs.events.appmessage({payload:{...paused,ACTIVITY_REVISION:5}});
  assert.equal(pkjs.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,5);
  for (const item of ['session','card']) assert.equal(read(item,now+3600).length,0);
  assert.equal(helpers.createPublisher({storage:shared,pluginUuid:uuid,maxRecords:8}).lastRevision(),5);
});

test('resuming a hidden pause republishes both source schemas with a fresh countdown', () => {
  const shared=storage();
  const pkjs=runtime('src/pkjs/index.js',shared);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_STATE:'paused',ACTIVITY_DEADLINE:0,
    ACTIVITY_DURATION:480,ACTIVITY_TOTAL_DURATION:600,ACTIVITY_VALID_UNTIL:now+300})});
  const resumedAt=now+3600;
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_REVISION:5,ACTIVITY_OBSERVED_AT:resumedAt,
    ACTIVITY_DEADLINE:resumedAt+480,ACTIVITY_VALID_UNTIL:resumedAt+540,ACTIVITY_TOTAL_DURATION:600})});
  for (const item of ['session','card']) {
    let answer;
    runtime('src/plugin/index.js',shared,resumedAt).source({category:'activity',item},
      {data(value) { answer=value; }});
    assert.equal(answer.instances.length,1);
    const p=answer.instances[0].properties;
    assert.equal(p.state.shortText.text,'active');
    assert.equal(p.observed_at.timestamp.value,resumedAt);
    assert.equal(p.valid_until.timestamp.value,resumedAt+540);
    if (item==='card') {
      assert.equal(p.main_running.boolean.value,true);
      assert.equal(p.main_base.numericValue.value,480);
      assert.equal(p.support_type,undefined);
    }
  }
});

test('Done hides a completed run durably without cancelling it or another timer', () => {
  const shared=storage();
  const pkjs=runtime('src/pkjs/index.js',shared);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_SLOT:0,ACTIVITY_TOTAL_DURATION:600,
    ACTIVITY_SHOW_TERMINAL:0})});
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_SLOT:1,ACTIVITY_RUN:'other:1',
    ACTIVITY_REVISION:5,ACTIVITY_TOTAL_DURATION:1200,ACTIVITY_SHOW_TERMINAL:0,
    ACTIVITY_DEADLINE:now+1200,ACTIVITY_VALID_UNTIL:now+1260})});
  const completed=snapshot({ACTIVITY_SLOT:0,ACTIVITY_REVISION:6,ACTIVITY_STATE:'completed',
    ACTIVITY_CLOCK:'none',ACTIVITY_DEADLINE:0,ACTIVITY_OBSERVED_AT:now+600,
    ACTIVITY_VALID_UNTIL:now+660,ACTIVITY_TOTAL_DURATION:600,ACTIVITY_SHOW_TERMINAL:1});
  pkjs.events.appmessage({payload:completed});
  function read(at) {
    let answer;
    runtime('src/plugin/index.js',shared,at).source({category:'activity',item:'card'},
      {data(value) { answer=value; }});
    return answer.instances;
  }
  assert.equal(read(now+601)[0].properties.show_terminal.boolean.value,true);
  pkjs.events.appmessage({payload:{...completed,ACTIVITY_REVISION:7,ACTIVITY_SHOW_TERMINAL:0}});
  let cards=read(now+602);
  assert.equal(cards[0].properties.state.shortText.text,'completed');
  assert.equal(cards[0].properties.show_terminal.boolean.value,false);
  assert.equal(cards[0].properties.revision.numericValue.value,7);
  assert.equal(cards[0].properties.observed_at.timestamp.value,now+600);
  assert.equal(cards[1].properties.state.shortText.text,'active');

  /* A cold phone handshake receives the saved acknowledgment and original
   * expiry. Delayed completion and repeated polling cannot revive the result. */
  const reopened=runtime('src/pkjs/index.js',shared,now+700);
  reopened.events.ready();
  assert.equal(reopened.sent[0].ACTIVITY_LAST_REVISION,7);
  reopened.events.appmessage({payload:{...completed,ACTIVITY_REVISION:8,ACTIVITY_SHOW_TERMINAL:0}});
  assert.equal(reopened.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,8);
  reopened.events.appmessage({payload:completed});
  assert.equal(reopened.sent.at(-1).ACTIVITY_ERROR,'STALE_REVISION');
  cards=read(now+700);
  assert.deepEqual(Array.from(cards,card=>card.instanceId),['other:1']);
  const full=helpers.createProvider({storage:shared,pluginUuid:uuid,now:()=>(now+602)*1000})
    .source({category:'activity',item:'card'}).instances;
  assert.equal(full[0].properties.show_terminal.boolean.value,false);
});

for (const state of ['active', 'paused']) test('Reset withdraws the '+state+' timer durably while preserving its sibling and requiring a new run to Start', async () => {
  const shared=storage(),pkjs=runtime('src/pkjs/index.js',shared);
  const original=snapshot({ACTIVITY_SLOT:0,ACTIVITY_STATE:state,ACTIVITY_TOTAL_DURATION:600,
    ACTIVITY_DEADLINE:state==='active'?now+480:0,ACTIVITY_DURATION:state==='paused'?480:0,
    ACTIVITY_VALID_UNTIL:now+(state==='active'?540:300),ACTIVITY_SHOW_TERMINAL:0});
  const sibling=snapshot({ACTIVITY_SLOT:1,ACTIVITY_RUN:'other:1',ACTIVITY_REVISION:5,
    ACTIVITY_TOTAL_DURATION:1200,ACTIVITY_DEADLINE:now+1200,ACTIVITY_VALID_UNTIL:now+1260,ACTIVITY_SHOW_TERMINAL:0});
  pkjs.events.appmessage({payload:original});pkjs.events.appmessage({payload:sibling});
  const before=await readSummary(shared);
  assert.equal(before.instances[0].properties.activity_count.numericValue.value,2);
  const reset={...original,ACTIVITY_REVISION:6,ACTIVITY_STATE:'cancelled',ACTIVITY_CLOCK:'none',
    ACTIVITY_OBSERVED_AT:now+1,ACTIVITY_VALID_UNTIL:Math.max(original.ACTIVITY_VALID_UNTIL,now+61),ACTIVITY_DEADLINE:0,ACTIVITY_DURATION:0,
    ACTIVITY_TOTAL_DURATION:600,ACTIVITY_SHOW_TERMINAL:0};
  pkjs.events.appmessage({payload:reset});assert.equal(pkjs.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,6);
  const hidden=await readSummary(shared,now+2);
  assert.equal(hidden.instances[0].properties.activity_count.numericValue.value,1);
  assert.ok(hidden.instances[0].properties.snapshot_sequence.numericValue.value>before.instances[0].properties.snapshot_sequence.numericValue.value);
  assert.deepEqual(Array.from(hidden.instances.slice(1),item=>item.instanceId),['other:1']);
  function compatibility(item,at=now+2){
    let response;runtime('src/plugin/index.js',shared,at).source({category:'activity',item},{data(value){response=value;},error(code){throw Error(code);}});
    return response.instances;
  }
  for (const item of ['live','session','card']) {
    const instances=compatibility(item),ready=instances.find(item=>item.instanceId===original.ACTIVITY_RUN).properties;
    assert.equal(ready.state.shortText.text,'cancelled');assert.equal(ready.revision.numericValue.value,6);
    assert.equal(ready.observed_at.timestamp.value,now+1);assert.equal(ready.valid_until.timestamp.value,reset.ACTIVITY_VALID_UNTIL);
    if(item!=='session')assert.equal(ready.show_terminal.boolean.value,false);
    const other=instances.find(item=>item.instanceId==='other:1').properties;
    assert.equal(other.state.shortText.text,'active');assert.equal(other.revision.numericValue.value,5);
    assert.equal(other.observed_at.timestamp.value,now);assert.equal(other.valid_until.timestamp.value,now+1260);
  }
  const reopened=runtime('src/pkjs/index.js',shared,now+2);reopened.events.ready();
  assert.equal(reopened.sent[0].ACTIVITY_LAST_REVISION,6);
  reopened.events.appmessage({payload:{...reset,ACTIVITY_REVISION:7}});
  assert.equal(reopened.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,7);
  reopened.events.appmessage({payload:original});assert.equal(reopened.sent.at(-1).ACTIVITY_ERROR,'STALE_REVISION');
  reopened.events.appmessage({payload:{...original,ACTIVITY_REVISION:8}});
  assert.equal(reopened.sent.at(-1).ACTIVITY_ERROR,'TERMINAL_RUN','Only explicit Start with a new run can resume a reset timer');
  assert.deepEqual(Array.from((await readSummary(shared,now+2)).instances.slice(1),item=>item.instanceId),['other:1']);
  const reloaded=compatibility('card').find(item=>item.instanceId===original.ACTIVITY_RUN).properties;
  assert.equal(reloaded.observed_at.timestamp.value,now+1);assert.equal(reloaded.valid_until.timestamp.value,reset.ACTIVITY_VALID_UNTIL);
  assert.deepEqual(Array.from(compatibility('live',reset.ACTIVITY_VALID_UNTIL+1),item=>item.instanceId),['other:1'],'Expired reset tombstone cannot return through polling');
  const started={...original,ACTIVITY_RUN:'6b49d200:00000002',ACTIVITY_REVISION:8,ACTIVITY_STATE:'active',ACTIVITY_CLOCK:'deadline',
    ACTIVITY_OBSERVED_AT:now+3,ACTIVITY_VALID_UNTIL:now+663,ACTIVITY_DEADLINE:now+603,ACTIVITY_DURATION:0};
  reopened.events.appmessage({payload:started});assert.equal(reopened.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,8);
  const active=await readSummary(shared,now+3);
  assert.deepEqual(Array.from(active.instances.slice(1),item=>item.instanceId),[started.ACTIVITY_RUN,'other:1']);
  assert.equal(JSON.parse(active.instances[1].properties.content.longText.text).headline.deadline,now+603);
});

test('failed Reset snapshot save leaves accepted activities intact and retries only when native sends again',async()=>{
  const shared=storage(),write=shared.setItem.bind(shared);let fail=false;
  shared.setItem=(key,value)=>{if(fail)throw Error('test reset storage failure');write(key,value);};
  const pkjs=runtime('src/pkjs/index.js',shared);
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_SLOT:0})});
  pkjs.events.appmessage({payload:snapshot({ACTIVITY_SLOT:1,ACTIVITY_RUN:'other:1',ACTIVITY_REVISION:5})});
  const reset=snapshot({ACTIVITY_SLOT:0,ACTIVITY_REVISION:6,ACTIVITY_STATE:'cancelled',ACTIVITY_CLOCK:'none',
    ACTIVITY_DEADLINE:0,ACTIVITY_SHOW_TERMINAL:0});
  fail=true;pkjs.events.appmessage({payload:reset});
  assert.equal(pkjs.sent.at(-1).ACTIVITY_ERROR,'STORAGE_FAILED');assert.equal(pkjs.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,undefined);
  fail=false;
  assert.equal(helpers.createPublisher({storage:shared,pluginUuid:uuid,maxRecords:8}).lastRevision(),5);
  const preserved=await readSummary(shared);
  assert.deepEqual(Array.from(preserved.instances.slice(1),item=>item.instanceId),[snapshot().ACTIVITY_RUN,'other:1']);
  pkjs.events.appmessage({payload:reset});assert.equal(pkjs.sent.at(-1).ACTIVITY_ACCEPTED_REVISION,6);
  assert.deepEqual(Array.from((await readSummary(shared)).instances.slice(1),item=>item.instanceId),['other:1']);
});

function retryClock() {
  let sequence = 0;
  const timers = new Map(), delays = [];
  return {
    timers, delays,
    later(fn, delay) {const id = ++sequence; timers.set(id, fn); delays.push(delay); return id;},
    cancel(id) {timers.delete(id);},
    next() {
      const [id, fn] = timers.entries().next().value;
      timers.delete(id); fn();
    }
  };
}

test('bootstrap recovers a thrown bridge call without duplicate ready requests', () => {
  const clock = retryClock(); let requests = 0;
  const app = runtime('src/pkjs/index.js', storage(), now, {...clock,
    send(message, success) {
      if (message.ACTIVITY_REQUEST && ++requests === 1) throw new Error('bridge not ready');
      success();
    }});
  assert.doesNotThrow(() => app.events.ready());
  app.events.ready();
  assert.equal(requests, 1);
  assert.deepEqual(clock.delays, [500]);
  clock.next();
  assert.equal(requests, 2);
  assert.equal(clock.timers.size, 0);
  app.events.appmessage({payload:snapshot()});
  assert.equal(app.sent.at(-1).ACTIVITY_ACCEPTED_REVISION, 4);
});

test('bootstrap retries are bounded even when failure callbacks also throw', () => {
  const clock = retryClock(); let requests = 0;
  const app = runtime('src/pkjs/index.js', storage(), now, {...clock,
    send(message, success, failure) {
      if (!message.ACTIVITY_REQUEST) return success();
      ++requests; failure(); failure(); throw new Error('duplicate bridge outcome');
    }});
  assert.doesNotThrow(() => app.events.ready());
  assert.equal(clock.timers.size, 1);
  clock.next(); clock.next();
  assert.equal(requests, 3);
  assert.deepEqual(clock.delays, [500, 1000]);
  assert.equal(clock.timers.size, 0);
  app.events.ready();
  assert.equal(requests, 3);
});

test('old transport callbacks cannot unlock the next in-flight receipt', () => {
  const callbacks = [];
  const app = runtime('src/pkjs/index.js', storage(), now, {
    send(message, success, failure) {callbacks.push({success, failure});}
  });
  app.events.ready();
  app.events.appmessage({payload:snapshot()});
  callbacks[0].success();
  assert.equal(app.sent.length, 2);
  app.events.appmessage({payload:snapshot({ACTIVITY_REVISION:5})});
  callbacks[0].failure(); callbacks[0].success();
  assert.equal(app.sent.length, 2, 'first receipt still owns the only transport slot');
  callbacks[1].success();
  assert.equal(app.sent.length, 3);
  assert.equal(app.sent[2].ACTIVITY_ACCEPTED_REVISION, 5);
});

test('an accepted native snapshot cancels pending bootstrap retries', () => {
  const clock = retryClock(); let requests = 0;
  const app = runtime('src/pkjs/index.js', storage(), now, {...clock,
    send(message, success, failure) {
      if (message.ACTIVITY_REQUEST) {++requests; failure();} else success();
    }});
  app.events.ready();
  assert.equal(clock.timers.size, 1);
  app.events.appmessage({payload:snapshot()});
  assert.equal(clock.timers.size, 0);
  assert.equal(requests, 1);
  assert.equal(app.sent.at(-1).ACTIVITY_ACCEPTED_REVISION, 4);
});

test('a thrown receipt does not wedge later authoritative updates', async () => {
  let failReceipt = true;
  const shared = storage();
  const app = runtime('src/pkjs/index.js', shared, now, {
    send(message, success) {
      if (message.ACTIVITY_ACCEPTED_REVISION && failReceipt) {
        failReceipt = false; throw new Error('temporary receipt transport error');
      }
      success();
    }
  });
  app.events.ready();
  assert.doesNotThrow(() => app.events.appmessage({payload:snapshot()}));
  app.events.appmessage({payload:snapshot({ACTIVITY_REVISION:5})});
  assert.equal(app.sent.at(-1).ACTIVITY_ACCEPTED_REVISION, 5);
  const result = await readSummary(shared);
  assert.equal(result.instances[1].properties.revision.numericValue.value, 5);
});

test('a stale rejected snapshot does not cancel a necessary bootstrap retry', () => {
  const shared = storage();
  runtime('src/pkjs/index.js', shared).events.appmessage({payload:snapshot({ACTIVITY_REVISION:5})});
  const clock = retryClock(); let requests = 0;
  const app = runtime('src/pkjs/index.js', shared, now, {...clock,
    send(message, success, failure) {
      if (message.ACTIVITY_REQUEST && ++requests === 1) failure(); else success();
    }});
  app.events.ready();
  app.events.appmessage({payload:snapshot()});
  assert.equal(app.sent.at(-1).ACTIVITY_ERROR, 'STALE_REVISION');
  assert.equal(clock.timers.size, 1);
  clock.next();
  assert.equal(requests, 2);
  assert.equal(app.sent.at(-1).ACTIVITY_LAST_REVISION, 5);
});
