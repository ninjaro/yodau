#include "shell/app_settings.hpp"
#include "shell/main_window.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_board.hpp"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScopeGuard>
#include <QScrollArea>
#include <QSettings>
#include <QStackedWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>
#include <QtTest/QtTest>
#include <shared_ui/qt/action_group.hpp>
#include <shared_ui/qt/appearance_editor.hpp>
#include <shared_ui/qt/presentation.hpp>
#include <shared_ui/qt/readout_group.hpp>
#include <shared_ui/qt/settings_section.hpp>

#ifdef KC_KDE
#include <KAboutData>
#include <KActionCollection>
#endif

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::pool_menu_restores_choice() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Pool presentation actions are desktop-only");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto old_organization = QCoreApplication::organizationName();
    const auto old_application = QCoreApplication::applicationName();
    const auto old_format = QSettings::defaultFormat();
    const auto old_path
        = QFileInfo(QSettings(
                        QSettings::IniFormat, QSettings::UserScope,
                        QStringLiteral("yodau_path_probe")
                    )
                        .fileName())
              .path();
    const auto restore_identity = qScopeGuard([&] {
        QCoreApplication::setOrganizationName(old_organization);
        QCoreApplication::setApplicationName(old_application);
        QSettings::setDefaultFormat(old_format);
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, old_path
        );
    });
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, directory.path()
    );
    QCoreApplication::setOrganizationName(QStringLiteral("yodau"));
    QCoreApplication::setApplicationName(QStringLiteral("pool-tests"));
    {
        main_window window;
        auto* board = window.findChild<stream_board*>();
        QVERIFY(board != nullptr);
        auto* right
            = window.findChild<QAction*>(QStringLiteral("desktop_pool_right"));
        auto* left
            = window.findChild<QAction*>(QStringLiteral("desktop_pool_left"));
        auto* bottom
            = window.findChild<QAction*>(QStringLiteral("desktop_pool_bottom"));
        QVERIFY(right && left && bottom);
        auto* presentation = window.findChild<QMenu*>(
            QStringLiteral("desktop_presentation_menu")
        );
        auto* pool_menu
            = window.findChild<QMenu*>(QStringLiteral("desktop_pool_menu"));
        QVERIFY(presentation && pool_menu);
        QVERIFY(presentation->actions().contains(pool_menu->menuAction()));
        QVERIFY(pool_menu->actions().contains(right));
        bool reachable = false;
        for (auto* action : window.menuBar()->actions()) {
            if (action->menu()
                && action->menu()->actions().contains(
                    presentation->menuAction()
                )) {
                reachable = true;
            }
        }
        QVERIFY(reachable);
#ifdef KC_KDE
        QCOMPARE(
            window.actionCollection()->action(
                QStringLiteral("view_presentation")
            ),
            presentation->menuAction()
        );
#endif
        QVERIFY(bottom->isChecked());
        auto* lines = window.findChild<QDockWidget*>(
            QStringLiteral("main_window_line_dock")
        );
        QVERIFY(lines != nullptr);
        lines->hide();
        right->trigger();
        QCOMPARE(board->current_pool_side(), pool_side::right);
        QVERIFY(
            right->isChecked() && !left->isChecked() && !bottom->isChecked()
        );
        QVERIFY(lines->isHidden());
        QSettings saved;
        saved.setFallbacksEnabled(false);
        QVERIFY(!yodau::shell::load_desktop_presentation(saved));
        auto* canvas = window.findChild<QAction*>(
            QStringLiteral("desktop_preset_canvas")
        );
        auto* reset = window.findChild<QAction*>(
            QStringLiteral("desktop_preset_reset")
        );
        QVERIFY(canvas && reset);
        canvas->trigger();
        reset->trigger();
        QCOMPARE(board->current_pool_side(), pool_side::right);
        left->trigger();
        QCOMPARE(board->current_pool_side(), pool_side::left);
        QVERIFY(left->isChecked() && !right->isChecked());
    }
    {
        main_window restored;
        auto* board = restored.findChild<stream_board*>();
        QVERIFY(board != nullptr);
        QCOMPARE(board->current_pool_side(), pool_side::left);
        QVERIFY(restored
                    .findChild<QAction*>(QStringLiteral("desktop_pool_left"))
                    ->isChecked());
        QFile blocker(directory.filePath(QStringLiteral("blocked")));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, blocker.fileName()
        );
        int warnings = 0;
        QTimer::singleShot(0, &restored, [&warnings] {
            for (auto* widget : QApplication::topLevelWidgets()) {
                if (auto* dialog = qobject_cast<QMessageBox*>(widget)) {
                    ++warnings;
                    dialog->accept();
                }
            }
        });
        auto* right = restored.findChild<QAction*>(
            QStringLiteral("desktop_pool_right")
        );
        right->trigger();
        QCOMPARE(warnings, 1);
        QCOMPARE(board->current_pool_side(), pool_side::left);
        QVERIFY(!right->isChecked());
        QVERIFY(restored
                    .findChild<QAction*>(QStringLiteral("desktop_pool_left"))
                    ->isChecked());
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, directory.path()
        );
    }
#endif
}

void main_window_tests::window_style_restores_choice() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Window style actions are desktop-only");
#else
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto old_organization = QCoreApplication::organizationName();
    const auto old_application = QCoreApplication::applicationName();
    const auto old_format = QSettings::defaultFormat();
    const auto old_path
        = QFileInfo(QSettings(
                        QSettings::IniFormat, QSettings::UserScope,
                        QStringLiteral("yodau_path_probe")
                    )
                        .fileName())
              .path();
    const auto restore_identity = qScopeGuard([&] {
        QCoreApplication::setOrganizationName(old_organization);
        QCoreApplication::setApplicationName(old_application);
        QSettings::setDefaultFormat(old_format);
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, old_path
        );
    });
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, directory.path()
    );
    QCoreApplication::setOrganizationName(QStringLiteral("yodau"));
#ifdef KC_KDE
    // KMainWindow uses KDE's cached identity; isolate the path, not its name.
    QCoreApplication::setApplicationName(
        KAboutData::applicationData().componentName()
    );
#else
    QCoreApplication::setApplicationName(QStringLiteral("chrome-tests"));
#endif
    using namespace shared_ui::qt;
    const auto key = QStringLiteral("desktop/presentation/chrome");
    {
        QSettings unknown;
        unknown.setValue(key, QStringLiteral("future"));
        QCOMPARE(
            yodau::shell::load_desktop_chrome(unknown),
            yodau::shell::desktop_chrome::native
        );
        unknown.setValue(
            QStringLiteral("processing/sentinel"), QStringLiteral("keep")
        );
    }
    {
        main_window window;
        window.resize(1024, 768);
        window.show();
        auto* appearance
            = window.findChild<QAction*>(QStringLiteral("desktop_appearance"));
        auto* presentation = window.findChild<QMenu*>(
            QStringLiteral("desktop_presentation_menu")
        );
        auto* model = window.findChild<appearance_model*>();
        QVERIFY(appearance && presentation && model);
        QVERIFY(presentation->actions().contains(appearance));
        QVERIFY(
            !window.findChild<QAction*>(QStringLiteral("desktop_chrome_slate"))
        );
        QCOMPARE(model->families().size(), 1);
        QCOMPARE(model->families().first().role, family_role::material);
        QCOMPARE(model->presentation_material(), material::system);
        QVERIFY(window.styleSheet().isEmpty());
        auto* lines = window.findChild<QDockWidget*>(
            QStringLiteral("main_window_line_dock")
        );
        auto* board = window.findChild<stream_board*>();
        QVERIFY(lines && board);
        lines->hide();
        const auto side = board->current_pool_side();

        appearance->trigger();
        auto* editor = window.findChild<appearance_editor*>();
        QPointer<QDialog> dialog = window.findChild<QDialog*>(
            QStringLiteral("desktop_appearance_dialog")
        );
        QVERIFY(editor && dialog);
        appearance->trigger();
        QCOMPARE(window.findChildren<appearance_editor*>().size(), 1);
        QCOMPARE(editor->option_buttons().size(), 3);
        editor->option_buttons().at(2)->click(); // Slate, via the real editor.
        QCOMPARE(
            model->effective(QStringLiteral("chrome")), QStringLiteral("slate")
        );
        QVERIFY(model->dirty());
        // Draft and preview are isolated from the accepted window and store.
        QVERIFY(window.styleSheet().isEmpty());
        QCOMPARE(QSettings().value(key).toString(), QStringLiteral("future"));
        auto* preview = editor->findChild<settings_section*>();
        QVERIFY(preview);
        QCOMPARE(preview->presentation_material(), material::slate);
        QVERIFY(!editor->findChildren<action_group*>().isEmpty());
        QVERIFY(!editor->findChildren<readout_group*>().isEmpty());
        editor->command(editor_command::cancel)->click();
        QTRY_VERIFY(!dialog);
        QVERIFY(!model->dirty());
        QCOMPARE(model->presentation_material(), material::system);
        QCOMPARE(QSettings().value(key).toString(), QStringLiteral("future"));

        appearance->trigger();
        editor = window.findChild<appearance_editor*>();
        dialog = window.findChild<QDialog*>(
            QStringLiteral("desktop_appearance_dialog")
        );
        QVERIFY(editor && dialog);
        editor->option_buttons().at(2)->click();
        editor->command(editor_command::apply)->click();
        QVERIFY(!editor->save_pending() && !model->dirty());
        QCOMPARE(QSettings().value(key).toString(), QStringLiteral("slate"));
        QVERIFY(window.styleSheet().contains(QStringLiteral("#3daee9")));
        QVERIFY(lines->isHidden());
        QCOMPARE(board->current_pool_side(), side);
        QSettings saved;
        QCOMPARE(
            saved.value(QStringLiteral("processing/sentinel")).toString(),
            QStringLiteral("keep")
        );
        QVERIFY(!yodau::shell::load_desktop_presentation(saved));

        // Escape and title-bar close also discard uncommitted changes.
        editor->option_buttons().at(1)->click();
        QTest::keyClick(dialog, Qt::Key_Escape);
        QTRY_VERIFY(!dialog);
        QCOMPARE(model->presentation_material(), material::slate);
        QCOMPARE(QSettings().value(key).toString(), QStringLiteral("slate"));
        appearance->trigger();
        editor = window.findChild<appearance_editor*>();
        dialog = window.findChild<QDialog*>(
            QStringLiteral("desktop_appearance_dialog")
        );
        QVERIFY(editor && dialog);
        editor->option_buttons().at(1)->click();
        // Native preview inside an accepted Slate window is genuinely native.
        preview = editor->findChild<settings_section*>();
        QCOMPARE(preview->presentation_material(), material::system);
        QCOMPARE(
            preview->palette().color(QPalette::Window),
            QApplication::palette().color(QPalette::Window)
        );
        dialog->close();
        QTRY_VERIFY(!dialog);
        QVERIFY(!model->dirty());
        QCOMPARE(model->presentation_material(), material::slate);
    }
    {
        main_window restored;
        auto* model = restored.findChild<appearance_model*>();
        QVERIFY(model);
        QCOMPARE(model->presentation_material(), material::slate);
        QVERIFY(!restored.styleSheet().isEmpty());
        restored.findChild<QAction*>(QStringLiteral("desktop_appearance"))
            ->trigger();
        auto* editor = restored.findChild<appearance_editor*>();
        QPointer<QDialog> dialog = restored.findChild<QDialog*>(
            QStringLiteral("desktop_appearance_dialog")
        );
        QVERIFY(editor && dialog);
        // The shared Defaults command restores Native, not Canvas or a
        // workspace.
        editor->command(editor_command::defaults)->click();
        editor->command(editor_command::ok)->click();
        QTRY_VERIFY(!dialog);
        QCOMPARE(model->presentation_material(), material::system);
        QVERIFY(restored.styleSheet().isEmpty());
        QCOMPARE(QSettings().value(key).toString(), QStringLiteral("native"));
    }
    {
        main_window window;
        auto* model = window.findChild<appearance_model*>();
        window.findChild<QAction*>(QStringLiteral("desktop_appearance"))
            ->trigger();
        auto* editor = window.findChild<appearance_editor*>();
        QPointer<QDialog> dialog = window.findChild<QDialog*>(
            QStringLiteral("desktop_appearance_dialog")
        );
        QVERIFY(model && editor && dialog);
        editor->option_buttons().at(2)->click();
        // A regular file in place of the settings directory forces write
        // failure, independent of UID/permissions. No production test hook is
        // required.
        const auto blocked = directory.filePath(QStringLiteral("blocked"));
        QFile blocker(blocked);
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, blocked);
        editor->command(editor_command::apply)->click();
        QVERIFY(model->dirty() && !editor->save_pending());
        QVERIFY(!editor->notices().isEmpty());
        QVERIFY(window.styleSheet().isEmpty());
        QCOMPARE(
            model->saved().overrides.value(QStringLiteral("chrome")),
            QStringLiteral("system")
        );
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, directory.path()
        );
        QCOMPARE(QSettings().value(key).toString(), QStringLiteral("native"));
        // Retry persists the original draft only after the host can write
        // again.
        editor->command(editor_command::apply)->click();
        QVERIFY(!model->dirty());
        QCOMPARE(QSettings().value(key).toString(), QStringLiteral("slate"));
        editor->option_buttons().at(1)->click();
        // Main-window teardown safely destroys a still-open, dirty editor.
    }
#endif
}

void main_window_tests::desktop_preserves_layout_overrides() {
    using namespace yodau::shell;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const auto path = directory.filePath(QStringLiteral("ui.ini"));
    QSettings settings(path, QSettings::IniFormat);
    settings.setValue(
        QStringLiteral("layout/stream_grid/sizing_mode"),
        QStringLiteral("fixed")
    );
    settings.setValue(
        QStringLiteral("mobile/session"), QByteArray("unchanged")
    );
    QMainWindow window;
    auto* streams = new QDockWidget(QStringLiteral("Streams"), &window);
    auto* editor = new QDockWidget(QStringLiteral("Editor"), &window);
    auto* logs = new QDockWidget(QStringLiteral("Logs"), &window);
    const std::array<QDockWidget*, 3> docks { streams, editor, logs };
    for (std::size_t index = 0; index < docks.size(); ++index) {
        docks[index]->setObjectName(QStringLiteral("dock_%1").arg(index));
        window.addDockWidget(Qt::LeftDockWidgetArea, docks[index]);
    }
    editor->hide();
    QVERIFY(save_main_window_state(window, settings));
    const auto old_layout
        = settings.value(QStringLiteral("desktop/main_window/layout"));
    QVERIFY(!load_desktop_presentation(settings));
    QVERIFY(editor->isHidden());

    desktop_presentation value;
    value.preset = desktop_preset::canvas;
    apply_desktop_presentation(value, docks);
    QVERIFY(!streams->isHidden());
    QVERIFY(editor->isHidden());
    QVERIFY(logs->isHidden());
    logs->show(); // Equivalent intent to its existing toggle action or close
                  // box.
    window.hide(); // Parent visibility must not become a panel override.
    capture_desktop_panel_overrides(value, docks);
    QVERIFY(!value.panel_overrides[0]);
    QVERIFY(!value.panel_overrides[1]);
    QCOMPARE(value.panel_overrides[2], std::optional<bool>(true));
    QVERIFY(save_desktop_presentation(settings, value));
    QSettings reloaded(path, QSettings::IniFormat);
    QCOMPARE(
        load_desktop_presentation(reloaded),
        std::optional<desktop_presentation>(value)
    );
    QCOMPARE(
        settings.value(QStringLiteral("desktop/main_window/layout")), old_layout
    );
    QCOMPARE(
        settings.value(QStringLiteral("layout/stream_grid/sizing_mode"))
            .toString(),
        QStringLiteral("fixed")
    );
    QCOMPARE(
        settings.value(QStringLiteral("mobile/session")).toByteArray(),
        QByteArray("unchanged")
    );
    value.reset_overrides();
    apply_desktop_presentation(value, docks);
    QVERIFY(logs->isHidden());
    QVERIFY(save_desktop_presentation(settings, value));
    QVERIFY(
        !settings.contains(QStringLiteral("desktop/presentation/panels/logs"))
    );
    value.preset = desktop_preset::classic;
    apply_desktop_presentation(value, docks);
    for (auto* dock : docks) {
        QVERIFY(!dock->isHidden());
    }
    streams->hide();
    capture_desktop_panel_overrides(value, docks);
    QCOMPARE(value.panel_overrides[0], std::optional<bool>(false));
    QVERIFY(save_desktop_presentation(settings, value));
    QCOMPARE(
        load_desktop_presentation(settings),
        std::optional<desktop_presentation>(value)
    );
    settings.setValue(
        QStringLiteral("desktop/presentation/preset"), QStringLiteral("future")
    );
    settings.setValue(
        QStringLiteral("desktop/presentation/panels/streams"),
        QStringLiteral("invalid")
    );
    QCOMPARE(
        load_desktop_presentation(settings),
        std::optional<desktop_presentation>(desktop_presentation {})
    );
    QSettings unwritable(directory.path(), QSettings::IniFormat);
    QVERIFY(!save_desktop_presentation(unwritable, value));
}

void main_window_tests::window_shell_and_shared_workflows() {
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    const QByteArray previous_config_home = qgetenv("XDG_CONFIG_HOME");
    const auto restore_config_home = qScopeGuard([previous_config_home]() {
        if (previous_config_home.isNull()) {
            qunsetenv("XDG_CONFIG_HOME");
        } else {
            qputenv("XDG_CONFIG_HOME", previous_config_home);
        }
    });
    QVERIFY(qputenv("XDG_CONFIG_HOME", temporary_directory.path().toUtf8()));

    {
        main_window window;
#if !defined(KC_KDE)
        QVERIFY(
            window.findChild<QAction*>(
                QStringLiteral("main_import_line_configuration_action")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QAction*>(
                QStringLiteral("main_export_line_configuration_action")
            )
            != nullptr
        );
#endif
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
        QVERIFY(
            window.findChild<QDockWidget*>(
                QStringLiteral("main_window_streams_dock")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QDockWidget*>(
                QStringLiteral("main_window_line_dock")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QDockWidget*>(
                QStringLiteral("main_window_log_dock")
            )
            != nullptr
        );
#else
        QVERIFY(
            window.findChild<QScrollArea*>(
                QStringLiteral("mobile_streams_scroll")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QScrollArea*>(
                QStringLiteral("mobile_lines_scroll")
            )
            != nullptr
        );
        auto* page_stack = window.findChild<QStackedWidget*>(
            QStringLiteral("mobile_page_stack")
        );
        QVERIFY(page_stack != nullptr);
        QCOMPARE(page_stack->count(), 4);
        auto* lines_action
            = window.findChild<QAction*>(QStringLiteral("mobile_navigation_2"));
        QVERIFY(lines_action != nullptr);
        lines_action->trigger();
        QCOMPARE(page_stack->currentIndex(), 2);
        QTest::keyClick(&window, Qt::Key_Back);
        QCOMPARE(page_stack->currentIndex(), 0);
#endif

#ifdef KC_KDE
        QVERIFY(
            window.actionCollection()->action(
                QStringLiteral("line_configuration_import")
            )
            != nullptr
        );
        QVERIFY(
            window.actionCollection()->action(
                QStringLiteral("line_configuration_export")
            )
            != nullptr
        );
        QVERIFY(
            window.actionCollection()->action(QStringLiteral("file_quit"))
            != nullptr
        );
        QVERIFY(
            window.actionCollection()->action(
                QStringLiteral("options_configure")
            )
            != nullptr
        );
#else
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
        QVERIFY(
            window.findChild<QAction*>(QStringLiteral("main_quit_action"))
            != nullptr
        );
        QVERIFY(
            window.findChild<QAction*>(
                QStringLiteral("main_preferences_action")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QAction*>(QStringLiteral("main_about_action"))
            != nullptr
        );
#endif
#endif
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
        QVERIFY(
            window.findChild<QToolBar*>(
                QStringLiteral("mobile_document_toolbar")
            )
            != nullptr
        );
        QVERIFY(
            window.findChild<QToolBar*>(
                QStringLiteral("mobile_navigation_toolbar")
            )
            != nullptr
        );
#else
        QVERIFY(
            window.findChild<QToolBar*>(QStringLiteral("main_toolbar"))
            != nullptr
        );
#endif
    }
}

void main_window_tests::window_grid_preferences() {
    QTemporaryDir temporary_directory;
    QVERIFY(temporary_directory.isValid());
    const auto old_organization = QCoreApplication::organizationName();
    const auto old_application = QCoreApplication::applicationName();
    const auto old_format = QSettings::defaultFormat();
    const auto old_path
        = QFileInfo(QSettings(
                        QSettings::IniFormat, QSettings::UserScope,
                        QStringLiteral("yodau_path_probe")
                    )
                        .fileName())
              .path();
    const auto restore = qScopeGuard([&] {
        QCoreApplication::setOrganizationName(old_organization);
        QCoreApplication::setApplicationName(old_application);
        QSettings::setDefaultFormat(old_format);
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, old_path
        );
    });
    // setPath() from another case takes precedence over XDG_CONFIG_HOME.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(
        QSettings::IniFormat, QSettings::UserScope, temporary_directory.path()
    );
    QCoreApplication::setOrganizationName(QStringLiteral("yodau"));
    QCoreApplication::setApplicationName(
        QStringLiteral("grid-preference-tests")
    );

    {
        main_window window;
        auto* direction = window.findChild<QComboBox*>(
            QStringLiteral("settings_grid_scroll_direction_combo")
        );
        auto* sizing = window.findChild<QComboBox*>(
            QStringLiteral("settings_grid_sizing_mode_combo")
        );
        auto* preserve = window.findChild<QCheckBox*>(
            QStringLiteral("settings_grid_preserve_order_checkbox")
        );
        QVERIFY(direction != nullptr);
        QVERIFY(sizing != nullptr);
        QVERIFY(preserve != nullptr);
        direction->setCurrentIndex(
            direction->findData(QStringLiteral("vertical"))
        );
        sizing->setCurrentIndex(sizing->findData(QStringLiteral("fixed")));
        preserve->setChecked(true);
        QSettings().sync();
    }

    {
        main_window restored;
        const auto* grid = restored.findChild<grid_view*>();
        QVERIFY(grid != nullptr);
        QCOMPARE(
            grid->current_scroll_direction(),
            grid_view::scroll_direction::vertical
        );
        QVERIFY(grid->preserves_stream_order());
        QCOMPARE(grid->current_sizing_mode(), grid_view::sizing_mode::fixed);
    }
}

// NOLINTEND(readability-convert-member-functions-to-static)
