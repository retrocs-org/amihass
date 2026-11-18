# AmiHASS roadmap

## Service and Preferences

- [x] Separate AmiHASSService and AmiHASSPrefs; close Preferences without stopping
  the service. Preserve custom install paths and connection settings on update.
- [ ] Validate fresh installation, development-build migration, update and both
  uninstall choices in Workbench for each package layout.
- [x] Validate authenticated HTTPS, catalogue discovery and a state read against
  a real Home Assistant server without sending device controls.
- [ ] Show distinct DNS, TLS, HTTP/authentication and offline errors in Preferences.

## Shared connection framework

- [ ] Add a portable WebSocket state machine with selected-entity subscriptions.
- [ ] Add optional native bsdsocket.library/AmiSSL transport using Exec-aware waits.
  Keep verified REST fallback and explicitly report the transport in use.
- [x] Add REST health checks, configurable reconnect intervals and client server status.
- [ ] Add an authentication-rejected
  state that waits for credential correction or an explicit user retry.
- [x] Add ARexx QUERY, LIST, STATUS and validated action commands backed by the
  existing cache/queue, plus script examples. Do not expose the token.
- [ ] Design an SDK event subscription API with coalesced bounded notifications,
  safe client teardown and no replay of stale queued actions.

## Entity browsing and controls

- [x] Bundle a standalone read-only Browser with offline sample entities,
  search, domain filters and JSON attribute details.
- [x] Connect the Browser to live SDK catalogue paging and entity state reads,
  with separately selected sample data.
- [ ] Extend live Browser details with complete typed attributes through the SDK.

- [ ] Add optional Home Assistant label/area filters to discovery.
- [ ] Extend SDK capabilities and action payloads for brightness, colour, media
  controls and covers. Keep unsupported entities read-only.
- [ ] Improve display-only Unicode fallbacks for names and units.
- [ ] Consider camera snapshots in a separate optional client, fetched/scaled on
  the server with datatype-based display and bounded image sizes.

See [ami2ha review](AMI2HA-REVIEW.md) for implementation references, licensing,
verification limits and adaptation requirements. These tasks are not release
claims; the current service polls REST endpoints.
