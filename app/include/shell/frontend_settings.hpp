#ifndef YODAU_APP_SHELL_FRONTEND_SETTINGS_HPP
#define YODAU_APP_SHELL_FRONTEND_SETTINGS_HPP

#include "shell/app_settings.hpp"

inline QString default_frontend_algorithm_id() {
    return default_app_algorithm_id();
}

inline QStringList frontend_algorithm_ids() {
    return app_algorithm_ids();
}

inline QString frontend_algorithm_display_name(const QString& algorithm_id) {
    return app_algorithm_display_name(algorithm_id);
}

inline QString normalized_frontend_algorithm_id(const QString& algorithm_id) {
    return normalized_app_algorithm_id(algorithm_id);
}

#endif // YODAU_APP_SHELL_FRONTEND_SETTINGS_HPP
