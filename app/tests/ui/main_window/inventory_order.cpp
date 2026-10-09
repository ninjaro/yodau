#include "ui/main_window_tests.hpp"

#include "widgets/stream_inventory_panel.hpp"

#include <QDrag>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QPushButton>
#include <QTimer>
#include <QTreeWidget>
#include <QtTest/QtTest>

void main_window_tests::inventory_order_controls() {
    stream_inventory_panel panel;
    QSignalSpy order_spy(&panel, &stream_inventory_panel::stream_order_changed);
    QSignalSpy show_spy(&panel, &stream_inventory_panel::show_stream_changed);
    panel.add_stream_entry(
        QStringLiteral("charlie"), QStringLiteral("source-c")
    );
    panel.add_stream_entry(
        QStringLiteral("alpha"), QStringLiteral("source-a"), true
    );
    panel.add_stream_entry(QStringLiteral("bravo"), QStringLiteral("source-b"));
    auto* list = panel.findChild<QTreeWidget*>();
    auto* earlier
        = panel.findChild<QPushButton*>(QStringLiteral("stream_order_earlier"));
    auto* later
        = panel.findChild<QPushButton*>(QStringLiteral("stream_order_later"));
    QVERIFY(list != nullptr);
    QCOMPARE(order_spy.count(), 0);
    QCOMPARE(show_spy.count(), 0);
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QVERIFY(earlier == nullptr && later == nullptr);
    QCOMPARE(list->dragDropMode(), QAbstractItemView::NoDragDrop);
    return;
#else
    QVERIFY(earlier && later);
    QVERIFY(!earlier->isEnabled() && !later->isEnabled());
    QVERIFY(!list->accessibleDescription().isEmpty());
    QVERIFY(
        !earlier->accessibleName().isEmpty()
        && !later->accessibleName().isEmpty()
    );
    QCOMPARE(list->dragDropMode(), QAbstractItemView::InternalMove);
    QVERIFY(
        list->dragEnabled() && list->acceptDrops() && list->showDropIndicator()
    );
    QCOMPARE(list->selectionMode(), QAbstractItemView::SingleSelection);
    auto* selected = list->topLevelItem(1);
    list->setCurrentItem(selected);
    QVERIFY(earlier->isEnabled() && later->isEnabled());
    earlier->click();
    QCOMPARE(
        panel.stream_order(), QStringList({ "alpha", "charlie", "bravo" })
    );
    QCOMPARE(list->currentItem(), selected);
    QCOMPARE(selected->checkState(0), Qt::Checked);
    QCOMPARE(selected->text(2), QStringLiteral("source-a"));
    QCOMPARE(order_spy.count(), 1);
    QVERIFY(!earlier->isEnabled());
    earlier->click(); // Boundary no-op.
    QCOMPARE(order_spy.count(), 1);
    QTest::keyClick(list, Qt::Key_Down, Qt::ControlModifier);
    QTest::keyClick(list, Qt::Key_Down, Qt::ControlModifier);
    QCOMPARE(
        panel.stream_order(), QStringList({ "charlie", "bravo", "alpha" })
    );
    QVERIFY(!later->isEnabled());
    QTest::keyClick(list, Qt::Key_Down, Qt::ControlModifier);
    QCOMPARE(order_spy.count(), 3);
    QCOMPARE(show_spy.count(), 0);
    for (int index = 0; index < list->topLevelItemCount(); ++index) {
        const auto* item = list->topLevelItem(index);
        QVERIFY(!(item->flags() & Qt::ItemIsDropEnabled));
        QCOMPARE(item->childCount(), 0);
    }
    panel.set_stream_checked(QStringLiteral("bravo"), true);
    QCOMPARE(show_spy.count(), 1);
    QCOMPARE(order_spy.count(), 3);
    panel.add_stream_entry(QStringLiteral("delta"), QStringLiteral("source-d"));
    QCOMPARE(order_spy.count(), 4);
    QCOMPARE(order_spy.last().at(0).toStringList(), panel.stream_order());
    panel.add_stream_entry(
        QStringLiteral("delta"), QStringLiteral("duplicate")
    );
    QCOMPARE(order_spy.count(), 4);
    panel.remove_stream_entry(QStringLiteral("missing"));
    QCOMPARE(order_spy.count(), 4);
    panel.remove_stream_entry(QStringLiteral("alpha"));
    QCOMPARE(order_spy.count(), 5);
    QVERIFY(!panel.stream_order().contains(QStringLiteral("alpha")));
    panel.clear_stream_entries();
    QCOMPARE(order_spy.count(), 6);
    QVERIFY(order_spy.last().at(0).toStringList().isEmpty());
    panel.add_stream_entry(QStringLiteral("new"), QStringLiteral("source"));
    QCOMPARE(
        order_spy.count(), 6
    ); // A cleared catalogue starts without intent.
    panel.add_stream_entry(QStringLiteral("last"), QStringLiteral("source"));
    list->setCurrentItem(list->topLevelItem(0));
    QTest::keyClick(list, Qt::Key_Down, Qt::ControlModifier);
    panel.remove_stream_entry(QStringLiteral("new"));
    panel.remove_stream_entry(QStringLiteral("last"));
    QVERIFY(order_spy.last().at(0).toStringList().isEmpty());
    const auto count = order_spy.count();
    panel.add_stream_entry(QStringLiteral("fresh"), QStringLiteral("source"));
    QCOMPARE(order_spy.count(), count);
#endif
}

void main_window_tests::inventory_order_drop_data() {
    QTest::addColumn<QString>("target");
    for (const auto* target :
         { "before", "after", "end", "self", "copy", "foreign", "cancel" }) {
        QTest::newRow(target) << QString::fromLatin1(target);
    }
}

void main_window_tests::inventory_order_drop() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop order prototype only");
#else
    QFETCH(QString, target);
    stream_inventory_panel panel;
    panel.resize(560, 420);
    panel.add_stream_entry(
        QStringLiteral("alpha"), QStringLiteral("source-a"), true
    );
    panel.add_stream_entry(QStringLiteral("bravo"), QStringLiteral("source-b"));
    panel.add_stream_entry(
        QStringLiteral("charlie"), QStringLiteral("source-c")
    );
    panel.show();
    QCoreApplication::processEvents();
    auto* list = panel.findChild<QTreeWidget*>();
    QVERIFY(list != nullptr);
    auto* item = list->topLevelItem(1);
    list->setCurrentItem(item, 1);
    const auto before = panel.stream_order();
    QSignalSpy order_spy(&panel, &stream_inventory_panel::stream_order_changed);
    QSignalSpy show_spy(&panel, &stream_inventory_panel::show_stream_changed);
    const auto last_rect = list->visualItemRect(list->topLevelItem(2));
    QPoint position(last_rect.center().x(), last_rect.bottom() - 1);
    QStringList expected { "alpha", "charlie", "bravo" };
    if (target == QStringLiteral("before")) {
        const auto rect = list->visualItemRect(list->topLevelItem(0));
        position = QPoint(rect.center().x(), rect.top() + 1);
        expected = { "bravo", "alpha", "charlie" };
    } else if (target == QStringLiteral("end")) {
        position.setY(last_rect.bottom() + 20);
    } else if (target == QStringLiteral("self")) {
        position = list->visualItemRect(item).center();
        expected = before;
    } else if (
        target == QStringLiteral("copy") || target == QStringLiteral("foreign")
        || target == QStringLiteral("cancel")
    ) {
        expected = before;
    }
    QWidget foreign;
    QDrag drag(
        target == QStringLiteral("foreign") ? static_cast<QObject*>(&foreign)
                                            : static_cast<QObject*>(list)
    );
    drag.setMimeData(list->model()->mimeData({ list->model()->index(1, 1) }));
    const auto action
        = target == QStringLiteral("copy") ? Qt::CopyAction : Qt::MoveAction;
    bool delivered = false;
    bool same_source = false;
    QTimer delivery;
    delivery.setSingleShot(true);
    connect(&delivery, &QTimer::timeout, &panel, [&] {
        delivered = true;
        QDragEnterEvent enter(
            position, action, drag.mimeData(), Qt::LeftButton, Qt::NoModifier
        );
        QApplication::sendEvent(list->viewport(), &enter);
        if (target == QStringLiteral("cancel")) {
            same_source = enter.source() == list;
            QDragLeaveEvent leave;
            QApplication::sendEvent(list->viewport(), &leave);
            QDrag::cancel();
            return;
        }
        QDragMoveEvent move(
            position, action, drag.mimeData(), Qt::LeftButton, Qt::NoModifier
        );
        QApplication::sendEvent(list->viewport(), &move);
        QDropEvent drop(
            position, action, drag.mimeData(), Qt::LeftButton, Qt::NoModifier
        );
        same_source = drop.source() == list;
        QApplication::sendEvent(list->viewport(), &drop);
        QDrag::cancel();
    });
    delivery.start(0);
    drag.exec(action, action);
    if (!delivered) {
        QSKIP("Platform did not run an internal drag event loop");
    }
    QCOMPARE(same_source, target != QStringLiteral("foreign"));
    QCOMPARE(panel.stream_order(), expected);
    QCOMPARE(list->topLevelItemCount(), 3);
    QCOMPARE(list->currentItem(), item);
    QCOMPARE(order_spy.count(), expected == before ? 0 : 1);
    QCOMPARE(show_spy.count(), 0);
    QVERIFY(panel.stream_order().contains(QStringLiteral("bravo")));
    for (int index = 0; index < list->topLevelItemCount(); ++index) {
        auto* row = list->topLevelItem(index);
        QCOMPARE(row->childCount(), 0);
        if (row->text(1) == QStringLiteral("bravo")) {
            QCOMPARE(row, item);
            QCOMPARE(row->text(2), QStringLiteral("source-b"));
            QCOMPARE(row->checkState(0), Qt::Unchecked);
        }
    }
#endif
}
