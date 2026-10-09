#include "ui/main_window_tests.hpp"

#include "widgets/log_area_view.hpp"
#include "widgets/log_toolbar_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QTextBlock>
#include <QTextCursor>
#include <QTextLayout>
#include <QToolButton>
#include <QtTest/QtTest>

namespace log_tests {

app_log_entry entry(int index) {
    app_log_entry value;
    value.message = QString("entry-%1").arg(index);
    value.stream_name = "camera";
    value.event_type = "motion";
    return value;
}

} // namespace log_tests

void main_window_tests::log_presentation_controls() {
    app_log_buffer buffer;
    log_toolbar_panel toolbar;
    toolbar.set_log_buffer(&buffer);
    auto* freeze = toolbar.findChild<QCheckBox*>("log_freeze_view");
    auto* follow = toolbar.findChild<QCheckBox*>("log_follow_latest");
    auto* more = toolbar.findChild<QToolButton*>("log_more_filters");
    auto* clear = toolbar.findChild<QPushButton*>("log_clear_filters");
    QVERIFY(!toolbar.view_paused() && toolbar.follows_latest());
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QVERIFY(!freeze && !follow && !more && !clear);
#else
    toolbar.show();
    QVERIFY(freeze && follow && more && clear);
    auto* advanced = toolbar.findChild<QWidget*>("log_advanced_filters");
    QVERIFY(advanced && advanced->isHidden());
    more->setFocus();
    QTest::keyClick(more, Qt::Key_Space);
    QVERIFY(!advanced->isHidden());
    QCOMPARE(more->arrowType(), Qt::DownArrow);
    buffer.append(log_tests::entry(1));
    auto* event
        = toolbar.findChild<QComboBox*>("settings_log_event_filter_combo");
    auto* search
        = toolbar.findChild<QLineEdit*>("settings_log_search_filter_edit");
    QVERIFY(event && search);
    QVERIFY(!event->accessibleName().isEmpty());
    event->setCurrentIndex(event->findData("motion"));
    search->setText("entry-1");
    QTest::keyClick(more, Qt::Key_Space);
    QVERIFY(advanced->isHidden());
    QVERIFY(more->text().contains("1 active"));
    toolbar.set_log_mode(app_log_mode::debug);
    freeze->setFocus();
    QTest::keyClick(freeze, Qt::Key_Space);
    QVERIFY(toolbar.view_paused());
    auto* state = toolbar.findChild<QLabel*>("log_view_state");
    QVERIFY(state && state->text().contains("export remain live"));
    QSignalSpy filters(&toolbar, &log_toolbar_panel::view_state_changed);
    clear->setFocus();
    QTest::keyClick(clear, Qt::Key_Space);
    QCOMPARE(filters.count(), 1);
    QCOMPARE(event->currentIndex(), 0);
    QVERIFY(search->text().isEmpty());
    QVERIFY(!more->text().contains("active"));
    QCOMPARE(toolbar.log_mode(), app_log_mode::debug);
    QVERIFY(toolbar.view_paused());
    QCOMPARE(buffer.entries().size(), 1);
#endif
}

void main_window_tests::log_view_freeze() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop log presentation only");
#else
    app_log_buffer buffer;
    buffer.set_capacity(4);
    log_toolbar_panel toolbar;
    toolbar.set_log_buffer(&buffer);
    log_area_view view;
    log_area_view area(app_log_area::active);
    view.set_log_toolbar(&toolbar);
    area.set_log_toolbar(&toolbar);
    for (int index = 0; index < 4; ++index) {
        buffer.append(log_tests::entry(index));
    }
    const auto frozen = view.toPlainText();
    const auto count = view.visible_entries().size();
    auto* freeze = toolbar.findChild<QCheckBox*>("log_freeze_view");
    QVERIFY(freeze);
    freeze->setChecked(true);
    for (int index = 4; index < 13; ++index) {
        buffer.append(log_tests::entry(index));
    }
    QVERIFY(buffer.entries().size() <= buffer.capacity());
    QCOMPARE(view.toPlainText(), frozen);
    QCOMPARE(area.toPlainText(), frozen);
    QCOMPARE(view.visible_entries().size(), count);
    toolbar.set_log_mode(app_log_mode::debug);
    auto* search
        = toolbar.findChild<QLineEdit*>("settings_log_search_filter_edit");
    search->setText("entry-12");
    QCOMPARE(view.toPlainText(), frozen);
    QCOMPARE(view.active_mode(), app_log_mode::release);
    const auto report = toolbar.compose_log_report();
    QVERIFY(report.contains("entry-12"));
    QVERIFY(!report.contains("entry-0"));
    QTemporaryDir files;
    const auto path = files.filePath("logs.json");
    QVERIFY(toolbar.write_log_report(std::nullopt, path));
    QFile saved(path);
    QVERIFY(saved.open(QIODevice::ReadOnly));
    const auto data = saved.readAll();
    QVERIFY(data.contains("entry-12") && !data.contains("entry-0"));
    buffer.clear();
    QCOMPARE(view.toPlainText(), frozen);
    buffer.append(log_tests::entry(12));
    freeze->setChecked(false);
    QCOMPARE(view.active_mode(), app_log_mode::debug);
    QCOMPARE(view.visible_entries().size(), 1);
    QVERIFY(view.toPlainText().contains("stream=camera"));
    QCOMPARE(area.toPlainText(), view.toPlainText());
    buffer.clear();
    QVERIFY(view.toPlainText().isEmpty());

    // A detached or destroyed toolbar cannot leave stale callbacks or mode.
    auto* other = new log_toolbar_panel;
    other->set_log_buffer(&buffer);
    other->findChild<QCheckBox*>("log_freeze_view")->setChecked(true);
    view.set_log_toolbar(other);
    QVERIFY(view.toPlainText().isEmpty());
    other->findChild<QCheckBox*>("log_freeze_view")->setChecked(false);
    freeze->setChecked(true);
    buffer.append(log_tests::entry(20));
    QVERIFY(view.toPlainText().contains("entry-20"));
    delete other;
    QVERIFY(view.toPlainText().isEmpty());
    QVERIFY(view.append_entry(log_tests::entry(21)));
    QVERIFY(view.toPlainText().contains("entry-21"));
#endif
}

void main_window_tests::log_view_follow() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop log presentation only");
#else
    app_log_buffer buffer;
    log_toolbar_panel toolbar;
    toolbar.set_log_buffer(&buffer);
    log_area_view view;
    view.set_log_toolbar(&toolbar);
    view.resize(420, 100);
    view.show();
    toolbar.show();
    for (int index = 0; index < 80; ++index) {
        buffer.append(log_tests::entry(index));
    }
    auto* scroll = view.verticalScrollBar();
    QTRY_VERIFY(scroll->maximum() > 0);
    QCOMPARE(scroll->value(), scroll->maximum());
    auto* follow = toolbar.findChild<QCheckBox*>("log_follow_latest");
    follow->setFocus();
    QTest::keyClick(follow, Qt::Key_Space);
    QVERIFY(!toolbar.follows_latest());
    auto cursor = view.textCursor();
    cursor.setPosition(0);
    cursor.movePosition(QTextCursor::NextWord, QTextCursor::KeepAnchor);
    view.setTextCursor(cursor);
    scroll->setValue(0);
    const auto selection = view.textCursor().selectedText();
    buffer.append(log_tests::entry(80));
    QCOMPARE(scroll->value(), 0);
    QCOMPARE(view.textCursor().selectedText(), selection);
    QTest::keyClick(follow, Qt::Key_Space);
    QCOMPARE(scroll->value(), scroll->maximum());
    QCOMPARE(view.visible_entries().size(), 81);
    scroll->triggerAction(QAbstractSlider::SliderToMinimum);
    QVERIFY(!toolbar.follows_latest());
    buffer.append(log_tests::entry(81));
    QCOMPARE(scroll->value(), 0);
#endif
}

void main_window_tests::log_toolbar_layout_data() {
    QTest::addColumn<int>("width");
    QTest::addColumn<bool>("rtl");
    QTest::newRow("compact") << 360 << false;
    QTest::newRow("compact-rtl") << 360 << true;
    QTest::newRow("wide") << 800 << false;
    QTest::newRow("wide-rtl") << 800 << true;
}

void main_window_tests::log_toolbar_layout() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop log layout only");
#else
    QFETCH(int, width);
    QFETCH(bool, rtl);
    log_toolbar_panel toolbar;
    toolbar.setLayoutDirection(rtl ? Qt::RightToLeft : Qt::LeftToRight);
    toolbar.resize(width, 300);
    toolbar.show();
    QCoreApplication::processEvents();
    QVERIFY2(
        toolbar.minimumSizeHint().width() <= width,
        qPrintable(QString::number(toolbar.minimumSizeHint().width()))
    );
    auto* more = toolbar.findChild<QToolButton*>("log_more_filters");
    more->click();
    QCoreApplication::processEvents();
    QCOMPARE(toolbar.width(), width);
    for (const auto* name :
         { "settings_copy_logs_button", "settings_copy_summary_button",
           "settings_save_logs_button", "log_freeze_view", "log_follow_latest",
           "log_clear_filters" }) {
        auto* control = toolbar.findChild<QWidget*>(name);
        QVERIFY(control && control->isVisible());
        QVERIFY(toolbar.rect().contains(
            QRect(control->mapTo(&toolbar, QPoint()), control->size())
        ));
    }
#endif
}

void main_window_tests::log_palette_ink() {
    log_area_view view;
    QVERIFY(view.append_entry(log_tests::entry(1)));
    view.show();
    for (const auto& ink : { QColor("#202122"), QColor("#f1f2f3") }) {
        auto palette = view.palette();
        palette.setColor(QPalette::Text, ink);
        palette.setColor(
            QPalette::Base,
            ink.lightness() < 128 ? QColor(Qt::white) : QColor(Qt::black)
        );
        view.setPalette(palette);
        QCoreApplication::processEvents();
        const auto block = view.document()->firstBlock();
        const auto position = block.text().indexOf("info");
        QVERIFY(position >= 0);
        QColor actual;
        for (const auto& format : block.layout()->formats()) {
            if (format.start <= position
                && position < format.start + format.length) {
                actual = format.format.foreground().color();
            }
        }
        QCOMPARE(actual, ink);
    }
}

void main_window_tests::log_native_scroll() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    app_log_buffer buffer;
    log_toolbar_panel toolbar;
    toolbar.set_log_buffer(&buffer);
    log_area_view view;
    view.set_log_toolbar(&toolbar);
    view.resize(420, 100);
    view.show();
    for (int index = 0; index < 80; ++index) {
        buffer.append(log_tests::entry(index));
    }
    auto* scroll = view.verticalScrollBar();
    QTRY_VERIFY(scroll->maximum() > 0);
    QCOMPARE(scroll->value(), scroll->maximum());
    scroll->setValue(0);
    buffer.append(log_tests::entry(80));
    QCOMPARE(scroll->value(), 0);
    QCOMPARE(view.visible_entries().size(), 81);
#else
    QSKIP("Legacy Android scroll behavior only");
#endif
}
