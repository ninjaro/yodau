#ifndef YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
#define YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP

#include "shell/frontend_settings.hpp"

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

    bool is_draft_preview() const;
    stream_settings current_stream_settings() const;

    void set_active(bool val);
    void set_drawing_enabled(bool on);
    void set_stream_settings(const stream_settings& settings_value);

    void
    set_draft_params(
        const QString& name, const QColor& color, bool closed,
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
    void set_labels_enabled(bool on);

    void set_source(const QUrl& source);
    void set_loop(bool on);
    void set_camera_id(const QByteArray& id);

    struct event_instance {
        QPointF pos_pct;
        QColor color;
        QDateTime ts;
    };

    void add_event(const QPointF& pos_pct, const QColor& color);
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

protected:
    void paintEvent(QPaintEvent* event) override;

    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;

private:
    void build_ui();
    void update_icon();

    void draw_poly_with_points(
        QPainter& p, const std::vector<QPointF>& pts_pct, const QColor& color,
        bool closed, Qt::PenStyle style, qreal width
    ) const;

    void draw_persistent(QPainter& p) const;
    void draw_draft(QPainter& p) const;
    void draw_hover_point(QPainter& p) const;
    void draw_hover_coords(QPainter& p) const;
    void draw_preview_segment(QPainter& p) const;
    void draw_stream_name(QPainter& p) const;

    QPointF label_pos_px(const line_instance& l) const;

    QPointF to_pct(const QPointF& pos_px) const;
    QPointF to_px(const QPointF& pos_pct) const;
    void draw_events(QPainter& p);

    double segment_impact_k(
        const QPointF& a_pct, const QPointF& b_pct,
        const QVector<hit_info>& hit_points, double falloff_pct, double ktime
    ) const;
    void draw_wave_overlay(QPainter& p, const line_instance& line_value) const;
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

    QString draft_line_name;
    QColor draft_line_color { Qt::red };
    bool draft_line_closed { false };
    QString draft_line_width_text { default_line_width_text() };
    QString draft_line_length_text { default_line_length_text() };
    QString draft_line_response_text { default_line_response_text() };
    std::vector<QPointF> draft_line_points_pct;
    std::optional<QPointF> hover_point_pct;

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
    QElapsedTimer repaint_timer;
    QElapsedTimer animation_clock;
    QTimer* animation_timer { nullptr };
    int repaint_interval_ms { 66 };
    QHash<QString, QDateTime> line_highlights;
    int line_highlight_ttl_ms { 2500 };

    QHash<QString, QVector<hit_info>> line_hits;
    QHash<QString, QVector<line_wave_pulse>> line_waves;
};

#endif // YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
