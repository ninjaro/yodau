#include "ui/main_window_tests.hpp"

#include "widgets/processing_readout.hpp"

#include <QLabel>
#include <QScrollArea>
#include <QSignalSpy>
#include <QTimer>
#include <QVBoxLayout>
#include <QtTest/QtTest>
#include <limits>

namespace {

using shared_ui::qt::readout_freshness;
using shared_ui::qt::readout_group;
using shared_ui::qt::readout_item;

// Sections: 0 active stream, 1 GUI observations, 2 effective policy.
const readout_item& item(const processing_readout& view, int section, int row) {
    return view.sections().at(section)->items().at(row);
}

// What the readout shows for a row: its value, or the host's reason when the
// observation is missing (the value is then a dash).
QString text(const processing_readout& view, int section, int row) {
    const auto* group = view.sections().at(section);
    const auto& shown = group->items().at(row);
    return shown.available ? group->shown_value(row) : group->shown_detail(row);
}

QString first_line(const processing_readout& view, int section, int row) {
    return text(view, section, row).section('\n', 0, 0);
}

} // namespace

void main_window_tests::
    processing_readout_preserves_selection_during_refresh() {
    QWidget host;
    auto* layout = new QVBoxLayout(&host);
    auto* view = new processing_readout(&host);
    layout->addWidget(view);
    processing_sample sample;
    sample.stream_name = "camera";
    sample.source_status = "Ready";
    view->set_reader([&sample] { return sample; });
    host.resize(600, 700);
    host.show();
    host.activateWindow();
    QCoreApplication::processEvents();
    auto* group = view->sections().front();
    auto* label = qobject_cast<QLabel*>(group->item_widgets().front());
    QVERIFY(label);
    label->setFocus();
    label->setSelection(0, 6);
    QSignalSpy changed(
        group, &shared_ui::qt::readout_group::presentation_changed
    );
    auto* timer = view->findChild<QTimer*>("processing_refresh_timer");
    QVERIFY(timer);
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(changed.count(), 0);
    sample.source_status = "Waiting for another frame";
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(group->item_widgets().front(), label);
    QCOMPARE(label->selectedText(), QStringLiteral("camera"));
    QVERIFY(label->hasFocus());
    QCOMPARE(group->items()[1].value, sample.source_status);
    view->set_reader({});
    QVERIFY(!timer->isActive());
}

void main_window_tests::processing_readout_states() {
    processing_sample sample;
    processing_readout view;
    view.set_reader([&sample] { return sample; });
    view.show();
    QCOMPARE(view.sections().size(), 3);
    QVERIFY(!item(view, 0, 0).available);
    QCOMPARE(text(view, 0, 0), QStringLiteral("No active stream"));
    QCOMPARE(view.sections()[0]->shown_value(0), QString(QChar(0x2014)));
    QCOMPARE(text(view, 1, 0), QStringLiteral("No sample"));
    QCOMPARE(text(view, 1, 1), QStringLiteral("No sample"));
    sample.stream_name = "<b>camera</b>";
    sample.source_status = "Loading media";
    sample.algorithm_id = "line_crossing";
    sample.preset_id = "balanced";
    sample.processing_enabled = true;
    sample.metrics.input_fps = 500;
    sample.metrics.processed_width
        = 999; // Prepared dimensions are not a sample.
    sample.metrics.processing_summary = "old summary";
    const auto update = [&] { view.set_reader([&sample] { return sample; }); };
    update();
    QCOMPARE(text(view, 0, 0), sample.stream_name);
    QCOMPARE(text(view, 0, 1), QStringLiteral("Loading media"));
    QVERIFY(!item(view, 1, 0).available && !item(view, 1, 1).available);
    QCOMPARE(text(view, 1, 0), QStringLiteral("No sample"));
    QCOMPARE(text(view, 1, 1), QStringLiteral("No sample"));
    QVERIFY(!item(view, 1, 2).available);
    QVERIFY(!text(view, 1, 2).contains("old summary"));
    QVERIFY(text(view, 2, 1).contains("Unavailable"));

    sample.input_age_ms = 2999;
    sample.result_age_ms = 3000;
    sample.metrics.input_fps = std::numeric_limits<double>::infinity();
    sample.metrics.core_fps = 12.5;
    sample.metrics.input_width = 1920;
    sample.metrics.input_height = 1080;
    sample.metrics.processed_width = 640;
    sample.metrics.processed_height = 360;
    sample.metrics.processing_summary = "<b>tracks 2</b>";
    sample.metrics.effective_display_fps = 24;
    sample.metrics.effective_core_fps = 8;
    sample.metrics.effective_processing_pixels = 640 * 360;
    sample.metrics.manual_policy_active = true;
    update();
    // The two delivery paths age independently.
    QCOMPARE(
        first_line(view, 1, 0),
        QStringLiteral("Recent — 3.0 s since GUI delivery")
    );
    QVERIFY(item(view, 1, 0).freshness == readout_freshness::live);
    QVERIFY(text(view, 1, 0).contains("Rate pending"));
    QVERIFY(text(view, 1, 0).contains("1920 × 1080"));
    QCOMPARE(
        first_line(view, 1, 1),
        QStringLiteral("Stale — 3.0 s since GUI delivery")
    );
    QVERIFY(item(view, 1, 1).freshness == readout_freshness::stale);
    QVERIFY(text(view, 1, 1).contains("12.5 fps (arrival EMA)"));
    QVERIFY(text(view, 1, 1).contains("640 × 360"));
    QVERIFY(item(view, 1, 1).detail.isEmpty());
    QCOMPARE(text(view, 1, 2), sample.metrics.processing_summary);
    // A retained summary is not dated by the newest callback.
    QVERIFY(!item(view, 1, 2).freshness);
    QCOMPARE(text(view, 2, 0), QStringLiteral("line_crossing / balanced"));
    QVERIFY(text(view, 2, 1).contains("Manual policy"));
    QVERIFY(text(view, 2, 1).contains("Repaint target: 24 fps"));
    QVERIFY(text(view, 2, 1).contains("Analysis target: 8 fps"));
    QVERIFY(text(view, 2, 1).contains("230400 px"));
    sample.application_active = false;
    update();
    QCOMPARE(
        item(view, 1, 1).detail,
        QStringLiteral("Inactive — observations retained")
    );
    QVERIFY(first_line(view, 1, 1).startsWith("Stale"));
    QVERIFY(view.sections()[1]->shown_detail(1).startsWith("Inactive"));
    sample.processing_enabled = false;
    update();
    QCOMPARE(item(view, 1, 1).detail, QStringLiteral("Processing unavailable"));
    sample.result_age_ms = -1;
    update();
    QVERIFY(!item(view, 1, 1).available);
    QCOMPARE(
        text(view, 1, 1), QStringLiteral("Processing unavailable\nNo sample")
    );
    sample.application_active = true;
    sample.processing_enabled = true;
    sample.result_age_ms = 0;
    sample.metrics.core_fps = std::numeric_limits<double>::quiet_NaN();
    sample.metrics.processed_width = 0;
    update();
    QVERIFY(text(view, 1, 1).contains("Rate pending"));
    QVERIFY(text(view, 1, 1).contains("Size unavailable"));
    QCOMPARE(
        first_line(view, 1, 1),
        QStringLiteral("Recent — 0.0 s since GUI delivery")
    );
    sample.input_age_ms = 1234;
    sample.result_age_ms = 1500;
    update();
    QCOMPARE(
        first_line(view, 1, 0),
        QStringLiteral("Recent — 1.2 s since GUI delivery")
    );
    QCOMPARE(
        first_line(view, 1, 1),
        QStringLiteral("Recent — 1.5 s since GUI delivery")
    );
    sample = {};
    update();
    QCOMPARE(text(view, 0, 0), QStringLiteral("No active stream"));
    QCOMPARE(text(view, 1, 1), QStringLiteral("No sample"));
    QVERIFY(!text(view, 1, 2).contains("tracks 2"));
    QVERIFY(!item(view, 2, 1).available);
    // Every shown text stays plain: a stream name cannot become markup.
    for (const auto* label : view.findChildren<QLabel*>()) {
        QCOMPARE(label->textFormat(), Qt::PlainText);
    }
}

void main_window_tests::processing_readout_lifetime() {
    QWidget parent;
    processing_readout view(&parent);
    int reads = 0;
    view.set_reader([&reads] {
        ++reads;
        processing_sample sample;
        sample.stream_name = QString::number(reads);
        return sample;
    });
    auto* timer = view.findChild<QTimer*>("processing_refresh_timer");
    QVERIFY(timer && !timer->isActive());
    QCOMPARE(reads, 0);
    parent.show();
    QTRY_VERIFY(timer->isActive());
    QVERIFY(reads >= 1);
    const int shown_reads = reads;
    QTRY_VERIFY_WITH_TIMEOUT(reads > shown_reads, 2000);
    parent.hide();
    QVERIFY(!timer->isActive());
    const int hidden_reads = reads;
    // Even a late timeout must not query the owner after the view is hidden.
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(reads, hidden_reads);
    parent.show();
    QVERIFY(timer->isActive());
    QCOMPARE(reads, hidden_reads + 1);
    view.set_reader({});
    QVERIFY(!timer->isActive());
    QCOMPARE(text(view, 0, 0), QStringLiteral("No active stream"));
    QVERIFY(QMetaObject::invokeMethod(timer, "timeout", Qt::DirectConnection));
    QCOMPARE(reads, hidden_reads + 1);
}

void main_window_tests::processing_readout_layout_data() {
    QTest::addColumn<Qt::LayoutDirection>("direction");
    QTest::newRow("ltr") << Qt::LeftToRight;
    QTest::newRow("rtl") << Qt::RightToLeft;
}

void main_window_tests::processing_readout_layout() {
    QFETCH(Qt::LayoutDirection, direction);
    processing_readout view;
    auto font = view.font();
    font.setPointSizeF(font.pointSizeF() * 1.3);
    view.setFont(font);
    view.setLayoutDirection(direction);
    view.resize(240, 320);
    view.set_reader([] {
        processing_sample sample;
        sample.stream_name = QString(120, 'W');
        sample.source_status = "Source error: <b>unavailable</b>";
        sample.processing_enabled = true;
        sample.result_age_ms = 100;
        sample.metrics.processing_summary = QString(240, 'W');
        return sample;
    });
    view.show();
    auto* scroll = view.findChild<QScrollArea*>("processing_scroll");
    QVERIFY(scroll);
    QTRY_VERIFY(scroll->viewport()->width() > 0);
    const auto inside = [scroll](QWidget* widget) {
        scroll->ensureWidgetVisible(widget);
        QTRY_VERIFY(scroll->viewport()->rect().intersects(
            QRect(widget->mapTo(scroll->viewport(), QPoint()), widget->size())
        ));
        const int x = widget->mapTo(scroll->viewport(), QPoint()).x();
        QVERIFY(x >= 0);
        QVERIFY(x + widget->width() <= scroll->viewport()->width());
    };
    for (auto* label : view.findChildren<QLabel*>()) {
        if (!label->isVisibleTo(&view)) {
            continue;
        }
        QCOMPARE(label->textFormat(), Qt::PlainText);
        QVERIFY(label->wordWrap());
        QVERIFY(label->width() > 0 && label->height() > 0);
        inside(label);
    }
    QList<QWidget*> items;
    for (auto* section : view.sections()) {
        QVERIFY(!section->accessibleName().isEmpty());
        items += section->item_widgets();
    }
    QCOMPARE(items.size(), 7);
    for (auto* focusable : items) {
        // Values are selectable text that also takes keyboard focus.
        auto* value = qobject_cast<QLabel*>(focusable);
        QVERIFY(value);
        QVERIFY(
            value->textInteractionFlags().testFlag(Qt::TextSelectableByKeyboard)
        );
        QVERIFY(
            value->textInteractionFlags().testFlag(Qt::TextSelectableByMouse)
        );
        QVERIFY(!focusable->accessibleName().isEmpty());
        inside(focusable);
        focusable->setFocus(Qt::TabFocusReason);
        QTRY_COMPARE(QApplication::focusWidget(), focusable);
    }
    items.first()->setFocus();
    QTest::keyClick(items.first(), Qt::Key_Tab);
    QTRY_COMPARE(QApplication::focusWidget(), items[1]);
}
