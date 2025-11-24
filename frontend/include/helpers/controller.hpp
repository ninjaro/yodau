#ifndef YODAU_FRONTEND_HELPERS_CONTROLLER_HPP
#define YODAU_FRONTEND_HELPERS_CONTROLLER_HPP

#include <QColor>
#include <QImage>
#include <QMap>
#include <QObject>
#include <QPointF>
#include <QRandomGenerator>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>

#include <vector>

#include "stream_manager.hpp"
#include "widgets/stream_cell.hpp"

class board;
class grid_view;
class settings_panel;

/**
 * @file controller.hpp
 * @brief Declares frontend controller that binds GUI widgets to backend
 * streams.
 *
 * The controller is the "glue" layer between:
 * - backend core (@ref yodau::backend::stream_manager),
 * - UI settings panel (stream/line management),
 * - main board view (grid + active stream view),
 * - per-stream widgets (@ref stream_cell).
 *
 * It owns no heavy resources; instead it orchestrates lifetimes of widgets,
 * forwards frames to backend, and applies backend events back to the UI.
 */

/**
 * @brief Frontend coordinator for streams, lines, templates, and events.
 *
 * Responsibilities:
 * - Initialize settings UI from current backend state.
 * - Add/remove/show streams in the grid view.
 * - Manage "active" stream state (focused view and edit mode).
 * - Convert GUI frames to backend frames and push them for analysis.
 * - Receive backend events and reflect them visually:
 *   - motion events -> transient bubbles,
 *   - tripwire events -> line highlight w/ hit position.
 * - Maintain in-memory line templates and per-stream line instances.
 * - Adapt repaint and analysis throttling based on number of visible streams.
 *
 * Threading:
 * - Backend events may arrive from worker threads. @ref on_backend_event
 *   re-dispatches to the GUI thread when needed using Qt queued invocation.
 * - All other slots are expected to be called from the GUI thread.
 */
class controller final : public QObject {
    Q_OBJECT

public:
    /**
     * @brief Construct a controller.
     *
     * The controller does not take ownership of passed pointers; they must stay
     * alive for the lifetime of this object.
     *
     * On construction it:
     * - initializes state from backend,
     * - installs backend hooks (frame processor, batch event sink, etc.),
     * - configures initial UI candidates,
     * - sets up settings/grid signal connections.
     *
     * @param mgr Backend stream manager (may be null; controller becomes
     * inert).
     * @param panel Settings panel for stream/line/forms (may be null).
     * @param zone Main board widget (may be null).
     * @param parent QObject parent.
     */
    explicit controller(
        yodau::backend::stream_manager* mgr, settings_panel* panel, board* zone,
        QObject* parent = nullptr
    );

    /**
     * @brief Populate settings UI from backend at startup.
     *
     * Reads stream list from backend, adds them to settings panel,
     * and registers them as existing names to prevent duplicates.
     *
     * Safe to call multiple times; it will re-add current backend streams.
     */
    void init_from_backend();

public slots:
    // add tab

    /**
     * @brief Handler for adding a file stream from UI.
     *
     * Delegates to @ref handle_add_stream_common with type "file".
     *
     * @param path Local filesystem path to media.
     * @param name Optional desired stream name.
     * @param loop Whether playback should loop at end-of-file.
     */
    void handle_add_file(const QString& path, const QString& name, bool loop);

    /**
     * @brief Handler for adding a local capture device from UI.
     *
     * Delegates to @ref handle_add_stream_common with type "local".
     *
     * @param source Device path/id (e.g., "/dev/video0").
     * @param name Optional desired stream name.
     */
    void handle_add_local(const QString& source, const QString& name);

    /**
     * @brief Handler for adding a network URL stream from UI.
     *
     * Validates scheme and delegates to @ref handle_add_stream_common with
     * type "url".
     *
     * @param url Stream URL (rtsp/http/https).
     * @param name Optional desired stream name.
     */
    void handle_add_url(const QString& url, const QString& name);

    /**
     * @brief Handler for detecting available local sources.
     *
     * Invokes backend local discovery and updates the UI list of local sources.
     */
    void handle_detect_local_sources();

    // streams tab / grid

    /**
     * @brief Handler for stream visibility toggles in UI.
     *
     * When @p show is true:
     * - adds stream tile to grid,
     * - configures its source/loop based on backend stream type,
     * - installs persistent line overlays,
     * - connects tile frame_ready to @ref on_gui_frame.
     *
     * When false:
     * - removes tile from grid,
     * - also clears active view if this stream was active.
     *
     * Updates repaint + analysis caps after change.
     *
     * @param name Stream name.
     * @param show Whether to show or hide it.
     */
    void handle_show_stream_changed(const QString& name, bool show);

    // backend

    /**
     * @brief Append a textual message to the "active log" in settings.
     *
     * Used for general backend/UI status messages.
     *
     * @param text Message to append.
     */
    void handle_backend_event(const QString& text);

    /**
     * @brief Slot receiving GUI frames from stream tiles.
     *
     * Converts a @ref QImage to backend @ref yodau::backend::frame and pushes
     * it to the backend for analysis.
     *
     * @param stream_name Stream name.
     * @param image Latest frame from GUI.
     */
    void on_gui_frame(const QString& stream_name, const QImage& image);

private slots:
    // active tab

    /**
     * @brief Handler for selecting an active stream in settings.
     *
     * Moves the stream cell into the active container (or clears active view),
     * applies edit mode settings, and refreshes persistent lines/templates.
     *
     * @param name Stream name to activate, or empty to deactivate.
     */
    void on_active_stream_selected(const QString& name);

    /**
     * @brief Handler for toggling active edit mode.
     *
     * When @p drawing_new is true, user draws a new line.
     * Otherwise user previews/templates existing lines.
     *
     * @param drawing_new New edit mode flag.
     */
    void on_active_edit_mode_changed(bool drawing_new);

    /**
     * @brief Handler for changes to "new line" draft parameters.
     *
     * Stores parameters locally and applies them to active cell draft.
     *
     * @param name Draft line name.
     * @param color Draft line color.
     * @param closed Draft closed flag.
     */
    void on_active_line_params_changed(
        const QString& name, const QColor& color, bool closed
    );

    /**
     * @brief Handler for saving a newly drawn draft line.
     *
     * Reads draft points from active cell, converts to string, adds line to
     * backend, connects it to the active stream, and updates UI state.
     *
     * @param name Desired line name (may be auto-resolved by backend).
     * @param closed Whether line is closed.
     */
    void on_active_line_save_requested(const QString& name, bool closed);

    /**
     * @brief Handler for selecting a template while in template mode.
     *
     * Preview is applied to active cell draft.
     *
     * @param template_name Template name.
     */
    void on_active_template_selected(const QString& template_name);

    /**
     * @brief Handler for changing template preview color.
     *
     * Reapplies template preview with new color (template mode only).
     *
     * @param color New preview color.
     */
    void on_active_template_color_changed(const QColor& color);

    /**
     * @brief Handler for adding the selected template to the active stream.
     *
     * Connects backend line, adds a persistent line instance to UI, and resets
     * template form.
     *
     * @param template_name Template to add.
     * @param color Color to render this instance in the active stream.
     */
    void on_active_template_add_requested(
        const QString& template_name, const QColor& color
    );

    /**
     * @brief Handler for undoing last draft point.
     *
     * Removes last draft vertex from active cell.
     */
    void on_active_line_undo_requested();

    /**
     * @brief Handler for toggling persistent label visibility in active view.
     *
     * @param on True to enable labels.
     */
    void on_active_labels_enabled_changed(bool on);

private:
    // setup

    /** @brief Connect settings_panel signals to controller slots. */
    void setup_settings_connections();

    /** @brief Connect grid_view signals to controller handlers. */
    void setup_grid_connections();

    // add helpers

    /**
     * @brief shared implementation for add-stream commands.
     *
     * Performs validation (for urls), adds stream to backend,
     * stores its source/loop flags, logs to UI, and registers in settings.
     *
     * @param source Path/URL/device.
     * @param name Optional desired name.
     * @param type Type string passed to backend ("file","local","url").
     * @param loop Loop flag.
     */
    void handle_add_stream_common(
        const QString& source, const QString& name, const QString& type,
        bool loop
    );

    /**
     * @brief Add newly created backend stream into UI structures.
     *
     * Updates settings entries and refreshes repaint/analysis caps.
     *
     * @param final_name Actual backend name used for stream.
     * @param source_desc Human-readable source description.
     */
    void register_stream_in_ui(
        const QString& final_name, const QString& source_desc
    );

    /**
     * @brief Current wall-clock timestamp as human-readable string.
     *
     * Used for log messages.
     *
     * @return Timestamp like "HH:mm:ss".
     */
    static QString now_ts();

    // grid / active helpers

    /**
     * @brief Handle focus/enlarge requests from grid tiles.
     *
     * Toggles active view for the given stream.
     */
    void handle_enlarge_requested(const QString& name);

    /** @brief Return to grid mode (clear active stream). */
    void handle_back_to_grid();

    /** @brief Convenience alias for activating a thumbnail stream. */
    void handle_thumb_activate(const QString& name);

    /**
     * @brief Get active cell or log a failure.
     *
     * @param fail_prefix Prefix for log messages when lookup fails.
     * @return Active stream cell, or nullptr.
     */
    stream_cell* active_cell_checked(const QString& fail_prefix);

    /**
     * @brief Sync persistent line overlays and template candidates for active
     * stream.
     */
    void sync_active_persistent();

    /**
     * @brief Apply a template preview into the active cell draft.
     *
     * Clears existing draft, loads template points, and applies preview color.
     *
     * @param template_name Template to preview.
     */
    void apply_template_preview(const QString& template_name);

    /**
     * @brief Append a message to the active log (if settings exists).
     */
    void log_active(const QString& msg) const;

    /**
     * @brief Convert draft points to backend format string.
     *
     * Output is parsable by backend @ref yodau::backend::parse_points.
     *
     * @param pts Percentage points.
     * @return String like "(x,y); (x,y); ...".
     */
    static QString points_str_from_pct(const std::vector<QPointF>& pts);

    /**
     * @brief Apply effects of a newly added line to UI and state.
     *
     * Adds persistent line instance, registers as template, connects to
     * backend, resets draft form, and refreshes template candidates.
     *
     * @param cell Active cell.
     * @param final_name Backend-resolved line name.
     * @param pts Draft points.
     * @param closed Closed flag.
     */
    void apply_added_line(
        stream_cell* cell, const QString& final_name,
        const std::vector<QPointF>& pts, bool closed
    );

    /** @brief Push per-stream persistent lines into the active UI cell. */
    void sync_active_cell_lines() const;

    /**
     * @brief Collect template names already used by a stream.
     *
     * @param stream Stream name.
     * @return Set of used template names.
     */
    QSet<QString> used_template_names_for_stream(const QString& stream) const;

    /**
     * @brief List templates not present in a given used set.
     *
     * @param used Already used names.
     * @return Candidate list.
     */
    QStringList template_candidates_excluding(const QSet<QString>& used) const;

    /**
     * @brief Update repaint interval caps for all visible tiles.
     */
    void update_repaint_caps();

    /**
     * @brief Update backend analysis interval based on visible tile count.
     */
    void update_analysis_caps();

    /**
     * @brief Handle a single backend event (GUI-thread safe).
     */
    void on_backend_event(const yodau::backend::event& e);

    /**
     * @brief Handle a batch of backend events.
     */
    void on_backend_events(const std::vector<yodau::backend::event>& evs);

    /**
     * @brief Choose repaint interval given number of visible streams.
     *
     * @param n Visible stream count.
     * @return Interval in ms.
     */
    static int repaint_interval_for_count(int n);

    /**
     * @brief Resolve a tile widget for a given stream name.
     *
     * Prefers active cell if it matches @p name, otherwise looks in grid.
     *
     * @param name Stream name.
     * @return Stream cell pointer or nullptr.
     */
    stream_cell* tile_for_stream_name(const QString& name) const;

    /**
     * @brief Convert a QImage into backend frame.
     *
     * Ensures RGB888 format and fills @ref yodau::backend::frame fields.
     *
     * @param image GUI image.
     * @return Backend frame copy.
     */
    yodau::backend::frame frame_from_image(const QImage& image) const;

private:
    // external

    /** @brief Backend stream manager (non-owning). */
    yodau::backend::stream_manager* stream_mgr { nullptr };

    /** @brief Settings panel (non-owning). */
    settings_panel* settings { nullptr };

    /** @brief Main board/zone widget (non-owning). */
    board* main_zone { nullptr };

    /** @brief Grid view extracted from board (non-owning). */
    grid_view* grid { nullptr };

    // active state

    /** @brief Name of currently active stream (empty if none). */
    QString active_name;

    /** @brief True if active edit mode is "draw new line". */
    bool drawing_new_mode { true };

    /** @brief Whether labels are enabled in active cell. */
    bool active_labels_enabled { true };

    /** @brief Draft line name being edited. */
    QString draft_line_name;

    /** @brief Draft line preview color. */
    QColor draft_line_color { Qt::red };

    /** @brief Draft line closed flag. */
    bool draft_line_closed { false };

    /**
     * @brief Stored template geometry (percentage coordinates).
     *
     * Templates are shared "line definitions" that can be added to multiple
     * streams.
     */
    struct tpl_line {
        /** @brief Template vertices in percentage coordinates. */
        std::vector<QPointF> pts_pct;
        /** @brief Whether template is closed. */
        bool closed { false };
    };

    /** @brief Template registry keyed by template name. */
    QMap<QString, tpl_line> templates;

    /**
     * @brief Per-stream persistent line instances keyed by stream name.
     *
     * Each instance carries its own color and closed flag for rendering.
     */
    QMap<QString, std::vector<stream_cell::line_instance>> per_stream_lines;

    /** @brief Remembered stream sources (as QUrl) keyed by stream name. */
    QMap<QString, QUrl> stream_sources;

    /** @brief Remembered stream loop flags keyed by stream name. */
    QMap<QString, bool> stream_loops;

    /** @brief Repaint interval for active (focused) stream in ms. */
    int active_interval_ms { 33 };

    /** @brief Repaint interval for idle grid streams in ms. */
    int idle_interval_ms { 66 };
};

#endif // YODAU_FRONTEND_HELPERS_CONTROLLER_HPP