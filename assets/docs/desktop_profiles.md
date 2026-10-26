# Desktop GUI profiles

The `yodau` executable has two desktop build profiles over the same app
library, controller, processing core, widgets, and saved-data formats. Choosing
a profile changes the top-level shell integration; it does not create a second
application model.

The profiles preserve the same processing, actions and persistence contracts.
Proposal layouts do not redefine those boundaries.

## Material ownership

Native and Slate are presentation choices over the same workspace and processing
model. Appearance owns Slate's palette, fonts and nested control/dock styling;
the host assigns canvas/viewport roles and retains all workspace policy. Scopes
are window-local, so previews can select another material independently. Native
restores the actual Qt/KDE palette and style. Shared fonts and control assets
come from the provider's `share/shared_ui/assets` data (or embedded provider
resources); custom SDK prefixes must expose their shared data to Qt.

## Shared secondary presentation

Stream cells supply six host-owned actions to Appearance's `action_group`:
size decrease/increase, rotate, Fit/Stretch, Enlarge/Restore and Close. Existing
button object names, tooltips, accessible names/descriptions and keyboard
operations remain. The shared toolbar arranges and styles the controls; a
small desktop cell keeps every command reachable through its overflow menu.
The Android branch explicitly keeps inline controls and 48px touch targets;
native Qt branch fixtures are not Android device acceptance.

Appearance's `settings_section` presents source-name validation and the
processing-budget form. The host retains uniqueness checks, source discovery,
manual/automatic processing state, legal ranges and bindings. Errors follow
material colours and clear without destroying the field's original help or
style. Conditional rows hide their captions and errors together. Native uses
the active Qt/KDE style and compact processing fields; no stream or processing
policy is implemented in Appearance. Source mode/file-choice groups and the
algorithm-specific form remain host components.

## Plain Qt profile

The default profile uses `QMainWindow` and Qt Widgets only. It provides a small
File/View/Settings/Help menu surface, dock toggles, stream settings, line
editing and line-configuration import/export without
requiring KDE Frameworks.

```bash
cmake -S . -B build-qt -DECOSYSTEM_PROFILE_KDE=OFF
cmake --build build-qt
```

Android uses a separate touch shell over the same application model; see
[Android](android.md). Desktop-only dock, About, and exit actions remain
excluded from that build.

## KDE profile

The KDE profile uses `KMainWindow`, `KActionCollection` and standard KDE
actions. C++ constructs the File/View/Settings/Help menus and the toolbar;
`KHelpMenu` supplies the KDE Help actions and dialogs, and `KShortcutsDialog`
keeps keyboard shortcuts configurable. The Preferences action raises the
existing stream-settings dock, and the standard Help menu uses the application's
`KAboutData`. The application publishes `org.ninjaro.yodau` as its desktop file
identity and uses the same identifier for icon-theme lookup and Android
packaging metadata.

```bash
cmake -S . -B build-kde -DECOSYSTEM_PROFILE_KDE=ON
cmake --build build-kde
```

From the authoritative `work/yodau/` checkout, with `marx` on `PATH`, the
equivalent profile command is:

```bash
marx build kde app:app
```

The KDE profile needs the manifest's KF6 ConfigWidgets, CoreAddons, I18n,
WidgetsAddons, and XmlGui dependencies. It deliberately does not link KIO or
KDEGames because the shell does not use either framework.

Both desktop profiles construct their menus programmatically in C++.
They reuse the existing import/export, presentation, and four dock actions.
The `main_toolbar` contains the same Streams, Lines, and Logs buttons. KDE also
keeps its statusbar/toolbar visibility controls, language selection, Configure
Keyboard Shortcuts, Find Action (`Ctrl+Alt+I`), and standard Help/About dialogs.
KDE loads and saves config-based shortcut overrides in the `Shortcuts` group.

The KDE toolbar's button list is now fixed. The XMLGUI **Configure Toolbars**
editor and XML-based menu/toolbar overrides are no longer supported; existing
XML customizations are not imported, including shortcuts stored in an old
XMLGUI file. Reapply those through **Configure Keyboard Shortcuts** to save them
in KConfig. The toolbar still moves, docks, hides, and
restores through the shared Qt window-state record. KDE toolbar appearance
controls remain available through its context menu. XMLGUI client merging and
its D-Bus action export are also no longer provided by the window; application
actions and saved stream/line data are unchanged.

Regenerate CMake or materialize a developer build with the standard ecosystem
templates, from `work/yodau/` with `marx` on `PATH`:

```bash
marx sync
marx build debug app:app
```

No project-specific template override or custom XMLGUI resource is required.
The `XmlGui`
Framework dependency remains because it provides `KMainWindow`,
`KActionCollection`, `KHelpMenu`, `KToolBar`, and `KShortcutsDialog`; the
application does not use an XMLGUI client or layout file.

## Desktop presentation presets

**View → Desktop presentation** offers Classic and Canvas. Classic shows the
existing Streams, Lines and Logs docks. Canvas leaves Streams available and
hides Lines and Logs; the active view and visual pool are never switched away.
The same existing panel toggles are available in the submenu and remain
independent overrides. **Reset panels to preset** clears those overrides for
the selected preset. A reset does not change dock placement, stream selection,
pool placement/sizing/order, processing settings or line data.

The app-local `desktop_presentation` model contains only this implemented
panel selection. It is not a future
scene/layout format. Qt and KDE use the same actions/model; KDE registers
the C++ submenu as `view_presentation`. Android does not create or apply this menu.

Persistence precedence is deliberate:

1. Restore the existing named Qt docks/toolbars and their placement.
2. If `desktop/presentation/preset` is absent, leave that existing layout alone.
   Merely opening/closing the app does not opt a user into a new preset.
3. After explicit preset selection, `desktop/presentation/preset` and the
   `panels/streams`, `panels/editor`, `panels/logs` overrides in that group own
   visibility. Unknown preset IDs fall back to Classic; unknown panel values
   inherit the preset. No migrations are performed on read.
4. Panel toggles save current overrides; normal window shutdown also captures
   close-button changes. Capture uses explicit hidden state, not effective
   visibility, so hiding the parent window does not hide every panel next time.

Preset/reset write failures are reported before changing the live layout;
shutdown/override persistence failures are logged. Like the existing shell
state, an unsaved dock-close change is not guaranteed to survive a crash.
The separate Qt geometry record and mobile/daemon configuration are not
rewritten by the presentation preference functions.

## Pool placement

**View → Desktop presentation → Pool position** offers **Bottom**, **Right**
and **Left** in both Qt and KDE, with keyboard menu mnemonics and exclusive
checked state. This is independent of Classic/Canvas and panel reset. Switching
side recomposes the existing active container and pool without recreating
cells/media players or changing line editing, rotation, manual thumbnail sizes,
scroll direction or preserve-order policy. Left/right describe physical sides
even when the shell uses right-to-left text.

The active view retains the primary 3:1 stretch along the composition axis.
The remaining pool stays visible and scrollable; it need not fit all thumbnails
at once. With no active stream, the pool fills the board; with no remaining
streams, the active view fills it. Existing grid controls still own adaptive/
fixed sizing and horizontal/vertical scrolling. Side attachment does not
silently select a new grid policy.

Only explicit side selection writes `desktop/presentation/pool_side` as
`bottom`, `right` or `left`. Missing/unknown values restore Bottom without
write-back. Selecting a side does not opt a existing dock layout into a panel
preset. A save failure reports an error and retains the previous live side
and checked action. Qt/KDE share this preference; Android neither restores nor
writes it and retains its current touch workflow. Automatic attachment remains
outside this implemented surface; neither placement nor thumbnail sizing adds a splitter.

## Window style

**View → Desktop presentation → Appearance…** opens the shared Appearance
editor in both Qt and KDE. Its single **Window material** family offers
**Native** and **Slate**. Slate is the material of the desktop proposal's `1a`
shell: a `#2a2e32` window with `#31363b` docks and `#2f3439` menu and dock
toggle bars, group boxes outlined in `#4d5359` with their titles centred on
the border, tabs drawn as raised tabs, `#232629` inputs, log and stream
frames, ring radio buttons and a single `#3daee9` accent for selection,
checked boxes, pressed buttons and focus. Dock toggles stay grey (checked ones
lighter) and dock close buttons sit at the right of the title. Text uses Noto
Sans and figures DejaVu Sans Mono, as in the prototype; both ship as Latin
subsets under `assets/brand/fonts/` (OFL and the DejaVu/Bitstream licence
alongside) and installed copies take precedence. It restyles the existing
menus, toolbar, docks, forms, tables, transport and log view.

Like pool placement it is independent of Classic/Canvas and panel reset: it
never shows, hides, moves or recreates a dock, cell or player, and it does not
change processing, line data or overlay colours. Only an explicit choice writes
`desktop/presentation/chrome` as `native` or `slate`. Missing or unknown values
start Native without write-back, and a save failure is reported while the
previous style stays live. Android creates no desktop Appearance entry.

The editor owns selection, availability, defaults and draft cancellation through
one Appearance model per main window. The former pair of checkable Window style
menu actions is removed. The single **Native default** preset is only a material
default; it has no relationship to the Classic/Canvas workspace presets.
**Defaults** selects Native in the draft. **Apply** persists then applies without
closing; **OK** persists then closes. **Cancel**, Escape and window closure discard
only unsaved changes. A failed save leaves the accepted window unchanged and the
draft available for retry. Reopening uses the last accepted state.

Preview settings, actions and readouts are real shared controls with sample
content. They have their own material scope, including a Native preview inside
a Slate window, and never bind to live streams. No new preference store or generic
host settings controller is introduced: the model projects the existing scalar
key into shared material IDs (`native` → `system`, `slate` → `slate`). An unknown
stored scalar still renders Native and is left untouched until an explicit save.
The editor never stores Canvas, pool placement, dock layout or processing state.

The Streams dock now scrolls its source, grid and inventory forms when the
window is short, instead of squeezing their group boxes below their minimum
height. Its minimum width is unchanged.

## Thumbnail sizing

In fixed grid sizing mode, drag an inactive thumbnail's bottom-right grip to
change its size. The grip remains at the physical bottom-right in RTL shells.
The source aspect ratio and display rotation determine both dimensions; this
does not stretch the source or resize the active editor. Scrolling can reveal
a grip outside the current pool viewport. Release commits; Escape, lost
capture/focus, hiding, deactivation or an incompatible mode/aspect/rotation
change cancels and restores the starting size.

The existing `+` / `-` buttons remain the accessible keyboard fallback, with
the same 16-unit steps and 96-unit minimum short side. Adaptive mode and the
active stream show no grip. Qt and KDE share this interaction; Android creates
no desktop grip and retains its existing touch controls.

Manual sizes remain session-local. Switching to adaptive mode keeps them;
returning to fixed mode still asks whether to restore them or initialize from
the current adaptive rectangles. No daemon configuration, new preference key
or general session format is introduced. Geometry remains provider-owned; the
application applies returned rectangles without a second layout engine.

## Stream order

In the Streams list, drag a stream name before/after another row, or select it
and use **Move earlier / Move later** or **Ctrl+Up / Ctrl+Down**. This desktop
prototype also includes active and hidden sources, so their chosen positions
are kept when they return to the pool. It does not change source visibility,
selection in the active editor, manual sizes or processing priority.

Enable **Preserve order** in the existing grid controls to make Packing retain
this sequence. Leaving it off allows Packing to arrange cells freely; the
chosen sequence is still remembered. Reordering never toggles that preference
automatically. Until an explicit move, the grid retains its default alphabetical
input order. New entries append after opt-in; deleting/clearing the catalogue
removes its corresponding order intent.

Order lasts for this session only and is not added to line configuration or
desktop settings. Qt/KDE use the same controls; Android retains its existing
list workflow. Pinning and priority are not exposed because their semantics
and solver constraints remain research.

## Canvas tools

Enable **View → Desktop presentation → Canvas tools** for a compact strip
above the active video. It is hidden by default, independent of Classic/Canvas
and panel reset. The strip appears only with an active stream; the pool stays
available. Hiding it never saves, cancels or changes processing.

- **Draw geometry**: click to add points, or use the canvas's arrows and
  Space/Enter. Choose a name and optional Closed setting, then Save using the
  existing line workflow (at least two points). Escape clears the drawing;
  Backspace/Ctrl+Z removes the last point.
- **Edit points**: choose an enabled geometry, drag points/the shape or use
  existing keyboard editing. Undo/Redo/Revert act on the same history as the
  dock. Enter the variant name and Save; existing replacement and profile
  rules apply. This does not introduce in-place scene/object editing.
- **Select**: stop drawing; existing templates remain in the Lines dock.
  This is passive cursor mode, not a new click-to-pick entity model. An
  unfinished drawing requires explicit Discard/Cancel. Changing a modified
  edit target also asks before discarding its changes.

The Lines dock remains reachable for full profiles, template application and
numeric point controls. Draw/Edit hand-offs retain pending work within the
same context; changing streams resets the edit target so matching names do
not share geometry. Qt/KDE share these controls. Android adds no strip.

Only the independent visibility flag `desktop/presentation/canvas_tools` is
persisted. Old/missing preferences keep the strip hidden; no line/daemon or
session format changes. Save failures leave live visibility unchanged.

## Contextual inspector

**View → Desktop presentation → Contextual inspector** changes the existing
Lines dock into context-focused geometry properties. Classic presentation is
the default; turn the option off to restore the original controls. The dock's
existing Lines toggle, close and move actions remain independent: enabling
this choice does not reveal a hidden dock. The active view and pool stay present.

The header identifies the active stream and current task. Draw/Templates,
Saved geometry and Edit points reuse the same profile, template, list and
numeric controls. Before an edit target is selected, its point/history/save
controls are hidden rather than presenting a bank of disabled fields. With no
active stream, guidance replaces the forms. Long forms scroll; native keyboard
navigation still reaches their controls. Canvas tools and this inspector use
the same editor, including retained drafts, variant names and undo/redo.

Profiles still describe an unfinished drawing; editing saved geometry still
saves a variant in frame-relative coordinates, not an in-place scene object.
Existing advanced expansion choices are unchanged. Configured-source settings
remain in Streams and can still tune hidden/non-active sources independently.

Only `desktop/presentation/context_inspector` is persisted. It defaults off,
is independent of presets/reset and canvas-tool visibility, and is excluded
from Android. Write failure leaves the live presentation unchanged.

## String / wave preview

In **Lines → Draw geometry** (Draw in Classic), enable **String / wave preview**
to inspect the current drawing profile. It starts collapsed in both Qt and KDE.
The preview is a synthetic hit on a sample open/closed path, not live video
geometry or audio. Its dashed baseline and hit marker provide a reference.
Automatic colours use a representative palette colour, not a camera sample.

**Muted**, **Balanced** and **Resonant** apply the existing width/span/response
fields as one change. Name, colour and closure stay intact; **Add line** still
owns saving. Custom numeric or mixed profiles remain Custom without resetting
their values. **Advanced settings** still exposes the exact editable fields
and optional string vocabulary. Tooltips explain their existing visible effects.

Use the keyboard-native **sample age** slider to scrub travel/decay from 0 to
2400 ms. **Standard / Strong / Extreme** compare 1×/2×/3× displacement only in
the preview; they do not alter live waves, processing or exported profiles.
There is no background animation. Collapse/hide the form without changing a
draft; preview choices are transient and are not restored on restart.

No new physical parameters or mobile controls are introduced. Existing presets
continue through the same template/save and GUI/daemon import/export paths.


## File playback

Enlarge a local file stream to show **Play/Pause**, **Restart**, a **0.25×–2× rate**
selector and a position slider below the active video. Both Qt and KDE use the
same player/controls. Cameras, remote URLs and blank streams show no transport;
pool thumbnails remain lightweight. The active view and pool remain available.

Loading/unavailable files and application suspension disable commands. Seeking
requires a known duration and a seekable backend; Restart also requires seeking.
Drag/release to seek, press Escape to cancel a drag, or use keyboard
arrows/Page/Home/End. Time and speed report the player's properties; rate support
depends on the backend. There is no frame-step or previous/next-event promise.

Pause survives deactivation/reactivation; an interrupted playing file resumes.
Moving the same cell into/out of the pool retains its rate/position. A new source
starts at the existing 1× default. Transport choices are transient, not new saved
configuration fields. The existing configured loop policy remains authoritative.

Playback controls do **not** retime analysis or guarantee every frame is
processed. Seeking/restarting/looping can trigger detector events from frame
discontinuities. Use this prototype for visual inspection, not calibrated event
replay.

## Advanced logs

The optional **Logs** dock now keeps common filters in compact rows. **More
filters** expands event, line/region, algorithm and subsystem choices; its badge
reports active folded filters. **Clear filters** resets only filters/search,
not log mode, collected entries or processing.

**Freeze view** holds displayed logs for inspection while collection continues.
The state label explicitly warns that copy/save still exports the *live current
filtered buffer*, not the frozen view. Unfreeze to show current retained entries;
older events may have been evicted by the existing bounded history. **Follow
latest** scrolls receiving logs to the tail; switch it off to read/select text
without append scrolling. Scrolling away from the tail also turns following off.
These choices are transient and shared by log views
using the same toolbar, not new saved configuration fields.

Qt/KDE retain the existing dock toggles and Classic/Canvas visibility rules;
Android retains its existing log toolbar.

## Processing readout

Enable **View → Processing readout** (`Ctrl+Shift+P`) to inspect the active
visible stream in an optional desktop dock. It starts hidden; close or toggle
it to stop refreshing. Qt and KDE share the panel and saved dock placement /
visibility. Classic/Canvas and reset leave this independent advanced panel
alone; no configuration or processing setting changes when it is shown.

The panel reports source state, input and processed-result GUI arrival rates /
dimensions, current algorithm/preset and policy targets, and the last result
summary. Targets are not measured throughput. A first arrival has dimensions
but says **Rate pending**; **No sample** means no corresponding arrival yet.
**Stale** means no GUI delivery for at least three seconds, not a diagnosed
failure. Paused/ended/low-rate sources may legitimately be stale; suspended
sources and unavailable processing have separate labels. Input and result
ages are independent, and old results can predate the current settings.

The values are grouped as **Active stream**, **GUI observations** and
**Effective policy**. A missing observation shows a dash with its reason; an
input or result path older than three seconds gets a dashed edge besides its
**Stale** text. Values wrap and can be selected and copied with the mouse or
keyboard. The saved **Appearance** material also restyles these sections (Native or Slate).

It follows the canvas's active stream, not the configured-source selector in
Streams. Returning to the pool clears the readout. No CPU/RAM percentages,
pipeline stages, editable graph, history or replay are inferred. While visible
it refreshes once per second; hiding the panel/window stops its reader. Android
does not add this desktop surface.

## Window ownership and fallback icon

The application entrypoint owns the main window in both desktop profiles.
Delete-on-close is disabled, so normal close ends the event loop and entrypoint
cleanup destroys the window once, including its persistence cleanup. The KDE
shell must not independently delete that same owned object.

The application prefers the `org.ninjaro.yodau` themed icon. When unavailable,
it uses the embedded `:/yodau/brand/bug.svg` fallback; no alternate SVG or obsolete
resource alias is required.

## Shared persistence contract

Both desktop profiles use the same `QSettings` organization/application names
and the same `desktop/main_window/*` record for window geometry, dock
placement, toolbar state, and visibility. The KDE shell intentionally does not
enable a separate KDE autosave record, so switching profiles restores the
same state. The integer passed to Qt's state decoder is a binary-state tag,
not application schema negotiation; restore rejects invalid Qt payloads.

Stream and line data do not depend on the selected shell:

- both profiles call the same `stream_controller` import/export methods;
- `.yodau.json` remains the strict versionless line-configuration format shared
  with `yodau-daemon`;
- algorithm, source/output, appearance, geometry, and line-profile fields are
  unchanged;
- no KDE or Qt GUI type is introduced into the core or daemon entrypoint.

Use **File → Export line configuration** in either GUI profile, then pass that
file to `yodau-daemon --config`. A file exported by one profile can be imported
by the other profile and by the daemon without conversion.
