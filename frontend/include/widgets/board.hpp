#ifndef YODAU_FRONTEND_WIDGETS_BOARD_HPP
#define YODAU_FRONTEND_WIDGETS_BOARD_HPP

#include <QWidget>

class QString;
class QVBoxLayout;

class grid_view;
class stream_cell;

class board final : public QWidget {
    Q_OBJECT
public:
    explicit board(QWidget* parent = nullptr);

    grid_view* grid_mode() const;
    stream_cell* active_cell() const;

    void set_active_stream(const QString& name);
    void clear_active();
    stream_cell* take_active_cell();

private:
    grid_view* grid;

    QWidget* active_container;
    QVBoxLayout* active_layout;
    stream_cell* active_tile;
};

#endif // YODAU_FRONTEND_WIDGETS_BOARD_HPP
