#ifndef YODAU_APP_SHELL_MOBILE_SESSION_STORE_HPP
#define YODAU_APP_SHELL_MOBILE_SESSION_STORE_HPP

#include <QByteArray>
#include <QString>

class QSettings;

namespace yodau::shell {

inline constexpr int mobile_session_schema_version = 1;
inline constexpr qsizetype maximum_mobile_configuration_bytes = 4 * 1024 * 1024;

enum class mobile_page {
    monitor = 0,
    streams = 1,
    lines = 2,
    logs = 3,
};

struct mobile_session_state {
    mobile_page page { mobile_page::monitor };
    QByteArray line_configuration;
    QString warning;
};

[[nodiscard]] QString default_mobile_session_configuration_path();
[[nodiscard]] mobile_page normalized_mobile_page(int page_index);

[[nodiscard]] bool save_mobile_navigation(
    QSettings& settings, mobile_page page, QString* error_message = nullptr
);

[[nodiscard]] bool save_mobile_session(
    QSettings& settings, const QString& configuration_path, mobile_page page,
    const QByteArray& line_configuration, QString* error_message = nullptr
);

[[nodiscard]] bool clear_mobile_configuration(
    QSettings& settings, const QString& configuration_path, mobile_page page,
    QString* error_message = nullptr
);

[[nodiscard]] mobile_session_state
load_mobile_session(QSettings& settings, const QString& configuration_path);

} // namespace yodau::shell

#endif // YODAU_APP_SHELL_MOBILE_SESSION_STORE_HPP
