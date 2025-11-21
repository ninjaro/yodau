#ifndef YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
#define YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP

#include <QWidget>

class QPushButton;
class QLabel;

class stream_cell final : public QWidget {
    Q_OBJECT

public:
    explicit stream_cell(const QString& name, QWidget* parent = nullptr);

    [[nodiscard]] const QString& get_name() const;
    bool is_active() const;

    void set_active(const bool val);

signals:
    void request_close(const QString& name);
    void request_focus(const QString& name);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void build_ui();
    void update_icon();

    QString name;
    QPushButton* close_btn { nullptr };
    QPushButton* focus_btn { nullptr };
    QLabel* name_label { nullptr };

    bool active { false };
};

#endif // YODAU_FRONTEND_WIDGETS_STREAM_CELL_HPP
