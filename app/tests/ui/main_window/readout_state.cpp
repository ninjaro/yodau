#include "ui/main_window_tests.hpp"

#include "shell/stream_controller.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QTemporaryDir>
#include <QtTest/QtTest>

void main_window_tests::processing_readout_observations() {
    yodau::core::stream_manager manager;
    manager.add_stream("", "one", "file", false);
    manager.add_stream("", "two", "file", false);
    settings_panel settings;
    stream_board board;
    stream_controller controller(&manager, &settings, &board);
    QVERIFY(controller.active_processing_sample().stream_name.isEmpty());
    controller.handle_show_stream_changed("one", true);
    controller.handle_show_stream_changed("two", true);
    controller.focus_stream("one");
    auto* cell = board.active_cell();
    QVERIFY(cell != nullptr);
    auto sample = controller.active_processing_sample();
    QCOMPARE(sample.stream_name, QStringLiteral("one"));
    QCOMPARE(sample.source_status, QStringLiteral("No source"));
    QCOMPARE(sample.input_age_ms, qint64(-1));
    QCOMPARE(sample.result_age_ms, qint64(-1));
    QVERIFY(sample.application_active);
#ifdef YODAU_OPENCV
    QVERIFY(sample.processing_enabled);
#endif
    QVERIFY(sample.metrics.effective_display_fps > 0);
    QVERIFY(!sample.algorithm_id.isEmpty() && !sample.preset_id.isEmpty());
    auto policy = cell->current_stream_settings();
    policy.manual_processing_policy_enabled = true;
    policy.manual_processing_pixels = 1920 * 1080;
    settings.active_stream_settings_changed(policy);
    const auto original_settings = cell->current_stream_settings();

    QImage first(64, 48, QImage::Format_RGB888);
    first.fill(Qt::black);
    controller.on_gui_frame("one", first);
    sample = controller.active_processing_sample();
    QVERIFY(sample.input_age_ms >= 0);
    QCOMPARE(sample.metrics.input_width, 64);
    QCOMPARE(sample.metrics.input_height, 48);
    QCOMPARE(sample.metrics.input_fps, 0.0);
    QCOMPARE(sample.result_age_ms, qint64(-1));
    QCOMPARE(sample.metrics.processed_width, 0);
    if (sample.processing_enabled) {
        QTRY_VERIFY_WITH_TIMEOUT(
            controller.active_processing_sample().result_age_ms >= 0, 2000
        );
        sample = controller.active_processing_sample();
        QCOMPARE(sample.metrics.processed_width, 64);
        QCOMPARE(sample.metrics.processed_height, 48);
        QVERIFY(!sample.metrics.processing_summary.isEmpty());
    }

    QImage second(128, 64, QImage::Format_RGB888);
    second.fill(Qt::white);
    controller.on_gui_frame("one", second);
    sample = controller.active_processing_sample();
    QCOMPARE(sample.metrics.input_width, 128);
    QCOMPARE(
        sample.metrics.processed_width, sample.processing_enabled ? 64 : 0
    );
    // Querying is read-only: it must not substitute prepared-input dimensions
    // for the last delivered result or mutate stream policy/configuration.
    for (int i = 0; i < 10; ++i) {
        QCOMPARE(
            controller.active_processing_sample().metrics.processed_width,
            sample.metrics.processed_width
        );
    }
    const auto unchanged = cell->current_stream_settings();
    QCOMPARE(unchanged.algorithm_id, original_settings.algorithm_id);
    QCOMPARE(unchanged.algorithm_preset, original_settings.algorithm_preset);
    QCOMPARE(
        unchanged.manual_processing_policy_enabled,
        original_settings.manual_processing_policy_enabled
    );
    QCOMPARE(
        unchanged.manual_processing_pixels,
        original_settings.manual_processing_pixels
    );
    if (sample.processing_enabled) {
        QTRY_COMPARE_WITH_TIMEOUT(
            controller.active_processing_sample().metrics.processed_width, 128,
            2000
        );
        QCOMPARE(
            controller.active_processing_sample().metrics.processed_height, 64
        );
    }
    board.set_application_active(false);
    sample = controller.active_processing_sample();
    QVERIFY(!sample.application_active);
    QVERIFY(sample.source_status.startsWith("Inactive"));
    board.set_application_active(true);
    controller.focus_stream("two");
    sample = controller.active_processing_sample();
    QCOMPARE(sample.stream_name, QStringLiteral("two"));
    QCOMPARE(sample.input_age_ms, qint64(-1));
    QCOMPARE(sample.result_age_ms, qint64(-1));
    QVERIFY(sample.metrics.processing_summary.isEmpty());
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    auto* second_cell = board.active_cell();
    second_cell->set_source(
        QUrl::fromLocalFile(directory.filePath("missing.mp4"))
    );
    QTRY_VERIFY(
        controller.active_processing_sample().source_status.startsWith(
            "Source error"
        )
        || controller.active_processing_sample().source_status
            == "Invalid media"
    );
    second_cell->set_source({});
    QCOMPARE(
        controller.active_processing_sample().source_status,
        QStringLiteral("No source")
    );
    board.grid_mode()->add_stream("orphan");
    board.set_active_stream("orphan");
    QVERIFY(controller.active_processing_sample().stream_name.isEmpty());
    controller.return_to_stream_grid();
    QVERIFY(controller.active_processing_sample().stream_name.isEmpty());
}
