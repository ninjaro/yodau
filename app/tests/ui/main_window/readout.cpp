#include "ui/main_window_tests.hpp"

#include "widgets/processing_readout.hpp"

#include <QLabel>
#include <QScrollArea>
#include <QTimer>
#include <QtTest/QtTest>
#include <limits>

namespace {

QString text(const processing_readout& view, const char* name) {
    return view.findChild<QLabel*>(QString::fromLatin1(name))->text();
}

} // namespace

void main_window_tests::processing_readout_states() {
    processing_sample sample;
    processing_readout view;
    view.set_reader([&sample] { return sample; });
    view.show();
    QCOMPARE(
        text(view, "processing_target"), QStringLiteral("No active stream")
    );
    QCOMPARE(text(view, "processing_input"), QStringLiteral("No sample"));
    QCOMPARE(text(view, "processing_result"), QStringLiteral("No sample"));
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
    QCOMPARE(text(view, "processing_target"), sample.stream_name);
    QCOMPARE(text(view, "processing_input"), QStringLiteral("No sample"));
    QCOMPARE(text(view, "processing_result"), QStringLiteral("No sample"));
    QVERIFY(!text(view, "processing_summary").contains("old summary"));
    QVERIFY(text(view, "processing_policy").contains("Unavailable"));

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
    QVERIFY(text(view, "processing_input").startsWith("Recent"));
    QVERIFY(text(view, "processing_input").contains("Rate pending"));
    QVERIFY(text(view, "processing_input").contains("1920 × 1080"));
    QVERIFY(text(view, "processing_result").startsWith("Stale"));
    QVERIFY(text(view, "processing_result").contains("12.5 fps (arrival EMA)"));
    QVERIFY(text(view, "processing_result").contains("640 × 360"));
    QCOMPARE(
        text(view, "processing_summary"), sample.metrics.processing_summary
    );
    QCOMPARE(
        text(view, "processing_algorithm"),
        QStringLiteral("line_crossing / balanced")
    );
    QVERIFY(text(view, "processing_policy").contains("Manual policy"));
    QVERIFY(text(view, "processing_policy").contains("Repaint target: 24 fps"));
    QVERIFY(text(view, "processing_policy").contains("Analysis target: 8 fps"));
    QVERIFY(text(view, "processing_policy").contains("230400 px"));
    sample.application_active = false;
    update();
    QVERIFY(text(view, "processing_result").startsWith("Inactive"));
    QVERIFY(text(view, "processing_result").contains("Stale"));
    sample.processing_enabled = false;
    update();
    QVERIFY(
        text(view, "processing_result").startsWith("Processing unavailable")
    );
    sample.application_active = true;
    sample.processing_enabled = true;
    sample.result_age_ms = 0;
    sample.metrics.core_fps = std::numeric_limits<double>::quiet_NaN();
    sample.metrics.processed_width = 0;
    update();
    QVERIFY(text(view, "processing_result").contains("Rate pending"));
    QVERIFY(text(view, "processing_result").contains("Size unavailable"));
    sample = {};
    update();
    QCOMPARE(
        text(view, "processing_target"), QStringLiteral("No active stream")
    );
    QCOMPARE(text(view, "processing_result"), QStringLiteral("No sample"));
    QVERIFY(!text(view, "processing_summary").contains("tracks 2"));
    QCOMPARE(text(view, "processing_policy"), QStringLiteral("Unavailable"));
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
    QCOMPARE(
        text(view, "processing_target"), QStringLiteral("No active stream")
    );
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
    const auto labels = view.findChildren<QLabel*>();
    for (auto* label : labels) {
        QCOMPARE(label->textFormat(), Qt::PlainText);
        QVERIFY(label->wordWrap());
        QVERIFY(label->width() > 0 && label->height() > 0);
        scroll->ensureWidgetVisible(label);
        QTRY_VERIFY(scroll->viewport()->rect().intersects(
            QRect(label->mapTo(scroll->viewport(), QPoint()), label->size())
        ));
        const int x = label->mapTo(scroll->viewport(), QPoint()).x();
        QVERIFY(x >= 0);
        QVERIFY(x + label->width() <= scroll->viewport()->width());
        if (label->objectName().startsWith("processing_")) {
            QVERIFY(!label->accessibleName().isEmpty());
            label->setFocus(Qt::TabFocusReason);
            QTRY_COMPARE(QApplication::focusWidget(), label);
        }
    }
    auto* target = view.findChild<QLabel*>("processing_target");
    target->setFocus();
    QTest::keyClick(target, Qt::Key_Tab);
    QTRY_COMPARE(
        QApplication::focusWidget(),
        view.findChild<QLabel*>("processing_source")
    );
}
