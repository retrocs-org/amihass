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
- ACTION: owner, entity and action request Toggle, Turn on, Turn off or Activate.
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
UI clients can use normal Exec signals from client.port. Query before acting,
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
recent authenticated REST reachability, not a persistent WebSocket connection.

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

## Room catalogue

`AMIHASS_AREAS` (kind 12) lists Home Assistant rooms through the bridge. Use
`amihass_send` with an offset and optional refresh flag; other filters are unused.
Replies contain at most 16 entries, `total`, and the requested offset. Each item
has the stable room ID in `id` and display name in `name`. Repeat with later
offsets for all rooms; handle WAIT/BUSY and errors asynchronously. Pass a selected
ID to `amihass_send_area` for device/entity discovery. An empty area means all
rooms, including unassigned items. Sample mode supplies the same room catalogue
interface locally. This additive operation preserves ABI 1 and message size.
