# Android application

The Android build uses the same stream controller, processing logic, and
versioned `.yodau.json` line-configuration format as the Qt and KDE desktop
builds. Its shell is adapted for touch rather than desktop docks.

When an Android OpenCV package is not supplied, the app registers a bounded
dependency-free motion baseline. It samples at most a 64-pixel longest edge,
detects frame-delta motion, applies the shared motion-region filter, and emits
tripwire crossings through the same event contract. Desktop builds with OpenCV
continue to use the full algorithm set and existing processing backends.

## Mobile workflow

The bottom navigation exposes four pages:

- **Monitor** shows the active stream and visible-stream grid.
- **Streams** adds camera, video-document, and network sources and configures
  processing.
- **Lines** draws, edits, enables, and removes line geometry.
- **Logs** exposes the same filtered operator diagnostics as desktop.

The Import and Export actions use Android's system document provider. They
transfer configuration bytes through the shared parser/serializer, so scoped
storage does not force the core or daemon to understand `content://` URIs.
Selected video documents are passed to Qt Multimedia as content URLs.

Local camera inventory comes from `QMediaDevices`; the friendly camera name is
shown while its opaque `QCameraDevice::id()` remains the configured source id.
The app requests `CAMERA` permission only when that source is about to start.

When the application leaves the foreground, players and cameras stop and the
active operator configuration is atomically saved in app-private storage. The
next launch validates size, schema, and content before restoring it in a
paused state. No camera or video starts until the operator enables the restored
stream. Removing the last selected configuration clears the snapshot instead
of resurrecting stale state. Android Back first returns from a detail page to
Monitor, then returns an enlarged stream to the grid.

## Build and deploy

The workspace Android profile requires a Qt Android SDK, Android SDK/NDK, and
an emulator or device for deployment:

```bash
marx build android app:app
marx run android app:app \
  --android-mode device
```

The application id is `org.ninjaro.yodau`. Android settings live in
`manifest.json` under `android`: camera hardware remains optional, application
backup is disabled because configurations may contain private source URLs, and
unused Bluetooth, recording and legacy storage permissions are excluded. Camera
permission is requested at runtime before a local camera starts. Import/export
uses Android's document provider.

Run these commands from `work/yodau/` with the current `marx` actor on `PATH`.
The `app:app` artifact declares `qt_android`. Standard ecosystem CMake generates
an overlay under `<build>/ecosystem/android/app__app/package/` and sets
`QT_ANDROID_PACKAGE_SOURCE_DIR` before Qt finalization. It patches the installed
Qt manifest template, preserving deployment placeholders and provider metadata;
Qt supplies Gradle scaffolding and runtime resources during deployment.

`assets/brand/bug.svg` is the authoritative launcher artwork. The shared helper
renders its existing `spin` group as a static launcher (the SVG canvas includes
an animation orbit). PNG mipmaps are generated at five densities in the build directory, including
the round-icon reference. Android configuration requires Python 3.8+ and the host
`rsvg-convert` utility (librsvg); an explicit path can be supplied with
`-DECOSYSTEM_ANDROID_SVG_CONVERTER=/path/to/rsvg-convert`. Missing conversion tools
produce a configuration diagnostic. Native desktop builds do not require them.
There is no authored Android package directory or project-local generator override.
