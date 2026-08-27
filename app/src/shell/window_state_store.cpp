#include "shell/window_state_store.hpp"

#include <QByteArray>
#include <QMainWindow>
#include <QSettings>

namespace {

constexpr auto schema_key = "desktop/main_window/schema_version";
constexpr auto geometry_key = "desktop/main_window/geometry";
constexpr auto layout_key = "desktop/main_window/layout";
constexpr int qt_main_window_state_version = 1;

QString settings_error_text(const QSettings::Status status) {
    switch (status) {
    case QSettings::AccessError:
        return QStringLiteral("The desktop settings file is not writable.");
    case QSettings::FormatError:
        return QStringLiteral("The desktop settings file is malformed.");
    case QSettings::NoError:
        break;
    }
    return {};
}

void set_error(QString* error_message, const QString& value) {
    if (error_message != nullptr) {
        *error_message = value;
    }
}

} // namespace

namespace yodau::shell {

bool save_main_window_state(
    const QMainWindow& window, QSettings& settings, QString* error_message
) {
    set_error(error_message, {});
    const int existing_schema_version = settings.value(schema_key, 0).toInt();
    if (existing_schema_version > main_window_state_schema_version) {
        set_error(
            error_message,
            QStringLiteral(
                "A newer desktop state schema is present and was not "
                "overwritten: %1"
            )
                .arg(existing_schema_version)
        );
        return false;
    }

    settings.setValue(schema_key, main_window_state_schema_version);
    settings.setValue(geometry_key, window.saveGeometry());
    settings.setValue(
        layout_key, window.saveState(qt_main_window_state_version)
    );
    settings.sync();

    const QString error = settings_error_text(settings.status());
    set_error(error_message, error);
    return error.isEmpty();
}

bool restore_main_window_state(
    QMainWindow& window, QSettings& settings, QString* error_message
) {
    set_error(error_message, {});
    if (settings.status() != QSettings::NoError) {
        set_error(error_message, settings_error_text(settings.status()));
        return false;
    }

    const int schema_version = settings.value(schema_key, 0).toInt();
    if (schema_version == 0) {
        return false;
    }
    if (schema_version != main_window_state_schema_version) {
        set_error(
            error_message,
            QStringLiteral("Unsupported desktop state schema version: %1")
                .arg(schema_version)
        );
        return false;
    }

    const QByteArray geometry = settings.value(geometry_key).toByteArray();
    const QByteArray layout = settings.value(layout_key).toByteArray();
    if (geometry.isEmpty() || layout.isEmpty()) {
        set_error(
            error_message,
            QStringLiteral("The saved desktop window state is incomplete.")
        );
        return false;
    }

    const bool geometry_restored = window.restoreGeometry(geometry);
    const bool layout_restored
        = window.restoreState(layout, qt_main_window_state_version);
    if (!geometry_restored || !layout_restored) {
        set_error(
            error_message,
            QStringLiteral("The saved desktop window state is invalid.")
        );
        return false;
    }
    return true;
}

bool save_main_window_state(const QMainWindow& window, QString* error_message) {
    QSettings settings;
    return save_main_window_state(window, settings, error_message);
}

bool restore_main_window_state(QMainWindow& window, QString* error_message) {
    QSettings settings;
    return restore_main_window_state(window, settings, error_message);
}

} // namespace yodau::shell
