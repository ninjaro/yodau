## CLI

### Streams

A *stream* represents a video source (local device, file, HTTP/HTTPS, RTSP).

Each stream has:

* `name` – stream identifier (unique).
* `path` – device path, file path, or URL.
* `type` – one of `local | file | http | rtsp`.
  If `type` is not provided, it is inferred from `path`:
    * `local` : `/dev/video*`
    * `rtsp` : `rtsp://...`
    * `http` : `http://...` or `https://...`
    * everything else is `file`
* `loop` – whether file playback should loop.
* `active_pipeline` – one of `manual | automatic | none`.

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
```

* `--name` is required; fails with an error if the stream does not exist.

### Lines

A *line* describes a polyline or polygon in normalized coordinates. Lines can later be attached to streams.

Each line has:

* `name` – line identifier.
* `path` – sequence of points.
* `close` – whether the line is treated as closed (polygon).

Coordinates are specified and stored as **floating-point percentages** in the range `[0.0, 100.0]`, where:

* `(0, 0)`   – top-left corner
* `(100, 100)` – bottom-right corner

#### add-line

```bash
yodau> add-line --path=<coords> [--name=<name>] [--close=<0|1>]
```

* `path` is required.
* `name` and `close` are optional.
* `close` defaults to `false`.
* If `name` is omitted, a name like `line_0`, `line_1`, ... is auto-generated.
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
2 lines:
    Line(name=line_0, closed=false, points=[(0, 0); (100, 0)])
    Line(name=door, closed=true, points=[(10, 20); (20, 20); (20, 80); (10, 80)])
```

```bash
yodau> set-line --stream=<stream-name> --line=<line-name>
```

* Fails with an error if either the stream or the line does not exist.