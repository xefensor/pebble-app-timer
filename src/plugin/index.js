'use strict';

var activities = require('pebble-activities/publisher');
var summary = require('pebble-activities/summary');
var timerUuid = '50e9ef12-a5ab-4369-a3b5-3447bfba5151';
function generatedTimerName(name) {
  if (typeof name !== 'string') return false;
  var match, seconds;
  if ((match = /^(\d+) h timer$/.exec(name))) seconds = Number(match[1]) * 3600;
  else if ((match = /^(\d+) min timer$/.exec(name))) seconds = Number(match[1]) * 60;
  else if ((match = /^(\d+):(\d{2}):(\d{2}) timer$/.exec(name))) {
    if (Number(match[2]) > 59 || Number(match[3]) > 59) return false;
    seconds = Number(match[1]) * 3600 + Number(match[2]) * 60 + Number(match[3]);
  } else if ((match = /^(\d{2}):(\d{2}) timer$/.exec(name))) {
    if (Number(match[1]) > 59 || Number(match[2]) > 59) return false;
    seconds = Number(match[1]) * 60 + Number(match[2]);
  } else return false;
  if (!isFinite(seconds) || seconds < 5 || seconds > 86400) return false;
  function two(value) { return value < 10 ? '0' + value : String(value); }
  var canonical = seconds % 3600 === 0 ? seconds / 3600 + ' h timer' :
    seconds % 60 === 0 ? seconds / 60 + ' min timer' :
    seconds >= 3600 ? Math.floor(seconds / 3600) + ':' + two(Math.floor(seconds / 60) % 60) + ':' + two(seconds % 60) + ' timer' :
    two(Math.floor(seconds / 60)) + ':' + two(seconds % 60) + ' timer';
  return name === canonical;
}
var provider = activities.createProvider({
  storage: localStorage,
  pluginUuid: timerUuid,
  presentation: 'simple',
  appName: 'Timer',
  // Keep the previous card source readable for hosts which have not adopted
  // activity/live. Normal/compact display geometry belongs to each host.
  timerPresentation: 'compact'
});
var summaries = summary.defineSnapshotProvider({
  storage: localStorage,
  pluginUuid: timerUuid,
  displayName: 'Timer',
  icon: 'timer',
  readSnapshot: function () {
    // The native owner already persisted the snapshot through its receipt
    // protocol. A source read preserves those anchors and acknowledged results.
    return activities.readSnapshots({storage: localStorage, pluginUuid: timerUuid});
  },
  adapt: function (snapshot) {
    var adapted = summary.adapters.timerSnapshot(snapshot);
    // The native list generates duration labels, not custom instance names.
    // Keep the identity footer useful without repeating a configured duration.
    // Repeat duration is independent of totalDuration after edits/snoozes.
    if (adapted && generatedTimerName(adapted.record.name)) delete adapted.record.name;
    return adapted;
  }
});

Pebble.registerSourceHandler(function (request, respond) {
  if (request && request.category === 'activity' && request.item === summary.ITEM) {
    return summaries.handleSource(request, respond);
  }
  return provider.handleSource(request, respond);
});
