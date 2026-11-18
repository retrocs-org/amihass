# Selected-entity push investigation

Status: investigated and measured on 18 November 2026. Version 0.1 continues to
use the existing selected-entity REST scheduler. No persistent push transport is
enabled or advertised by the app.

## What was verified

A read-only, certificate-verified host WebSocket probe authenticated to the
configured Home Assistant instance and used `subscribe_entities` with exactly
three explicit entity IDs. The server accepted it and returned only those three
entities. The initial JSON payload was 1,689 bytes; the first event arrived in
12 ms in this single local-network observation. The subscription was removed and
the socket closed. No service actions were sent. These are host observations,
not Amiga performance measurements, and are not a latency guarantee.

The offline experiment `python3 tools/analyze_push.py` builds explicit, unique
selection lists and refuses empty/unfiltered subscriptions, invalid IDs and more
than 48 selected entities. Its synthetic request payloads are 68, 153, 473 and
905 bytes for 1, 6, 24 and 48 entities. For comparison, a fully active 30-second
poll interval corresponds to 2, 12, 48 and 96 state requests per minute before
other scheduling limits. Event traffic depends on the entities' change rate.

## Amiga compatibility and RAM

Workbench.app's isolated Workbench 3.2.1/68060 guest has curl 8.23.0,
libcurl 8.23.0 and OpenSSL 3.6.2. Its reported protocols include `ws` and `wss`.
That indicates protocol support in the library; it does not give the current
command-line worker the bidirectional frame API needed to authenticate, send
subscriptions, process fragmented events and answer pings.

Eight authenticated, read-only HTTPS API probes passed. After reclamation,
cycles 6–8 each had 16,155,856 Fast bytes free. Their observed minima ranged
from 10,676,400 to 10,871,200 bytes. These whole-guest snapshots imply roughly
5.3–5.5 MB of transient pressure in that phase; they do not isolate TLS memory.
Earlier cycles also overlapped boot/background activity. The final free-memory
reading exceeded the second-cycle baseline, passing the retained-growth check.
This does not measure the resident cost of an Amiga WebSocket connection.

A separate first curl-version invocation measured 19,585,592 Fast bytes before,
13,220,216 after the command and 16,019,488 after FLUSH. It shows why an initial
library-load reading must not be mistaken for steady-state usage or a leak.

## Proposed implementation boundary

- A separate native worker using libcurl's WebSocket API, outside the service loop.
- One connection per server; explicit active entity IDs only, deduplicated across clients.
- At most the existing 48 selected entities and a 64 KiB assembled-message limit.
- Handle compressed additions, attribute deltas and removals, with per-entity caps.
- Verify TLS; keep tokens in worker-private memory, never argv or diagnostic exports.
- Replace subscriptions when leases change; close the connection when idle.
- On disconnect, unsupported command, oversized messages or worker failure, mark
  affected values stale and resume the existing bounded REST scheduler.
- Reconcile selected entities once after reconnect; never replay device actions.
- Keep Low memory mode on REST until measured resident RAM and fragmentation tests
  justify an opt-in setting. No placeholder setting is shipped in version 0.1.

Before enabling it, implement the worker and test fragmented frames, pings,
auth rejection, selection replacement, reconnect and idle cleanup on the minimum
supported Amiga configuration. A host probe cannot certify that native worker.

## Primary references

- [Home Assistant WebSocket API](https://contributors.home-assistant.io/core/internals/websocket-api/)
  documents authentication, filtered `subscribe_entities`, compressed state deltas
  and unsubscribe commands. Use entity filtering rather than all state events.
- [curl WebSocket support](https://curl.se/docs/websocket.html) distinguishes
  libcurl frame APIs from command-line support.
