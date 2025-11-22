#ifndef YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
#define YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP

#include <QColor>
#include <QPointF>
#include <QWidget>

#include <optional>
#include <vector>

class QPushButton;
class QLabel;

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

    void set_active(const bool val);

    void set_drawing_enabled(bool on);
    void
    set_draft_params(const QString& name, const QColor& color, bool closed);
    void set_draft_points_pct(const std::vector<QPointF>& pts);
    void clear_draft();

    [[nodiscard]] std::vector<QPointF> draft_points_pct() const;
    [[nodiscard]] bool draft_closed() const;
    [[nodiscard]] QString draft_name() const;
    [[nodiscard]] QColor draft_color() const;

    void set_persistent_lines(const std::vector<line_instance>& lines);
    void add_persistent_line(const line_instance& line);
    void clear_persistent_lines();

    void set_draft_preview(bool on);
    bool is_draft_preview() const;

signals:
    void request_close(const QString& name);
    void request_focus(const QString& name);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    void build_ui();
    void update_icon();

    QPointF to_pct(const QPointF& pos_px) const;
    QPointF to_px(const QPointF& pos_pct) const;

    QString name;
    QPushButton* close_btn { nullptr };
    QPushButton* focus_btn { nullptr };
    QLabel* name_label { nullptr };

    bool active { false };

    bool drawing_enabled { false };
    QString draft_line_name;
    QColor draft_line_color { Qt::red };
    bool draft_line_closed { false };
    std::vector<QPointF> draft_line_points_pct;
    std::optional<QPointF> hover_point_pct;

    std::vector<line_instance> persistent_lines;
    bool draft_preview { false };
};

#endif // YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
