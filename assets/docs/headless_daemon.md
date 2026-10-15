# Headless daemon

`yodau-daemon` runs the core capture, processing, overlay, and virtual-camera
pipeline without constructing `QApplication`, `QWidget`, or any other GUI
object. Line configuration is intentionally done in the desktop application:
export a JSON line configuration there and pass the same file to the daemon.
The plain Qt and KDE GUI profiles use the same exporter and schema; desktop
window/dock settings are stored separately and are never consumed by the
daemon.

```bash
yodau-daemon --config camera-lines.json \
  --source /dev/video0 --output /dev/yodau0
```

The source, output, and stream name normally come from the exported file. The
command-line options are deployment-time overrides. Stop the process with
`SIGINT` or `SIGTERM`. The daemon keeps only the newest pending output frame per
stream, so a slow virtual-camera consumer causes frame drops instead of
unbounded memory growth or capture-thread blocking.

The daemon consumes the exported source/output identity, analysis cadence,
algorithm/preset/parameter values, and each line's enabled state, geometry,
direction, and processing profile. Desktop display preferences and appearance
fields remain in the same document for lossless Qt round trips; the headless
publisher currently renders deterministic line colors and labels rather than
reproducing the desktop theme.

Exports are strict, versioned JSON. On Unix, the desktop app writes them
atomically with mode `0600`. A network source URL may contain credentials, so
treat the file as a secret, do not commit it, and retain owner-only permissions
when copying it. Daemon logs redact URL user information and query values.

## Linux device setup

The daemon writes V4L2 YUYV frames; it does not create a kernel video device.
Provision one with a V4L2 loopback driver. `exclusive_caps=1` makes the node
advertise output capability before yodau connects and capture capability to
consumers after it connects, which is required by some video clients:

```bash
sudo modprobe v4l2loopback devices=1 video_nr=10 \
  card_label=yodau exclusive_caps=1
sudo ln -s /dev/video10 /dev/yodau0
yodau-daemon --config camera-lines.yodau.json --output /dev/yodau0
ffplay -f v4l2 /dev/yodau0
```

`-f v4l2` is the portable FFmpeg invocation because a bare character-device
path does not reliably identify its input demuxer. Builds that auto-detect
V4L2 may also accept `ffplay /dev/yodau0`; yodau cannot control that client-side
probing behavior.

The configured output must be a V4L2 **video-output** device, not a physical
capture node. The daemon opens it nonblocking and reports format/open/write
errors in its status output. After startup it continues monitoring the output;
if the publisher remains unavailable for two seconds, the daemon exits with a
failure status so `systemd` or another supervisor can restart it. A camera,
HTTP, or RTSP source ending without a stop request is also a failure. Natural
EOF is successful only for a non-looping file source.

Within a virtual-camera publisher, an explicitly configured output is reserved
for one stream. Other streams report `already assigned` even if the selected
device is unavailable. That assignment rejection remains the primary status
through subsequent frame validation and background worker completion; those
operations cannot make the rejected stream an output owner. Release the rejected
stream to discard its binding. After releasing the owner as well, publishing
again makes a fresh assignment, with fresh frame counters and error state.
Separate publishers with different output destinations keep independent state.

## Source-camera exclusivity

For V4L2 sources the daemon holds a nonblocking advisory `flock`, keyed by the
device major/minor identity, for its entire run. The lock uses the current
user's runtime directory when available. Once frames are flowing, the daemon
opens a second probe handle and requests V4L2 capture buffers. It continues only
when the driver rejects that second buffer owner with `EBUSY`; if the driver
permits it or the result is ambiguous, startup fails closed. This verifies
exclusive frame-buffer ownership rather than assuming that an advisory lock
protects against unrelated applications.

Linux does not provide a portable userspace switch that makes every V4L2
driver and every non-cooperating application honor an exclusive `open`. The
buffer-owner check proves that a second client cannot start the normal MMAP
capture path on the deployed driver, while the daemon is active. For a stronger
security boundary (including unusual read-based drivers), run the daemon as a
dedicated service user and grant that user exclusive device access with udev
permissions/device cgroups or a device broker. Do not rely on `flock` alone.

For that deployment model, create a dedicated `yodau` service account and a
udev rule that matches the physical camera by stable vendor/product/serial
attributes, assigns the node to that account, and uses mode `0600`. Do not match
only `video0`, because kernel indexes can change after reboot. The loopback
output should use a separate rule that grants read access to the intended
consumer group while retaining write access for the daemon. This separation
keeps desktop applications away from the source but lets them open
`/dev/yodau0`.

The daemon fails closed when it cannot acquire its source lease, cannot prove
exclusive V4L2 buffer ownership, cannot open the configured source, or cannot
make the virtual output ready within ten seconds. It never silently publishes
the unprocessed source camera.

V4L2 frame waits are stop-aware and checked at short intervals. HTTP and RTSP
capture requests configure a five-second open timeout and a one-second read
timeout where the selected OpenCV backend supports those properties. Backend
support varies; shutdown can still be delayed by a backend that ignores its
timeout controls, and such a deployment should use a service-level stop
timeout as a final guard.
