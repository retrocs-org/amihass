# Changelog

## AmiHASS 0.3

### Features

- Preview and generate a Dashboard page from a selected room.
- Optional camera refresh intervals for visible Dashboard tiles.

- Source-specific Dashboard favourites with persistent add and remove.

- Saved server sidebar with per-connection settings.
- Installation Health Check and manual reconnect in Preferences.
- Cross-page Dashboard tile swaps with settings preserved.
- Shared camera snapshots and compact Dashboard previews.
- Button and input-button presses with delivery receipts.
- Optional selected-entity live updates with REST fallback.
- Paged label selection in the Browser and Dashboard picker.
- Sensor gauges with adjustable ranges.
- Polling presets and a background request budget.
- Dashboard media playback, track information and volume.
- Local dependency checks in Preferences.
- Dashboard polling estimates and directional tile moves.
- More device types: climate modes, automations, selects, numbers, vacuums, valves, humidifiers and alarm panels.
- Scenes and scripts as Dashboard buttons, with optional confirmation.
- Several entities in one SDK query.
- All entity attributes in the SDK, ARexx and Browser.
- 24-hour history graphs on Dashboard tiles, the SDK and ARexx.
- ARexx WAITEVENT waits for Home Assistant changes.
- Per-page refresh speed and time-of-day pages on the Dashboard.
- Home Assistant persistent notifications in the SDK and ARexx.
- Saved server profiles for quick switching.
- Swedish and German catalogs for all windows.
- Native HTTP with a kept-open connection; curl only as fallback.
- ARexx WAITEVENT on several entities at once.
- History graphs: 6 h and 1 h windows, low/high values, gaps; busy sensors read in pieces.
- Alarm panels that need a code, with a keypad.
- Drag tiles to move or swap them in Dashboard Edit mode.
- AmigaGuide manual, opened with the Help key.
- Release archives compressed (-lh5-) without an external archiver.

### Enhancements

- Direct layout export/import access without server credentials.
- Distinct offline, unavailable and cached tile indicators.

- Discovery progress, server contact age and resource summary in Preferences.

- Shared components use SYS:AmiHASS, with an Expert location choice.

- Connect is disabled while the server is connected.
- Dock integration uses NovaDock instead of a separate helper app.
- Retry countdowns and clearer confirmed or cached tile values.
- Repeatable fixture tests with idle and RAM measurements.
- Transport and camera diagnostics with decoder RAM measurements.
- Smaller preview caches and faster low-memory cleanup.
- Readable UTF-8 names and punctuation on Amiga fonts.
- Device picker remembers room, category and search within a session.
- Recovery notices identify unsaved backup settings and layouts.
- Extended sample-only client and memory stability checks.
- Latest-value brightness and color requests avoid intermediate updates.
- Interactive controls and confirmation reads take priority.
- HTTP timing, byte counts and peak RAM in Diagnostics.
- Slower background polling and clearer stale tile values.
- Capability-aware Dashboard controls.
- Fresh GitHub release tokens generated automatically.
- Due entities are read together in one batched request.
- Small AmiHASSFetch helper replaces reloading the service per request.
- Token kept in memory; no settings file read per request.
- Network checks every few seconds instead of every second.
- CPU-scaled HTTP timeouts for 68000 and 68020 machines.
- Faster Browser and device catalogue templates for large homes.
- Live updates change subscriptions without reconnecting.
- Live updates keep only the attributes AmiHASS reads.
- Dashboard redraws only tiles whose content changed.
- Dashboard, Browser and Preferences sleep until input or a reply.
- Server URLs may use a reverse-proxy path prefix.
- Specific Server Settings errors for the URL and the token.
- Access token stored apart from settings; RAM copies removed at startup.
- Script doors option for lock and cover opening from ARexx and CLI.
- Proxy and gateway errors reported separately.
- Entity limit reported to clients, Dashboard and Diagnostics.
- Entity limit sized from free memory, 32 to 128.
- Unchanged entities are confirmed in a few bytes.
- Smaller camera images for Dashboard tiles.

### Bug fixes

- Device Browser and batched reads accept full-size native HTTP requests.
- Discovery errors keep healthy server connections and tiles running.
- Health Check instructions fit inside the window.

- Server Settings opens correctly and shows saved server profiles.
- Standalone SDK and downloader helpers build correctly.
- Camera preview timeouts release image buffers immediately.
- Server address changes clear values from the previous server.
- Manual reconnect preserves client selections and saved settings.
- Abandoned camera requests stop before downloading.
- Button receipts stay isolated between clients.
- Unavailable entities back off without interrupting healthy devices.
- Interrupted saves recover the last complete configuration or layout.
- Offline tile edits preserve saved actions.
- Door actions recheck connection and state after confirmation.
- Rejected credentials pause automatic retries.
- Portable directory paths in LHA packages.
- Long entity states truncate instead of failing the entity or the catalogue.
- One large entity no longer stops live updates for all entities.
- Volume, positions and climate limits with long decimals are read correctly.
- Climate controls work when the server sends no temperature step.
- Fahrenheit set points are accepted on the step grid.
- Pastel and grey colour changes confirm correctly.
- ARexx ACTION returns a non-zero RC when nothing was sent.
- Trailing slashes in the server URL are removed.
- Abandoned downloader requests no longer leak memory or temporary files.
- Missing curl or a full T: no longer marks the server as down.
- Clock changes at boot no longer expire leases or timeouts.
- Locks and covers no longer open from scripts without consent.
- Remove settings also clears boot-time copies in RAM.
- Damaged legacy path files no longer block saving settings.

## AmiHASS 0.2

### Features

- Capability-gated media playback, track information and volume through the C SDK.
- Shared sample media controls for device-free integration testing.
- Smart locks and motorised doors with shared sample controls.

### Enhancements

- Capability-filtered Dashboard actions and clear door states.
- Confirmation before unlocking or opening real doors.
- Optional Preferences launch after a fresh installation.
- Updated artwork icons for the installer and drawer.
- Shorter Install script and Workbench icon names.

### Bug fixes

- Packaged files use the current date instead of 1980.
- Staging removes obsolete installer names.

## AmiHASS 0.1

### Features

- SDK feature negotiation for rooms, combined filters and subscriptions.

- Room selection with named rooms and paged discovery.

- Shared project logo for the website and About window.

- Shared entity subscriptions with automatic polling cleanup.
- Combined room, device-type and search filters.
- Dashboard sending, confirmation and failure feedback.
- Live RAM measurements and repeatable resource reports.
- Verified release readiness and standalone SDK packaging.

- Capability-gated SDK fan speed and cover movement with shared sample readback.

- Read-only Dashboard mode with safe state inspection.
- Per-tile refresh, display choices and optional timestamps.
- Previewable Lighting, Climate and Overview starter layouts.
- Dashboard Undo, direct page selection and keyboard shortcuts.
- Verified device capabilities and data-source warnings.
- Credential-free diagnostic report export.
- Dashboard backup and restore without server credentials.
- Tile previews, duplication and moves between pages.
- Actionable connection errors and per-client diagnostics.
- Named dashboard pages with saved color presets.
- Inline brightness, climate targets and aged offline readings.
- NovaDock integration with shared sample and server controls.
- Masked token display with explicit Show/Hide.
- Dashboard closing offers Save, Discard and Keep Editing.
- Light brightness, RGB color and climate targets with confirmed readback.
- Flexible dashboard grids with guided device selection and saved actions.
- Developer SDK with a working example client.
- ARexx command port with status and sample-control scripts.
- PHP website with GitHub downloads and real Amiga screenshots.
- Device-first browser with scoped entities, state details and valid all-area catalogue templates.
- Shared sample devices with simulated controls for client apps.
- Shared Home Assistant connection for Amiga applications.
- Preferences with service controls, offline saving and boot startup.
- Optional Low memory mode with idle library reclamation.
- One configuration file for all bridge settings.
- Optional reconnect intervals and shared server connection status.
- Server settings with manual token entry, import and connection validation.
- Server logs, live bridge diagnostics and an About window.
- Entity state polling and supported device controls.
- Local message-port SDK for application integration.
- Installer with Update, Uninstall and settings retention choices.
