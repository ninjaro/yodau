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
Android. The processing core uses scoped callback-completion expectations.
The daemon publishes its own owned main-loop heartbeat and rare
`configuration_loaded`, `stream_started`, `output_ready`, and `stream_stopped`
transitions. The GUI adds only the optional Qt event-loop heartbeat.

No core fact is rebroadcast through `stream_controller`. Application code does
not own protocol negotiation, JSON, sockets, reconnects, queues, process-memory
sampling, or Monitor UI. With no watcher, pulse and breadcrumb publication returns before clock
lookup or payload work. Expected-progress scope transitions still record their
start and retirement. Local per-camera FPS, latency, and algorithm feedback
remain inside Yodau because they are operational UI information rather than
watchdog probes.


## Expected progress domains

Debug Linux builds outside Android optionally publish to the external watchdog.
A core expectation covers one executing frame-processing callback, including
algorithm setup, processing and its synchronous result observer. It ends on
return or exception; scope retirement is completion evidence, not a success
receipt. There is no per-frame heartbeat. Between executing callbacks, waiting
for input, playback pauses, skipped analysis frames and EOF are idle, not
promises of future frame completion. A pause, EOF or stop request does not
retire an executing callback; a seek or in-flight frame can still invoke
processing during a pause.

The headless supervisor separately owns main progress from configuration loading
through worker teardown, except that an empty configuration path returns before
claiming. Its existing returned status polls pulse only that owner, including
unchanged, unready or erroneous status; this proves supervisor responsiveness,
not source capture, output success or algorithm progress.
The GUI's event-loop timer owns GUI responsiveness independently. The CLI prompt
and the unassigned worker channel have no expectation.

Activity, CPU use, breadcrumbs, unowned pulses and another channel's progress
cannot refresh a scoped obligation. Sampling may miss short scopes or transient
stalls. A continuously observed overdue callback is possible-hang evidence,
not proof of deadlock; the pending-state heartbeat label does not imply that
the callback should send intermediate heartbeats.

Only one callback can own core at a time. Other concurrent callbacks continue
normally, with an explicit warning once per runtime that their independent
monitoring is unavailable. Their progress cannot satisfy or retire the owner;
they do not silently acquire the channel later. Independent stall detection for
all overlapping streams/runtimes is unsupported. Idle evidence cannot establish
that those unmonitored callbacks completed. Downstream preview publication and
capture open/read are outside this core domain.

Detach, replacement and destruction retain their callback-generation and join
rules. A late retired callback is inert; cancellation retires an expectation
when executing work actually unwinds, rather than declaring stalled work stopped.
EOF and completed shutdown leave no frame expectation. No timeout values,
playback semantics, capture scheduling or publication throttle are changed.
