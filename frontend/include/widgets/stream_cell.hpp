#ifndef YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
#define YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP

#include <QCamera>
#include <QCameraDevice>
#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QHash>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QPointF>
#include <QString>
#include <QUrl>
#include <QVector>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>

#include <optional>
#include <vector>

/**
 * @file stream_cell.hpp
 * @brief Declarative UI widget for visualizing a single stream with overlays.
 *
 * This header declares @ref stream_cell, a Qt widget that:
 * - displays video frames (from a media player or camera),
 * - supports interactive drawing of "draft" lines in percentage coordinates,
 * - renders persistent (saved) lines on top of the video,
 * - shows transient event markers (e.g. motion/tripwire hits),
 * - emits signals for user actions (close/focus) and frame availability.
 *
 * The implementation lives in stream_cell.cpp.
 */

class QLabel;
class QPushButton;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;

/**
 * @brief Widget representing one video stream cell in the frontend.
 *
 * A stream cell is a self-contained view of a video source that can be:
 * - inactive (thumbnail in a grid),
 * - active (focused/enlarged and optionally editable).
 *
 * Coordinate system:
 * - All geometry overlays (draft and persistent lines, events) are stored
 *   in **percentage coordinates**:
 *   - x,y in range [0.0; 100.0]
 *   - (0,0) is top-left of the widget
 *   - (100,100) is bottom-right of the widget
 *
 * Interaction:
 * - When drawing is enabled and the cell is active:
 *   - Left click adds a draft point at cursor position.
 *   - Mouse move updates hover position (preview segment).
 *   - Backspace / Ctrl+Z removes last draft point.
 *
 * Rendering:
 * - Video frame fills the widget area.
 * - Persistent lines are drawn solid, with optional labels.
 * - Draft line is drawn dashed.
 * - Hover point/coords and preview segments are shown while drawing.
 * - Recent events are drawn as fading circles.
 * - Line highlights (by name and optional hit point) animate for a short TTL.
 */
class stream_cell final : public QWidget {
    Q_OBJECT

public:
    /**
     * @brief Instance of a persistent (saved) line to be rendered on the
     * stream.
     *
     * Line is described in percentage coordinates.
     */
    struct line_instance {
        /** @brief Name of the underlying template/definition in backend. */
        QString template_name;
        /** @brief Line color used for rendering (default: red). */
        QColor color { Qt::red };
        /** @brief Whether the polyline should be treated as closed polygon. */
        bool closed { false };
        /** @brief Polyline points in percentage coordinates. */
        std::vector<QPointF> pts_pct;
    };

    /**
     * @brief Construct a stream cell.
     *
     * @param name Logical stream name displayed in the widget.
     * @param parent Optional parent widget.
     */
    explicit stream_cell(const QString& name, QWidget* parent = nullptr);

    /**
     * @brief Get logical name of this stream cell.
     * @return Reference to stored name.
     */
    [[nodiscard]] const QString& get_name() const;

    /**
     * @brief Check whether this cell is currently active (focused).
     * @return true if active.
     */
    bool is_active() const;

    /**
     * @brief Get current draft polyline points (percentage coordinates).
     * @return Copy of draft points.
     */
    [[nodiscard]] std::vector<QPointF> draft_points_pct() const;

    /**
     * @brief Get whether current draft line is closed.
     * @return true if draft is closed.
     */
    [[nodiscard]] bool draft_closed() const;

    /**
     * @brief Get current draft line name.
     * @return Draft name, may be empty.
     */
    [[nodiscard]] QString draft_name() const;

    /**
     * @brief Get current draft line color.
     * @return Draft color.
     */
    [[nodiscard]] QColor draft_color() const;

    /**
     * @brief Check whether the draft is in preview-only mode.
     *
     * Preview mode means draft is shown but not necessarily editable.
     */
    bool is_draft_preview() const;

    /**
     * @brief Set active (focused) state.
     *
     * Deactivating a cell automatically disables drawing and clears draft.
     *
     * @param val New active state.
     */
    void set_active(bool val);

    /**
     * @brief Enable or disable interactive drawing on this cell.
     *
     * Enabling drawing also enables mouse tracking to receive hover updates.
     *
     * @param on True to enable drawing.
     */
    void set_drawing_enabled(bool on);

    /**
     * @brief Set draft line parameters (name, color, closed flag).
     *
     * Does not modify draft points.
     *
     * @param name Draft line name.
     * @param color Draft line color.
     * @param closed Draft line closed flag.
     */
    void
    set_draft_params(const QString& name, const QColor& color, bool closed);

    /**
     * @brief Replace current draft points (percentage coordinates).
     *
     * @param pts New draft points.
     */
    void set_draft_points_pct(const std::vector<QPointF>& pts);

    /**
     * @brief Clear all draft data (points, hover point, preview flag).
     */
    void clear_draft();

    /**
     * @brief Replace all persistent lines.
     *
     * @param lines New list of persistent line instances.
     */
    void set_persistent_lines(const std::vector<line_instance>& lines);

    /**
     * @brief Append a persistent line to the list.
     *
     * @param line Line instance to add.
     */
    void add_persistent_line(const line_instance& line);

    /**
     * @brief Remove all persistent lines.
     */
    void clear_persistent_lines();

    /**
     * @brief Enable or disable draft preview mode.
     *
     * @param on True to enable preview.
     */
    void set_draft_preview(bool on);

    /**
     * @brief Enable or disable rendering of persistent line labels.
     *
     * Labels are only shown when the cell is active.
     *
     * @param on True to enable labels.
     */
    void set_labels_enabled(bool on);

    /**
     * @brief Set media player source.
     *
     * If a media player is available, switches it to @p source and starts
     * playback.
     *
     * @param source URL of local file or network stream.
     */
    void set_source(const QUrl& source);

    /**
     * @brief Enable or disable looping for file-based playback.
     *
     * @param on True to loop on end-of-media.
     */
    void set_loop(bool on);

    /**
     * @brief Switch to camera input by device id.
     *
     * Stops any previous player/camera, creates capture session if needed,
     * finds a matching camera device by @p id, and starts capture.
     *
     * @param id Camera device id returned by Qt multimedia.
     */
    void set_camera_id(const QByteArray& id);

    /**
     * @brief Instance of a transient visual event marker.
     *
     * Events are maintained in percentage coordinates and rendered as fading
     * circles for a fixed TTL.
     */
    struct event_instance {
        /** @brief Event position in percentage coordinates. */
        QPointF pos_pct;
        /** @brief Event color. */
        QColor color;
        /** @brief Timestamp when event was added. */
        QDateTime ts;
    };

    /**
     * @brief Add a transient event marker.
     *
     * @param pos_pct Event position in percentage coordinates.
     * @param color Event color.
     */
    void add_event(const QPointF& pos_pct, const QColor& color);

    /**
     * @brief Set minimum repaint interval for video frame updates.
     *
     * Frames arriving faster than this interval will be coalesced.
     *
     * @param ms Interval in milliseconds (> 0).
     */
    void set_repaint_interval_ms(int ms);

    /**
     * @brief Highlight a persistent line by name.
     *
     * Triggers a temporal highlight animation (thicker/alpha pulse).
     *
     * @param line_name Name of the line to highlight.
     */
    void highlight_line(const QString& line_name);

    /**
     * @brief Highlight a line and record a hit position for spatial falloff.
     *
     * The highlight animation will concentrate around @p pos_pct if the line
     * supports segment-wise highlighting.
     *
     * @param line_name Name of the line to highlight.
     * @param pos_pct Hit position in percentage coordinates.
     */
    void highlight_line_at(const QString& line_name, const QPointF& pos_pct);

signals:
    /**
     * @brief Emitted when the user requests closing this stream cell.
     *
     * Typically triggered by clicking the close button.
     *
     * @param name Stream cell name.
     */
    void request_close(const QString& name);

    /**
     * @brief Emitted when the user requests focusing/enlarging this cell.
     *
     * Typically triggered by clicking the focus button.
     *
     * @param name Stream cell name.
     */
    void request_focus(const QString& name);

    /**
     * @brief Emitted whenever a new frame image becomes available.
     *
     * Useful for forwarding frames to backend processing or snapshots.
     *
     * @param stream_name Name of the stream.
     * @param image Converted QImage representing the latest frame.
     */
    void frame_ready(const QString& stream_name, const QImage& image);

protected:
    /**
     * @brief Paint handler.
     *
     * Draws:
     * - widget background,
     * - last video frame or "no signal" message,
     * - stream name,
     * - events, lines, draft and hover overlays.
     */
    void paintEvent(QPaintEvent* event) override;

    /**
     * @brief Mouse press handler for drawing draft points.
     */
    void mousePressEvent(QMouseEvent* event) override;

    /**
     * @brief Mouse move handler for hover updates while drawing.
     */
    void mouseMoveEvent(QMouseEvent* event) override;

    /**
     * @brief Leave handler to clear hover state.
     */
    void leaveEvent(QEvent* event) override;

    /**
     * @brief Key press handler for draft undo.
     */
    void keyPressEvent(QKeyEvent* event) override;

private:
    /** @brief Build child UI widgets (buttons, sink/player connections). */
    void build_ui();
    /** @brief Update focus button icon/tooltip based on active state. */
    void update_icon();

    /**
     * @brief Draw a polyline/polygon with point markers.
     *
     * Points are given in percentage coordinates and projected to widget space.
     *
     * @param p Painter to draw with.
     * @param pts_pct Points in percentage coordinates.
     * @param color Stroke color.
     * @param closed Whether to draw polygon instead of polyline.
     * @param style Pen style.
     * @param width Pen width in pixels.
     */
    void draw_poly_with_points(
        QPainter& p, const std::vector<QPointF>& pts_pct, const QColor& color,
        bool closed, Qt::PenStyle style, qreal width
    ) const;

    /** @brief Draw all persistent lines and their labels/highlights. */
    void draw_persistent(QPainter& p) const;
    /** @brief Draw the draft line (if any). */
    void draw_draft(QPainter& p) const;
    /** @brief Draw hover point indicator (if any). */
    void draw_hover_point(QPainter& p) const;
    /** @brief Draw hover coordinate text (if enabled). */
    void draw_hover_coords(QPainter& p) const;
    /** @brief Draw preview segment from last draft point to hover point. */
    void draw_preview_segment(QPainter& p) const;
    /** @brief Draw stream name overlay at top-left. */
    void draw_stream_name(QPainter& p) const;

    /**
     * @brief Compute label anchor position for a line in pixel coordinates.
     *
     * Uses first or last point depending on closed flag.
     */
    QPointF label_pos_px(const line_instance& l) const;

    /**
     * @brief Convert pixel position to percentage coordinates.
     */
    QPointF to_pct(const QPointF& pos_px) const;

    /**
     * @brief Convert percentage coordinates to pixel position.
     */
    QPointF to_px(const QPointF& pos_pct) const;

    /**
     * @brief Draw transient events and prune expired ones.
     */
    void draw_events(QPainter& p);

private slots:
    /**
     * @brief Slot called when the video sink receives a new frame.
     *
     * Converts the frame to QImage, emits @ref frame_ready,
     * and schedules repaint respecting @ref repaint_interval_ms.
     */
    void on_frame_changed(const QVideoFrame& frame);

    /**
     * @brief Slot called on media status changes.
     *
     * Used to implement looping on end-of-media when enabled.
     */
    void on_media_status_changed(QMediaPlayer::MediaStatus status);

    /**
     * @brief Slot called when media player errors occur.
     *
     * Stores the error string and triggers a repaint.
     */
    void
    on_player_error(QMediaPlayer::Error error, const QString& error_string);

    /**
     * @brief Slot called on camera errors.
     *
     * Stores camera error string and triggers a repaint.
     */
    void on_camera_error(QCamera::Error error);

private:
    /** @brief Logical stream name. */
    QString name;

    /** @brief UI close button (top-right). */
    QPushButton* close_btn { nullptr };
    /** @brief UI focus/enlarge button (top-right). */
    QPushButton* focus_btn { nullptr };
    /** @brief Optional name label (unused in current implementation). */
    QLabel* name_label { nullptr };

    /** @brief Whether the cell is focused/active. */
    bool active { false };

    /** @brief Whether interactive drawing is enabled. */
    bool drawing_enabled { false };
    /** @brief Whether draft line is shown in preview mode. */
    bool draft_preview { false };
    /** @brief Whether persistent line labels are shown when active. */
    bool labels_enabled { true };

    /** @brief Draft line name. */
    QString draft_line_name;
    /** @brief Draft line color. */
    QColor draft_line_color { Qt::red };
    /** @brief Draft line closed flag. */
    bool draft_line_closed { false };
    /** @brief Draft polyline points in percentage coordinates. */
    std::vector<QPointF> draft_line_points_pct;
    /** @brief Current hover position in percentage coordinates. */
    std::optional<QPointF> hover_point_pct;

    /** @brief Persisted lines to render. */
    std::vector<line_instance> persistent_lines;

    /** @brief Media player for file/URL sources. */
    QMediaPlayer* player { nullptr };
    /** @brief Video sink feeding decoded frames into the widget. */
    QVideoSink* sink { nullptr };
    /** @brief Most recent received frame as an image. */
    QImage last_frame;
    /** @brief Whether playback looping is enabled. */
    bool loop_enabled { true };
    /** @brief Last error string to display when no frame is available. */
    QString last_error;

    /** @brief Active camera (if using live input). */
    QCamera* camera { nullptr };
    /** @brief Capture session binding camera to sink. */
    QMediaCaptureSession* session { nullptr };
    /** @brief Selected camera id. */
    QByteArray camera_id;

    /** @brief Transient events currently displayed. */
    QVector<event_instance> events;

    /** @brief Timer throttling repaint frequency. */
    QElapsedTimer repaint_timer;
    /** @brief Minimum repaint interval in ms. */
    int repaint_interval_ms { 66 };

    /**
     * @brief Line highlight timestamps by line name.
     *
     * Used to animate highlight effects for a fixed TTL.
     */
    QHash<QString, QDateTime> line_highlights;

    /** @brief Highlight time-to-live in ms. */
    int line_highlight_ttl_ms { 2500 };

    /**
     * @brief Hit info attached to a line highlight.
     *
     * Allows spatially localized highlight (falloff from hit position).
     */
    struct hit_info {
        /** @brief Hit position in percentage coordinates. */
        QPointF pos_pct;
        /** @brief Hit timestamp. */
        QDateTime ts;
    };

    /** @brief Optional hit positions per line name. */
    QHash<QString, hit_info> line_hits;
};

#endif // YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP