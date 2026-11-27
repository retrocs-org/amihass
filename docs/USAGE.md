# AmiHASS 0.3 - Amiga Home Assistant

A standalone Home Assistant bridge for Classic AmigaOS 3.x and 68000 or better.
AmiHASSService owns the connection, credentials, selected-entity cache and
serialized HTTPS requests. AmiHASSPrefs is its Workbench configuration/status app.
Other Amiga applications use the public local message-port SDK independently
of NovaDock. NovaDock HASS tiles are one client.

Each HTTP request runs `C:curl` through the small AmiHASSFetch helper installed
next to the service; selected entities that are due together are read in one
batched request. Entities already held are read conditionally: when their
last_updated is unchanged, Home Assistant answers with a few bytes instead of
the full state. Dashboard camera tiles request 96x60 thumbnails; the camera
viewer keeps 320x200. On 68000 and 68020 machines the HTTP time limits are raised
automatically for slower TLS handshakes.

REST polling is the default. Version 0.3 optionally keeps a selected-entity
WebSocket connection, with REST fallback. Control QA includes shared samples and an approved real heater,
color light and dimmable light; other device integrations remain unverified.

## Install

Unpack into a separate drawer and double-click Install. Installer 42+
is required. Novice uses standard service/Preferences/startup drawers, enables
boot startup and shows the final results page. Other modes can
choose all three drawers. Every mode offers Update or Uninstall when installed.
Updates preserve configuration and restart the service only if it was running;
the results page reports restarted or left stopped. Fresh-install results show
the installed Preferences path for configuring and starting the service.
Uninstall stops the service, removes both applications and offers Keep settings
or Remove settings. Unrelated files remain.

Open the installed AmiHASSPrefs (SYS:Prefs/AmiHASSPrefs by default), choose Server Settings, enter an HTTP or HTTPS server origin and import a
long-lived Home Assistant token from a local text file. The server URL, token,
poll interval (30 seconds by default), boot startup and component paths are saved in
ENVARC:AmiHASS/AmiHASS.cfg and the token in ENVARC:AmiHASS/AmiHASS.token. Both
are local plain text; AmiHASS.cfg never contains the token, and the service
removes the boot-time copies in ENV: (RAM). Exported SDK messages and
diagnostics reports never contain the token. An https:// URL enables encryption; an http:// URL
uses unencrypted HTTP. HTTPS uses normal certificate
verification and requires compatible C:curl, CA certificates and a correct clock.
For an existing NovaDock HASS setup, re-enter the server and import the token;
legacy NovaDock credentials are not silently copied or removed.

Use Server Settings to edit the server and token. Its Save button
persists those fields, then closes the window. Its Close button discards edits.
Test connection has a separate section and shows success or failure in the
Connection row. General Settings keeps poll-interval edits in memory until the
main Preferences Save button writes them with the boot-start choice. Close, Escape and the window close gadget
discard unsaved edits. Start and Stop control the service immediately; opening
Preferences does not start it. Closing Preferences leaves a running service active.
The service survives closing/restarting NovaDock and starts on demand from an
installed client. No device command is sent on startup or when saving settings.

## Build and test

Provision a baseline Amiga 68000 C compiler and NDK first. With vbcc configured:

```sh
make VC='vc +aos68k'
make check
python3 tools/make_icons.py
python3 tools/stage.py /your/builds/AmiHASS
```

A local.mk may set VC/CFLAGS and is ignored by Git. The sources do not require
an adjacent NovaDock checkout. Public SDK files and a C example are under SDK/.
Compilation and portable tests do not replace native install/update/uninstall,
UI and real-device validation.

## Shell interface

```text
AmiHASSPrefs CONFIG
AmiHASSService STATUS
AmiHASSService QUERY light.desk
AmiHASSService ACTION light.desk Toggle
AmiHASSService STOP
Run >NIL: AmiHASSService SERVICE
```

QUERY and ACTION are asynchronous acceptance/cache interfaces. A first QUERY may
return Updating; poll again after the service has fetched the entity. ACTION is
permitted only after a fresh state query and must be followed by state readback.
Scenes and scripts support Activate (also Run) and Confirm and run, which asks first on the Dashboard. Automations support Trigger and Toggle. Climate entities also take `HVAC <mode>` for each mode the entity lists (off, heat, cool, auto, heat_cool, dry, fan_only). Selects take Next option, Previous option and `Option <name>`; numbers and input numbers take `Value N`; humidifiers take `Humidity N`; vacuums take Start, Pause cleaning, Stop, Return home and Locate; valves take Open valve, Close valve, Stop valve and `Position N`; alarm panels take Arm home, Arm away, Arm night and Disarm (Disarm follows the Script doors setting, like locks); a panel that needs a code asks for it on a keypad on the Dashboard each time (ARexx: `ACTION entity Disarm 1234`), and the code is used for that one request and never stored. Sensors remain read-only. Climate target temperature is available through Controls when the entity reports a single target, bounds and step.

## Limits and privacy

One server per service, 32 to 128 selected entities (sized from free memory),
eight queued actions, 16-entry catalogue pages, 64 KB HTTP response cap (256 KB
for camera images and history) and 95-character entity IDs. Duplicate
entity selections share a reading. Clients use SUBSCRIBE and renew at
least once a minute; inactive leases expire after two minutes. UNSUBSCRIBE or
closing the SDK client releases its leases. Legacy recurring QUERY remains supported. No clients
means no selected-entity HTTP polling; the background service still runs.
Network failures back off and clear queued controls. They are never replayed
when the connection returns. Unavailable/stale entities cannot be controlled.
The local IPC interface trusts local Amiga programs. It does not expose a TCP
listener and cannot isolate credentials from other local OS programs.

See docs/SDK.md for ABI, ownership and example usage. MIT licence; see NOTICE.

Concurrent catalogue browsers share four bounded, 60-second page-cache entries;
another client cannot cancel an in-flight catalogue page.

## Development direction

See [roadmap](TODO.md) and [ami2ha design review](AMI2HA-REVIEW.md) for
native networking details and selected-entity update policies.

Server Settings infers encryption from the URL and tests the authenticated
Home Assistant API. Save works even when connection testing fails. Network shows whether
the TCP/IP stack has an active, non-loopback interface with an assigned IPv4
address; a resident socket library or an Up interface without an address does
not count. Server shows Not Configured, Not Connected,
Connection Failure or Connected. Credentials can still be removed offline.

Show server logs opens local AmiHASS service and connection diagnostics. Use
Refresh for new entries. Logs are temporary, rotate at 32 KB, and exclude tokens
and response bodies. You can also open the viewer with `AmiHASSPrefs LOGS`.

Enter a token directly in the Token box, or use Import token to fill it from a
text file. Test connection and Save use the current box contents. Close discards
unsaved edits.

Start is disabled while the service runs; Stop is disabled while it is stopped.
Save is independent of connection testing. Diagnostics shows bridge traffic,
polling, action attempts, caches and retry state since service start. About
shows the app purpose, author, website and GitHub page.

All persistent bridge settings use ENVARC:AmiHASS/AmiHASS.cfg: server URL,
poll interval, boot startup and installed component paths; the token is in
ENVARC:AmiHASS/AmiHASS.token. Preferences replaces both together, so URL and
token always match. Existing AmiHASS connection/path files and tokens stored in
older cfg files migrate on the next successful save; a damaged legacy path keeps
its default. Uninstall offers Keep settings or Remove settings for these files.

General Settings > Script doors (off by default) controls who may unlock or
open locks and open or position covers. Off: only the Dashboard, which asks
for confirmation first; ARexx, `AmiHASSService ACTION` and other SDK clients
get AMIHASS_UNSUPPORTED with state `confirmation_required`. Locking and closing
are always allowed.

HTTP 502, 503 and 504 are reported as "Proxy reached; Home Assistant behind it
did not answer" rather than a generic server error. The bridge holds 48
selected entities on small machines and up to 64, 96 or 128 with 1, 2 or 8 MB
free (32 in Low memory mode); further requests get AMIHASS_BUSY with state
`selection_full`, the Dashboard shows "Entity limit reached", and Diagnostics
lists the limit and the refusal count.

Novice installation uses default drawers, enables boot startup and shows
the final results page. Fresh installations then offer to open Preferences. Other modes can choose service, Preferences
and startup drawers. All modes offer Update or Uninstall when installed.
Updates preserve settings, restart a previously running service and leave a
stopped service stopped. The results page reports the service state and the
installed Preferences path for first-time configuration.

## Device browser

AmiHASSBrowser is installed directly as SYS:Utilities/AmiHASSBrowser with its
own Workbench icon. Launch it directly or use Device Browser in Preferences.
It defaults to Devices and reports service/server connection problems
explicitly. Search by device name or ID, select a row and choose Open to browse
that device's entities. Back returns to the device search. Devices with exposed
state-bearing entities are listed; All entities includes helpers and entities
without an associated device. Start/configure the service in Preferences.

Search filters names and IDs on the server. In entity views, Type accepts any
Home Assistant domain, including custom integrations; All removes that filter.
Previous/Next fill a screen of rows using small bridge pages. Refresh updates the catalogue snapshot.
Discovery does not subscribe to every returned entity. Only four small pages
are cached, rather than the full server catalogue.

Open on an entity reads its state, unit, device class, timestamp and cache age.
These are one-shot reads using the shared cache; the open detail window asks
periodically and the service limits HTTP reads to its configured poll interval.
Closing details leaves no Browser polling lease. Other clients' subscriptions
continue normally. Full arbitrary attribute JSON is not yet in the live record.

The view selector offers Devices and All entities. To enable sample data,
choose Sample data from Data source in the main Preferences window, then Save.
Start the service if stopped. The running bridge supplies four simulated devices
and 22 entities to all clients without networking, a token or Home Assistant.
The Browser is read-only, but client apps can control sample lights, switches,
fans and other supported domains using the normal SDK. Turn on, Turn off and
Toggle change shared state immediately; other clients see the new state on QUERY.
Scene/script activation is also simulated. Unsupported controls remain rejected.

Sample entities use reserved `.amihass_sample_` IDs, distinct from real entities.
Changes last until bridge reload/restart; configuration stores the mode, not
simulated states. Disabling sample mode restores the saved real server settings
and rejects sample controls. Saving/reloading drops old queues and caches. An
already-running real request may finish; no new real requests are started in
sample mode. The Browser resets its catalogue when the source changes.
The sample banner and Preferences server row identify the simulation explicitly.
The Browser sends only STATUS, DEVICES, LIST and QUERY, never actions or tokens.

Server Settings includes Automatic reconnect and Retry every: 1, 10, 15 or
30 minutes. The selector is disabled when automatic reconnect is off. Settings
are saved in AmiHASS.cfg; older cfg files default to automatic retry every
minute. An initial connection attempt is still made when the service starts.
After a failure with retries off, save settings, restart the service or run
AmiHASSService RECONNECT to try again. Failed device actions are never replayed.
Clients can distinguish a running bridge from an established Home Assistant
connection through STATUS. The service checks server reachability when idle.

## Connection-test troubleshooting

Preferences supports connection tests when launched from Workbench or Shell.
Worker preparation failures and request timeouts have separate messages.
The test uses the same 10-second connect and 30-second request limits as the
service, with a 40-second worker watchdog.

If letters unexpectedly appear uppercase with Caps Lock off, tap and release
both Shift keys inside Workbench. A held Shift modifier can also turn digits
into symbols and make Shift+Backspace clear a line.

Low memory mode (General Settings) defaults to off. Apply keeps the draft;
Preferences Save stores it in AmiHASS.cfg and reloads the bridge. When enabled,
the service asks AmigaOS to reclaim unused libraries after 30 seconds without
a completed request, waiting for bridge downloaders, connection tests and queued
work to finish. This is a system-wide Avail FLUSH operation: in-use libraries
remain loaded, but subsequent requests may need to reload unused libraries.
Active traffic can postpone reclamation. It reduces idle RAM, not peak HTTPS
memory. Reclamation results appear in server logs. Existing cfg files default off.

The Browser fills its visible list by combining bounded 16-record bridge pages
(up to 32 displayed records). Device names prefer the user-assigned name, then
the integration's device name. Devices without either use a representative
entity's friendly name; their registry IDs remain internal to device selection.

Device search matches all entered words, so "living room light" also finds
"Living Room 1 Light" names.

## Dashboard

Open SYS:Utilities/AmiHASSDashboard. Configure/start the shared bridge in
Preferences first. Layout selects 1-4 columns and 1-6 rows, with at most 24
cells. Previous rows/Next rows scroll a row at a time when the complete grid
will not fit. Normal refresh reads only visible tiles; explicit actions still
receive readback. The app uses native 24x24 device artwork and Workbench colours.

Click an empty cell to add a tile:

1. Choose a type such as Light, Switch or Sensor.
2. Filter and select a device; only devices with that type's entities appear.
3. Select the matching entity belonging to that device.
4. Choose its click action and label, then Apply.

All / Back on the device page skips the device filter and lists every entity
of the selected type, including helpers without a device. On the entity page
it returns to devices. All types includes custom integrations. Each request is
paged through the bridge, and the same filtering works with sample data.

Available click actions follow the bridge policy: Read only always, and Toggle,
Turn on, Turn off, Activate, Confirm and run (scenes and scripts) or another
action of the device type where supported. The History graph readings draw the
last 24, 6 or 1 hours of a numeric entity under its value, refreshed every
five minutes from the service (one graph at a time; up to 64 points). The
lowest and highest value show beside the graph when the tile is wide
enough, and times the entity was unavailable leave a gap. A sensor that
updates so often that its history exceeds one 256 KB response is read in 4
(or 12) smaller pieces automatically. Unsupported sensor, cover, lock
and unsupported controls are not offered. Climate single-target temperature is available through Controls. Unavailable/stale/disconnected readings
cannot send actions. The bridge rejects sample IDs in real mode. A tile action
is sent once and its displayed state changes only after bridge readback.

Edit mode changes tile clicks to settings; Done returns to normal controls.
In Edit mode a tile can also be dragged with the mouse: drop it on an empty
tile to move it there, or on an occupied one to swap the two (Undo reverts
it, Save keeps it).

The Help key opens the AmigaGuide manual (AmiHASS.guide, installed in the
AmiHASS drawer) in MultiView, in the Dashboard and Device Browser; the Dock
has AmiHASS Help in the Tools menu.
Tile settings change the label/action and move the tile to a row and column.
Moving swaps the destination occupant, preserving both tiles. Clear removes a
tile. Layout adds/removes rows or columns, refusing any resize that would discard
an occupied cell; move or clear those cells first. Layout provides previewable Lighting, Climate and Overview sample templates.
Use requires a preview and replacement confirmation in Sample data mode.
Save persists edits; Close offers Save, Discard or Keep Editing. Layout Undo
does not undo controls already sent.

ENVARC:AmiHASS/Dashboard.cfg contains layout dimensions, labels, IDs and actions;
no credentials. It also stores read-only mode and per-tile refresh/display/timestamp
options. Read-only mode disables Dashboard actions while allowing state inspection
and layout editing. Service settings remain in AmiHASS.cfg. Old six-tile files load
as 2 columns/3 rows with Read only actions until explicitly configured. Saving
migrates the file format without changing the app version. Updates use temporary
replacement/rollback files. Uninstall Keep settings preserves the layout;
Remove settings removes it.

Pages has timing for each page. Refresh sets how often its Automatic tiles are
read: Normal (the service default), Fast (5 s), Slow (1 minute) or Very slow
(5 minutes); tiles with their own refresh keep it. Show from and Show until
make the Dashboard open that page during those hours (until may be past
midnight; the same hour twice means all day), at start-up and when the hours
begin, after a minute without input and never while editing. Set timing saves
it in ENVARC:AmiHASS/DashboardPages.cfg; a rename keeps it, a removed page
drops it, and Remove settings deletes the file.

Dashboard subscribes to visible tiles only and releases
leases when tiles are hidden or the app closes. Shared entities use the fastest
active interval; clients renew leases within two minutes.

## ARexx

The running bridge exposes the optional AMIHASS ARexx port. Run RexxMast and use
OPTIONS RESULTS followed by ADDRESS AMIHASS. STATUS, DEVICES, LIST, ENTITIES,
QUERY and ACTION use the shared provider, including sample controls. See
../ARexx/README.MD for grammar, return codes and runnable scripts.
WAITEVENT lets a script wait until Home Assistant changes an entity, for
example a door sensor or an `input_text` that an automation sets to send a
message to the Amiga. ATTRIBUTES and HISTORY read full attributes and recent
numeric history.

## NovaDock integration

Use NovaDock’s HASS plugin for dock access to AmiHASS. It uses the shared
bridge and supports both Server data and Sample data.
See https://novadock.retrocs.org/.

## Server connections

Server Settings lists saved connections on the left. Select one to edit its
name, URL, token and automatic retry settings on the right. New creates an
unsaved connection. Switching rows keeps edits in memory without changing
the running bridge. Save stores and activates the selected connection, then
closes the window. Close discards unsaved edits. Delete asks for a second
press before removing a saved connection.

Connect starts the saved server connection and is disabled while connected.
Sample data does not need a server connection. Health checks the installation.

The service talks to one server at a time. Saved addresses and retry settings
are in ENVARC:AmiHASS/Servers.cfg; tokens are kept in Servers.token. The active
connection remains in AmiHASS.cfg and its separate token file. Removing
settings during uninstall removes saved connections too.

## Languages

All windows follow the Workbench language through locale.library. Swedish
(svenska) and German (deutsch) catalogs are installed to LOCALE:Catalogs;
with another language, or without locale.library, windows stay in English.
Entity names, states from Home Assistant and actions sent to it are never
translated. Some German labels are longer than the English ones and may be
cut in narrow buttons. Translators edit locale/texts.tsv (window text) and
locale/<language>.ct and run tools/make_catalogs.py, which refuses a
translation that changes the % format codes; new window text must be added
to texts.tsv or locale/untranslated.txt or the tests fail.

## Native HTTP

The service keeps one connection to Home Assistant open between requests
(HTTP/1.1 keep-alive; HTTPS through AmiSSL, checked against AmiSSL's
certificates) in a second copy of AmiHASSService that it starts itself. This
saves starting curl and a new TLS handshake for every request, and curl is
then only needed as a fallback. If that connection fails at the TLS level
(for example AmiSSL does not trust a certificate that curl's bundle does) or
answers garbled three times, the service switches back to curl until
Reconnect or a settings change. Set the variable with
`SetEnv SAVE AmiHASS/HTTP curl` to always use curl; delete it to use native
HTTP again. The native worker is not used in Low memory mode.

## Brightness, color and temperature

Select a configured Dashboard tile, then choose Controls. Supported lights expose
brightness from 0–100% and, when available, RGB sliders with a hexadecimal color
preview. Releasing brightness sends one change. RGB changes stay as a draft until
Set Color. Close does not undo a command already sent. A zero brightness switches
the sample light off; color changes turn it on.

Climate entities with a single reported target, minimum, maximum and positive
step expose a Target field and Set Temperature. Values use the server's native
C/F units. The bridge reads the entity unit or fetches Home Assistant's
authenticated configuration; target controls stay disabled until units are known. Targets outside the
reported range or step are rejected in both the Dashboard and bridge. Setting a
target does not change HVAC mode. Dual heat/cool target ranges are not supported.

The service validates capabilities from its latest state read. Unsupported,
stale or unavailable controls are rejected. Unknown brightness/color readings
stay unknown; a color draft is not a confirmed state. Commands use the existing
serial worker, with normal state readback. No request is sent while dragging.
Shared samples support the living-room light's brightness/RGB, the desk light's
brightness, and hallway heating's 7–35 C target in 0.5 C steps.

Native control QA passed against an approved real heater, color light and
dimmable light. Original target, color, brightness and on/off settings were
restored and verified. Loopback and dialog tests cover rejected commands,
connection failure, delayed/unknown readback and confirmation timeout.

Control confirmation waits for a fresh reading that matches the requested value,
with one-percent brightness rounding and three RGB channel values of tolerance.
Temperature confirmation allows 0.01 degrees of numeric rounding. A differing or
unknown state stays pending; after 45 seconds the UI reports a timeout instead
of confirming success. These checks never resend the action automatically.
Sample lights remember their last nonzero brightness when switched back on.

Tokens are hidden by default in Server Settings. Choose Show to reveal or edit
the token, then Hide to mask it again. Imported tokens stay hidden. Connection
tests hide the field automatically; closing the panel clears its token buffers.

Closing a modified Dashboard through Close, Escape or the window close gadget
asks Save, Discard or Keep Editing. A failed save keeps the layout in memory and
allows retry. The parent window stays open while this choice is shown, so even
an allocation failure cannot silently discard unsaved edits. Device commands
already sent are not undone by discarding a layout.

## Installing network dependencies

See [REQUIREMENTS.txt](../REQUIREMENTS.txt) for curl and AmiSSL downloads,
installation steps, CPU compatibility, certificate setup and token-free HTTPS
checks. AmiHASS invokes `C:curl`; AmiSSL is required by curl builds that use it
for TLS. Sample data needs neither dependency.

## Browser room selection

Use **Room → Choose room...** to load available Home Assistant rooms through
the central bridge. The selector displays room names and retains their stable
IDs. Choose a room and **Select**, or choose **All rooms** to include devices
and entities without a room. **Previous/Next** browse additional 16-room pages;
**Refresh** reloads the current page. **Close** keeps the previous selection.
Room, type and name filters combine, and Back preserves them. Shared Sample
data offers House, Kitchen, Living room and Study without network requests.
If the service is stopped or unreachable, All rooms remains available.

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

Offline tile edits retain a saved action when capabilities are unavailable.
Choose Keep saved action to retain it, or Read only explicitly to clear it. After a real unlock/open confirmation,
the Dashboard checks the same server, fresh state and current capabilities;
changes, cancellation or failed checks send no action. HTTP 401/403 pauses
automatic retries. Correct and save Server Settings, restart the service, or
request AMIHASS_RECONNECT to retry.

### Resource costs and background updates

Diagnostics cycles through Totals, Clients, RAM and HTTP. HTTP shows the last,
longest and total request duration in whole seconds, serialized request bytes,
response-body bytes and peak observed Fast RAM drop. These are application-level
measurements, not TLS/network traffic or per-process memory allocations.

Dashboard polls visible tiles only. Inactive windows request four times the tile
interval (up to one hour; the background default is two minutes). Returning to
the window restores its normal interval. Hidden rows/pages and closed clients
release their leases; shared entities use the fastest remaining client interval.
A client that stops renewing slows after 90 seconds and expires after 120 seconds.
Cached values stay visible after disconnection, labelled Offline or Stale with
their age. Expired explicit refresh values cannot send actions until refreshed.

Use Edit Tile > More > Shift direction > Shift to swap a tile left, right, up or
down. Both occupied tiles are preserved; Undo restores the prior layout. Device
controls show supported actions, or read-only details for sensors. Brightness,
color and temperature controls disable when their capability disappears.

General Settings > Dependencies checks local C:curl availability, whether the
AmiSSL master library opens, configured CA paths/AmiSSL:Certs and the clock.
These checks do not contact a server and cannot prove curl/TLS compatibility or
certificate trust. See REQUIREMENTS.txt; Test connection verifies the real setup.

### Responsive controls and save recovery

Brightness changes settle for 0.3 seconds before sending. Only the latest draft
value is retained, including changes made while an earlier action awaits
confirmation. RGB sliders update a local preview; Set Color sends the latest
settled color. Closing the panel discards any unsent draft.

The bridge prioritizes action confirmation reads, queued actions and explicit
state reads before catalogue work. An HTTP request already running completes
first. Background polling and catalogue requests still share the remaining
scheduler time.

Unavailable states, missing entities (HTTP 404) and invalid individual state
responses use per-entity exponential polling backoff, bounded by one hour and
eight doublings. Healthy entities keep their intervals. A successful available
state resets backoff. Explicit fresh reads can retry immediately; ordinary
cache reads cannot defeat backoff. Network and authentication errors retain
their connection-wide handling.

Dashboard's hint line estimates visible unique entities and polling requests per
minute, using the fastest interval for duplicate tiles and the service default.
Sample data reports zero HTTP polling. This estimate excludes connection checks,
action readbacks, browsing and other clients; paging changes the visible set.

Saving writes a temporary file before replacing the current file. If interrupted
between the rename steps, loading recovers a validated .old backup when the main
file is missing. Incomplete temporary files are never loaded. Write/close errors
leave the previous file intact; unsuccessful rollback leaves the backup available
for recovery. Fix the storage problem and save again to complete replacement.

### Polling profiles and background budget

General Settings offers Economy (60 seconds), Balanced (30 seconds), Responsive
(10 seconds) and Custom. These set the service default; explicit tile intervals
still take precedence. Background/min is 0 for unlimited or 1-120 scheduler
attempts per minute. It limits automatic state polls, catalogue work and periodic
connection/config checks. Actions, their confirmation reads, explicit fresh reads
and initial/reconnect probes retain priority and do not consume this budget.
A low budget can delay catalogue pages until the next minute. HTTP Diagnostics
shows the current limit, usage and reset delay. Existing configurations default
to unlimited; the new value stays in AmiHASS.cfg.

Media tiles offer capability-filtered Play, Pause, Previous and Next actions,
plus a Controls window with title, artist and volume. Volume changes retain only
the latest settled value. Play/Pause and Volume wait for matching state readback;
Previous/Next needs a changed reported track title to confirm. Unsupported or
disconnected controls are disabled. Read-only mode sends no media actions.

The device picker has an explicit Search button. Room, category and the device
search remain available between additions during the current Dashboard session.
Changing between Sample and Server data clears room/search to avoid stale
filters. No additional service configuration file is created.

When Preferences or Dashboard recovers a complete .old backup, a notice explains
what was recovered. The recovered draft is marked unsaved. Save keeps it; closing
without saving leaves the backup for the next start.

### Live updates, labels and gauges

Enable **Live updates** in General Settings for selected-entity WebSocket updates.
REST is the default and fallback; Low memory mode uses REST. All clients share
one central cache. See [Selected updates](SELECTED_UPDATES.md).

Use **Label** in the Device Browser or Dashboard picker alongside room, type and
search filters. Leave it empty for all labels. Sample labels: `amiga`, `lighting`,
`climate` and `media`. Labels filter discovery, not access to saved entity IDs.

For numeric sensors, choose **Gauge** under a tile’s Main reading and enter its
minimum and maximum. Stale, offline or unavailable readings have no filled bar.
Names use Amiga-compatible Latin-1 characters and readable punctuation fallbacks;
unsupported glyphs display as `?`. Transport identifiers retain their UTF-8 bytes.

### Camera snapshots

Choose a **Camera** in the Dashboard picker and set its click action to
**Snapshot**, or use **Snapshot** in the Device Browser’s camera details.
The viewer defaults to manual refresh. Optional intervals are 30 seconds,
one minute and five minutes; automatic refresh pauses while the window is
inactive and stops when it closes. Failed refreshes leave the displayed image
in place with a failure message. A snapshot is a still image, not live video.

In the Device Browser’s live entity details, **Attributes** opens a list of
every attribute the entity reports (up to 64), with its type when it is not
text or a number. Sensors and numbers also show the low, high and latest value
of the last 24 hours. **Refresh** reads them again.

The central service retrieves an authenticated image from Home Assistant and
shares a bounded cache between clients. No camera URL or token is passed to the
viewer. Downloads allow at most 256 KiB and images up to 640×480. The request asks
Home Assistant for 320×200; integrations that cannot return a small enough image
are rejected. Camera or image failures do not disconnect healthy devices.

JPEG and PNG need matching Amiga picture datatypes. Sample data supplies an ILBM
image and works offline. The viewer releases its image and temporary RAM files
on close; the service drops an unused snapshot cache after 30 seconds.

### Buttons, labels and camera previews

Use the Button or Input button category and choose **Press**. The Dashboard
reports **Delivered to Home Assistant** after that client’s request succeeds;
it does not invent an on/off state or claim that an automation finished.
Unavailable buttons cannot be pressed. Sample buttons are included offline.

**Choose label** opens a paged list with All labels, Previous, Next and Refresh.
Selection uses stable IDs even when names match. Labels work with room, type and
search filters. Changing data source clears the previous label selection.

For a selected camera tile, **Refresh photo** loads a small preview directly into
the tile. Clicking its Snapshot action opens the full viewer. Previews show image
age, use a compact 16-colour bitmap and refresh only on request. Up to four
previews are kept, or one in Low memory mode; full image decoding resources and
RAM files are released after each preview. Closing a page or viewer releases its
snapshot lease. Abandoned queued requests are cancelled; an already-running
bounded download may finish, but its abandoned image is discarded. Low memory
mode frees the encoded image immediately when its last client leaves.

Diagnostics has Transport and Cameras categories. Check the transport, watched
entities, connection attempts, updates, last error, image traffic, cache size and
reported decoder memory pressure. The decoder metric is an observed whole-system
Fast RAM drop, not an exact allocation measurement for one process.

### Recovery and installation health

Preferences shows the remaining automatic retry time. **Reconnect now** requests
a new connection attempt using saved settings without saving the Preferences
draft or dropping client selections. It is disabled while the service or network
is unavailable, in Sample data mode, or when no server is configured.

**Health Check** checks the service port, installed service executable, central
configuration path, curl, AmiSSL, certificate paths and network readiness.
**Check again** repeats these local checks. It does not contact Home Assistant
or operate devices; use Server Settings → Test connection to verify TLS and
authentication. Tokens and server URLs are omitted from this screen.

Dashboard values show when they were last confirmed by the bridge. Cached
values retain their age after failures and show Stale or Offline. Changing the
server address clears old values and controls until the new server supplies data.

In tile settings, **Move / more → Swap** exchanges complete tiles between any
two pages, preserving actions, refresh intervals, gauges and display settings.
Move and Duplicate still require an empty destination. Shift swaps neighbouring
tiles. Changes remain a draft until Save and can be undone.

## Shared installation components

The service, helpers and manual are installed in SYS:AmiHASS/. Preferences
remains in SYS:Prefs/; Browser and Dashboard remain in SYS:Utilities/.
Expert installation lets you select another parent drawer and creates an
AmiHASS drawer inside it. Novice and Average use the default location on a
fresh installation. Updates preserve a saved custom location; the old default
SYS:Utilities/AmiHASS is moved to SYS:AmiHASS. Saved settings and tokens stay
in ENVARC:AmiHASS/. Uninstall still offers Keep settings or Remove settings.

## Connection and resource overview

Preferences shows discovery separately from the connection: Not requested,
Loading devices, Ready or Discovery failed. A failed discovery can be retried
with Refresh in Device Browser; server logs contain diagnostic details.
The Contact row identifies the saved server and the age of the last successful
server response. Sample data is marked local. Resources shows cached entity
slots, pending actions/active requests and tracked RAM in kB. Tracked RAM covers
the broker, selected-entity/lease buffers and encoded camera cache; it is not
total system usage and excludes the transport worker and TLS libraries. Low
marks Low memory mode. Full measurements remain available in Diagnostics.

## Dashboard favourites

In the Dashboard device picker, select an entity and click Add fav. Use
Favourites to reuse a saved entity or remove an entry. Up to 32 choices are
stored in ENVARC:AmiHASS/Favourites.cfg. Lists are separated by server URL,
with a separate Sample data list. Selecting a favourite does not send an action;
the tile editor checks current capabilities before offering controls. Keep
settings preserves favourites during uninstall; Remove settings deletes them.

## Sharing Dashboard layouts

Open Layout, then Export/Import. Export writes an .ahdash file with pages,
tiles, actions, display settings and color presets. It contains no server URL,
token, current device readings or camera images. Import previews the page and
preset counts and asks before replacing the draft. Save the Dashboard to keep
an import. Entity IDs must exist on the receiving Home Assistant instance.
Camera auto-refresh settings and favourites are separate app settings.

## Creating a room page

Open Layout, Templates, From room. Choose a room, optionally choose a device
type, then Preview. The bridge reads only that room's matching entities.
The preview has up to 24 unique entities; use Preview page to inspect them all.
The count shows when more entities exist. Use Page asks before replacing the
current page and verifies the source again. Generated tiles have no click
actions assigned. Adjust the tiles and save when ready. Existing Dashboard
read-only mode is preserved. Cancellation or a failed request keeps the page.

## Availability and camera refresh

Offline means the bridge has no server connection. Unavailable means that
entity has no usable current state. No value means no successful reading yet.
Cached marks a retained reading that
is no longer current; its age remains visible. Device actions stay blocked
when a value is stale or unavailable.

Camera previews show their age since the last successful image refresh.
Refresh photo fetches a preview manually. Layout, Cameras offers Manual
(the default), 1 minute, 5 minutes or 15 minutes. Automatic refresh affects
only visible camera tiles and pauses when the window is inactive, editing
or disconnected. Only one preview is fetched at a time; failures keep the
previous image and wait for the next interval. Settings are stored in
ENVARC:AmiHASS/CameraRefresh.cfg; invalid settings fall back to Manual.
Keep settings preserves this choice during uninstall; Remove settings deletes it.
