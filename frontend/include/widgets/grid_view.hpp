#ifndef YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP
#include <QWidget>

class grid_view final : public QWidget {
    Q_OBJECT
public:
    explicit grid_view(QWidget* parent = nullptr);
};
#endif // YODAU_FRONTEND_WIDGETS_GRID_VIEW_HPP