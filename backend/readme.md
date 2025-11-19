## CLI

### Streams

A *stream* represents a video source (local device, file, HTTP/HTTPS, RTSP).

Each stream has:

* `name` – stream identifier (unique).
* `path` – device path, file path, or URL.
* `type` – one of `local | file | http | rtsp`.
  If `type` is not provided, it is inferred from `path`:

    * `/dev/video*` → `local`
    * `rtsp://...`   → `rtsp`
    * `http://...` or `https://...` → `http`
    * everything else → `file`
* `loop` – `1` or `0`. For file streams, tells whether playback should loop.
* `active` – `1` or `0`. Indicates whether this stream is currently started (running).

```bash
yodau> list-streams
# Example output:
streams: 2
name=cam0   type=local path=/dev/video0   loop=1 active=1
name=dogcam type=rtsp  path=rtsp://...    loop=1 active=0
```

```bash
yodau> add-stream --path=<path> [--name=<name>] [--type=<type>] [--loop=<0|1>]
```

* `path` is required.
* `name`, `type`, `loop` are optional.
* Boolean values are `0` (false) or `1` (true). Default for `loop` is `1`.
* If `name` is omitted, the implementation may auto-generate it (e.g. `stream0`, `stream1`, ...).
* If `name` is already in use, the command fails with an error.

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
```

* If the stream does not exist, an error is printed.

### Lines

A *line* describes a polyline or polygon in normalized coordinates. Lines can later be attached to streams.

Each line has:

* `name` – line identifier.
* `path` – sequence of points.
* `close` – `1` or `0`. If `1`, the line is treated as closed (polygon).

Coordinates are specified and stored as **floating-point percentages** in the range `[0.0, 100.0]`, where:

* `(0, 0)`   – top-left corner
* `(100, 100)` – bottom-right corner

#### add-line

```bash
yodau> add-line --path=<coords> [--name=<name>] [--close=<0|1>]
```

* `path` is required.
* `name` and `close` are optional.
* `close` is `0` or `1`, default is `0`.
* If `name` is omitted, a name like `line0`, `line1`, ... is auto-generated.
* `path` is a semicolon-separated list of points, each point as `x,y`.
  Parentheses around points are allowed but ignored.

Examples:

```bash
yodau> add-line --path=0,0;100,0
yodau> add-line --path=10,20;20,20;20,80;10,80 --name=door --close=1
yodau> add-line --path=33.3,10;66.6,10;50,50
```

```bash
yodau> list-lines
# Example output:
lines: 2
name=line0 close=0 path=0,0;100,0
name=door  close=1 path=10,20;20,20;20,80;10,80
```

```bash
yodau> set-line --stream=<stream-name> --line=<line-name>
```

* Fails with an error if either the stream or the line does not exist.