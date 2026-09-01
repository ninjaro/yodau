#ifndef YODAU_APP_WIDGETS_GRID_VIEW_HPP
#define YODAU_APP_WIDGETS_GRID_VIEW_HPP

#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

class QScrollArea;

class stream_cell;

class grid_view final : public QWidget {
    Q_OBJECT
public:
    enum class scroll_direction { horizontal, vertical };
    enum class sizing_mode { adaptive, fixed };

    explicit grid_view(QWidget* parent = nullptr);

    [[nodiscard]] bool has_stream(const QString& name) const;
    [[nodiscard]] QStringList stream_names() const;
    [[nodiscard]] int layout_row_count() const;
    [[nodiscard]] int layout_column_count() const;
    [[nodiscard]] int layout_cell_count() const;
    [[nodiscard]] scroll_direction current_scroll_direction() const noexcept;
    [[nodiscard]] bool preserves_stream_order() const noexcept;
    [[nodiscard]] sizing_mode current_sizing_mode() const noexcept;

    void add_stream(const QString& name);
    void remove_stream(const QString& name);

    stream_cell* take_stream_cell(const QString& name);
    void put_stream_cell(stream_cell* cell);
    [[nodiscard]] stream_cell* peek_stream_cell(const QString& name) const;
    void set_application_active(bool active);
    void set_scroll_direction(scroll_direction direction);
    void set_preserve_stream_order(bool preserve);
    void set_sizing_mode(sizing_mode mode);

signals:
    void stream_closed(const QString& name);
    void stream_enlarge(const QString& name);

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void rebuild_layout();
    void close_requested(const QString& name);
    void enlarge_requested(const QString& name);

    QScrollArea* scroll;
    QWidget* grid_container;
    QMap<QString, stream_cell*> tiles;
    int last_layout_rows { 0 };
    int last_layout_columns { 0 };
    bool application_active_ { true };
    scroll_direction scroll_direction_ { scroll_direction::horizontal };
    bool preserve_stream_order_ { false };
    sizing_mode sizing_mode_ { sizing_mode::adaptive };
    bool has_remembered_manual_sizes_ { false };
};

#endif // YODAU_APP_WIDGETS_GRID_VIEW_HPP
