#ifndef YODAU_APP_WIDGETS_STREAM_CELL_HPP
#define YODAU_APP_WIDGETS_STREAM_CELL_HPP

#include "shell/app_settings.hpp"
#include "shell/app_log.hpp"
#include "widgets/processing_overlay.hpp"

#include <QCamera>
#include <QCameraDevice>
#include <QColor>
#include <QDateTime>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QPointF>
#include <QString>
#include <QTimer>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>

#include <optional>
#include <vector>

class QLabel;
class QPushButton;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QWheelEvent;

struct hit_info {
    QPointF pos_pct;
    QDateTime ts;
    double strength { 1.0 };
};

class stream_cell final : public QWidget {
    Q_OBJECT

public:
    struct line_instance {
        QString template_name;
        QColor color { Qt::red };
        QString color_mode_id { default_line_color_mode_id() };
        bool enabled { true };
        bool closed { false };
        QString width_text { default_line_width_text() };
        QString length_text { default_line_length_text() };
        QString response_text { default_line_response_text() };
        std::vector<QPointF> pts_pct;
    };

    explicit stream_cell(const QString& name, QWidget* parent = nullptr);

    [[nodiscard]] const QString& get_name() const;
    bool is_active() const;

    [[nodiscard]] std::vector<QPointF> draft_points_pct() const;
    [[nodiscard]] bool draft_closed() const;
    [[nodiscard]] QString draft_name() const;
    [[nodiscard]] QColor draft_color() const;
    [[nodiscard]] bool is_drawing_enabled() const;
    [[nodiscard]] bool has_line_edit_preview() const;
    [[nodiscard]] QString line_edit_preview_name() const;

    bool is_draft_preview() const;
    stream_settings current_stream_settings() const;
    stream_runtime_metrics current_runtime_metrics() const;
    app_log_mode current_log_mode() const;

    void set_active(bool val);
    void set_drawing_enabled(bool on);
    void set_stream_settings(const stream_settings& settings_value);
    void set_runtime_metrics(const stream_runtime_metrics& metrics);
    void set_log_mode(app_log_mode mode);

    void
    set_draft_params(
        const QString& name, const QColor& color, bool closed,
        const QString& color_mode_id = default_line_color_mode_id(),
        const QString& width_text = default_line_width_text(),
        const QString& length_text = default_line_length_text(),
        const QString& response_text = default_line_response_text()
    );
    void set_draft_points_pct(const std::vector<QPointF>& pts);
    void clear_draft();

    void set_persistent_lines(const std::vector<line_instance>& lines);
    void add_persistent_line(const line_instance& line);
    void clear_persistent_lines();

    void set_draft_preview(bool on);
    void set_line_edit_preview(const std::optional<line_instance>& line_value);
    void set_labels_enabled(bool on);

    void set_source(const QUrl& source);
    void set_loop(bool on);
    void set_camera_id(const QByteArray& id);

    struct event_instance {
        QPointF pos_pct;
        QColor color;
        QDateTime ts;
    };

    using processing_overlay_kind = ::processing_overlay_kind;
    using processing_overlay_instance = ::processing_overlay_instance;

    void add_event(const QPointF& pos_pct, const QColor& color);
    void set_processing_overlays(std::vector<processing_overlay_instance> overlays);
    void set_repaint_interval_ms(int ms);
    void highlight_line(const QString& line_name);

    void highlight_line_at(
        const QString& line_name, const QPointF& pos_pct, double strength = 1.0,
        const QString& direction = QString(), double speed = 1.0
    );

signals:
    void request_close(const QString& name);
    void request_focus(const QString& name);
    void frame_ready(const QString& stream_name, const QImage& image);
    void line_edit_point_selected(int visible_index);
    void line_edit_shape_drag_requested(QPointF delta_pct);
    void
    line_edit_point_move_requested(int visible_index, QPointF point_pct);
    void line_edit_point_split_requested(int visible_index);
    void line_edit_shape_rotate_requested(double delta_degrees, int visible_pivot_index);

protected:
    void paintEvent(QPaintEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    void build_ui();
    void update_icon();

    void draw_poly_with_points(
        QPainter& p, const std::vector<QPointF>& pts_pct, const QColor& color,
        bool closed, Qt::PenStyle style, qreal width
    ) const;

    void draw_persistent(QPainter& p) const;
    void draw_draft(QPainter& p) const;
    void draw_line_edit_preview(QPainter& p) const;
    void draw_hover_point(QPainter& p) const;
    void draw_hover_coords(QPainter& p) const;
    void draw_preview_segment(QPainter& p) const;
    void draw_stream_name(QPainter& p) const;
    void draw_runtime_metrics(QPainter& p) const;

    QPointF label_pos_px(const line_instance& l) const;

    QPointF to_pct(const QPointF& pos_px) const;
    QPointF to_px(const QPointF& pos_pct) const;
    std::optional<int> line_edit_vertex_at(const QPointF& pos_px) const;
    bool line_edit_segment_hit(const QPointF& pos_px) const;
    void reset_line_edit_drag_state();
    void sync_mouse_tracking();
    void draw_processing_overlays(QPainter& p) const;
    void draw_events(QPainter& p);

    double segment_impact_k(
        const QPointF& a_pct, const QPointF& b_pct,
        const QVector<hit_info>& hit_points, double falloff_pct, double ktime
    ) const;
    void draw_wave_overlay(
        QPainter& p, const line_instance& line_value, const QColor& line_color
    ) const;
    void add_line_wave(
        const QString& line_name, const QPointF& pos_pct, double strength,
        const QString& direction, double speed
    );
    void prune_line_waves(qint64 now_ms);
    void update_animation_timer();

private slots:
    void on_close_clicked();
    void on_focus_clicked();
    void on_frame_changed(const QVideoFrame& frame);
    void on_media_status_changed(QMediaPlayer::MediaStatus status);
    void
    on_player_error(QMediaPlayer::Error error, const QString& error_string);
    void on_camera_error(QCamera::Error error);
    void on_animation_tick();

public:
    struct line_wave_pulse {
        int source_segment_index { -1 };
        qint64 start_ms { 0 };
        double origin_path_pos { 0.0 };
        double amplitude { 0.0 };
        double speed { 0.0 };
        double spread { 0.0 };
        double frequency_hz { 0.0 };
        double damping_per_s { 0.0 };
        int travel_sign { 1 };
        int displacement_sign { 1 };
    };

private:
    QString name;

    QPushButton* close_btn { nullptr };
    QPushButton* focus_btn { nullptr };
    QLabel* name_label { nullptr };

    bool active { false };

    bool drawing_enabled { false };
    bool draft_preview { false };
    bool labels_enabled { true };
    stream_settings stream_settings_value;
    stream_runtime_metrics runtime_metrics_value;
    app_log_mode log_mode_value { app_log_mode::release };

    QString draft_line_name;
    QColor draft_line_color { Qt::red };
    QString draft_line_color_mode_id { default_line_color_mode_id() };
    bool draft_line_closed { false };
    QString draft_line_width_text { default_line_width_text() };
    QString draft_line_length_text { default_line_length_text() };
    QString draft_line_response_text { default_line_response_text() };
    std::vector<QPointF> draft_line_points_pct;
    std::optional<QPointF> hover_point_pct;
    std::optional<line_instance> line_edit_preview_;

    std::vector<line_instance> persistent_lines;

    QMediaPlayer* player { nullptr };
    QVideoSink* sink { nullptr };
    QImage last_frame;
    bool loop_enabled { true };
    QString last_error;
    QCamera* camera { nullptr };
    QMediaCaptureSession* session { nullptr };
    QByteArray camera_id;
    QVector<event_instance> events;
    std::vector<processing_overlay_instance> processing_overlays;
    QElapsedTimer repaint_timer;
    QElapsedTimer animation_clock;
    QTimer* animation_timer { nullptr };
    int repaint_interval_ms { 66 };
    QHash<QString, QDateTime> line_highlights;
    int line_highlight_ttl_ms { 2500 };
    std::optional<int> line_edit_selected_vertex_;
    std::optional<int> line_edit_pressed_vertex_;
    std::vector<QPointF> line_edit_press_points_pct_;
    QPointF line_edit_press_origin_pct_;
    QPoint line_edit_press_origin_px_;
    bool line_edit_press_active_ { false };
    bool line_edit_press_moved_ { false };
    bool line_edit_press_hit_shape_ { false };
    bool line_edit_pressed_vertex_was_selected_ { false };
    enum class line_edit_drag_mode {
        none,
        shape,
        point,
    };
    line_edit_drag_mode line_edit_drag_mode_ { line_edit_drag_mode::none };

    QHash<QString, QVector<hit_info>> line_hits;
    QHash<QString, QVector<line_wave_pulse>> line_waves;
};

#endif // YODAU_APP_WIDGETS_STREAM_CELL_HPP
