#ifndef YODAU_FRONTEND_WIDGETS_CAROUSEL_VIEW_HPP
#define YODAU_FRONTEND_WIDGETS_CAROUSEL_VIEW_HPP
#include <QWidget>

class carousel_view final : public QWidget {
    Q_OBJECT
public:
    explicit carousel_view(QWidget* parent = nullptr);
};

#endif // YODAU_FRONTEND_WIDGETS_CAROUSEL_VIEW_HPP