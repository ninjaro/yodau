#ifndef YODAU_FRONTEND_HELPERS_STR_LABEL_HPP
#define YODAU_FRONTEND_HELPERS_STR_LABEL_HPP

#include <QString>

/**
 * @def str_label(text)
 * @brief Create a user-visible localized label.
 *
 * This macro provides a lightweight abstraction over translation/localization:
 * - If KC_KDE is defined, it expands to KDE's @c i18n(text) for runtime
 *   translation using KLocalizedString.
 * - Otherwise it expands to @c QStringLiteral(text) to produce an efficient,
 *   compile-time UTF-16 QString.
 *
 * Usage:
 * @code
 * auto title = str_label("Settings");
 * @endcode
 *
 * @param text Narrow string literal to translate or wrap as a QString.
 * @return A localized QString.
 */

#ifdef KC_KDE
#include <KLocalizedString>
#define str_label(text) i18n(text)
#else
#define str_label(text) QStringLiteral(text)
#endif

#endif // YODAU_FRONTEND_HELPERS_STR_LABEL_HPP