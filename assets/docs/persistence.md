# Configuration and watchdog boundaries

Yodau retains its existing `core/` and `app/` source blocks. The typed configuration
model belongs to `yodau::core`; desktop/mobile presentation state is application-owned.

## Application persistence

- The core owns typed line configuration, strict JSON representation and bounded
  atomic file persistence.
- The shell owns Qt window state and mobile-session persistence.

The desktop application, Android lifecycle workflow, and headless daemon still
exchange the same validated document. Current-format validation, the 4 MiB
document bound, line/point limits, owner-only Unix export, and transactional
import behavior are unchanged.

## Watchdog diagnostics

The shared producer boundary enables itself only for Debug Linux builds outside
Android. The processing core publishes a rate-limited completion heartbeat.
The daemon publishes its own main-loop heartbeat and rare
`configuration_loaded`, `stream_started`, `output_ready`, and `stream_stopped`
transitions. The GUI adds only the optional Qt event-loop heartbeat.

No core fact is rebroadcast through `stream_controller`. Application code does
not own protocol negotiation, JSON, sockets, reconnects, queues, process-memory
sampling, or Monitor UI. With no watcher, client calls return before clock
lookup or payload work. Local per-camera FPS, latency, and algorithm feedback
remain inside Yodau because they are operational UI information rather than
watchdog probes.
