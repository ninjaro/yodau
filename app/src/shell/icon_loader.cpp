#include "shell/icon_loader.hpp"

#include <shared_ui/qt/icons.hpp>

QIcon icon_loader::themed(
    const std::initializer_list<const char*> names,
    const QStyle::StandardPixmap fallback
) {
    return shared_ui::qt::themed_icon(names, fallback);
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
