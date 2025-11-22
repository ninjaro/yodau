#ifndef YODAU_FRONTEND_WIDGETS_BOARD_HPP
#define YODAU_FRONTEND_WIDGETS_BOARD_HPP

#include <QWidget>

class QVBoxLayout;
class QWidget;
class grid_view;
class stream_cell;

class board final : public QWidget {
    Q_OBJECT
public:
    explicit board(QWidget* parent = nullptr);

    grid_view* grid_mode() const;

    void set_active_stream(const QString& name);
    void clear_active();
    stream_cell* take_active_cell();

    stream_cell* active_cell() const;

    // signals:
    // void active_shrink_requested(const QString& name);
    // void active_close_requested(const QString& name);

private:
    QWidget* active_container;
    QVBoxLayout* active_layout;
    stream_cell* active_tile;

    grid_view* grid;
};

#endif // YODAU_FRONTEND_WIDGETS_BOARD_HPP
