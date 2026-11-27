# AmiHASS local client SDK - ABI 1

Include SDK/amihass.h and link SDK/amihass_client.c plus SDK/quote.c, using
SDK/ on your include path. SDK/nd_reply.h implements owned reply
ports. This API is for Classic AmigaOS (32-bit pointers, ULONG/LONG fields),
not a cross-machine wire protocol. No NovaDock library or executable is needed.

The named service port is AmiHASS.Service. Each AmiHASSMessage contains a native
Exec Message, AMIHASS_MAGIC, AMIHASS_ABI and exact mn_Length. The service rejects
incompatible envelopes. Use the SDK rather than inventing structure packing.
Client and service must use the same NDK alignment conventions.

- QUERY: owner and entity select/cache a reading and renew its 120-second lease.
  With `refresh=AMIHASS_QUERY_ONCE`, request a cached/on-demand reading without
  creating or renewing that lease. Repeat after WAIT; HTTP refreshes respect the
  poll interval. This does not cancel another client's regular subscription.
- DEVICES (kind 9): domain, filter, offset and refresh browse devices with exposed states.
  An empty domain returns all devices; a domain such as light returns only
  devices with at least one matching entity. The same filter works in sample mode.
  Each record uses id for the 32-character device registry ID, name for the device
  name, state for manufacturer and device_class for model. These are device
  records, not entity IDs; do not pass them to QUERY or ACTION.
- LIST: type, filter, offset and optional refresh browse the entire server catalogue.
  Pages contain 16 sorted metadata entries and a total count. An empty entity
  field browses all entities, including those without devices. Set entity to a
  device registry ID to scope discovery to that device. Domain/filter still apply.
  Device and entity scopes have separate cache keys; the packet layout and ABI
  remain unchanged. Older services do not support DEVICES.
- ACTION: owner, entity and action request Toggle, Turn on, Turn off, Activate or
  another action of the device type (see USAGE.md).
  Unsupported/stale/unavailable entities are rejected. Accepted actions return
  WAIT, not proof of completion; QUERY until the reported state confirms it.
- STATUS: returns configured server origin, uptime, recent server contact,
  active subscription count and queued action count. No authentication data.
- RELOAD: reload saved shared settings, clear cached data and queued commands,
  and invalidate in-flight responses from the previous configuration.
- STOP: acknowledge shutdown; wait until the port disappears before uninstalling.

Zero-initialize AmiHASSClient. amihass_send permits one outstanding request per
client. Poll amihass_reply while continuing your own event loop, read the result
and FreeVec the returned message. amihass_client_close handles a late reply by
abandoning the owned reply port; the receiver then reclaims it safely. The local
request timeout is ten seconds. Never free an in-flight request yourself.

amihass_start(NULL) uses AmiHASS.cfg/AmiHASS:/standard locations to launch the
separate service when absent. It never launches the calling application's code
as a backend. Missing installations return failure. amihass_launch can open the
AmiHASSPrefs CONFIG window; only fixed app names and arguments are accepted.

Results: OK, WAIT, UNCONFIGURED, FAILED, STALE, BUSY, UNSUPPORTED, UNAVAILABLE.
An on/open state is confirmed server data, not a guessed state after a command.
UI clients can use normal Exec signals from client.port;
`amihass_client_signal(&client)` returns that signal mask (0 before the first
request) so an event loop can `Wait()` for input and the reply together
instead of polling with Delay(). Query before acting,
respect backoff/error states and keep confirmation in your application's flow.
The example only reads status; it never switches a device.

Connection settings belong to AmiHASS, not to individual client applications.
Tokens must never be placed in IPC packets, command arguments, logs or layouts.

Concurrent catalogue browsers share four bounded, 60-second page-cache entries;
another client cannot cancel an in-flight catalogue page.

## Diagnostics

`AMIHASS_DIAGNOSTICS` is a read-only request using the existing ABI 1 message
layout. Set `offset` to zero, then advance by `count` until reaching `total`.
Each `items[]` entry contains a metric key in `id`, a display label in `name`
and an unsigned decimal value in `state`. No credentials or response bodies
are returned. Current snapshots contain 25 counters and gauges in two pages.

Counters reset when the service restarts and include status/diagnostic observer
traffic. Bridge byte counters measure IPC packets. HTTP attempts count requests
dispatched to the downloader, not confirmed device actions. HTTP completions mean
curl completed successfully; entity parsing and action state readback remain
separate. Response-byte counters count body bytes read, excluding HTTP headers.

## Service state and Home Assistant connection

A successful STATUS reply means the local bridge is running; it does not imply
Home Assistant is reachable. Check `reply->status.connected` separately. It is
zero at startup, on network/request failure, after a configuration change and
while reconnecting. It becomes one only after a validated authenticated API
check or valid server data. REST health checks run when idle (60 seconds), so
status does not depend on any app having selected an entity. This represents
recent authenticated reachability. Optional live-update heartbeats maintain it
while the selected WebSocket subscription is healthy.

The existing ABI 1 packet layout is unchanged. STATUS additionally returns:

- `item.state`: `not_configured`, `network_down`, `connecting`, `connected`,
  `retry_wait` or `disconnected`.
- `age`: seconds since the last successful server contact (zero if none).
- Three `items[]` key/value entries: `reconnect.enabled` (0/1),
  `retry.interval` (seconds) and `retry.remaining` (seconds until automatic retry;
  zero when automatic reconnect is disabled). Values are in `state`.

Older clients can continue reading the existing `status.connected` boolean.
For an older bridge without detailed status, use that boolean as the fallback.
Apps such as NovaDock can display the local service state and remote server
state independently. No credentials are included.

`AMIHASS_RECONNECT` (8) explicitly reloads settings and requests a fresh
connection attempt, including when automatic reconnect is disabled. It clears
stale work; it does not replay queued controls. Its OK reply acknowledges the
request, not a successful connection. Follow with STATUS. Saving server settings
or restarting the service also allows a fresh attempt.

Preferences controls automatic retries at 60, 600, 900 or 1800 seconds after a
failure. Ordinary QUERY/LIST requests do not bypass that delay. A failed action
is discarded. Diagnostics adds `http.health`, counting API health-check attempts.

## Shared sample mode

Preferences > Data source > Sample data enables a bridge-wide provider.
Server data is the default. Save applies the choice; Close discards it.
Clients use the same DEVICES, LIST, QUERY and ACTION messages. STATUS returns
`item.state=AMIHASS_SERVER_SAMPLE` (`sample`), `status.server=sample://amihass`
and `status.connected=1` to indicate provider readiness. This explicitly denotes
a simulation, not an established Home Assistant connection. `last_contact` is
zero and HTTP counters stay unchanged. Show the sample source to the user.

Sample IDs contain `.amihass_sample_`; do not strip or rewrite them. ACTION
returns WAIT on accepted simulated control; QUERY reads the resulting shared
state, including from a second client. On/off/toggle and scene/script activation
follow the supported action policy; other operations are rejected. Sample
state resets on reload/restart. Switching providers clears caches/queued actions;
real mode rejects these reserved IDs, preventing sample selections from becoming
real device actions. Sample mode never starts new HTTP requests.

## Parameter controls (ABI 1)

AMIHASS_ACTION accepts the strings `Brightness 40`, `Color #123456` and
`Temperature 22.5`. Keep the complete action under 20 bytes including NUL.
Brightness is an integer percentage, color is six hexadecimal RGB digits, and
temperature is a decimal in the server's native units with at most two decimal
places. A normal QUERY must obtain a fresh, available state before ACTION.

QUERY retains the normal on/off/HVAC state in `item`. Optional `items[0..count)`
contain capability metadata: `control.brightness` (0–100, unit %), `control.color`
(#RRGGBB), and `control.temperature`, `control.minimum`, `control.maximum`,
`control.step`. Temperature metadata carries a verified C/F unit from the entity or server configuration. Missing keys
mean unsupported controls; `unknown` brightness/color means a supported control
without a current value. Do not treat a draft as confirmed state. This extension
uses existing message fields, keeps ABI 1, and requires no token in a client.

Clients should check advertised bounds and step locally. The bridge also rejects
unsupported parameters, out-of-range targets, stale states and unavailable
entities. WAIT means queued, not successful: query again for confirmed state.
On/off-only lights remain valid clients but have no brightness or RGB controls.
Color-temperature-only lights have brightness but no RGB selector. Climate
single-target changes preserve HVAC mode; dual heat/cool ranges are not included.

`AMIHASS_QUERY_FRESH` starts one fresh state read. Follow it with ordinary QUERY
requests while the result is WAIT. Repeatedly sending FRESH after every completed
read would deliberately invalidate the cache again. Clients should compare the
reported parameter with their requested target, and report a bounded timeout
when the target never arrives. Temperature actions require a verified C/F unit.

## Additive ABI 1 operations

SUBSCRIBE (10) uses QUERY_INTERVAL in refresh and returns the same state/control
metadata as QUERY. Renew within 120 seconds. UNSUBSCRIBE (11) releases one entity,
or all entities when entity is empty, for the requesting port. Its reply is OK
when there was nothing to release. New SDK close sends a reply-free cleanup packet:
request age carries the old port identity, which the service compares without
dereferencing. Existing message sizes, ABI number and result values are unchanged.

For LIST and DEVICES, request item.id optionally carries an area name or ID.
Use amihass_send_area rather than mutating a sent message. The area is part of
the shared page-cache key and never contaminates another client's catalogue.
Unknown areas return no matches. Sample areas are Living room, Study, Kitchen
and House (IDs living_room, study, kitchen and house).

DIAGNOSTICS with domain memory exposes numeric system-wide Fast RAM observations
and the current lease count. Ordinary diagnostics totals retain their old indices.
The service is not an allocator profiler; readings include other Amiga programs.

## Multi-entity queries, attributes and history (ABI 1)

Check STATUS for `feature.query_many`, `feature.attributes` and
`feature.history`. All three keep the message size and ABI.

- `AMIHASS_QUERY_MANY` (16) / `AMIHASS_SUBSCRIBE_MANY` (17): put up to 16 entity
  IDs in `items[i].id` with `count`, or call `amihass_query_many()`. The reply
  has each reading in `items[i]` and its own result in
  `AMIHASS_ITEM_RESULT(m,i)`; `m->result` is OK once the request was processed
  and `total` counts the entities that were OK. Refresh flags are as for QUERY;
  SUBSCRIBE_MANY also takes a lease on each entity.
- `AMIHASS_ATTRIBUTES` (18), `amihass_attributes(c,owner,entity,offset,fresh)`:
  every attribute of one entity, 16 per page, at most 64. `items[i].id` is the
  key, `.name` the value as text (lists and objects as compact JSON, cut at 63
  characters) and `.device_class` the type: string, number, boolean, list,
  object or null. Answers are kept for 10 seconds; `fresh` skips that.
- `AMIHASS_HISTORY` (19), `amihass_history(c,owner,entity,hours,offset)`:
  states over the last 1-48 hours (0 means 24), thinned to at most 64
  points spread across the window, oldest first, 16 per page. `items[i].state`
  is the value and `.updated` the server time; a non-numeric state
  (unavailable, unknown) is a gap with an empty `state`.
  The window start comes from the server's clock, never the Amiga's. Answers
  are kept for 60 seconds. A window too large for one 256 KB response is read in 4
  or 12 pieces (each thinned to its share of the 64 points).

ATTRIBUTES and HISTORY reply WAIT while the service fetches. Ask again about
once a second with the same arguments; a different entity while one is being
fetched gets BUSY. Missing entities reply UNAVAILABLE. The service keeps one
answer of each kind and frees it after a minute without requests.

## Persistent notifications (ABI 1)

`AMIHASS_NOTIFICATIONS` (20), `amihass_notifications(c,owner,id,offset)`.
With no ID it lists up to 8 Home Assistant persistent notifications:
`items[i].id` the notification ID, `.name` the title, `.state` the start of
the message and `.updated` the creation time. With an ID, `item` holds the
ID, title and time and the message (up to 255 characters) comes in
63-character pieces in `items[i].name`; `total` is its length. `m->age` is a
serial that changes with every update, so a client can tell when to read
again. Notifications reach the Amiga over the live-update connection only:
when live updates are off the reply is UNSUPPORTED with `item.state`
`live_updates_off`; the first request answers WAIT while the connection
starts (it is kept open for two minutes after the last request, also when no
entity is selected). Older services do not know this kind.

## Room catalogue

`AMIHASS_AREAS` (kind 12) lists Home Assistant rooms through the bridge. Use
`amihass_send` with an offset and optional refresh flag; other filters are unused.
Replies contain at most 16 entries, `total`, and the requested offset. Each item
has the stable room ID in `id` and display name in `name`. Repeat with later
offsets for all rooms; handle WAIT/BUSY and errors asynchronously. Pass a selected
ID to `amihass_send_area` for device/entity discovery. An empty area means all
rooms, including unassigned items. Sample mode supplies the same room catalogue
interface locally. This additive operation preserves ABI 1 and message size.

## Feature negotiation

STATUS includes optional `feature.areas`, `feature.catalogue_filters` and
`feature.subscriptions` entries in items[], each with state `1` when supported.
Missing entries mean unverified support, even when the app version is 0.3.
Check these keys before offering room/type-scoped discovery or subscriptions.
Unknown keys can be ignored. Older clients keep reading the original status
and reconnect fields. The ABI and packet size remain unchanged.

## Media-player controls

The service accepts `Play`, `Pause`, `Previous`, `Next` and `Volume 0` through
`Volume 100` for media players that advertise those capabilities. QUERY replies
carry optional `control.media` flags (1 play, 2 pause, 4 previous, 8 next, 16
volume), `control.media_volume`, `control.media_title` and `control.media_artist`.
The title and artist use each item's `name`; volume uses integer-percent `state`.
The SDK's `AHControls` decoder exposes these as typed fields. Missing metadata
means unsupported or unknown; clients must not invent controls.

ACTIONS still require a fresh available state. WAIT is queued, not confirmation:
query again to confirm playback/volume, and compare update timestamps for track
changes. The shared sample player supports these operations without networking.
These optional metadata items preserve Exec message ABI 1.

## Locks and doors

Use Lock, Unlock or Open lock for smart locks. Open lock releases the latch
and requires the advertised OPEN feature. QUERY replies expose control.lock,
control.unlock and control.lock_open. Locks requiring a keypad code remain
read-only; PIN entry is not supported. Motorised doors use Open cover,
Close cover and Stop cover when supported. Door contact sensors are read-only.
Dashboard tiles offer these explicit actions; confirmation follows state readback.
The sample front-door lock and garage door can be controlled offline.
A lock with supported_features=0 still supports lock/unlock; it has no
latch-opening action. Bluetooth and cloud entities are separate choices.

Dashboard action choices use the selected entity's reported capabilities.
If capabilities cannot be verified, only Read only is offered. Real Unlock,
latch-open and door-open actions ask for confirmation with the device name
and entity ID. Cancel, Escape and closing the dialog send nothing. Sample
controls do not need confirmation. Door contacts display Open/Closed; lock
states remain separate. SDK and ARexx clients manage their own confirmation UI.

SDK state values stay unchanged (for example off, locked, unlocked).
`amihass_display_state` supplies human-readable UI labels without changing them.

STATUS may report `auth_rejected` with connected=0 after HTTP 401/403.
Automatic polls and retries stay paused until settings reload or an explicit
AMIHASS_RECONNECT request. The message layout and ABI remain unchanged.

The DIAGNOSTICS domain `cost` provides `http.request_bytes`,
`http.last_seconds`, `http.max_seconds`, `http.total_seconds`,
`http.body_bytes` and `memory.request.peak`. Durations have one-second
resolution; bytes exclude TLS overhead. RAM is sampled system-wide Fast free
memory, not per-process allocations. A lease without renewal slows after
90 seconds and expires after 120 seconds; shared leases retain the fastest
active client interval. Message size and ABI remain unchanged.

STATUS now includes additive `poll.seconds` metadata for the configured default.
State polling backs off unavailable/missing entities independently; explicit
QUERY_FRESH bypasses that delay. Confirmation reads precede queued actions, then
explicit reads precede catalogue work. An in-flight HTTP request is not cancelled.

Budget diagnostics: `background.limit`, `background.used` and
`background.wait_seconds` follow the previous six `cost` items. A zero limit
means unlimited. The optional eighth numeric field in AmiHASS.cfg stores the
limit; older files remain readable. `AmiHASSConfig.recovered` is transient and
never written. Dashboard load_status helpers expose validated backup recovery
without changing saved layout structures.

### Label scopes (ABI 1)

`amihass_send_scope(client, kind, owner, device, offset, refresh, domain, filter,
area, label)` adds a label to LIST/DEVICES discovery. Use an empty label for all.
The label is limited to 63 bytes and is carried in request `item.name`; the room
remains in request `item.id`. Reply fields retain their existing meaning. The
message size and ABI are unchanged; older SDK calls leave the label empty.

STATUS adds `transport.mode` (`REST`, `REST fallback` or `WebSocket`),
`push.updates` and `push.failures`. Client subscription leases determine live
selections. One-shot discovery does not create an unfiltered live subscription.

### Camera snapshots (ABI 1)

Check STATUS for `feature.camera_snapshots=1`. `AMIHASS_SNAPSHOT` (13) is a
read-only image request; it never calls a Home Assistant service action.

Use `amihass_snapshot(client, owner, camera_id, offset, version, fresh)`:

- Begin with offset 0, version NULL and fresh 1. Poll WAIT with fresh 0.
- On OK, `amihass_snapshot_data(reply, &size)` validates and exposes the bytes.
- `reply->total` is the complete encoded image length (maximum 262144 bytes).
- `reply->refresh` is the chunk length, at most `AMIHASS_SNAPSHOT_BYTES` (2048).
- Binary bytes occupy `reply->items`; `reply->count` is zero. Do not parse them
  as entity records. `item.state` identifies JPEG, PNG or ILBM.
- Save `item.updated` as the opaque version. For subsequent offsets, pass it
  as version. Check total/version on every chunk; STALE requires a new read.
- `reply->age` is the cache age, not proof of when the camera captured a frame.

Message size remains unchanged. Keep replies alive until their bytes have been
copied, then FreeVec them normally. Closing a client releases its pending reply
ownership. The service holds one image cache; another camera may return BUSY.
Fresh requests for the same camera are limited to one per five seconds. No
background snapshots are fetched without a client request. Do not send arbitrary
URLs, filenames or camera access tokens. The central configured bearer token is
used only for the fixed `/api/camera_proxy/<entity_id>?width=320&height=200` path.

### Stateless presses and additional catalogues

`button.*` and `input_button.*` accept `Press`; Toggle and on/off are rejected.
ACTION replies return WAIT and a delivery ticket in `age`. Keep the same client
port open and query the same entity: `control.press=yes` advertises support and
`control.delivery` equals that ticket only after successful delivery. Match the
exact ticket, not another client’s receipt. Do not treat an ordinary state change
as proof of delivery. An unavailable entity, failed HTTP request or expired client
must not be reported as delivered. Keep one outstanding press per client/entity.
Receipts are bounded and expire after 120 seconds; close releases them.

`AMIHASS_LABELS` (14) uses normal paged catalogue replies: `items[].id` is a
stable label ID, `name` is its display name. IDs are limited to 63 bytes to fit the
existing ABI 1 label scope. STATUS advertises `feature.labels=1`.

DIAGNOSTICS domains `transport` and `camera` return bounded read-only metrics.
Optional `AMIHASS_CAMERA_STATS` (15) reports client decoder pressure: offset is
encoded bytes (0..262144), refresh is an observed Fast RAM drop (0..64 MiB).
Neither URLs nor credentials are accepted. SDK close also releases camera leases,
cancels abandoned queued image jobs and discards abandoned in-flight results.

Manual reconnect preserves subscriptions and configuration generation. It clears
the previous transport error, authentication pause and retry delay; the next
scheduler pass attempts the saved connection. STATUS retry.remaining is a
nonnegative number of seconds; use it together with the server state.

### Parsing notes (ABI 1, unchanged layout)

- Numbers are hundredths. Values with more than two decimals (for example a
  `volume_level` of 0.4000000059604645) are rounded half-up instead of rejected;
  exponent notation is accepted.
- Long states, names and units are truncated on whole characters instead of
  failing the entity: `state` holds at most 39 bytes.
- `struct AHControls` gained a trailing `step_default` field: set when the
  server sent no `target_temp_step`, in which case the step is 0.5 for C and
  1 for F, matching Home Assistant's frontend. Rebuild clients that allocate
  AHControls themselves; the IPC message layout is unchanged.
- Server URLs may carry a reverse-proxy path prefix
  (`https://host/homeassistant`); `amihass_url_clean()` trims blanks and
  trailing slashes from user input before `amihass_url()` validates it.

### Refusal markers

- An ACTION that unlocks or opens a lock, or opens or positions a cover, from
  any owner other than `AmiHASS.Dashboard` returns AMIHASS_UNSUPPORTED with
  `item.state` = `AMIHASS_STATE_CONFIRMATION`, unless the user enabled General
  Settings > Script doors. Confirm with the user before sending such actions.
- When all selected-entity slots are in use (48 to 128, sized from free
  memory at each settings reload), QUERY/SUBSCRIBE/ACTION return
  AMIHASS_BUSY with `item.state` = `AMIHASS_STATE_SELECTION_FULL`; release
  selections (UNSUBSCRIBE) before retrying.
- `amihass_snapshot_size(..., small)` requests a 96x60 thumbnail.
- `amihass_action_sensitive(entity, action)` reports whether an action is one
  of these.

AMIHASS_DISCONNECT (21) pauses server transport and automatic retries while
keeping the broker alive. STATUS returns server state `paused`. Queued actions
are discarded; in-flight replies from the prior generation are ignored.
AMIHASS_RECONNECT resumes transport; saved policy is unchanged. Sample mode
rejects DISCONNECT because its shared data provider is already offline.

The DIAGNOSTICS domain `overview` returns six bounded items: discovery.state,
cache.entities, cache.pages, memory.tracked (bytes), requests.pending and
memory.low. Discovery describes the last catalogue request independently of
server connectivity. Tracked memory includes the broker, entity/lease buffers
and encoded camera cache, excluding transport/TLS worker allocations. STATUS
last_contact plus reply age identifies the most recent successful response.
