# AmiHASS roadmap

## Completed and verified

- [x] Separate bridge service and Preferences with persistent component paths.
- [x] Current runtime package: fresh install, update and both uninstall choices
  in all three Installer modes; optional Preferences launch after installation.
- [x] Authenticated HTTPS discovery and read-only state queries.
- [x] Distinct DNS, TLS, HTTP/authentication and network errors.
- [x] Configurable reconnect intervals and client-visible server status.
- [x] Rejected credentials pause automatic HTTP traffic until settings reload
  or an explicit reconnect request.
- [x] Shared selected-entity polling, leases and client cleanup.
- [x] Device and room browsing with combined room, type and name filters.
- [x] Brightness, RGB, temperature, media, cover and lock capabilities.
- [x] Shared offline sample devices and client-visible action readback.
- [x] Capability-filtered Dashboard actions; offline edits preserve saved actions.
- [x] Door-state labels and confirmation with post-consent connection/state checks.
- [x] ARexx discovery, queries, status and validated actions.
- [x] Current archive dates and portable LHA paths; exact Amiga OLHa 0.4.4 tests.
- [x] Fresh repository-scoped GitHub App tokens generated at publication time.

## Remaining work

- [x] Validate migration from older development-only layouts separately from
  the supported installed-package paths (legacy path files, Private/connection
  and in-cfg tokens; damaged legacy paths keep defaults).
- [x] Add selected-entity native WebSocket updates with bounded REST fallback.
- [x] Add optional native bsdsocket.library/AmiSSL transport using Exec-aware
  waits, with verified REST fallback and explicit transport reporting.
- [x] Home Assistant change events for scripts (ARexx WAITEVENT on shared leases).
- [x] Per-page refresh profiles and time-of-day Dashboard pages.
- [x] Persistent notifications over the live-update connection.
- [x] Saved server profiles (one active server at a time).
- [x] locale.library catalogs (Swedish, German).
- [x] All window text in the catalogs (English-keyed table).
- [x] Native HTTP worker with keep-alive and curl fallback (needs testing on
  real 68k hardware).
- [x] Expose complete typed entity attributes through the SDK and live Browser.
- [x] Numeric history in the SDK and ARexx, and history-graph Dashboard tiles.
- [x] Multi-entity SDK queries; scenes/scripts as confirmable Dashboard buttons;
  climate modes, automations, selects, numbers, vacuums, valves, humidifiers
  and code-free alarm panels.
- [x] Add Home Assistant label filters alongside existing room filters.
- [x] Improve display-only Unicode fallbacks for names and units.
- [ ] Add lock PIN entry without saving codes in layouts or diagnostics.
- [x] Add shared bounded camera snapshots, server-side size requests and a
  native datatype viewer in Dashboard and Browser.

See [ami2ha review](AMI2HA-REVIEW.md) for references and licensing.
REST is the default; native verified-TLS WebSocket updates are optional in 0.3.
