#ifndef YODAU_FRONTEND_HELPERS_ICON_LOADER_HPP
#define YODAU_FRONTEND_HELPERS_ICON_LOADER_HPP
#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <initializer_list>

class icon_loader {
public:
    static QIcon themed(
        std::initializer_list<const char*> names,
        QStyle::StandardPixmap fallback
    );
};
#endif // YODAU_FRONTEND_HELPERS_ICON_LOADER_HPP