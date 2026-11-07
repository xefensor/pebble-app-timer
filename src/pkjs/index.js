'use strict';

var activities = require('pebble-activities/publisher');
var settings = require('./settings');
var publisher = activities.createPublisher({
  storage: localStorage,
  pluginUuid: '50e9ef12-a5ab-4369-a3b5-3447bfba5151',
  private: false,
  maxRecords: 8
});

// Timer owns one transport queue. Saving a snapshot and delivering its
// application receipt are different from Pebble's lower-level transport ACK.
var queue = [];
var busy = false;
function flush() {
  if (busy || !queue.length) return;
  busy = true;
  var item = queue.shift(), settled = false;
  function done(ok) {
    if (settled) return;
    settled = true;
    busy = false;
    if (!ok && item.failure) item.failure();
    flush();
  }
  try {
    Pebble.sendAppMessage(item.message, function () { done(true); }, function () { done(false); });
  } catch (error) { done(false); }
}
function send(message, failure) {
  if (message.ACTIVITY_ACCEPTED_REVISION !== undefined) {
    queue = queue.filter(function (pending) {
      return pending.message.ACTIVITY_ACCEPTED_REVISION === undefined;
    });
  }
  if (message.TIMER_APPEARANCE_VERSION !== undefined) {
    queue = queue.filter(function(pending) { return pending.message.TIMER_APPEARANCE_VERSION === undefined; });
  }
  queue.push({message: message, failure: failure});
  flush();
}

var appearanceGeneration = 0, appearanceTimer = null;
function appearance(chosen) {
  var message = settings.message(chosen);
  if (!message) return;
  var generation = ++appearanceGeneration, attempts = 0;
  if (appearanceTimer !== null) clearTimeout(appearanceTimer);
  appearanceTimer = null;
  function deliver() {
    if (generation !== appearanceGeneration) return;
    attempts++;
    send(message, function() {
      if (generation === appearanceGeneration && attempts < 3)
        appearanceTimer = setTimeout(function() { appearanceTimer = null; deliver(); }, attempts * 500);
    });
  }
  deliver();
}
Pebble.addEventListener('configmessage', function(event) {
  if (!event || typeof event.respond !== 'function') return;
  settings.handle(localStorage, event.message || event.data || event.payload || event, function(response) { event.respond(response); }, appearance);
});

var requestStarted = false, requestAttempts = 0, requestTimer = null, ownerAccepted = false;
function requestSnapshot() {
  if (ownerAccepted || requestAttempts >= 3) return;
  ++requestAttempts;
  send({ ACTIVITY_REQUEST: 1, ACTIVITY_LAST_REVISION: publisher.lastRevision() }, function () {
    if (ownerAccepted || requestAttempts >= 3 || requestTimer !== null) return;
    requestTimer = setTimeout(function () { requestTimer = null; requestSnapshot(); }, requestAttempts * 500);
  });
}
Pebble.addEventListener('ready', function () {
  if (requestStarted) return;
  requestStarted = true;
  // Reconcile the phone choice on each runtime start, including its explicit
  // default after a phone-store reset. Native startup keeps the last confirmed
  // appearance until this bounded update arrives.
  appearance(settings.load(localStorage));
  requestSnapshot();
});

Pebble.addEventListener('appmessage', function (event) {
  var payload = event && event.payload;
  if (!payload || payload.ACTIVITY_SCHEMA_VERSION === undefined) return;
  var receipt = publisher.handleMessage(payload);
  if (requestStarted && receipt && receipt.ACTIVITY_ACCEPTED_REVISION !== undefined) {
    ownerAccepted = true;
    if (requestTimer !== null) clearTimeout(requestTimer);
    requestTimer = null;
    queue = queue.filter(function (pending) { return pending.message.ACTIVITY_REQUEST === undefined; });
  }
  if (receipt && receipt.ACTIVITY_ERROR) {
    receipt.ACTIVITY_RUN = payload.ACTIVITY_RUN;
    receipt.ACTIVITY_REVISION = payload.ACTIVITY_REVISION;
  }
  if (receipt) send(receipt);
});
