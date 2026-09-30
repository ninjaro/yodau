#include "shell/window_state_store.hpp"

#include <QByteArray>
#include <QDockWidget>
#include <QMainWindow>
#include <QSettings>

namespace {

constexpr auto geometry_key = "desktop/main_window/geometry";
constexpr auto layout_key = "desktop/main_window/layout";
// QMainWindow requires the same application-defined tag when restoring its
// opaque saveState payload. This is format identity for the active Qt API,
// not compatibility negotiation.
constexpr int qt_state_format_tag = 1;

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

bool desktop_presentation::visible(desktop_panel panel) const {
    const auto index = static_cast<std::size_t>(panel);
    if (index >= panel_overrides.size()) {
        return true;
    }
    return panel_overrides[index].value_or(
        preset != desktop_preset::canvas || panel == desktop_panel::streams
    );
}

void desktop_presentation::reset_overrides() { panel_overrides = {}; }

namespace {
    constexpr std::array panel_keys { "streams", "editor", "logs" };
}

std::optional<desktop_presentation>
load_desktop_presentation(QSettings& settings) {
    settings.beginGroup(QStringLiteral("desktop/presentation"));
    if (!settings.contains(QStringLiteral("preset"))) {
        settings.endGroup();
        return std::nullopt;
    }
    desktop_presentation value;
    if (settings.value(QStringLiteral("preset")).toString()
        == QStringLiteral("canvas")) {
        value.preset = desktop_preset::canvas;
    }
    for (std::size_t index = 0; index < panel_keys.size(); ++index) {
        const QString key = QStringLiteral("panels/%1")
                                .arg(QString::fromLatin1(panel_keys[index]));
        const QString choice = settings.value(key).toString();
        if (choice == QStringLiteral("shown")) {
            value.panel_overrides[index] = true;
        } else if (choice == QStringLiteral("hidden")) {
            value.panel_overrides[index] = false;
        }
    }
    settings.endGroup();
    return value;
}

bool save_desktop_presentation(
    QSettings& settings, const desktop_presentation& value
) {
    settings.beginGroup(QStringLiteral("desktop/presentation"));
    settings.setValue(
        QStringLiteral("preset"),
        value.preset == desktop_preset::canvas ? QStringLiteral("canvas")
                                               : QStringLiteral("classic")
    );
    for (std::size_t index = 0; index < panel_keys.size(); ++index) {
        const QString key = QStringLiteral("panels/%1")
                                .arg(QString::fromLatin1(panel_keys[index]));
        if (value.panel_overrides[index]) {
            settings.setValue(
                key,
                *value.panel_overrides[index] ? QStringLiteral("shown")
                                              : QStringLiteral("hidden")
            );
        } else {
            settings.remove(key);
        }
    }
    settings.endGroup();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

void apply_desktop_presentation(
    const desktop_presentation& value, const std::array<QDockWidget*, 3>& docks
) {
    for (std::size_t index = 0; index < docks.size(); ++index) {
        if (docks[index] != nullptr) {
            docks[index]->setVisible(
                value.visible(static_cast<desktop_panel>(index))
            );
        }
    }
}

void capture_desktop_panel_overrides(
    desktop_presentation& value, const std::array<QDockWidget*, 3>& docks
) {
    auto defaults = value;
    defaults.reset_overrides();
    for (std::size_t index = 0; index < docks.size(); ++index) {
        if (docks[index] == nullptr) {
            continue;
        }
        // isVisible() also becomes false when the parent window is hidden.
        const bool shown = !docks[index]->isHidden();
        value.panel_overrides[index]
            = shown == defaults.visible(static_cast<desktop_panel>(index))
            ? std::nullopt
            : std::optional<bool>(shown);
    }
}

bool save_main_window_state(
    const QMainWindow& window, QSettings& settings, QString* error_message
) {
    set_error(error_message, {});
    settings.setValue(geometry_key, window.saveGeometry());
    settings.setValue(layout_key, window.saveState(qt_state_format_tag));
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
        = window.restoreState(layout, qt_state_format_tag);
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
