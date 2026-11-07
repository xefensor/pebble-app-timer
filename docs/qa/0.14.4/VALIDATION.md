# Timer0.14.4 — full-height action rail

Local unpublished review revision on `design/pebble-native-redesign`.
Xef clarified that black is acceptable; the rail must span the entire height.
[Validation](validation.json) binds native captures to exact PBW `e882f66f1e711e7ded11d184e08d76d0341a4f573951672f4fad4d52a766e78a`.

The contrasting34px rail spans y0–227. Header text, dividers and footers stay
inside166px, keeping the26pt clock readable. Concise context preserves the
selected position without shrinking type. Timer uses consistent22pt footers and
complete elapsed values. Existing owner/phone identity and permissions remain.

SDK build, automated source/native tests and actual packaged runtime checks pass.
Current suites include 58 unmodified native captures. Each capture's hash and native payload
match the packaged candidate. 50 previous tracked artifacts retain their bytes.
Actual text ink checks and the detailed suite reports are linked in validation.
Standalone source is packaged after final evidence/docs and checked after commit.

Synthetic fixtures/peers run exact native bytes in isolated Emery QEMU. This is
not real phone/physical watch evidence. Phone settings use the unchanged source;
its preview has a local mock bridge. Fresh browser rendering remains unavailable.
No push, publication or change to main is part of this revision.
