# AmiHASS 0.2 - Amiga Home Assistant

A standalone Home Assistant bridge for Classic AmigaOS 3.x and 68000 or better.
AmiHASSService owns the connection, credentials, selected-entity cache and
serialized HTTPS requests. AmiHASSPrefs is its Workbench configuration/status app.
Other Amiga applications use the public local message-port SDK independently
of NovaDock. NovaDock HASS tiles are one client.

This initial version polls REST state endpoints; it does not keep a WebSocket
connection. Control QA includes shared samples and an approved real heater,
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
ENVARC:AmiHASS/AmiHASS.cfg. This is local plain text. Exported SDK messages
never contain the token. An https:// URL enables encryption; an http:// URL
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
Scripts/scenes support Activate; sensors, locks and covers remain read-only. Climate target temperature is available through Controls when the entity reports a single target, bounds and step.

## Limits and privacy

One server per service, 48 selected entities, eight queued actions, 16-entry
catalogue pages, 64 KB HTTP response cap and 95-character entity IDs. Duplicate
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
possible future native networking and WebSocket push updates.

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

All persistent bridge settings use ENVARC:AmiHASS/AmiHASS.cfg: server URL, token,
poll interval, boot startup and installed component paths. Preferences edits this
file; existing AmiHASS connection/path files migrate on the next successful save.
Uninstall offers Keep settings or Remove settings for this file.

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
Turn on, Turn off or Activate where supported. Unsupported sensor, cover, lock
and unsupported controls are not offered. Climate single-target temperature is available through Controls. Unavailable/stale/disconnected readings
cannot send actions. The bridge rejects sample IDs in real mode. A tile action
is sent once and its displayed state changes only after bridge readback.

Edit mode changes tile clicks to settings; Done returns to normal controls.
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
Remove settings removes it. Dashboard subscribes to visible tiles only and releases
leases when tiles are hidden or the app closes. Shared entities use the fastest
active interval; clients renew leases within two minutes.

## ARexx

The running bridge exposes the optional AMIHASS ARexx port. Run RexxMast and use
OPTIONS RESULTS followed by ADDRESS AMIHASS. STATUS, DEVICES, LIST, ENTITIES,
QUERY and ACTION use the shared provider, including sample controls. See
../ARexx/README.MD for grammar, return codes and runnable scripts.

Installation and update always finish with an Installer results page, including
Novice mode. It lists the service and Preferences locations, Browser/Dashboard
Utilities drawer, boot startup status and settings drawer, plus manual setup
instructions. Updates report whether the service restarted or remained stopped.
Fresh installations offer to open Preferences after the results page. Updates leave it closed.

Save shortcuts: Ctrl-S or Right-Amiga-S saves in windows with a Save button.
Apply accepts a subwindow draft; Close discards it.
Connection tests show elapsed time and can be cancelled with Close.
The service prioritizes queued actions and their state readback, alternates
background discovery with due state reads, and checks idle connectivity last.
An HTTPS request already in progress remains bounded by its timeout.

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
