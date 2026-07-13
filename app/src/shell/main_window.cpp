#include "shell/main_window.hpp"

#include "core/namespace_alias.hpp"
#include "monitor/runtime_bridge.hpp"
#include "shell/str_label.hpp"
#include "shell/stream_controller.hpp"
#include "streams/stream_manager.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QFileDialog>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStandardPaths>

main_window::main_window(
    bool enable_debug_monitor, const QString& debug_monitor_endpoint_name,
    QWidget* parent
)
    : base_main_window(parent)
    , main_zone(new stream_board(this))
    , settings(new settings_panel(this))
    , app_stream_controller(nullptr)
    , import_line_configuration_action(nullptr)
    , export_line_configuration_action(nullptr)
    , debug_monitor(nullptr)
    , toggle_debug_monitor_action(nullptr)
    , debug_monitor_status_label(nullptr) {
    setup_platform_layout();
    core_manager = std::make_unique<yodau::core::stream_manager>();
    auto* mgr = core_manager.get();

    debug_monitor = new yodau::monitor::runtime_bridge(
        yodau::monitor::runtime_bridge::runtime_options {
            .enabled = enable_debug_monitor,
            .requested_endpoint = debug_monitor_endpoint_name,
        },
        this
    );

    auto* ctrl
        = new stream_controller(mgr, settings, main_zone, debug_monitor, this);
    app_stream_controller = ctrl;
    setup_configuration_actions();

    connect(
        settings, &settings_panel::add_file_stream, ctrl,
        &stream_controller::handle_add_file
    );
    connect(
        settings, &settings_panel::add_local_stream, ctrl,
        &stream_controller::handle_add_local
    );
    connect(
        settings, &settings_panel::add_url_stream, ctrl,
        &stream_controller::handle_add_url
    );
    connect(
        settings, &settings_panel::detect_local_sources_requested, ctrl,
        &stream_controller::handle_detect_local_sources
    );
    connect(
        settings, &settings_panel::show_stream_changed, ctrl,
        &stream_controller::handle_show_stream_changed
    );
    ctrl->handle_detect_local_sources();
    setup_debug_monitor_ui();
}

main_window::~main_window() {
    delete app_stream_controller;
    app_stream_controller = nullptr;
    core_manager.reset();
}

void main_window::setup_configuration_actions() {
    auto* file_menu = menuBar()->addMenu(tr("&File"));

    import_line_configuration_action
        = file_menu->addAction(tr("&Import line configuration..."));
    import_line_configuration_action->setObjectName(
        QStringLiteral("main_import_line_configuration_action")
    );
    import_line_configuration_action->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I)
    );
    import_line_configuration_action->setToolTip(tr(
        "Load line geometry and processing settings from a yodau configuration"
    ));
    connect(
        import_line_configuration_action, &QAction::triggered, this,
        &main_window::on_import_line_configuration_triggered
    );

    export_line_configuration_action
        = file_menu->addAction(tr("&Export line configuration..."));
    export_line_configuration_action->setObjectName(
        QStringLiteral("main_export_line_configuration_action")
    );
    export_line_configuration_action->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)
    );
    export_line_configuration_action->setToolTip(
        tr("Save the selected stream's lines and processing settings")
    );
    connect(
        export_line_configuration_action, &QAction::triggered, this,
        &main_window::on_export_line_configuration_triggered
    );
}

void main_window::on_import_line_configuration_triggered() {
    if (app_stream_controller == nullptr) {
        return;
    }
    const QString path = QFileDialog::getOpenFileName(
        this, tr("Import line configuration"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
        tr("Yodau line configurations (*.yodau.json *.json);;All files (*)")
    );
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!app_stream_controller->import_line_configuration_from(path, &error)) {
        QMessageBox::critical(
            this, tr("Import failed"),
            error.isEmpty() ? tr("The configuration could not be imported.")
                            : error
        );
    }
}

void main_window::on_export_line_configuration_triggered() {
    if (app_stream_controller == nullptr) {
        return;
    }
    QString suggested_name
        = app_stream_controller->active_configuration_stream_name();
    if (suggested_name.trimmed().isEmpty()) {
        suggested_name = QStringLiteral("lines");
    }
    const QString initial_path
        = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
        + QLatin1Char('/') + suggested_name + QStringLiteral(".yodau.json");
    const QString path = QFileDialog::getSaveFileName(
        this, tr("Export line configuration"), initial_path,
        tr("Yodau line configurations (*.yodau.json *.json);;All files (*)")
    );
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!app_stream_controller->export_line_configuration_to(path, &error)) {
        QMessageBox::critical(
            this, tr("Export failed"),
            error.isEmpty() ? tr("The configuration could not be exported.")
                            : error
        );
    }
}
