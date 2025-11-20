#ifndef YODAU_FRONTEND_WIDGETS_VIEW_ZONE_HPP
#define YODAU_FRONTEND_WIDGETS_VIEW_ZONE_HPP
#include <QWidget>

class QStackedWidget;
class grid_view;
class carousel_view;

class view_zone final : public QWidget {
    Q_OBJECT
public:
    explicit view_zone(QWidget* parent = nullptr);

    void show_grid() const;
    void show_carousel() const;

private:
    QStackedWidget* mode_stack;
    grid_view* grid;
    carousel_view* carousel;
};

#endif // YODAU_FRONTEND_WIDGETS_VIEW_ZONE_HPP