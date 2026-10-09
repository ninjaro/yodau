#include "shell/app_settings.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/settings_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::settings_normalize_algorithm_and_width() {
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("contour mask")),
        QStringLiteral("contour_mask")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("spots")),
        QStringLiteral("spot_grid")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("auto")),
        QStringLiteral("hybrid_auto")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QStringLiteral("tracking")),
        QStringLiteral("centroid_track")
    );
    QCOMPARE(
        normalized_app_algorithm_id(QString()),
        QStringLiteral("motion_baseline")
    );
    QCOMPARE(
        default_algorithm_preset_id(QStringLiteral("hybrid_auto")),
        QStringLiteral("adaptive")
    );
    QCOMPARE(
        normalized_algorithm_preset_id(
            QStringLiteral("hybrid_auto"), QStringLiteral("simple")
        ),
        QStringLiteral("load_guard")
    );
    QCOMPARE(
        normalized_algorithm_preset_id(
            QStringLiteral("contour_mask"), QStringLiteral("debug")
        ),
        QStringLiteral("mask_heavy")
    );
    QCOMPARE(
        default_algorithm_preset_id(QStringLiteral("spot_grid")),
        QStringLiteral("balanced")
    );
    QCOMPARE(
        normalized_algorithm_preset_id(
            QStringLiteral("centroid_track"), QStringLiteral("debug")
        ),
        QStringLiteral("persistent")
    );
    QVERIFY(algorithm_summary_text(
                QStringLiteral("spot_grid"), QStringLiteral("dense"),
                QStringLiteral("auto")
    )
                .contains(QStringLiteral("point-style motion regions")));
    QVERIFY(algorithm_summary_text(
                QStringLiteral("hybrid_auto"), QStringLiteral("adaptive"),
                QStringLiteral("auto")
    )
                .contains(QStringLiteral("adapts between low-cost, tripwire")));
    QVERIFY(algorithm_summary_text(
                QStringLiteral("centroid_track"), QStringLiteral("persistent"),
                QStringLiteral("auto")
    )
                .contains(QStringLiteral("short tracks")));
    QCOMPARE(
        algorithm_badge_text(
            QStringLiteral("hybrid_auto"), QStringLiteral("load_guard"), false
        ),
        QStringLiteral("HA simple")
    );
    QCOMPARE(
        algorithm_badge_text(
            QStringLiteral("centroid_track"), QStringLiteral("persistent"), true
        ),
        QStringLiteral("CT debug-heavy")
    );
    QCOMPARE(
        normalized_operator_profile_id(QStringLiteral("debug-heavy")),
        QStringLiteral("debug_heavy")
    );
    QCOMPARE(
        [] {
            stream_settings settings_value;
            settings_value.algorithm_id = QStringLiteral("spot_grid");
            return apply_operator_profile(
                       settings_value, QStringLiteral("simple")
            )
                .algorithm_preset;
        }(),
        QStringLiteral("coarse")
    );
    QCOMPARE(
        [] {
            stream_settings settings_value;
            settings_value.algorithm_id = QStringLiteral("contour_mask");
            settings_value.algorithm_preset = QStringLiteral("mask_heavy");
            settings_value.algorithm_overlay_enabled = true;
            return inferred_operator_profile_id(settings_value);
        }(),
        QStringLiteral("debug_heavy")
    );
    QVERIFY([] {
        stream_settings settings_value;
        settings_value.algorithm_id = QStringLiteral("contour_mask");
        settings_value.algorithm_preset = QStringLiteral("mask_heavy");
        settings_value.algorithm_overlay_enabled = true;
        return operator_profile_summary_text(settings_value);
    }()
                .contains(QStringLiteral("debug-heavy")));

    QCOMPARE(
        normalized_line_width_text(QStringLiteral("String Heavy")),
        QStringLiteral("string_heavy")
    );
    QCOMPARE(
        normalized_line_length_text(QStringLiteral("drone")),
        QStringLiteral("long")
    );
    QCOMPARE(
        normalized_line_response_text(QStringLiteral("ringing")),
        QStringLiteral("resonant")
    );
    QCOMPARE(
        normalized_line_color_mode_id(QStringLiteral("negative")),
        QStringLiteral("negative_auto")
    );
    QCOMPARE(
        normalized_line_color_mode_id(QStringLiteral("random")),
        QStringLiteral("manual")
    );
    QVERIFY(random_manual_line_color().isValid());
    QVERIFY(auto_palette_line_color(1, 4).isValid());
    QVERIFY(softened_negative_line_color(QColor(QStringLiteral("#203040")))
                .isValid());
    QCOMPARE(
        normalized_line_width_text(QStringLiteral("6")), QStringLiteral("6.0")
    );
    QCOMPARE(line_width_visual_value(QStringLiteral("thin")), 2.0);
    QCOMPARE(line_width_visual_value(QStringLiteral("string_heavy")), 6.5);
    QCOMPARE(
        line_profile_summary_text(
            QStringLiteral("String Heavy"), QStringLiteral("long"),
            QStringLiteral("ringing")
        ),
        QStringLiteral("width=string_heavy length=long response=resonant")
    );
}

void main_window_tests::settings_emit_log_mode() {
    settings_panel panel;

    QSignalSpy log_mode_spy(&panel, &settings_panel::log_mode_changed);
    auto* log_mode_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_log_mode_combo")
    );

    QVERIFY(log_mode_combo != nullptr);
    QCOMPARE(panel.log_mode(), app_log_mode::release);

    log_mode_combo->setCurrentIndex(1);

    QCOMPARE(log_mode_spy.count(), 1);
    QCOMPARE(
        log_mode_spy.takeFirst().at(0).value<app_log_mode>(),
        app_log_mode::debug
    );
    QCOMPARE(panel.log_mode(), app_log_mode::debug);

    log_mode_combo->setCurrentIndex(0);

    QCOMPARE(log_mode_spy.count(), 1);
    QCOMPARE(
        log_mode_spy.takeFirst().at(0).value<app_log_mode>(),
        app_log_mode::release
    );
    QCOMPARE(panel.log_mode(), app_log_mode::release);
}

void main_window_tests::settings_algorithm_presets() {
    settings_panel panel;
    panel.set_existing_names({ QStringLiteral("cam-1") });
    panel.set_active_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-1"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("motion_baseline"),
            .algorithm_preset = QStringLiteral("balanced"),
            .movement_display_mode = QStringLiteral("off"),
            .algorithm_overlay_enabled = false,
        }
    );

    QSignalSpy stream_settings_spy(
        &panel, &settings_panel::active_stream_settings_changed
    );

    auto* algorithm_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_algorithm_combo")
    );
    auto* operator_profile_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_operator_profile_combo")
    );
    auto* algorithm_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_algorithm_advanced_checkbox")
    );
    auto* preset_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_algorithm_preset_combo")
    );
    auto* movement_display_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_movement_display_combo")
    );
    auto* summary_label = panel.findChild<QLabel*>(
        QStringLiteral("settings_stream_editor_algorithm_summary_label")
    );

    QVERIFY(algorithm_combo != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(algorithm_advanced_checkbox != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(movement_display_combo != nullptr);
    QVERIFY(summary_label != nullptr);
    QVERIFY(!algorithm_advanced_checkbox->isChecked());
    QVERIFY(preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());

    const int contour_mask_index
        = algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    algorithm_combo->setCurrentIndex(contour_mask_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }

    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("balanced")
    );
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("custom")
    );
    QVERIFY(
        summary_label->text().contains(QStringLiteral("contours and masks"))
    );

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);
    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, true);
    }
    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("mask_heavy")
    );

    algorithm_advanced_checkbox->setChecked(true);
    QVERIFY(!preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());

    const int movement_off_index
        = movement_display_combo->findData(QStringLiteral("off"));
    QVERIFY(movement_off_index >= 0);
    movement_display_combo->setCurrentIndex(movement_off_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QCOMPARE(settings_value.algorithm_overlay_enabled, false);
    }
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("custom")
    );
}

// NOLINTEND(readability-convert-member-functions-to-static)
