## CLI

The backend runtime now supports two rendering ownership modes:

* `backend_only` - used by the CLI. OpenCV capture, detection, and backend-owned preview routing now run in the backend library through `processing_preview_router`, which renders line, event, and algorithm overlays before optional virtual-camera publication.
* `frontend_only` - used by the Qt app. Qt decodes and draws frames, while the backend library only performs detection and emits events.

In CLI `backend_only` mode, `yodau` does not open preview windows. Instead, on Linux it tries to publish rendered frames into a V4L2 output device so the stream can be opened externally, for example with `ffplay /dev/yodau-video0`, and can appear in webcam pickers if the system exposes that loopback device to other applications.
That backend preview path now consumes `processing_result.overlays` in addition
to the generic stream-line and event markers.

The CLI now also keeps an in-memory log history with two output modes:

* `release` - concise operator-readable entries. Debug noise such as motion spam
  is hidden.
* `debug` - richer diagnostic entries with scope, subsystem, stream, and
  details.

```bash
yodau> set-log-mode debug
yodau> show-log --limit=50 --severity=warning --stream=cam0
yodau> clear-log
```

* `show-log` reads the retained CLI log history.
* `clear-log` clears that history.
* `set-log-mode` changes both immediate CLI log echoing and `show-log`
  formatting.

### Algorithms

The backend now exposes three concrete processing algorithms:

* `motion_baseline` - current OpenCV-heavy tripwire baseline.
* `spot_grid` - cheaper grid/downsampled motion detector.
* `contour_mask` - contour/mask-driven detector with centroid-path tripwire
  checks.

Algorithm selection is scoped per stream at runtime:

* one stream uses one active algorithm at a time
* the runtime has a default algorithm for streams without an override
* a stream override can be changed while the stream exists; the next processed
  frames use the new algorithm

```bash
yodau> list-algorithms
yodau> set-default-algorithm spot_grid
yodau> set-stream-algorithm cam0 contour_mask
yodau> set-stream-algorithm cam0 default
```

* `list-algorithms` prints the current default plus stream-specific overrides.
* `set-default-algorithm` changes the fallback algorithm for streams without
  an override.
* `set-stream-algorithm` sets one algorithm for one stream.
* Passing `default`, `reset`, or `clear` to `set-stream-algorithm` removes the
  stream override and falls back to the current default.
* The current default remains `motion_baseline` until changed explicitly.

### Benchmarks

The repo now also exposes a Stage 0 benchmark harness for backend algorithm
comparison. It currently replays synthetic fixtures that match the scenario ids
documented in `BENCHMARKS.md`, while recorded clips and richer metrics are
still follow-up work.

```bash
cmake -S . -B /tmp/yodau-bench-build -DECOSYSTEM_BUILD_BENCHMARKS=ON
cmake --build /tmp/yodau-bench-build --target backend_benchmarks__bench
/tmp/yodau-bench-build/yodau_bench --benchmark_filter='replay/.*/single_day_sparse'
```

Catalog files live under:

* `benchmarks/scenarios/`
* `benchmarks/hardware_profiles/`
* `benchmarks/reports/`

When run with `--benchmark_format=json`, the harness now emits counters for
synthetic FPS, avg/p95 latency, RSS, false positives, missed events, and
expected/detected tripwire counts, while Google Benchmark's JSON also carries
the CPU-time fields.

### Streams

A *stream* represents a video source (local device, file, HTTP/HTTPS, RTSP).

Each stream has:

* `name` - stream identifier (unique).
* `path` - device path, file path, or URL.
* `type` - one of `local | file | http | rtsp`.
  If `type` is not provided, it is inferred from `path`:
    * `local` : `/dev/video*`
    * `rtsp` : `rtsp://...`
    * `http` : `http://...` or `https://...`
    * everything else is `file`
* `loop` - whether file playback should loop.
* `active_pipeline` - one of `manual | automatic | none`.

```bash
yodau> list-streams
# Example output:
2 streams:
    Stream(name=cam0, path=/dev/video0, type=local, loop=true, active_pipeline=none)
    Stream(name=dogcam, path=rtsp://..., type=rtsp, loop=true, active_pipeline=none)
yodau> list-streams --connections
# Same as above, but also prints connected lines for each stream.
```

```bash
yodau> add-stream --path=<path> [--name=<name>] [--type=<type>] [--loop=<0|1>]
```

* `path` is required.
* `name`, `type`, `loop` are optional.
* Default for `loop` is `true`.
* If `name` is empty or already in use, a unique name like `stream_0`, `stream_1`, ... is auto-generated.

```bash
yodau> add-stream <path> [<name>] [<type>] [<loop>]
# Example usages:
yodau> add-stream /dev/video0 cam0 local 1
yodau> add-stream rtsp://example.com/live dogcam
yodau> add-stream /home/me/clip.mp4 clip1 file 0
```

```bash
yodau> start-stream --name=<stream-name>
yodau> stop-stream  --name=<stream-name>
yodau> list-virtual-cameras
```

* `--name` is required; fails with an error if the stream does not exist.
* The Linux backend expects an existing virtual camera output device.
  By default it prefers `/dev/yodau-video0`, `/dev/yodau-video1`, ... and then compatible loopback-style `/dev/videoN` outputs.
* Set `YODAU_VCAM_DEVICE=/dev/some-device` to pin the backend to a specific virtual camera device.
* `list-virtual-cameras` prints the backend stream-to-device bindings, readiness, and the last device error if no compatible sink is currently available.
* The device binding is finalized when the first rendered frame arrives, because the sink format is configured from the stream frame size.

### Lines

A *line* describes a polyline or polygon in normalized coordinates. Lines can later be attached to streams.

The current backend `line` type is geometry-only. It does not store visual
width, effective string length, damping/response, or other richer string-like
semantics. Those fields now have a separate backend `line_profile` foothold,
and frontend save/apply flows plus CLI line creation now populate that profile.
The core line contract still stays geometry-only, while backend-only preview
routing currently uses the profile's visual width.

Each line has:

* `name` - line identifier.
* `path` - sequence of points.
* `close` - whether the line is treated as closed (polygon).
* `dir` - optional tripwire crossing direction (`any`, `neg_to_pos`,
  `pos_to_neg`).

Coordinates are specified and stored as **floating-point percentages** in the range `[0.0, 100.0]`, where:

* `(0, 0)`   - top-left corner
* `(100, 100)` - bottom-right corner

#### add-line

```bash
yodau> add-line --path=<coords> [--name=<name>] [--close=<0|1>] \
                [--visual-width=<float>] [--interaction-width=<float>] \
                [--effective-length=<float>] [--damping=<float>]
```

* `path` is required.
* `name` and `close` are optional.
* `close` defaults to `false`.
* If `name` is omitted, a name like `line_0`, `line_1`, ... is auto-generated.
* `path` is a semicolon-separated list of points, each point as `x,y`.
  Parentheses around points are allowed but ignored.
* Optional profile flags update the separate backend `line_profile` record
  instead of widening the geometry-only `line` itself.

Examples:

```bash
yodau> add-line --path=0,0;100,0
yodau> add-line --path=10,20;20,20;20,80;10,80 --name=door --close=1
yodau> add-line --path=0,50;100,50 --name=trip --visual-width=5 --interaction-width=6 --effective-length=1.4 --damping=0.75
yodau> add-line --path=33.3,10;66.6,10;50,50
```

```bash
yodau> list-lines
# Example output:
2 lines:
    Line(name=line_0, closed=false, points=[(0, 0); (100, 0)]) profile(width=1, interaction=1, length=1, damping=0.5)
    Line(name=door, closed=true, points=[(10, 20); (20, 20); (20, 80); (10, 80)]) profile(width=5, interaction=6, length=1.4, damping=0.75)
```

```bash
yodau> set-line --stream=<stream-name> --line=<line-name>
```

* Fails with an error if either the stream or the line does not exist.
