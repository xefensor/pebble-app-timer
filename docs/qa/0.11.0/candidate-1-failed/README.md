# First 0.11.0 candidate: native renderer failure

Candidate PBW SHA-256: `d1a72a9386f57f73312b67381b822aba59a7aca669fc8fd6de23326ef00cf9e1`.

Actual isolated stock-derived Emery QEMU captures showed the Ready/editor sand at the bottom and running sand at the top. The paused horizontal 90-degree outline rendered correctly but the native 1-pixel polygon strips lost all sand at their final rotation. The native hourglass QA phase stopped on its sand-presence assertion; this candidate did not pass. The source records and unchanged native captures are retained in `native/ux.json` and adjacent PNGs.

An earlier harness-only failure compared source revisions across the explicit ready handshake. Reconfirmation legitimately advances those revisions while preserving owner observation/expiry; the corrected assertion checks those owner fields instead.

No real phone JavaScript or physical watch was exercised.
