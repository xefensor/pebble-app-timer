# Timer privacy

Timer stores timer durations, running/paused/finished state, deadlines, alarm bookkeeping and verified display preferences on the watch so timers can survive reopening and restart. It uses PebbleOS time and alarm services; a running timer does not require the phone.

The bundled phone plugin stores configuration choices through Pebble's configuration bridge. It publishes timer activity records, including timer identity, state and timing fields, to Pebble's activity sources for compatible consumers you choose. Timer does not request location, music, notification, Internet or app-launch permissions.

Timer has no account service, advertisements, analytics or developer-operated server. Pebble controls its phone storage and source delivery. Removing Timer removes its app-owned watch data; Pebble manages its own stored data. Avoid private information in support reports.
