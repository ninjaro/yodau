#include "ui/main_window_tests.hpp"

#include "shell/main_window.hpp"
#include "shell/window_state_store.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QCheckBox>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QSettings>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QtTest/QtTest>

namespace inspector_tests {

struct preferences_scope {
    QTemporaryDir directory;
    QString organization = QCoreApplication::organizationName();
    QString application = QCoreApplication::applicationName();
    QSettings::Format format = QSettings::defaultFormat();
    QString path = QFileInfo(QSettings(
                                 QSettings::IniFormat, QSettings::UserScope,
                                 "yodau_path_probe"
                             )
                                 .fileName())
                       .path();

    preferences_scope() {
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, directory.path()
        );
        QCoreApplication::setOrganizationName("yodau");
        QCoreApplication::setApplicationName("inspector-tests");
    }

    ~preferences_scope() {
        QCoreApplication::setOrganizationName(organization);
        QCoreApplication::setApplicationName(application);
        QSettings::setDefaultFormat(format);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, path);
    }
};

} // namespace inspector_tests

void main_window_tests::inspector_preferences() {
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QSettings settings(
        directory.filePath("preferences.ini"), QSettings::IniFormat
    );
    settings.setValue("layout/stream_grid/preserve_order", true);
    const auto keys = settings.allKeys();
    QVERIFY(!yodau::shell::load_context_inspector(settings));
    QCOMPARE(settings.allKeys(), keys);
    QVERIFY(yodau::shell::save_context_inspector(settings, true));
    QVERIFY(!yodau::shell::load_desktop_presentation(settings).has_value());
    QVERIFY(!yodau::shell::load_canvas_tools(settings));
    yodau::shell::desktop_presentation preset;
    preset.preset = yodau::shell::desktop_preset::canvas;
    QVERIFY(yodau::shell::save_desktop_presentation(settings, preset));
    preset.reset_overrides();
    QVERIFY(yodau::shell::save_desktop_presentation(settings, preset));
    QVERIFY(yodau::shell::load_context_inspector(settings));
    QVERIFY(settings.value("layout/stream_grid/preserve_order").toBool());
    QSettings reload(
        directory.filePath("preferences.ini"), QSettings::IniFormat
    );
    QVERIFY(yodau::shell::load_context_inspector(reload));
    QVERIFY(yodau::shell::save_context_inspector(reload, false));
    QVERIFY(!yodau::shell::load_context_inspector(settings));
    QSettings blocked(directory.path(), QSettings::IniFormat);
    QVERIFY(!yodau::shell::save_context_inspector(blocked, true));
    QVERIFY(!blocked.contains("desktop/presentation/context_inspector"));
    blocked.setValue("desktop/presentation/context_inspector", true);
    QVERIFY(!yodau::shell::save_context_inspector(blocked, false));
    QVERIFY(yodau::shell::load_context_inspector(blocked));
}

void main_window_tests::inspector_shell() {
    inspector_tests::preferences_scope preferences;
    QVERIFY(preferences.directory.isValid());
    {
        main_window window;
        auto* action = window.findChild<QAction*>("desktop_context_inspector");
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
        QVERIFY(action == nullptr);
        QVERIFY(window.findChild<QLabel*>("inspector_context") == nullptr);
#else
        QVERIFY(action && !action->isChecked());
        QVERIFY(window.findChild<QLabel*>("inspector_context") == nullptr);
        auto* menu = window.findChild<QMenu*>("desktop_presentation_menu");
        QVERIFY(menu && menu->actions().contains(action));
        auto* lines = window.findChild<QDockWidget*>("main_window_line_dock");
        auto* editor = qobject_cast<active_editor_panel*>(lines->widget());
        auto* settings = window.findChild<settings_panel*>();
        QVERIFY(lines && editor && settings);
        lines->hide();
        action->trigger();
        QVERIFY(action->isChecked() && lines->isHidden());
        auto* context = editor->findChild<QLabel*>("inspector_context");
        QVERIFY(context && !context->isHidden());
        QSettings saved;
        QVERIFY(!saved.contains("desktop/presentation/preset"));
        QVERIFY(!yodau::shell::load_canvas_tools(saved));
        auto* canvas = window.findChild<QAction*>("desktop_preset_canvas");
        auto* classic = window.findChild<QAction*>("desktop_preset_classic");
        auto* reset = window.findChild<QAction*>("desktop_preset_reset");
        canvas->trigger();
        reset->trigger();
        QVERIFY(lines->isHidden() && action->isChecked());
        classic->trigger();
        QVERIFY(!lines->isHidden() && action->isChecked());
        // Configured-source tuning remains independent of the active canvas.
        settings->add_stream_entry("hidden", "unused", false);
        const auto stream
            = settings->current_active_stream_settings().stream_name;
        QCOMPARE(stream, QStringLiteral("hidden"));
        auto* stream_controls = settings->findChild<active_stream_panel*>(
            "settings_stream_settings_tab"
        );
        QVERIFY(stream_controls != nullptr);
        settings->findChild<QTabWidget*>("settings_tabs")
            ->setCurrentWidget(stream_controls);
        QVERIFY(stream_controls && !stream_controls->isHidden());
        QVERIFY(context->text().contains("Activate a visible stream"));
        action->trigger();
        QVERIFY(context->isHidden());
        QCOMPARE(
            settings->current_active_stream_settings().stream_name, stream
        );
        action->trigger();

        QFile blocker(preferences.directory.filePath("blocker"));
        QVERIFY(blocker.open(QIODevice::WriteOnly));
        blocker.close();
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope, blocker.fileName()
        );
        bool warned = false;
        QTimer::singleShot(0, &window, [&] {
            auto* dialog
                = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (dialog != nullptr) {
                warned = true;
                dialog->accept();
            }
        });
        action->trigger();
        QVERIFY(warned && action->isChecked() && !context->isHidden());
        QSettings::setPath(
            QSettings::IniFormat, QSettings::UserScope,
            preferences.directory.path()
        );
        // Dock toggle checks follow effective visibility. Exercise the real
        // visible window before asking that native action to hide the panel.
        window.show();
        QTRY_VERIFY(lines->isVisible());
        lines->toggleViewAction()->trigger();
        QVERIFY(lines->isHidden());
#endif
    }
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    main_window restored;
    auto* action = restored.findChild<QAction*>("desktop_context_inspector");
    auto* lines = restored.findChild<QDockWidget*>("main_window_line_dock");
    QVERIFY(action->isChecked() && lines->isHidden());
    action->trigger();
    QVERIFY(!action->isChecked() && lines->isHidden());
    QVERIFY(restored.findChild<QLabel*>("inspector_context")->isHidden());
#endif
}

void main_window_tests::inspector_layout_data() {
    QTest::addColumn<QSize>("size");
    QTest::addColumn<pool_side>("side");
    for (const auto size : { QSize(640, 480), QSize(1280, 800) }) {
        for (const auto side :
             { pool_side::bottom, pool_side::left, pool_side::right }) {
            const auto name = QStringLiteral("%1-%2-%3")
                                  .arg(size.width())
                                  .arg(size.height())
                                  .arg(static_cast<int>(side))
                                  .toLatin1();
            QTest::newRow(name.constData()) << size << side;
        }
    }
}

void main_window_tests::inspector_layout() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop inspector only");
#else
    QFETCH(QSize, size);
    QFETCH(pool_side, side);
    inspector_tests::preferences_scope preferences;
    QVERIFY(preferences.directory.isValid());
    main_window window;
    auto* lines = window.findChild<QDockWidget*>("main_window_line_dock");
    auto* editor = qobject_cast<active_editor_panel*>(lines->widget());
    auto* board = window.findChild<stream_board*>();
    auto* settings = window.findChild<settings_panel*>();
    window.findChild<QDockWidget*>("main_window_streams_dock")->hide();
    window.findChild<QDockWidget*>("main_window_log_dock")->hide();
    window.findChild<QAction*>("desktop_context_inspector")->trigger();
    window.findChild<QAction*>("desktop_canvas_tools")->trigger();
    board->grid_mode()->add_stream("one");
    board->grid_mode()->add_stream("two");
    settings->set_active_candidates({ "one", "two" });
    settings->set_active_current("one");
    board->set_active_stream("one");
    board->set_pool_side(side);
    auto* cell = board->active_cell();
    cell->set_draft_points_pct({ { 10, 20 }, { 80, 70 } });
    const auto draft = cell->draft_points_pct();
    editor->findChild<QCheckBox*>("settings_active_line_advanced_checkbox")
        ->setChecked(true);
    window.resize(size);
    window.show();
    window.resizeDocks({ lines }, { 240 }, Qt::Horizontal);
    auto* scroll = editor->findChild<QScrollArea*>("inspector_scroll");
    auto* tabs = editor->findChild<QTabWidget*>("settings_active_editor_tabs");
    auto* save
        = editor->findChild<QPushButton*>("settings_active_line_save_button");
    QVERIFY(scroll && tabs && save);
    QTRY_VERIFY(board->grid_mode()->isVisible() && cell->isVisible());
    QVERIFY(editor->findChild<QLabel*>("inspector_context")->isVisible());
    QTRY_VERIFY(lines->width() < window.width() / 2);
    QTRY_VERIFY(board->width() > 0 && board->height() > 0);
    scroll->ensureWidgetVisible(save);
    QTRY_VERIFY(scroll->viewport()->rect().intersects(
        QRect(save->mapTo(scroll->viewport(), QPoint()), save->size())
    ));
    save->setFocus(Qt::TabFocusReason);
    QTRY_COMPARE(QApplication::focusWidget(), save);
    QTest::keyClick(save, Qt::Key_Backtab);
    auto* previous = QApplication::focusWidget();
    QVERIFY(previous != nullptr && editor->isAncestorOf(previous));
    QTest::keyClick(previous, Qt::Key_Tab);
    QTRY_COMPARE(QApplication::focusWidget(), save);
    editor->set_context_inspector(false);
    editor->set_context_inspector(true);
    QTRY_COMPARE(QApplication::focusWidget(), save);
    QCOMPARE(board->active_cell(), cell);
    QCOMPARE(cell->draft_points_pct(), draft);
    QVERIFY(board->grid_mode()->isVisible());
    lines->toggleViewAction()->trigger();
    QTRY_VERIFY(lines->isHidden());
    QCOMPARE(board->active_cell(), cell);
    QVERIFY(board->grid_mode()->isVisible() && cell->isVisible());
#endif
}
