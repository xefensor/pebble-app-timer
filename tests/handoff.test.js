'use strict';

const test = require('node:test');
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const helpers = require('pebble-activities/publisher');
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

function runtime(file, shared, at = now) {
  const events = {}, sent = [];
  let source;
  const pebble = {
    addEventListener(name, fn) { events[name] = fn; },
    sendAppMessage(message, success) {
      sent.push(JSON.parse(JSON.stringify(message)));
      success();
    },
    registerSourceHandler(fn) { source = fn; }
  };
  const api = {
    createPublisher(options) { return helpers.createPublisher(options); },
    createProvider(options) { return helpers.createProvider({...options, now: () => at * 1000}); }
  };
  vm.runInNewContext(fs.readFileSync(path.join(__dirname, '..', file), 'utf8'), {
    Pebble: pebble, localStorage: shared,
    require(name) {
      assert.equal(name, 'pebble-activities/publisher');
      return api;
    }
  }, {filename: file});
  return {events, sent, source};
}

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

test('one activity manifest declaration covers legacy sessions and bounded cards', () => {
  const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, '..', 'package.json'), 'utf8'));
  assert.equal(manifest.pebble.messageKeys.ACTIVITY_SLOT, 1021);
  assert.equal(manifest.pebble.messageKeys.ACTIVITY_TOTAL_DURATION, 1022);
  assert.equal(manifest.pebble.messageKeys.ACTIVITY_SHOW_TERMINAL, 1023);
  const sources = manifest.pebble.plugin.sources.filter(source => source.category === 'activity');
  assert.equal(sources.length, 1, 'Each source category must be declared once');
  const card = sources[0];
  assert.deepEqual(card.items, ['session', 'card']);
  assert.deepEqual(card.properties.deadline, ['timestamp']);
  assert.deepEqual(card.properties.duration, ['numericValue']);
  assert.deepEqual(card.properties.clock_at, ['timestamp']);
  assert.deepEqual(card.properties.main_base, ['numericValue']);
  assert.deepEqual(card.properties.support_anchor, ['timestamp']);
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
