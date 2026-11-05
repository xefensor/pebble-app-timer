'use strict';

var activities = require('pebble-activities/publisher');
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
  queue.push({message: message, failure: failure});
  flush();
}

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
