# AmiHASS SDK 0.1

A small C client for the central AmiHASSService on AmigaOS 3.x. Applications use
local Exec messages, never a Home Assistant token or their own HTTP connection.
The package includes the SDK source, required headers and AmiHASSExample.

## Build the independent example

From this directory, with vbcc and the Amiga NDK configured:

```
make VC='vc +aos68k'
```

Link `amihass_client.c` and `quote.c` with your app and `-lamiga`; add this
folder to your include path. Include `amihass.h`. Keep the message ABI and NDK
alignment unchanged. Initialize every `struct AmiHASSClient` to zero.

## Try it without a server

Start the service and select Sample data in AmiHASSPrefs, then Save.

```
AmiHASSExample STATUS
AmiHASSExample LIST
AmiHASSExample QUERY light.amihass_sample_living_room
AmiHASSExample ACTION light.amihass_sample_living_room "Toggle"
```

The ACTION command explicitly controls its supplied entity. In Server data
mode it can control real devices. The example reads current state first,
sends the action once, then reads bridge state again. Accepted does not mean
that a real device has already changed state. It does not auto-start the service.

## Integrate with an event loop

1. Send STATUS and check provider readiness and `item.state`. `sample` means
   simulated data; `connected` means the real server. Show the distinction.
2. Use DEVICES and scoped LIST for discovery. Pages have at most 16 records;
   use offset/total to retrieve more. Preserve entity IDs exactly.
3. Send QUERY and collect replies using `amihass_reply()` from your event loop.
   A client has at most one outstanding request. WAIT means retry the read later.
4. For a one-time read use `AMIHASS_QUERY_ONCE`. Regular QUERY renews a polling
   lease. Do not poll the whole server just to display a few readings.
5. ACTION supports the bridge's on/off/toggle and activation policy. Do not
   optimistically change the displayed device state; read it back.
6. Free each reply with FreeVec, and close the client on cancellation or exit.

See REFERENCE.md in the packaged SDK, or ../docs/SDK.md in the source repository,
for packet fields, errors, reconnect status and shared sample-mode behavior.
The Dashboard app is a larger native client example in src/dashboard.c
in the source repository: https://github.com/retrocs-org/amihass

## Recent client diagnostics

`AMIHASS_DIAGNOSTICS` with `domain="clients"` returns a paged, read-only view of
recent activity. Set `offset` for subsequent pages. ABI 1 is unchanged.

Client rows use `id`/`name` for the SDK owner name, `state` for request count,
`unit` for query count, `updated` for catalogue count and `device_class` for
action count. Selection rows follow their client, with `device_class="entity"`
and the selected entity ID in `id`. Counters are decimal strings.

Owner names must be printable and at most 19 characters. Keep an owner's name
stable and distinct per app or tile. Clients that omit owner names are not
individually tracked. The service retains at most 16 recent clients and 64
client/entity selections, with a 120-second activity window. Counts are retained
until an entry is reused. This reports activity, not live process registration.

On `STATUS`, optional `item.name` describes the last connection failure with a
suggested fix. Existing `item.state` and `status.connected` keep their meanings.

For an on-demand entity read with a custom cache-age preference, combine
`AMIHASS_QUERY_ONCE | AMIHASS_QUERY_INTERVAL(seconds)`. Accepted preferences
are 5–3600 seconds; zero retains the service default. The Dashboard offers a
small preset list and spaces its requests accordingly. Queries for the same
entity share the existing cache and downloader. `AMIHASS_QUERY_FRESH` remains
a one-time forced read; do not repeatedly resend it while awaiting a result.
Continuous legacy QUERY leases retain the service's default polling interval.
The local ABI is unchanged; custom preferences need the current service build.

## Optional fan and cover controls

QUERY metadata can contain `control.fan_speed`, `control.position`,
`control.cover_open`, `control.cover_close` and `control.cover_stop`.
Use `amihass_controls_items` and `amihass_control_allowed` before sending
`Fan speed 0..100`, `Position 0..100`, `Open cover`, `Close cover` or `Stop cover`.
Follow ACTION acceptance with QUERY readback. Unsupported or absent metadata
leaves controls unavailable. The wire message layout and ABI remain unchanged.

Fan speed requires Home Assistant SET_SPEED; cover actions require their
corresponding supported-feature flags. Locks remain read-only. Samples require
no server connection and never operate physical devices.

## Selected entity subscriptions

`amihass_subscribe(&client, "MyApp", "light.room", 5)` requests a shared polling
lease and returns a normal asynchronous QUERY-style snapshot. Read it with
`amihass_reply()`. Repeat within 120 seconds (60 seconds is recommended) to keep
the lease alive. Intervals are 5–3600 seconds; zero uses the service default.
Only explicitly selected entities are polled, once per entity at the shortest
active interval. No unsolicited event messages are sent to clients.

Use `amihass_unsubscribe(&client, "light.room")` to release one selection, or pass
NULL to release every selection on that client port. Consume the reply before
sending another request. `amihass_client_close()` also releases selections,
including when another request is still pending. If cleanup allocation fails,
the bounded lease still expires. Separate client ports remain independent.
Existing QUERY clients and QUERY_ONCE remain supported.

Catalogue calls can use `amihass_send_area()` with LIST or DEVICES. Its final
argument is an area name or ID; NULL/empty means all areas. It preserves device,
domain, search and paging arguments and never transfers an entire state catalogue.
These are additive ABI 1 operations and need the matching AmiHASSService build.

## Room catalogue

`AMIHASS_AREAS` (kind 12) lists Home Assistant rooms through the bridge. Use
`amihass_send` with an offset and optional refresh flag; other filters are unused.
Replies contain at most 16 entries, `total`, and the requested offset. Each item
has the stable room ID in `id` and display name in `name`. Repeat with later
offsets for all rooms; handle WAIT/BUSY and errors asynchronously. Pass a selected
ID to `amihass_send_area` for device/entity discovery. An empty area means all
rooms, including unassigned items. Sample mode supplies the same room catalogue
interface locally. This additive operation preserves ABI 1 and message size.
