#include "helpers/icon_loader.hpp"

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
#endif
    return QApplication::style()->standardIcon(fallback);
}
