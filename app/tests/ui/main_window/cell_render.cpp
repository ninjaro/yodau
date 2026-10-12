#include "shell/app_settings.hpp"
#include "ui/main_window/fixtures.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/stream_cell.hpp"

#include <QImage>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::cell_negative_auto_masks_content() {
    const QSize frame_size(240, 180);
    const QImage frame = main_window_tests_support::gradient_frame(frame_size);

    stream_cell::line_instance open_line;
    open_line.template_name = QStringLiteral("invert-line");
    open_line.color_mode_id = QStringLiteral("negative_auto");
    open_line.width_text = QStringLiteral("6");
    open_line.pts_pct = {
        QPointF(15.0, 25.0),
        QPointF(85.0, 25.0),
    };

    stream_cell::line_instance closed_line;
    closed_line.template_name = QStringLiteral("invert-area");
    closed_line.color_mode_id = QStringLiteral("negative_auto");
    closed_line.closed = true;
    closed_line.width_text = QStringLiteral("4");
    closed_line.pts_pct = {
        QPointF(25.0, 60.0),
        QPointF(75.0, 60.0),
        QPointF(75.0, 85.0),
        QPointF(25.0, 85.0),
    };

    const QImage idle_rendered = main_window_tests_support::render_frame(
        frame_size, frame, { open_line, closed_line }
    );
    const QImage rendered = main_window_tests_support::render_highlights(
        frame_size, frame, { open_line, closed_line },
        {
            {
                QStringLiteral("invert-line"),
                QPointF(50.0, 25.0),
            },
            {
                QStringLiteral("invert-area"),
                QPointF(50.0, 72.5),
            },
        }
    );

    const QPoint line_sample_a(60, 45);
    const QPoint line_sample_b(180, 45);
    const QColor idle_a = idle_rendered.pixelColor(line_sample_a);
    const QColor idle_b = idle_rendered.pixelColor(line_sample_b);
    const QColor rendered_a = rendered.pixelColor(line_sample_a);
    const QColor rendered_b = rendered.pixelColor(line_sample_b);

    QVERIFY(rendered_a != idle_a);
    QVERIFY(rendered_b != idle_b);

    const QPoint fill_sample(120, 130);
    const QPoint outside_sample(20, 130);
    const QColor source_fill = frame.pixelColor(fill_sample);
    QCOMPARE(idle_rendered.pixelColor(fill_sample), source_fill);
    const QColor rendered_fill = rendered.pixelColor(fill_sample);
    const QColor inverted_fill(
        255 - source_fill.red(), 255 - source_fill.green(),
        255 - source_fill.blue()
    );

    QVERIFY(
        main_window_tests_support::color_close(rendered_fill, inverted_fill)
    );
    QCOMPARE(
        rendered.pixelColor(outside_sample), frame.pixelColor(outside_sample)
    );
}

void main_window_tests::cell_status_badge_modes() {
    const stream_settings balanced_status {
        .stream_name = QStringLiteral("cam-status"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    const stream_settings debug_heavy_status {
        .stream_name = QStringLiteral("cam-status"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("contour_mask"),
        .algorithm_preset = QStringLiteral("mask_heavy"),
        .algorithm_overlay_enabled = true,
    };

    const QImage release_status = main_window_tests_support::render_status(
        balanced_status, app_log_mode::release
    );
    const QImage debug_status = main_window_tests_support::render_status(
        balanced_status, app_log_mode::debug
    );
    const QImage debug_heavy_algorithm_status
        = main_window_tests_support::render_status(
            debug_heavy_status, app_log_mode::debug
        );

    QVERIFY(
        main_window_tests_support::pixel_difference(
            release_status, debug_status
        )
        > 0
    );
    QVERIFY(
        main_window_tests_support::pixel_difference(
            debug_status, debug_heavy_algorithm_status
        )
        > 0
    );
}

void main_window_tests::cell_overlay_modes() {
    const stream_settings no_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("balanced"),
        .algorithm_overlay_enabled = false,
    };
    const stream_settings baseline_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("motion_baseline"),
        .algorithm_preset = QStringLiteral("debug"),
        .algorithm_overlay_enabled = true,
    };
    const stream_settings spot_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("spot_grid"),
        .algorithm_preset = QStringLiteral("dense"),
        .algorithm_overlay_enabled = true,
    };
    const stream_settings contour_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("contour_mask"),
        .algorithm_preset = QStringLiteral("mask_heavy"),
        .algorithm_overlay_enabled = true,
    };
    const stream_settings track_overlay {
        .stream_name = QStringLiteral("cam-render"),
        .labels_enabled = true,
        .algorithm_id = QStringLiteral("centroid_track"),
        .algorithm_preset = QStringLiteral("persistent"),
        .algorithm_overlay_enabled = true,
    };

    const QImage no_overlay_image
        = main_window_tests_support::render_events(no_overlay);
    const QImage baseline_overlay_image
        = main_window_tests_support::render_events(baseline_overlay);
    const QImage spot_overlay_image
        = main_window_tests_support::render_events(spot_overlay);
    const QImage contour_overlay_image
        = main_window_tests_support::render_events(contour_overlay);
    const QImage track_overlay_image
        = main_window_tests_support::render_events(track_overlay);

    QVERIFY(
        main_window_tests_support::pixel_difference(
            no_overlay_image, baseline_overlay_image
        )
        > 80
    );
    QVERIFY(
        main_window_tests_support::pixel_difference(
            baseline_overlay_image, spot_overlay_image
        )
        > 120
    );
    QVERIFY(
        main_window_tests_support::pixel_difference(
            spot_overlay_image, contour_overlay_image
        )
        > 120
    );
    QVERIFY(
        main_window_tests_support::pixel_difference(
            contour_overlay_image, track_overlay_image
        )
        > 120
    );
}

void main_window_tests::cell_wave_profiles() {
    stream_cell::line_instance dry_profile;
    dry_profile.template_name = QStringLiteral("line-a");
    dry_profile.color = QColor(QStringLiteral("#67c1ff"));
    dry_profile.width_text = QStringLiteral("thin");
    dry_profile.length_text = QStringLiteral("short");
    dry_profile.response_text = QStringLiteral("dry");
    dry_profile.pts_pct = {
        QPointF(15.0, 70.0),
        QPointF(50.0, 70.0),
        QPointF(85.0, 70.0),
    };

    stream_cell::line_instance resonant_profile = dry_profile;
    resonant_profile.width_text = QStringLiteral("string_heavy");
    resonant_profile.length_text = QStringLiteral("long");
    resonant_profile.response_text = QStringLiteral("resonant");

    const QImage dry_region
        = main_window_tests_support::render_wave(dry_profile);
    const QImage resonant_region
        = main_window_tests_support::render_wave(resonant_profile);

    QVERIFY(
        main_window_tests_support::pixel_difference(dry_region, resonant_region)
        > 500
    );
}

// NOLINTEND(readability-convert-member-functions-to-static)
