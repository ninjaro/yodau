# Algorithms

The core exposes a small set of selectable processing algorithms. Each
algorithm consumes frame input plus stream geometry and returns a
`processing_result` containing events, overlays, metrics, and diagnostics.

The full set below requires an OpenCV-enabled build. Without OpenCV, only the
portable `motion_baseline` is registered; catalogue descriptions alone do not
make unavailable algorithms executable.

## Selection

Algorithm selection is scoped by stream:

- the runtime has one default algorithm;
- each stream may override that default;
- a stream override can be reset to the default;
- algorithm and preset identifiers come from the current catalogue; historical
  synonyms such as algorithm `auto` or preset `basic` are not accepted.

## Implemented Algorithms

### `motion_baseline`

Reference OpenCV motion detector. It uses frame differencing and tripwire
geometry to detect crossings. It is the default because it is stable and
simple.

Best fit:

- simple tripwire scenes;
- low configuration overhead;
- baseline comparisons against other algorithms.

### `spot_grid`

Grid/downsampled motion detector. It favors cheap motion localization over
shape detail.

Best fit:

- quick motion spotting;
- noisy or low-cost preview feedback;
- scenes where exact object contours are not needed.

### `contour_mask`

Mask and contour based detector. It extracts motion regions, builds contour
overlays, and evaluates motion against line and region geometry.

Best fit:

- visible foreground blobs;
- region-of-interest behavior;
- shape-aware overlays.

### `centroid_track`

Centroid association detector. It tracks motion centers across frames and uses
track movement to evaluate crossings.

Best fit:

- sparse moving objects;
- directional feedback;
- track-oriented overlays.

### `hybrid_auto`

Adaptive selector that delegates to concrete detector families based on scene
conditions and recent processing cost.

Best fit:

- comparison runs;
- operator-facing automatic mode;
- streams where scene density changes over time.

## Output Contract

Algorithms should report:

- `events` for semantic outcomes such as motion, tripwire, and region hits;
- `overlays` for displayable points, polylines, polygons, and labels;
- `metrics` for timing and detector counters;
- `diagnostics` for debug explanations.

The UI should not infer algorithm internals from event text. Display-specific
rendering should prefer the structured overlay and metric fields when they are
available.

## Configuration

Configuration values are currently string keyed. Callers select current preset
identifiers before they reach the runtime. Operator profiles select the catalogue's
first, default or last preset; they do not introduce alternate preset identifiers.
An explicitly unavailable or unknown algorithm is rejected rather than replaced
with the default. Algorithm options should
remain narrow, documented, and stable enough for the CLI, app, and benchmarks
to use the same ids.

## Benchmarking

Algorithm changes should be compared with the benchmark harness before they
become defaults. Useful comparisons include:

- average and percentile latency;
- synthetic or measured FPS;
- expected versus detected tripwire counts;
- false positives and missed events;
- algorithm-specific counters.

Recorded real clips are preferred for quality decisions. Synthetic fixtures
are useful for repeatable smoke comparisons.
