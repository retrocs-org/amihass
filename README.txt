# AmiHASS 0.2

A shared Home Assistant bridge for Classic AmigaOS 3.x and 68000 or better.
AmiHASSService runs the bridge, AmiHASSPrefs manages its settings and status,
AmiHASSDashboard displays configurable tiles, and the SDK connects other
Amiga applications.

## Install

Run `Install`. Novice mode uses the standard drawers, enables boot
startup and shows the final results page. Other modes let you
choose service, Preferences and startup drawers. The results page shows where
to open Preferences to configure the server and start the service.

Enter the server URL and a long-lived access token in Server Settings, or
import the token from a text file. HTTPS enables encryption automatically.
Test connection shows its result; Save works even when the server is offline.
A configured TCP/IP stack, compatible `C:curl`, CA certificates and a correct
clock are required for HTTPS communication. AmiSSL is also required when curl
uses its TLS backend. Follow [the dependency installation guide](REQUIREMENTS.txt)
for download links, installation steps, CPU requirements and connection checks.

All persistent settings, including the token, are stored as local plain text in
`ENVARC:AmiHASS/AmiHASS.cfg`. Existing AmiHASS settings migrate on saving.
When already installed, every Installer mode offers **Update AmiHASS** or
**Uninstall AmiHASS**. Updates preserve settings and restore the previous service
state. Uninstall stops the service and offers **Keep settings** (the default) or
**Remove settings**.

Preferences includes Start/Stop controls, boot startup, polling settings,
server logs, live bridge diagnostics and About. Close discards unsaved edits;
Save saves and closes. Closing Preferences leaves a running service active.

## Device browser

Open `AmiHASSBrowser` from `SYS:Utilities/` or use **Device Browser** in
Preferences. It defaults to Devices: search by name or ID, select a device and
choose Open to browse its entities. Back returns to devices. All entities also
includes helpers and entities without a device. Type filters entities by domain.
The Room selector offers All rooms or Choose room. Choose room loads the
bridge’s room list, with Previous/Next for additional rooms; select a room and
press Select. Names are displayed, while stable room IDs filter the results.
The same selector works with shared Sample data.
Room, type and search filters work together in Browser. Dashboard selection
also supports an Area name or ID.
Previous/Next fill a screen of rows using small bridge pages; Refresh updates the catalogue. Open on an
entity shows its state and available SDK metadata through the bridge.

The browser is read-only and shares the service connection; no separate token
is needed. Start/configure the service in Preferences. To simulate devices,
choose **Sample data** in the main Preferences **Data source** selector, then **Save**.
The running bridge supplies shared sample data to every client; it never selects
sample mode automatically when the real server is unavailable.

## Dashboard

AmiHASSDashboard and its Workbench icon install directly in `SYS:Utilities/`.
Layout selects 1-4 columns and 1-6 rows (up to 24 tiles). Smaller screens page
through rows. Click an empty tile to choose a device type, filter matching
devices, select an entity and choose its click action. Sensors are read-only.
Use All / Back to browse a category's entities without selecting a device.

Edit mode changes tile clicks into editing; Done returns to normal operation.
Tile settings edit labels, actions and positions; moving swaps existing tiles.
Clear removes a tile. Resize refuses to discard occupied cells. Save stores
all pages and presets; Close offers Save, Discard or Keep Editing. In normal mode, clicking a
configured tile sends its selected action through the bridge and reads back
actual state. Sample layout provides six example tiles in Sample data mode.
Native device artwork is built into the app; no image loading is required.

Pages opens, adds and renames up to eight named dashboards. Remove a page after
clearing its tiles; at least one page remains. Each page has its own grid and
actions. Older single-page files load automatically. Only the active page is
queried, and switching pages drops its cached values.

New light and climate tiles open Controls by default. Light tiles show reported
brightness; climate tiles show the target and verified C/F unit. Controls offers
RGB presets and up to eight named saved colors, including the supplied presets.
Selecting a preset changes the draft; Set Color applies it. Save Preset adds or
replaces a color by name; Save in the Dashboard keeps it across launches.

Offline tiles retain their last successful value and show its age. Actions stay
blocked while disconnected or stale, until a successful state read. Values are
kept only in memory, scoped to the active page and selected data source.

### Undo, pages and capability checks

The main window's page selector switches directly between named dashboards.
Control-1 through Control-8 (or Right-Amiga with the number) selects a page.
Undo or Control-Z restores the previous dashboard edit, including tile deletion,
move or duplication. It does not reverse device actions. One snapshot is kept
in memory (31,760 bytes on the tested native build); cancelled edits keep the
previous snapshot. Undo after saving makes the restored layout unsaved again.

Tile editing warns when an entity belongs to the other data source and requires
Apply again to keep it. Controls still refuse mismatched sample/server targets.
The Device Browser's entity details show verified supported controls. The
Dashboard picker also offers Capabilities before selecting an entity; it reads
only that entity through the bridge.

### Read-only mode and tile options

**Layout → Read-only mode → Apply** disables all device actions in this Dashboard
and shows **READ ONLY** in the window title. Tile clicks and Details open a
read-only inspector. Layout editing remains available, and other bridge clients
keep their own controls. Use Save to persist the mode; it also travels in backups.

**Edit Tile** includes Refresh, Main reading and Show timestamp. Refresh offers
Service default, 5/10/30 seconds, 1 minute, 5 minutes or 15 minutes. The default
checks the cache every five seconds while the bridge uses its configured server
poll interval. Custom intervals request state freshness at the selected cadence;
network/scheduler delays can make updates later. Shared entities use one server
cache, so faster clients can supply newer data to slower clients. Duplicate tiles
in the same Dashboard share returned readings. Hidden pages/rows do not initiate
regular reads. Visible tiles share subscriptions; hiding tiles or closing Dashboard releases their polling leases.

Main reading offers Automatic, State, Brightness or Target temperature. Unsupported
readings say unavailable. Tile labels remain editable up to 32 characters.
Optional timestamps show when the reading was last fetched by the bridge, using
the Amiga's local clock; they are not the server's last-state-change timestamp.
Controls request a fresh reading before enabling actions.

**Layout → Templates** previews Lighting, Climate or Overview sample layouts.
Use Layout becomes available after Preview while Sample data is active; replacing
the current page requires confirmation. In Server data mode the templates are
preview-only. The other pages are preserved, and Undo can restore the replaced page.

### Backup and tile tools

Open **Layout → Backup** to export all dashboard pages, tiles and color presets
as an `.ahdash` file. Backups contain no server configuration or token. Import
validates a dashboard file and asks before replacing the draft; use **Save**
to keep the imported layout. Existing backup files require replacement confirmation.

In **Edit Tile**, choose **More...** to preview the label and click action, then
move or duplicate the tile to an empty position on any page. Occupied destinations
are refused. Changes remain part of the unsaved dashboard draft.

### Connection errors and client diagnostics

Connection tests distinguish DNS failure, rejected tokens, denied access, missing
API endpoints, redirects, timeouts, TLS/certificate problems and download storage
errors. Each has a short suggested fix. Server logs retain fixed error text and
numeric codes, never tokens or response bodies. Certificate checks remain enabled.

**Diagnostics → Export** writes a fresh `.txt` support report with app/ABI versions,
network state, connection/curl/HTTP error codes and bridge counters. It excludes
server URLs, tokens, client entity IDs and response bodies. Close cancels collection;
failed collection keeps an existing report.

In **Diagnostics**, **Clients / Totals** switches between overall bridge counters
and recent client activity. Clients show request, query, catalogue and action
counts, followed by their selected entities. The same entity can appear under
multiple clients. Activity expires after 120 seconds; this is not a process-liveness
check. Tracking is bounded to 16 recent clients and 64 client/entity selections.
Counts apply while an entry is retained; table reuse starts a new entry. HTTP
polling remains shared; client counters measure local bridge requests.

## NovaDock integration

NovaDock's HASS plugin uses the same local Exec-port SDK, without a separate
server connection or token. Its controls include brightness, RGB color and
supported climate targets. RGB and temperature inputs apply on Return; the
bridge checks capabilities, ranges and temperature steps. Shared sample devices
work across the Dashboard, NovaDock, SDK clients and ARexx scripts.

## Developers

The separate SDK download includes source, headers, a Makefile, API reference and
an independent compiled `AmiHASSExample` client. It demonstrates status,
discovery, state queries and explicit actions. Start with [SDK quickstart](SDK/README.MD).

ARexx scripts use the **AMIHASS** command port for status, discovery, queries
and supported actions. See [ARexx guide and examples](ARexx/README.MD).

The PHP project website is in [public-web](public-web/index.php); see its
README for local preview and hosting. Publication is managed separately.

## Build and test

With vbcc and the Amiga NDK configured:

```sh
make VC='vc +aos68k'
make check
```

Native application and Installer checks use Amiga Workbench.app's isolated
workflow; see [native QA](qa/README.md). Physical-device validation is pending.
See [usage](docs/USAGE.md), [SDK](docs/SDK.md) and [changelog](CHANGELOG.MD).

## Links and license

[Website](https://amihass.retrocs.org) ·
[GitHub](https://github.com/retrocs-org/amihass)

MIT. See [LICENSE.MD](LICENSE.MD) and [NOTICE](NOTICE).

Server Settings includes Automatic reconnect and Retry every: 1, 10, 15 or
30 minutes. The selector is disabled when automatic reconnect is off. Settings
are saved in AmiHASS.cfg; older cfg files default to automatic retry every
minute. An initial connection attempt is still made when the service starts.
After a failure with retries off, save settings, restart the service or run
AmiHASSService RECONNECT to try again. Failed device actions are never replayed.
Clients can distinguish a running bridge from an established Home Assistant
connection through STATUS. The service checks server reachability when idle.

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

Installation and update always finish with an Installer results page, including
Novice mode. It lists the service and Preferences locations, Browser/Dashboard
Utilities drawer, boot startup status and settings drawer, plus manual setup
instructions. Updates report whether the service restarted or remained stopped.
Fresh installations offer to open Preferences after the results page. Updates leave it closed.

Dashboard Controls supports light brightness, RGB color and single-target heater/AC temperature when the entity advertises support. Changes use the shared bridge and confirmed state readback.

Server tokens are hidden by default with Show/Hide. Dashboard closing protects unsaved layouts with Save, Discard and Keep Editing.

Selected-entity push updates were investigated; [findings and measurements](docs/SELECTED_UPDATES.md) describe the required native worker. Version 0.2 retains selected-entity REST polling.

## Release packaging

See [RELEASING.md](RELEASING.md) for the build, verification, preparation and
publication workflow. The main download is `AmiHASS-0.2.lha`; the client SDK
is a separate `AmiHASS-0.2-sdk.zip`. Both include SHA-256 checksum files.

The native SDK also exposes fan speed and cover position/open/close/stop when
the selected entity advertises those features. NovaDock uses these structured
bridge controls. They also work with the shared offline sample fan and cover.
The service remains version 0.2 and its Exec message ABI remains version 1.

## Shared polling and action feedback

Dashboard subscribes to visible entities only. Clients share one cached entity
and the fastest requested polling interval (5–3600 seconds). Changing a page,
scrolling away or closing releases the old selections. SDK clients renew leases
within 120 seconds; abandoned legacy clients expire automatically. Closing a
client releases all of its selections without waiting for a server response.

Tiles show Sending, Waiting for confirmation, Confirmed or Failed. A successful
HTTP request is not treated as device confirmation: a fresh state must match the
requested value. Confirmation times out after 45 seconds; commands are not replayed.

Diagnostics cycles through Totals, Clients and RAM. RAM shows system-wide free
Fast memory, observed HTTP lows, memory after a request, idle reclamation and
largest free block. These are sampled observations; other programs affect them.
Diagnostic exports include the numeric memory counters, without credentials.

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
