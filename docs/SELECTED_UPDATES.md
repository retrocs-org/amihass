# Selected-entity updates

Version 0.3 adds opt-in native WebSocket updates. Enable **Live updates** in
Preferences → General Settings, then Save. REST remains the default.

The central service opens one verified TLS connection to `/api/websocket`,
authenticates and calls `subscribe_entities` with the union of active client
selections. It never requests every entity. The existing limit is 48 unique
selected entities. Browser discovery stays paged and uses REST.

Initial states, typed attribute changes, attribute removals and removed entities
update the shared cache. SDK and ARexx clients read the same values. Controls and
confirmation reads continue through the existing REST action validator.

A connection failure returns watched entities to REST polling. Automatic retries
use the configured reconnect interval, with a minimum of one minute. A rejected
token pauses retries until Server Settings are saved or Reconnect is requested.
Low memory mode uses REST and closes the persistent connection. Releasing the
last selection closes the worker and its socket/TLS resources.

Frames and assembled messages are limited to 64 KiB; attributes to 4 KiB per
selected entity. Oversized or malformed updates cause REST fallback. Pings and
receive deadlines detect dead connections. No actions are queued or replayed by
the live worker. The service remains responsive while its worker handles DNS,
sockets and TLS.

STATUS includes `transport.mode`, `push.updates` and `push.failures`. These are
additive ABI 1 items. Live updates keep the observation age fresh; they do not
invent a Home Assistant state-change timestamp.

Protocol and cache tests use local fixtures. Native validation uses a disposable
certificate-verified TLS endpoint with a fake token. No real devices are operated.
The persistent TLS session has a resident RAM cost; use Low memory mode on small
systems. No latency or memory-saving guarantee is implied.

The framing, HTTP upgrade and socket/TLS foundation is adapted from
[ami2ha](https://github.com/evil4dmin/ami2ha), under the included MIT licence.
