#ifndef YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
#define YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP

#include <QCamera>
#include <QCameraDevice>
#include <QColor>
#include <QElapsedTimer>
#include <QImage>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QMediaPlayer>
#include <QPointF>
#include <QString>
#include <QVideoFrame>
#include <QVideoSink>
#include <QWidget>
#include <QDateTime>

#include <optional>
#include <vector>

class QLabel;
class QPushButton;
class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;

class stream_cell final : public QWidget {
    Q_OBJECT

public:
    struct line_instance {
        QString template_name;
        QColor color { Qt::red };
        bool closed { false };
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

    void set_active(bool val);
    void set_drawing_enabled(bool on);

    void
    set_draft_params(const QString& name, const QColor& color, bool closed);
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

signals:
    void request_close(const QString& name);
    void request_focus(const QString& name);

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

private slots:
    void on_frame_changed(const QVideoFrame& frame);
    void on_media_status_changed(QMediaPlayer::MediaStatus status);
    void
    on_player_error(QMediaPlayer::Error error, const QString& error_string);
    void on_camera_error(QCamera::Error error);

private:
    QString name;

    QPushButton* close_btn { nullptr };
    QPushButton* focus_btn { nullptr };
    QLabel* name_label { nullptr };

    bool active { false };

    bool drawing_enabled { false };
    bool draft_preview { false };
    bool labels_enabled { true };

    QString draft_line_name;
    QColor draft_line_color { Qt::red };
    bool draft_line_closed { false };
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
    int repaint_interval_ms { 66 };
    QHash<QString, QDateTime> line_highlights;
    int line_highlight_ttl_ms { 2500 };
};

#endif // YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
