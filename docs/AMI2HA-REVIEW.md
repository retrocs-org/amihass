# ami2ha design review

Reviewed 17 November 2026 at commit
`4b1f3482d5e83b0c449bdefe88a8c4976d312238`.

Reference: https://github.com/evil4dmin/ami2ha

## Useful approaches

1. A socket-independent protocol engine consumes/produces byte buffers. Its
   HTTP upgrade, authentication, WebSocket framing and entity deltas can be
   tested on a development host. Keep this layer inside AmiHASSService; no
   NovaDock dependency belongs in it.
2. Subscribe only to the union of active clients' entity IDs, using Home
   Assistant's `subscribe_entities`. Process initial/additional states, changed
   values, attribute removals and removed entities. Retain AmiHASS's subscription
   leases and limits. Avoid fetching the complete installation to update a tile.
3. Integrate bsdsocket.library readiness with Exec signals using WaitSelect.
   Use a long-lived verified TLS session, with a bounded REST fallback. Measure
   actual idle CPU, memory and reconnect time before making performance claims.
4. Heartbeats and explicit authentication failures distinguish a dead connection
   from unchanged data. Stop automatic authentication retries after a rejected
   token until the credentials are changed or the user explicitly retries.
5. An ARexx port gives scripts access to GET, LIST, STATUS and restricted actions.
   Implement this as an adapter to the same service state/cache and action
   validation as the C SDK; never create a second Home Assistant connection.
6. Capability-aware controls can select on/off, dimmer, colour, cover or media
   actions. Extend our versioned SDK deliberately, rather than guessing support
   from the domain alone. A command acknowledgement is not a confirmed state.
7. Label-based discovery can use a small server-side template. Offer labels and
   areas as optional browsing filters without silently replacing manually
   selected dock entities or reordering a user's layout.
8. Convert UTF-8 display names to the Amiga character set with readable fallbacks
   for typography and units. Keep machine-readable IDs unchanged and test
   truncated/malformed sequences and JSON escapes.

Relevant source areas: `src/core/ha_client.c`, `src/core/ws.c`,
`src/core/charset.c`, `src/net/socket.c`, `src/net/tls.c`,
`src/core/rexx_cmd.c` and `src/rexx/port.c`.

## Adaptation limits

AmiHASS remains a background broker plus AmiHASSPrefs. The reference's MUI
application/dashboard should not become a required dependency of the service
or NovaDock. Dashboard, camera and richer controls can be separate clients.

Keep HTTPS certificate/hostname verification enabled by default. An optional
AmiSSL transport must fail clearly when unavailable; it must never silently
fall back to plain HTTP. Preserve current credential ownership, bounded IPC,
installation maintenance guard, stale-state rules and no action replay after
reconnect. Version 0.3 keeps REST polling through curl as the default and adds
optional native WebSocket/AmiSSL updates. ARexx is implemented through the
AMIHASS command port; see ../ARexx/README.MD.

The reference uses a 2 MB WebSocket message cap. Do not copy that limit blindly
into a service intended for smaller Amigas. Bound entity count, selected
attributes, input/output buffers and queue sizes independently.

## Verification and reuse

The unmodified host test suite passes with its standard compiler flags. An
AddressSanitizer/UndefinedBehaviorSanitizer run stops in `test_auth_invalid`:
`fail_client` copies `c->error` onto itself through `strncpy`, which has undefined
behaviour for overlapping source/destination. Recheck that path, as well as
malformed frames, authentication and connection teardown, before adapting code.
This was a local source review; no real hardware or live device controls were
exercised, and the upstream project's runtime claims were not independently
certified.

The repository's LICENSE is MIT, copyright 2026 ami2ha contributors. Version 0.3
adapts its framing, HTTP and native TCP/TLS modules. The source commit and full
licence are retained with the imported files and runtime notices.

Protocol reference:
https://contributors.home-assistant.io/core/internals/websocket-api/

## Rechecked 24 November 2026

The public main branch still resolves to
`4b1f3482d5e83b0c449bdefe88a8c4976d312238`. This recheck read the current
README, selected-subscription/label client, character conversion and socket wait
code; no upstream code was imported or executed.

Best next candidates for AmiHASS:

1. UTF-8 display-name conversion, preserving machine IDs and JSON values.
2. Optional Home Assistant label discovery using bounded server-side templates.
3. Sensor gauges with user-defined ranges and explicit unavailable readings.
4. Native verified TLS and selected-entity WebSocket deltas inside the central
   service, preserving client leases and REST fallback. Measure before promising
   lower RAM or CPU: the reference's measurements are not AmiHASS measurements.
5. On-demand camera thumbnails in a separately bounded client/worker. Request
   small server-side images, keep certificate verification and avoid allowing a
   slow camera to block ordinary controls.
6. A native AmigaGuide manual accessible through MultiView.

ARexx, media capabilities, selected polling, capability gating and Dashboard
pages already exist here. The reference's ARexx notification subscriptions remain
marked as design work in docs/AREXX.md; do not list them as an implemented
feature to copy. Drag-and-drop is also listed as future work upstream.

References: [README](https://github.com/evil4dmin/ami2ha),
[client](https://github.com/evil4dmin/ami2ha/blob/main/src/core/ha_client.c),
[text](https://github.com/evil4dmin/ami2ha/blob/main/src/core/charset.c),
[network waits](https://github.com/evil4dmin/ami2ha/blob/main/src/net/socket.c).

## Implemented in 0.3

UTF-8 display folding, label-scoped discovery, sensor gauges and selected-entity
live updates are implemented. The portable framing/HTTP and native socket/TLS
modules are adapted under the upstream MIT licence; see `AMI2HA-LICENSE.txt`
and `src/vendor/ami2ha/README.txt`. The typed cache and service integration are
AmiHASS code. No upstream UI or application configuration is imported.
