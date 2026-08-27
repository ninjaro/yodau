#include "shell/icon_loader.hpp"

QIcon icon_loader::themed(
    const std::initializer_list<const char*> names,
    const QStyle::StandardPixmap fallback
) {

#if !(defined(KC_ANDROID) || defined(Q_OS_ANDROID))
    for (auto name : names) {
        QIcon ico = QIcon::fromTheme(QString::fromLatin1(name));
        if (!ico.isNull()) {
            return ico;
        }
    }
#else
    Q_UNUSED(names);
#endif
    return QApplication::style()->standardIcon(fallback);
}

QIcon icon_loader::title_bar_close_icon() {
#if defined(KC_KDE)
    return themed(
        { "window-close", "dialog-close", "edit-delete" },
        QStyle::SP_TitleBarCloseButton
    );
#else
    return themed(
        { "window-close", "dialog-close" }, QStyle::SP_TitleBarCloseButton
    );
#endif
}

QIcon icon_loader::title_bar_restore_icon() {
#if defined(KC_KDE)
    return themed(
        { "view-restore", "window-restore", "transform-scale" },
        QStyle::SP_TitleBarNormalButton
    );
#else
    return themed(
        { "view-restore", "window-restore" }, QStyle::SP_TitleBarNormalButton
    );
#endif
}

QIcon icon_loader::title_bar_maximize_icon() {
#if defined(KC_KDE)
    return themed(
        { "view-fullscreen", "window-maximize", "transform-scale" },
        QStyle::SP_TitleBarMaxButton
    );
#else
    return themed(
        { "view-fullscreen", "fullscreen", "window-maximize" },
        QStyle::SP_TitleBarMaxButton
    );
#endif
}
