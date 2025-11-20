#ifndef YODAU_FRONTEND_HELPERS_STR_LABEL_HPP
#define YODAU_FRONTEND_HELPERS_STR_LABEL_HPP
#include <QString>

#ifdef KC_KDE
#include <KLocalizedString>
#define str_label(text) i18n(text)
#else
#define str_label(text) QStringLiteral(text)
#endif

#endif // YODAU_FRONTEND_HELPERS_STR_LABEL_HPP