#include "shell/app_settings.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/stream_inventory_panel.hpp"
#include "widgets/stream_source_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTreeWidget>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::inventory_entries_and_visibility() {
    stream_inventory_panel panel;
    QSignalSpy show_spy(&panel, &stream_inventory_panel::show_stream_changed);

    auto* summary_label = panel.findChild<QLabel*>(
        QStringLiteral("settings_streams_summary_label")
    );
    QVERIFY(summary_label != nullptr);
    QVERIFY(
        summary_label->text().contains(QStringLiteral("No configured streams"))
    );

    panel.add_stream_entry(
        QStringLiteral("cam-1"), QStringLiteral("file:/tmp/demo.mp4")
    );
    panel.add_stream_entry(
        QStringLiteral("cam-2"), QStringLiteral("url:rtsp://cam-2"), true
    );

    auto* streams_list = panel.findChild<QTreeWidget*>(
        QStringLiteral("settings_streams_list")
    );
    QVERIFY(streams_list != nullptr);
    QCOMPARE(streams_list->topLevelItemCount(), 2);
    QCOMPARE(streams_list->topLevelItem(0)->text(1), QStringLiteral("cam-1"));
    QCOMPARE(streams_list->topLevelItem(1)->checkState(0), Qt::Checked);
    QVERIFY(summary_label->text().contains(QStringLiteral("2 configured")));
    QVERIFY(summary_label->text().contains(QStringLiteral("1 shown in grid")));

    streams_list->topLevelItem(0)->setCheckState(0, Qt::Checked);
    QCOMPARE(show_spy.size(), 1);
    QCOMPARE(show_spy.at(0).at(0).toString(), QStringLiteral("cam-1"));
    QCOMPARE(show_spy.at(0).at(1).toBool(), true);
    QVERIFY(summary_label->text().contains(QStringLiteral("2 shown in grid")));

    panel.remove_stream_entry(QStringLiteral("cam-2"));
    QCOMPARE(streams_list->topLevelItemCount(), 1);
    QVERIFY(summary_label->text().contains(QStringLiteral("1 configured")));

    panel.clear_stream_entries();
    QCOMPARE(streams_list->topLevelItemCount(), 0);
    QVERIFY(
        summary_label->text().contains(QStringLiteral("No configured streams"))
    );
}

void main_window_tests::source_modes_validation_and_requests() {
    qRegisterMetaType<app_log_entry>("app_log_entry");

    stream_source_panel panel;
    panel.set_existing_names({ QStringLiteral("dup") });
    panel.set_local_sources(
        QList<local_source_descriptor> {
            local_source_descriptor {
                .id = QStringLiteral("opaque-camera-id"),
                .display_name = QStringLiteral("Rear camera"),
            },
        }
    );

    auto* name_edit
        = panel.findChild<QLineEdit*>(QStringLiteral("settings_add_name_edit"));
    auto* local_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_add_local_radio")
    );
    auto* url_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_add_url_radio")
    );
    auto* local_sources_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_add_local_sources_combo")
    );
    auto* refresh_local_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_add_refresh_local_button")
    );
    auto* url_edit
        = panel.findChild<QLineEdit*>(QStringLiteral("settings_add_url_edit"));
    auto* add_button
        = panel.findChild<QPushButton*>(QStringLiteral("settings_add_button"));
    auto* summary_label = panel.findChild<QLabel*>(
        QStringLiteral("settings_add_summary_label")
    );

    QVERIFY(name_edit != nullptr);
    QVERIFY(local_radio != nullptr);
    QVERIFY(url_radio != nullptr);
    QVERIFY(local_sources_combo != nullptr);
    QVERIFY(refresh_local_button != nullptr);
    QVERIFY(url_edit != nullptr);
    QVERIFY(add_button != nullptr);
    QVERIFY(summary_label != nullptr);

    QSignalSpy add_local_spy(&panel, &stream_source_panel::add_local_stream);
    QSignalSpy add_url_spy(&panel, &stream_source_panel::add_url_stream);
    QSignalSpy refresh_spy(
        &panel, &stream_source_panel::detect_local_sources_requested
    );
    QSignalSpy log_spy(&panel, &stream_source_panel::log_requested);

    local_radio->click();
    name_edit->setText(QStringLiteral("dup"));
    QVERIFY(!add_button->isEnabled());
    QVERIFY(summary_label->text().contains(
        QStringLiteral("local source Rear camera")
    ));

    name_edit->setText(QStringLiteral(" cam-1 "));
    QCOMPARE(local_sources_combo->currentText(), QStringLiteral("Rear camera"));
    QCOMPARE(
        local_sources_combo->currentData().toString(),
        QStringLiteral("opaque-camera-id")
    );
    QVERIFY(add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("ready")));
    QVERIFY(summary_label->text().contains(QStringLiteral("cam-1")));

    add_button->click();
    QCOMPARE(add_local_spy.size(), 1);
    QCOMPARE(
        add_local_spy.at(0).at(0).toString(), QStringLiteral("opaque-camera-id")
    );
    QCOMPARE(add_local_spy.at(0).at(1).toString(), QStringLiteral("cam-1"));
    QVERIFY(!log_spy.empty());
    QCOMPARE(
        qvariant_cast<app_log_entry>(log_spy.at(0).at(0)).message,
        QStringLiteral("requested local stream add")
    );

    refresh_local_button->click();
    QCOMPARE(refresh_spy.size(), 1);
    QVERIFY(log_spy.size() >= 2);
    QCOMPARE(
        qvariant_cast<app_log_entry>(log_spy.at(1).at(0)).message,
        QStringLiteral("local source detection requested")
    );

    url_radio->click();
    QVERIFY(!add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("URL mode waits")));

    url_edit->setText(QStringLiteral("rtsp://example.test/live"));
    QVERIFY(add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("url")));
    QVERIFY(summary_label->text().contains(
        QStringLiteral("rtsp://example.test/live")
    ));
    add_button->click();
    QCOMPARE(add_url_spy.size(), 1);
    QCOMPARE(
        add_url_spy.at(0).at(0).toString(),
        QStringLiteral("rtsp://example.test/live")
    );
    QCOMPARE(add_url_spy.at(0).at(1).toString(), QStringLiteral("cam-1"));
    QVERIFY(log_spy.size() >= 3);
    QCOMPARE(
        qvariant_cast<app_log_entry>(log_spy.at(2).at(0)).message,
        QStringLiteral("requested url stream add")
    );

    panel.clear_inputs();
    QCOMPARE(name_edit->text(), QString());
    QCOMPARE(url_edit->text(), QString());
    QVERIFY(!add_button->isEnabled());
    QVERIFY(summary_label->text().contains(QStringLiteral("URL mode waits")));
}

void main_window_tests::stream_panel_selection_modes_and_settings() {
    active_stream_panel panel(
        active_stream_panel::panel_mode::stream_settings,
        QStringLiteral("settings_active")
    );
    panel.set_active_candidates(
        { QStringLiteral("cam-1"), QStringLiteral("cam-2") }
    );

    QSignalSpy stream_selected_spy(
        &panel, &active_stream_panel::stream_selected
    );
    QSignalSpy stream_settings_spy(
        &panel, &active_stream_panel::stream_settings_changed
    );

    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_stream_combo")
    );
    auto* active_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_labels_checkbox")
    );
    auto* standard_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_standard_labels_checkbox")
    );
    auto* operator_profile_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_operator_profile_combo")
    );
    auto* operator_profile_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_operator_profile_summary_label")
    );
    auto* algorithm_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_algorithm_combo")
    );
    auto* preset_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_algorithm_preset_combo")
    );
    auto* algorithm_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_algorithm_advanced_checkbox")
    );
    auto* movement_display_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_movement_display_combo")
    );
    auto* manual_processing_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_manual_processing_checkbox")
    );
    auto* processing_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_active_processing_advanced_checkbox")
    );
    auto* manual_display_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_display_fps_spin")
    );
    auto* manual_core_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_core_fps_spin")
    );
    auto* manual_processing_pixels_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_active_processing_pixels_spin")
    );
    auto* template_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_active_mode_template_radio")
    );

    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(active_labels_checkbox != nullptr);
    QVERIFY(standard_labels_checkbox != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(operator_profile_summary != nullptr);
    QVERIFY(algorithm_combo != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(algorithm_advanced_checkbox != nullptr);
    QVERIFY(movement_display_combo != nullptr);
    QVERIFY(processing_advanced_checkbox != nullptr);
    QVERIFY(manual_processing_checkbox != nullptr);
    QVERIFY(manual_display_fps_spin != nullptr);
    QVERIFY(manual_core_fps_spin != nullptr);
    QVERIFY(manual_processing_pixels_spin != nullptr);
    QVERIFY(template_radio != nullptr);
    QVERIFY(!template_radio->isVisible());

    QVERIFY(!panel.has_active_stream());
    QVERIFY(panel.drawing_new_mode());

    active_stream_combo->setCurrentText(QStringLiteral("cam-1"));
    QCOMPARE(stream_selected_spy.size(), 1);
    QCOMPARE(
        stream_selected_spy.at(0).at(0).toString(), QStringLiteral("cam-1")
    );
    QCOMPARE(stream_settings_spy.size(), 0);
    {
        const stream_settings settings_value = panel.current_stream_settings();
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.labels_enabled, true);
        QCOMPARE(settings_value.standard_labels_enabled, true);
        QCOMPARE(
            settings_value.algorithm_id, QStringLiteral("motion_baseline")
        );
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
        QVERIFY(!settings_value.manual_processing_policy_enabled);
        QCOMPARE(
            settings_value.manual_display_fps, default_manual_display_fps()
        );
        QCOMPARE(settings_value.manual_core_fps, default_manual_core_fps());
        QCOMPARE(
            settings_value.manual_processing_pixels,
            default_manual_processing_pixels()
        );
    }
    QVERIFY(panel.has_active_stream());
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("balanced")
    );
    QVERIFY(
        operator_profile_summary->text().contains(QStringLiteral("balanced"))
    );
    QVERIFY(!algorithm_advanced_checkbox->isChecked());
    QVERIFY(preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());
    QVERIFY(!processing_advanced_checkbox->isChecked());
    QVERIFY(manual_processing_checkbox->isHidden());
    QVERIFY(manual_display_fps_spin->isHidden());
    QVERIFY(manual_core_fps_spin->isHidden());
    QVERIFY(manual_processing_pixels_spin->isHidden());

    const int contour_mask_index
        = algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    algorithm_combo->setCurrentIndex(contour_mask_index);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
    }
    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("balanced")
    );
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("balanced")
    );

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
    }
    QCOMPARE(
        preset_combo->currentData().toString(), QStringLiteral("mask_heavy")
    );
    QVERIFY(
        operator_profile_summary->text().contains(QStringLiteral("debug-heavy"))
    );

    algorithm_advanced_checkbox->setChecked(true);
    QVERIFY(!preset_combo->isHidden());
    QVERIFY(!movement_display_combo->isHidden());

    const int movement_off_index
        = movement_display_combo->findData(QStringLiteral("off"));
    QVERIFY(movement_off_index >= 0);
    movement_display_combo->setCurrentIndex(movement_off_index);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(!settings_value.algorithm_overlay_enabled);
    }
    QCOMPARE(
        operator_profile_combo->currentData().toString(),
        QStringLiteral("custom")
    );

    active_labels_checkbox->setChecked(false);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QVERIFY(!settings_value.labels_enabled);
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
    }

    QVERIFY(active_labels_checkbox->toolTip().contains(
        QStringLiteral("saved line names")
    ));
    QVERIFY(standard_labels_checkbox->toolTip().contains(
        QStringLiteral("algorithm badges")
    ));

    standard_labels_checkbox->setChecked(false);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QVERIFY(!settings_value.standard_labels_enabled);
    }

    processing_advanced_checkbox->setChecked(true);
    QVERIFY(!manual_processing_checkbox->isHidden());
    QVERIFY(!manual_display_fps_spin->isHidden());
    QVERIFY(!manual_core_fps_spin->isHidden());
    QVERIFY(!manual_processing_pixels_spin->isHidden());

    manual_processing_checkbox->setChecked(true);
    QCOMPARE(stream_settings_spy.size(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(
            settings_value.manual_display_fps, default_manual_display_fps()
        );
    }

    manual_display_fps_spin->setValue(18);
    manual_core_fps_spin->setValue(9);
    manual_processing_pixels_spin->setValue(320 * 180);
    QCOMPARE(stream_settings_spy.size(), 3);
    {
        const auto args = stream_settings_spy.takeLast();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.manual_display_fps, 18);
        QCOMPARE(settings_value.manual_core_fps, 9);
        QCOMPARE(settings_value.manual_processing_pixels, 320 * 180);
    }
    stream_settings_spy.clear();

    panel.set_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = true,
            .algorithm_id = QStringLiteral("spot grid"),
            .algorithm_preset = QStringLiteral("dense"),
            .movement_display_mode = QStringLiteral("off"),
            .algorithm_overlay_enabled = false,
        }
    );
    const stream_settings current_settings = panel.current_stream_settings();
    QCOMPARE(current_settings.stream_name, QStringLiteral("cam-2"));
    QCOMPARE(current_settings.labels_enabled, true);
    QCOMPARE(current_settings.algorithm_id, QStringLiteral("spot_grid"));
    QCOMPARE(current_settings.algorithm_preset, QStringLiteral("dense"));
    QCOMPARE(current_settings.algorithm_overlay_enabled, false);
}

// NOLINTEND(readability-convert-member-functions-to-static)
