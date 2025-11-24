#ifndef YODAU_FRONTEND_HELPERS_ICON_LOADER_HPP
#define YODAU_FRONTEND_HELPERS_ICON_LOADER_HPP

#include <QApplication>
#include <QIcon>
#include <QStyle>
#include <initializer_list>

/**
 * @brief Utility for loading icons with theme support and fallbacks.
 *
 * This helper tries to obtain icons from the current desktop theme using a list
 * of possible icon names (in priority order). If none of the themed icons are
 * available, it falls back to a Qt standard pixmap from the current application
 * style.
 *
 * On Android builds, themed-icon lookup is skipped by design and the fallback
 * icon is returned immediately.
 */
class icon_loader {
public:
    /**
     * @brief Load a themed icon using multiple candidate names.
     *
     * The function iterates over @p names and returns the first non-null icon
     * found via @c QIcon::fromTheme. If all candidates fail (or if themed icons
     * are disabled on the platform), a standard style icon specified by
     * @p fallback is returned.
     *
     * @param names Candidate icon names in theme lookup order.
     * @param fallback Qt standard pixmap to use when no themed icon is found.
     * @return A valid icon: either a themed icon or a style fallback.
     */
    static QIcon themed(
        std::initializer_list<const char*> names,
        QStyle::StandardPixmap fallback
    );
};

#endif // YODAU_FRONTEND_HELPERS_ICON_LOADER_HPP