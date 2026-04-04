#ifndef YODAU_APP_SHELL_ICON_LOADER_HPP
#define YODAU_APP_SHELL_ICON_LOADER_HPP
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
    static QIcon title_bar_close_icon();
    static QIcon title_bar_restore_icon();
    static QIcon title_bar_maximize_icon();
};
#endif // YODAU_APP_SHELL_ICON_LOADER_HPP
