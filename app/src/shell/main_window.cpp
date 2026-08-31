#include "shell/main_window.hpp"

#include "shell/mobile_session_store.hpp"
#include "shell/str_label.hpp"
#include "shell/stream_controller.hpp"
#include "shell/window_state_store.hpp"
#include "streams/stream_manager.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QApplication>
#include <QFileDialog>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QSettings>
#include <QStackedWidget>
#include <QStandardPaths>

main_window::main_window(QWidget* parent)
    : base_main_window(parent)
    , main_zone(new stream_board(this))
    , settings(new settings_panel(this))
    , app_stream_controller(nullptr)
    , import_line_configuration_action(nullptr)
    , export_line_configuration_action(nullptr)
{
    setup_platform_layout();
    core_manager = std::make_unique<yodau::core::stream_manager>();
    auto* mgr = core_manager.get();

    auto* ctrl = new stream_controller(mgr, settings, main_zone, this);
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
    setup_desktop_shell();
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    connect(
        qApp, &QGuiApplication::applicationStateChanged, this,
        &main_window::on_application_state_changed
    );
    restore_mobile_session();
#else
    (void)yodau::shell::restore_main_window_state(*this);
#endif
}

main_window::~main_window() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    persist_mobile_session();
    if (main_zone != nullptr) {
        main_zone->set_application_active(false);
    }
#else
    (void)yodau::shell::save_main_window_state(*this);
#endif
    delete app_stream_controller;
    app_stream_controller = nullptr;
    core_manager.reset();
}

void main_window::setup_configuration_actions() {
    import_line_configuration_action
        = new QAction(tr("&Import line configuration..."), this);
    import_line_configuration_action->setObjectName(
        QStringLiteral("main_import_line_configuration_action")
    );
#if !defined(KC_KDE)
    import_line_configuration_action->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I)
    );
#endif
    import_line_configuration_action->setToolTip(tr(
        "Load line geometry and processing settings from a yodau configuration"
    ));
    connect(
        import_line_configuration_action, &QAction::triggered, this,
        &main_window::on_import_line_configuration_triggered
    );

    export_line_configuration_action
        = new QAction(tr("&Export line configuration..."), this);
    export_line_configuration_action->setObjectName(
        QStringLiteral("main_export_line_configuration_action")
    );
#if !defined(KC_KDE)
    export_line_configuration_action->setShortcut(
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)
    );
#endif
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
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    const QPointer<main_window> guarded_this(this);
    QFileDialog::getOpenFileContent(
        tr("Yodau line configurations (*.yodau.json *.json);;All files (*)"),
        [guarded_this](const QString&, const QByteArray& contents) {
            if (guarded_this == nullptr || contents.isEmpty()) {
                return;
            }
            guarded_this->import_mobile_configuration(contents, true);
        },
        this
    );
    return;
#else
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
#endif
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
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    suggested_name.replace('/', '_');
    suggested_name.replace('\\', '_');
    QByteArray contents;
    QString error;
    if (!app_stream_controller->export_line_configuration_data(
            &contents, &error
        )) {
        QMessageBox::critical(
            this, tr("Export failed"),
            error.isEmpty() ? tr("The configuration could not be exported.")
                            : error
        );
        return;
    }
    QFileDialog::saveFileContent(
        contents, suggested_name + QStringLiteral(".yodau.json"), this
    );
    return;
#else
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
#endif
}

void main_window::import_mobile_configuration(
    const QByteArray& contents, const bool activate_stream
) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    if (app_stream_controller == nullptr) {
        return;
    }
    QString error;
    QString imported_stream;
    if (!app_stream_controller->import_line_configuration_data(
            contents, &error, &imported_stream
        )) {
        if (mobile_status_label != nullptr) {
            mobile_status_label->setText(
                error.isEmpty()
                    ? tr("The saved configuration could not be restored.")
                    : error
            );
            mobile_status_label->show();
        }
        if (activate_stream) {
            QMessageBox::critical(
                this, tr("Import failed"),
                error.isEmpty() ? tr("The configuration could not be imported.")
                                : error
            );
        }
        return;
    }

    if (activate_stream) {
        app_stream_controller->focus_stream(imported_stream);
        show_mobile_page(static_cast<int>(yodau::shell::mobile_page::monitor));
        if (mobile_status_label != nullptr) {
            mobile_status_label->hide();
        }
    } else if (mobile_status_label != nullptr) {
        mobile_status_label->setText(
            tr("The saved operator configuration was restored paused. Enable "
               "the stream from Streams when you are ready.")
        );
        mobile_status_label->show();
    }
#else
    Q_UNUSED(contents);
    Q_UNUSED(activate_stream);
#endif
}

void main_window::restore_mobile_session() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSettings mobile_settings(
        QStringLiteral("ninjaro"), QStringLiteral("yodau")
    );
    const yodau::shell::mobile_session_state state
        = yodau::shell::load_mobile_session(
            mobile_settings,
            yodau::shell::default_mobile_session_configuration_path()
        );
    show_mobile_page(static_cast<int>(state.page));
    if (!state.warning.isEmpty() && mobile_status_label != nullptr) {
        mobile_status_label->setText(state.warning);
        mobile_status_label->show();
    }
    if (!state.line_configuration.isEmpty()) {
        import_mobile_configuration(state.line_configuration, false);
    }
#endif
}

void main_window::persist_mobile_session() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSettings mobile_settings(
        QStringLiteral("ninjaro"), QStringLiteral("yodau")
    );
    const auto page = yodau::shell::normalized_mobile_page(
        zones_stack != nullptr ? zones_stack->currentIndex() : 0
    );
    QByteArray contents;
    QString error;
    if (app_stream_controller == nullptr
        || app_stream_controller->active_configuration_stream_name()
               .isEmpty()) {
        static_cast<void>(yodau::shell::clear_mobile_configuration(
            mobile_settings,
            yodau::shell::default_mobile_session_configuration_path(), page
        ));
        return;
    }
    if (app_stream_controller != nullptr
        && app_stream_controller->export_line_configuration_data(
            &contents, &error
        )) {
        static_cast<void>(yodau::shell::save_mobile_session(
            mobile_settings,
            yodau::shell::default_mobile_session_configuration_path(), page,
            contents
        ));
        return;
    }
    static_cast<void>(
        yodau::shell::save_mobile_navigation(mobile_settings, page)
    );
#endif
}

void main_window::on_application_state_changed(
    const Qt::ApplicationState state
) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    const bool application_active = state == Qt::ApplicationActive;
    if (main_zone != nullptr) {
        main_zone->set_application_active(application_active);
    }
    if (!application_active) {
        persist_mobile_session();
    }
#else
    Q_UNUSED(state);
#endif
}

void main_window::keyPressEvent(QKeyEvent* event) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    if (event != nullptr
        && (event->key() == Qt::Key_Back || event->key() == Qt::Key_Escape)) {
        const int monitor_page
            = static_cast<int>(yodau::shell::mobile_page::monitor);
        if (zones_stack != nullptr
            && zones_stack->currentIndex() != monitor_page) {
            show_mobile_page(monitor_page);
            event->accept();
            return;
        }
        if (main_zone != nullptr && main_zone->active_cell() != nullptr
            && app_stream_controller != nullptr) {
            app_stream_controller->return_to_stream_grid();
            event->accept();
            return;
        }
    }
#endif
    base_main_window::keyPressEvent(event);
}
