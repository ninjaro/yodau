#include "shell/app_settings.hpp"
#include "shell/stream_catalog_state.hpp"
#include "shell/stream_catalog_workflow.hpp"
#include "shell/stream_controller.hpp"
#include "shell/stream_route_state.hpp"
#include "shell/stream_widget_bridge.hpp"
#include "streams/stream_manager.hpp"
#include "ui/main_window_tests.hpp"

#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QComboBox>
#include <QTreeWidget>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::catalog_settings_and_local_sources() {
    stream_catalog_state catalog_state;

    const stream_settings fallback_settings
        = catalog_state.settings_for(QStringLiteral(" cam-1 "));
    QCOMPARE(fallback_settings.stream_name, QStringLiteral("cam-1"));
    QVERIFY(fallback_settings.labels_enabled);
    QCOMPARE(fallback_settings.algorithm_id, QStringLiteral("motion_baseline"));
    QCOMPARE(fallback_settings.algorithm_preset, QStringLiteral("balanced"));
    QVERIFY(!fallback_settings.manual_processing_policy_enabled);
    QCOMPARE(
        fallback_settings.manual_display_fps, default_manual_display_fps()
    );
    QCOMPARE(fallback_settings.manual_core_fps, default_manual_core_fps());
    QCOMPARE(
        fallback_settings.manual_processing_pixels,
        default_manual_processing_pixels()
    );

    catalog_state.ensure_stream(QStringLiteral(" cam-1 "));
    const stream_settings ensured_settings
        = catalog_state.settings_for(QStringLiteral("cam-1"));
    QCOMPARE(ensured_settings.stream_name, QStringLiteral("cam-1"));
    QCOMPARE(
        catalog_state.algorithm_id_for(QStringLiteral("cam-1")),
        QStringLiteral("motion_baseline")
    );

    catalog_state.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral(" cam-1 "),
            .labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("debug"),
            .algorithm_overlay_enabled = true,
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 37,
            .manual_core_fps = 11,
            .manual_processing_pixels = 640 * 360,
        }
    );

    const stream_settings saved_settings
        = catalog_state.settings_for(QStringLiteral("cam-1"));
    QCOMPARE(saved_settings.stream_name, QStringLiteral("cam-1"));
    QVERIFY(!saved_settings.labels_enabled);
    QCOMPARE(saved_settings.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(saved_settings.algorithm_preset, QStringLiteral("mask_heavy"));
    QVERIFY(saved_settings.algorithm_overlay_enabled);
    QVERIFY(saved_settings.manual_processing_policy_enabled);
    QCOMPARE(saved_settings.manual_display_fps, 37);
    QCOMPARE(saved_settings.manual_core_fps, 11);
    QCOMPARE(saved_settings.manual_processing_pixels, 640 * 360);
}

void main_window_tests::catalog_seed_add_and_local_sources() {
    settings_panel panel;
    stream_board board;
    stream_widget_bridge widget_bridge(&board, &panel);
    stream_catalog_state catalog_state;
    yodau::core::stream_manager stream_mgr;
    stream_mgr.set_local_stream_detector([] {
        std::vector<yodau::core::stream> detected;
        detected.emplace_back(
            "/tmp/fake-rear-camera", "Rear camera", "local", false
        );
        return detected;
    });
    stream_catalog_workflow workflow(
        &stream_mgr, &panel, catalog_state, widget_bridge
    );

    stream_mgr.add_stream("/tmp/cam-1.mp4", "cam-1", "file", true);
    workflow.seed_from_core();

    auto* streams_list = panel.findChild<QTreeWidget*>(
        QStringLiteral("settings_streams_list")
    );
    QVERIFY(streams_list != nullptr);
    auto stream_row_for_name = [streams_list](const QString& name) -> int {
        for (int row = 0; row < streams_list->topLevelItemCount(); row += 1) {
            const auto* item = streams_list->topLevelItem(row);
            if (item != nullptr && item->text(1) == name) {
                return row;
            }
        }
        return -1;
    };

    const int initial_count = streams_list->topLevelItemCount();
    QVERIFY(initial_count >= 1);
    QVERIFY(stream_row_for_name(QStringLiteral("cam-1")) >= 0);
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-1")).stream_name,
        QStringLiteral("cam-1")
    );

    workflow.seed_from_core();
    QCOMPARE(streams_list->topLevelItemCount(), initial_count);

    const auto add_result = workflow.add_stream(
        QStringLiteral("/tmp/cam-2.mp4"), QStringLiteral("cam-2"),
        QStringLiteral("file"), true
    );
    QCOMPARE(add_result.entries.size(), 1);
    QCOMPARE(
        add_result.entries.front().message, QStringLiteral("stream added")
    );
    QCOMPARE(add_result.entries.front().stream_name, QStringLiteral("cam-2"));
    QCOMPARE(add_result.added_stream, QStringLiteral("cam-2"));
    QCOMPARE(
        add_result.entries.front().detail, QStringLiteral("file:/tmp/cam-2.mp4")
    );
    QVERIFY(add_result.refresh_fps);
    QCOMPARE(streams_list->topLevelItemCount(), initial_count + 1);
    QVERIFY(stream_row_for_name(QStringLiteral("cam-2")) >= 0);
    QCOMPARE(
        catalog_state.settings_for(QStringLiteral("cam-2")).stream_name,
        QStringLiteral("cam-2")
    );

    const auto invalid_url_result = workflow.add_stream(
        QStringLiteral("not-a-url"), QStringLiteral("cam-url"),
        QStringLiteral("url"), true
    );
    QCOMPARE(invalid_url_result.entries.size(), 1);
    QCOMPARE(
        invalid_url_result.entries.front().message,
        QStringLiteral("invalid url")
    );
    QVERIFY(!invalid_url_result.refresh_fps);
    QCOMPARE(streams_list->topLevelItemCount(), initial_count + 1);

    const auto local_result = workflow.detect_local_sources();
    QCOMPARE(local_result.entries.size(), 1);
    QCOMPARE(
        local_result.entries.front().message,
        QStringLiteral("local source inventory refreshed")
    );
    QVERIFY(
        local_result.entries.front().detail.contains(QStringLiteral("core="))
    );
    QVERIFY(
        local_result.entries.front().detail.contains(QStringLiteral("qt="))
    );
    QVERIFY(!local_result.refresh_fps);

    auto* local_sources_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_add_local_sources_combo")
    );
    QVERIFY(local_sources_combo != nullptr);
    QVERIFY(
        local_sources_combo->findData(QStringLiteral("/tmp/fake-rear-camera"))
        >= 0
    );
}

void main_window_tests::controller_adds_and_focuses_sources() {
    yodau::core::stream_manager manager;
    settings_panel settings;
    stream_board board;
    stream_controller controller(&manager, &settings, &board);

    controller.handle_add_file(
        QStringLiteral("/tmp/mobile-file.mp4"), QStringLiteral("mobile-file"),
        true
    );
    controller.focus_stream(QStringLiteral("mobile-file"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("mobile-file"));

    controller.handle_add_local(
        QStringLiteral("opaque-camera-id"), QStringLiteral("mobile-camera")
    );
    controller.focus_stream(QStringLiteral("mobile-camera"));
    QVERIFY(board.active_cell() != nullptr);
    QCOMPARE(board.active_cell()->get_name(), QStringLiteral("mobile-camera"));
    QVERIFY(board.grid_mode()->has_stream(QStringLiteral("mobile-file")));

    auto* streams_list = settings.findChild<QTreeWidget*>(
        QStringLiteral("settings_streams_list")
    );
    QVERIFY(streams_list != nullptr);
    int checked_rows = 0;
    for (int row = 0; row < streams_list->topLevelItemCount(); row += 1) {
        if (streams_list->topLevelItem(row)->checkState(0) == Qt::Checked) {
            checked_rows += 1;
        }
    }
    QCOMPARE(checked_rows, 2);
}

void main_window_tests::route_selection_and_add_validation() {
    stream_route_state route_state;

    QVERIFY(!route_state.has_active_stream());
    QCOMPARE(route_state.active_stream_name(), QString());

    route_state.set_active_stream(QStringLiteral(" cam-1 "));
    QVERIFY(route_state.has_active_stream());
    QCOMPARE(route_state.active_stream_name(), QStringLiteral("cam-1"));
    QVERIFY(route_state.is_active_stream(QStringLiteral("cam-1")));
    QCOMPARE(
        route_state.next_active_stream_for_enlarge(QStringLiteral("cam-1")),
        QString()
    );
    QCOMPARE(
        route_state.next_active_stream_for_enlarge(QStringLiteral("cam-2")),
        QStringLiteral("cam-2")
    );

    QVERIFY(!route_state.hide_stream(QStringLiteral("cam-2")));
    QVERIFY(route_state.has_active_stream());
    QVERIFY(route_state.hide_stream(QStringLiteral("cam-1")));
    QVERIFY(!route_state.has_active_stream());

    const auto valid_url = stream_route_state::validate_add_source(
        QStringLiteral("https://example.test/live"), QStringLiteral("url")
    );
    QVERIFY(valid_url.valid);
    QVERIFY(valid_url.message.isEmpty());

    const auto invalid_url = stream_route_state::validate_add_source(
        QStringLiteral("camera-feed"), QStringLiteral("url")
    );
    QVERIFY(!invalid_url.valid);
    QCOMPARE(invalid_url.message, QStringLiteral("invalid url"));
    QCOMPARE(invalid_url.detail, QStringLiteral("camera-feed"));

    const auto invalid_scheme = stream_route_state::validate_add_source(
        QStringLiteral("ftp://example.test/live"), QStringLiteral("url")
    );
    QVERIFY(!invalid_scheme.valid);
    QCOMPARE(invalid_scheme.message, QStringLiteral("unsupported url scheme"));
    QCOMPARE(invalid_scheme.detail, QStringLiteral("ftp"));

    const auto local_source = stream_route_state::validate_add_source(
        QStringLiteral("/tmp/cam.mp4"), QStringLiteral("file")
    );
    QVERIFY(local_source.valid);

    QCOMPARE(
        stream_route_state::source_description(
            QStringLiteral("rtsp://cam"), QStringLiteral("url")
        ),
        QStringLiteral("url:rtsp://cam")
    );
}

// NOLINTEND(readability-convert-member-functions-to-static)
