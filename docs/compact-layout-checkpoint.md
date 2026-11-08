# Compact layout checkpoint — 2026-11-09

List rows place countdown and state beside each other only when both measured
native widths plus the gap fit. Otherwise they stack and the supporting
state/saved-duration pair follows its own measured fit. Full values wrap rather
than losing digits. The compact ready row also bounds its progress track.
Details, editor, alarm, owner state and input bindings retain their designs.

Candidate: `build/candidate/Timer-0.14.5.pbw`

SHA-256: `c7265a5378ad8c5b894ff98c1e5bd75bdcef818d780534ab3b9bdc101879799f`.

`npm test`, `npm run build`, `npm run check:artifact`, `npm run check:config`
and `npm run check:docs` passed. The production row-planner regression exercises
142/143/144 px primary/support pairs, ready rows and long values with controlled
font measurements. Native verification is separate: the checkpoint exercises
actual OS sizes, light/dark, all eight slots, 24-hour values, warnings, selection,
independent SDK font bitmaps, persistence, locale lookup fixtures and navigation.
The font comparison isolates the countdown now that state shares its line; footer
comparison uses the established high-contrast foreground.

`build/qa/native-navigation/c7265a5378ad/1794178998008344729/report.json`
records 60 native editor/options/confirmation/alarm captures, safe Keep-first
Delete and saved-repeat behavior. The full list/font/heap checkpoint passed in
`build/qa/timer-checkpoint/c7265a5378ad/1794179170486007017/report.json`
(222 captures; identical owner banks across sizes/navigation, 40 diagnostic
open/Back cycles with 80 heap samples, then 20 exact-candidate cycles). Czech/German entries are explicitly separate locale
lookup fixtures. No data-writing fixture was installed on hardware.

Source checks use the available Node 26.12.0 / Python 3.13.16 and the existing
workspace recovery environment; the exact contributor runtime pins were not
available. Native builds use SDK 4.33.1 / CLI 5.0.40. Emulator firmware is v4.33.2.
No physical watch, real paired phone/provider, or actual Czech/German language
pack was tested. Candidate versions and public identities remain unchanged.
No release, push or publication is part of this checkpoint.
