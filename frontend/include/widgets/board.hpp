#ifndef YODAU_FRONTEND_WIDGETS_BOARD_HPP
#define YODAU_FRONTEND_WIDGETS_BOARD_HPP

#include <QWidget>

class QStackedWidget;
class grid_view;
class carousel_view;

class board final : public QWidget {
    Q_OBJECT

public:
    explicit board(QWidget* parent = nullptr);

    void show_grid();
    void show_carousel();

    [[nodiscard]] grid_view* grid_mode() const;
    [[nodiscard]] carousel_view* carousel_mode() const;

private:
    QStackedWidget* mode_stack { nullptr };
    grid_view* grid { nullptr };
    carousel_view* carousel { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_BOARD_HPP
