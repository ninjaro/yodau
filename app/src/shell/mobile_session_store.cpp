#include "shell/mobile_session_store.hpp"

#include "configuration/line_configuration_json.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>

#include <exception>
#include <string_view>

namespace {

constexpr auto page_key = "mobile/session/page";
constexpr auto has_configuration_key = "mobile/session/has_configuration";

void set_error(QString* error_message, const QString& value) {
    if (error_message != nullptr) {
        *error_message = value;
    }
}

QString settings_error_text(const QSettings::Status status) {
    switch (status) {
    case QSettings::AccessError:
        return QStringLiteral("The mobile settings file is not writable.");
    case QSettings::FormatError:
        return QStringLiteral("The mobile settings file is malformed.");
    case QSettings::NoError:
        break;
    }
    return {};
}

QString configuration_error_text(const std::exception& error) {
    return QString::fromUtf8(error.what());
}

bool configuration_is_valid(
    const QByteArray& configuration, QString* error_message
) {
    if (configuration.isEmpty()) {
        set_error(error_message, QStringLiteral("The configuration is empty."));
        return false;
    }
    if (configuration.size()
        > yodau::shell::maximum_mobile_configuration_bytes) {
        set_error(
            error_message,
            QStringLiteral("The configuration exceeds the 4 MiB safety limit.")
        );
        return false;
    }

    try {
        static_cast<void>(yodau::core::decode_line_configuration_json(
            std::string_view(
                configuration.constData(),
                static_cast<size_t>(configuration.size())
            )
        ));
        return true;
    } catch (const std::exception& error) {
        set_error(error_message, configuration_error_text(error));
        return false;
    }
}

} // namespace

namespace yodau::shell {

QString default_mobile_session_configuration_path() {
    const QString data_directory
        = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    return QDir(data_directory)
        .filePath(QStringLiteral("mobile-session.yodau.json"));
}

mobile_page normalized_mobile_page(const int page_index) {
    if (page_index < static_cast<int>(mobile_page::monitor)
        || page_index > static_cast<int>(mobile_page::logs)) {
        return mobile_page::monitor;
    }
    return static_cast<mobile_page>(page_index);
}

bool save_mobile_navigation(
    QSettings& settings, const mobile_page page, QString* error_message
) {
    set_error(error_message, {});
    settings.setValue(page_key, static_cast<int>(page));
    settings.sync();
    const QString error = settings_error_text(settings.status());
    set_error(error_message, error);
    return error.isEmpty();
}

bool save_mobile_session(
    QSettings& settings, const QString& configuration_path,
    const mobile_page page, const QByteArray& line_configuration,
    QString* error_message
) {
    set_error(error_message, {});
    QString validation_error;
    if (!configuration_is_valid(line_configuration, &validation_error)) {
        set_error(error_message, validation_error);
        return false;
    }

    const QFileInfo destination(configuration_path);
    if (!QDir().mkpath(destination.absolutePath())) {
        set_error(
            error_message,
            QStringLiteral("The mobile session directory could not be created.")
        );
        return false;
    }

    QSaveFile file(configuration_path);
    if (!file.open(QIODevice::WriteOnly)
        || file.write(line_configuration) != line_configuration.size()
        || !file.commit()) {
        set_error(
            error_message,
            file.errorString().isEmpty()
                ? QStringLiteral("The mobile session could not be saved.")
                : file.errorString()
        );
        return false;
    }

    if (!save_mobile_navigation(settings, page, error_message)) {
        return false;
    }
    settings.setValue(has_configuration_key, true);
    settings.sync();
    const QString settings_error = settings_error_text(settings.status());
    set_error(error_message, settings_error);
    return settings_error.isEmpty();
}

bool clear_mobile_configuration(
    QSettings& settings, const QString& configuration_path,
    const mobile_page page, QString* error_message
) {
    set_error(error_message, {});
    const QFileInfo configuration_file(configuration_path);
    if (configuration_file.exists() && !QFile::remove(configuration_path)) {
        set_error(
            error_message,
            QStringLiteral(
                "The saved mobile configuration could not be removed."
            )
        );
        return false;
    }
    if (!save_mobile_navigation(settings, page, error_message)) {
        return false;
    }
    settings.setValue(has_configuration_key, false);
    settings.sync();
    const QString error = settings_error_text(settings.status());
    set_error(error_message, error);
    return error.isEmpty();
}

mobile_session_state
load_mobile_session(QSettings& settings, const QString& configuration_path) {
    mobile_session_state state;
    if (settings.status() != QSettings::NoError) {
        state.warning = settings_error_text(settings.status());
        return state;
    }

    state.page = normalized_mobile_page(settings.value(page_key, 0).toInt());
    if (!settings.value(has_configuration_key, false).toBool()) {
        return state;
    }

    QFile file(configuration_path);
    if (!file.open(QIODevice::ReadOnly)) {
        state.warning = file.errorString();
        return state;
    }
    if (file.size() <= 0 || file.size() > maximum_mobile_configuration_bytes) {
        state.warning = file.size() > maximum_mobile_configuration_bytes
            ? QStringLiteral(
                  "The saved mobile configuration exceeds the 4 MiB safety "
                  "limit."
              )
            : QStringLiteral("The saved mobile configuration is empty.");
        return state;
    }

    const QByteArray configuration = file.readAll();
    QString validation_error;
    if (!configuration_is_valid(configuration, &validation_error)) {
        state.warning = validation_error;
        return state;
    }
    state.line_configuration = configuration;
    return state;
}

} // namespace yodau::shell
