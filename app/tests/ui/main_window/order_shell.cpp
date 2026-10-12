#include "ui/main_window_tests.hpp"

#include "shell/main_window.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"
#include "widgets/stream_inventory_panel.hpp"

#include <QCheckBox>
#include <QFileInfo>
#include <QScopeGuard>
#include <QSettings>
#include <QTemporaryDir>
#include <QTreeWidget>
#include <QtTest/QtTest>

void main_window_tests::stream_order_connects_to_grid() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop order prototype only");
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
    const auto restore = qScopeGuard([&] {
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
    QCoreApplication::setApplicationName(QStringLiteral("stream-order-tests"));
    QSettings preferences;
    preferences.setValue(
        QStringLiteral("layout/stream_grid/preserve_order"), false
    );
    preferences.sync();
    {
        main_window window;
        auto* board = window.findChild<stream_board*>();
        auto* settings = window.findChild<settings_panel*>();
        auto* inventory = window.findChild<stream_inventory_panel*>();
        QVERIFY(board && settings && inventory);
        auto* grid = board->grid_mode();
        for (const auto& name : { "charlie", "alpha", "bravo", "hidden" }) {
            settings->add_stream_entry(
                QString::fromLatin1(name), QStringLiteral("source")
            );
            if (QString::fromLatin1(name) != QStringLiteral("hidden")) {
                grid->add_stream(QString::fromLatin1(name));
            }
        }
        board->set_active_stream(QStringLiteral("bravo"));
        auto* active = board->active_cell();
        active->set_draft_points_pct({ QPointF(10, 20), QPointF(80, 90) });
        const auto draft = active->draft_points_pct();
        const auto keys = preferences.allKeys();
        auto* list = inventory->findChild<QTreeWidget*>();
        QVERIFY(list != nullptr);
        list->setCurrentItem(list->topLevelItem(1));
        QTest::keyClick(list, Qt::Key_Down, Qt::ControlModifier);
        const QStringList order { "charlie", "bravo", "alpha", "hidden" };
        QCOMPARE(grid->stream_order(), order);
        QCOMPARE(grid->stream_names(), QStringList({ "charlie", "alpha" }));
        QCOMPARE(board->active_cell(), active);
        QCOMPARE(active->draft_points_pct(), draft);
        QVERIFY(!grid->preserves_stream_order());
        preferences.sync();
        QCOMPARE(preferences.allKeys(), keys);
        QVERIFY(!preferences
                     .value(QStringLiteral("layout/stream_grid/preserve_order"))
                     .toBool());
        board->clear_active();
        QCOMPARE(grid->peek_stream_cell(QStringLiteral("bravo")), active);
        QCOMPARE(
            grid->stream_names(), QStringList({ "charlie", "bravo", "alpha" })
        );
        grid->add_stream(QStringLiteral("hidden"));
        QCOMPARE(grid->stream_names(), order);
        auto* preserve = window.findChild<QCheckBox*>(
            QStringLiteral("settings_grid_preserve_order_checkbox")
        );
        QVERIFY(preserve != nullptr);
        preserve->setChecked(true);
        QVERIFY(grid->preserves_stream_order());
        QCOMPARE(grid->stream_order(), order);
        settings->remove_stream_entry(QStringLiteral("charlie"));
        grid->remove_stream(QStringLiteral("charlie"));
        QCOMPARE(
            grid->stream_order(), QStringList({ "bravo", "alpha", "hidden" })
        );
        settings->clear_stream_entries();
        QVERIFY(grid->stream_order().isEmpty());
    }
    main_window restarted;
    QVERIFY(restarted.findChild<stream_board*>()
                ->grid_mode()
                ->stream_order()
                .isEmpty());
#endif
}
