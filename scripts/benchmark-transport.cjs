'use strict';

// Execute actual packaged scripts in isolated VMs. Counts transport work and
// recovery outcomes, never physical radio power or phone/watch battery life.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');
const crypto = require('node:crypto');
const directories = process.argv.slice(2);
if (directories.length !== 2) throw new Error('Usage: benchmark-transport.cjs BASELINE_PBW_DIRECTORY CANDIDATE_PBW_DIRECTORY');
const NOW = 1800000000000;
class FixedDate extends Date {
  constructor(...args) {super(...(args.length ? args : [NOW]));}
  static now() {return NOW;}
}

function create(directory, mode) {
  const events = {}, messages = [], callbacks = [], timers = new Map(), storage = new Map();
  let sequence = 0, thrown = false, active = 0, peak = 0;
  const source = fs.readFileSync(path.join(directory, 'pebble-js-app.js'), 'utf8');
  const info = JSON.parse(fs.readFileSync(path.join(directory, 'appinfo.json'), 'utf8'));
  const context = vm.createContext({
    Date: FixedDate, console,
    localStorage: {getItem(key) {return storage.has(key) ? storage.get(key) : null;},
      setItem(key, value) {storage.set(key, String(value));}, removeItem(key) {storage.delete(key);}},
    setTimeout(fn) {const id = ++sequence; timers.set(id, fn); return id;},
    clearTimeout(id) {timers.delete(id);},
    Pebble: {
      addEventListener(name, fn) {events[name] = fn;},
      sendAppMessage(message, success, failure) {
        messages.push(JSON.parse(JSON.stringify(message)));
        if (mode === 'throw-first' && !thrown) {thrown = true; throw new Error('temporary bridge failure');}
        if (mode !== 'delayed') return success();
        ++active; peak = Math.max(peak, active);
        let settled = false;
        function finish(fn) {if (!settled) {settled = true; --active;} fn();}
        callbacks.push({success() {finish(success);}, failure() {finish(failure);}});
      }
    }
  });
  vm.runInContext(source, context, {timeout:1000});
  return {events, messages, callbacks, timers,
    identity: {version:info.versionLabel, scriptSha256:crypto.createHash('sha256').update(source).digest('hex')},
    peak() {return peak;},
    ready() {let failed = false; try {events.ready();} catch (error) {failed = true;} return failed;},
    drain() {
      for (let n = 0; timers.size; ++n) {
        assert.ok(n < 10, 'unbounded retry chain');
        const [id, fn] = timers.entries().next().value; timers.delete(id); fn();
      }
    },
    snapshot(revision = 4) {
      events.appmessage({payload:{ACTIVITY_SCHEMA_VERSION:1, ACTIVITY_SLOT:0,
        ACTIVITY_RUN:'benchmark-owner:1', ACTIVITY_REVISION:revision,
        ACTIVITY_STATE:'active', ACTIVITY_KIND:'timer', ACTIVITY_CLOCK:'deadline',
        ACTIVITY_NAME:'Timer', ACTIVITY_MAIN:'', ACTIVITY_FOOTER:'Timer',
        ACTIVITY_OBSERVED_AT:NOW / 1000, ACTIVITY_VALID_UNTIL:NOW / 1000 + 660,
        ACTIVITY_DEADLINE:NOW / 1000 + 600, ACTIVITY_DURATION:0, ACTIVITY_CLOCK_AT:0,
        ACTIVITY_TOTAL_DURATION:600, ACTIVITY_PRIVATE:0, ACTIVITY_PUBLISH_SESSION:7}});
    }
  };
}

function measure(directory) {
  const duplicate = create(directory);
  for (let i = 0; i < 5; ++i) assert.equal(duplicate.ready(), false);
  const recovery = create(directory, 'throw-first');
  const bootstrapThrew = recovery.ready(); recovery.drain(); recovery.snapshot();
  const delayed = create(directory, 'delayed');
  delayed.ready(); delayed.snapshot(); delayed.callbacks[0].success(); delayed.snapshot(5);
  delayed.callbacks[0].failure(); delayed.callbacks[0].success();
  return {identity:duplicate.identity,
    duplicateReadyRequests:duplicate.messages.filter(m => m.ACTIVITY_REQUEST).length,
    temporaryFailure:{bootstrapThrew, requestAttempts:recovery.messages.filter(m => m.ACTIVITY_REQUEST).length,
      acceptedReceipts:recovery.messages.filter(m => m.ACTIVITY_ACCEPTED_REVISION).length,
      pendingRetryTimers:recovery.timers.size},
    duplicateCallbacks:{peakConcurrentMessages:delayed.peak(), messagesBeforeCurrentReceiptCompletes:delayed.messages.length}};
}

const baseline = measure(directories[0]), candidate = measure(directories[1]);
assert.equal(candidate.duplicateReadyRequests, 1);
assert.deepEqual(candidate.temporaryFailure, {bootstrapThrew:false, requestAttempts:2, acceptedReceipts:1, pendingRetryTimers:0});
assert.equal(candidate.duplicateCallbacks.peakConcurrentMessages, 1);
console.log(JSON.stringify({result:'PASS', scope:'Actual packaged scripts with simulated bridge events/timers; no real phone, radio or hardware battery measurement.', baseline, candidate}, null, 2));
