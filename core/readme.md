# CLI Reference

`yodau_cli` is an interactive shell for core streams, lines, algorithms, logs,
and virtual-camera preview state.

## Streams

```bash
yodau> list-streams
yodau> list-streams --connections
yodau> add-stream --path=<path> [--name=<name>] [--type=<type>] [--loop=<0|1>]
yodau> add-stream <path> [<name>] [<type>] [<loop>]
yodau> start-stream --name=<stream-name>
yodau> stop-stream --name=<stream-name>
yodau> list-virtual-cameras
```

Stream type is one of `local`, `file`, `http`, or `rtsp`. If omitted, the type
is inferred from the path.

## Lines

```bash
yodau> list-lines
yodau> add-line --path=<coords> [--name=<name>] [--close=<0|1>] \
                [--visual-width=<float>] [--interaction-width=<float>] \
                [--effective-length=<float>] [--damping=<float>]
yodau> set-line --stream=<stream-name> --line=<line-name>
```

`path` is a semicolon-separated list of normalized `x,y` points. Coordinates
use percentages in `[0.0, 100.0]`.

Open lines are tripwires. Closed lines are polygonal regions.

## Algorithms

```bash
yodau> list-algorithms
yodau> set-default-algorithm motion_baseline
yodau> set-default-algorithm hybrid_auto
yodau> set-stream-algorithm cam0 contour_mask
yodau> set-stream-algorithm cam0 default
```

Available algorithm ids:

- `motion_baseline`
- `spot_grid`
- `contour_mask`
- `centroid_track`
- `hybrid_auto`

Aliases `auto`, `adaptive`, and `hybrid` normalize to `hybrid_auto`. Passing
`default`, `reset`, or `clear` as a stream algorithm removes that stream's
override.

## Logs

```bash
yodau> set-log-mode release
yodau> set-log-mode debug
yodau> show-log --limit=50
yodau> show-log --severity=warning --stream=cam0 --event=tripwire
yodau> show-log --line=north --algorithm=contour_mask
yodau> clear-log
```

`release` mode is concise. `debug` mode includes subsystem, stream, line,
algorithm, event, and detail fields where available.

## Benchmarks

```bash
cmake -S . -B /tmp/yodau-bench-build -DECOSYSTEM_BUILD_BENCHMARKS=ON
cmake --build /tmp/yodau-bench-build --target core_benchmarks__bench app_benchmarks__bench
/tmp/yodau-bench-build/yodau_core_bench --benchmark_filter='replay/.*/single_day_sparse'
/tmp/yodau-bench-build/yodau_app_bench -functions
```
