# Readable supporting text — 8 November 2026

Timer's supporting text now uses the normal foreground colour. This removes
the grey wash from explanatory/help text and secondary labels: black on white,
white on black.

The change affects text ink, including inverted selection text where applicable.
Font sizes, layout, saved appearance choices, artwork, decorative separators,
source validity and action availability remain unchanged.

Current candidate: `build/candidate/Timer-0.14.5.pbw`. SHA-256:
`fcb627139b440bd763c4650e164e2bcde079d2b36e94598041ac7e283321e741`.

`npm test`, `npm run build`, `npm run check:artifact` and
`npm run check:config` passed. 60 captures cover all three actual OS sizes, Details, Add/Edit, Options, confirmation, alarm interruption and repeated Back/navigation.
Evidence: `build/qa/native-navigation/fcb627139b44/1794171938811263355/report.json`. Native runs used verified Emery 4.45.0 assets;
phone APIs/providers are simulated and no new physical-device test is claimed.

Existing checkpoint hashes describe their original candidates; this is a new
local candidate. No version bump or release archive was created.
