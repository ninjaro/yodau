#include "shell/app_settings.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/settings_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::settings_round_trip() {
    settings_panel panel;
    panel.set_existing_names(
        { QStringLiteral("cam-1"), QStringLiteral("cam-2") }
    );
    panel.set_template_candidates(
        { QStringLiteral("north"), QStringLiteral("south") }
    );

    panel.set_active_stream_settings(
        stream_settings {
            .stream_name = QStringLiteral("cam-2"),
            .labels_enabled = false,
            .standard_labels_enabled = false,
            .algorithm_id = QStringLiteral("contour mask"),
            .algorithm_preset = QStringLiteral("mask_heavy"),
            .algorithm_overlay_enabled = true,
            .manual_processing_policy_enabled = true,
            .manual_display_fps = 19,
            .manual_core_fps = 8,
            .manual_processing_pixels = 640 * 360,
        }
    );

    const stream_settings active_stream
        = panel.current_active_stream_settings();
    QCOMPARE(active_stream.stream_name, QStringLiteral("cam-2"));
    QCOMPARE(active_stream.labels_enabled, false);
    QCOMPARE(active_stream.standard_labels_enabled, false);
    QCOMPARE(active_stream.algorithm_id, QStringLiteral("contour_mask"));
    QCOMPARE(active_stream.algorithm_preset, QStringLiteral("mask_heavy"));
    QCOMPARE(active_stream.algorithm_overlay_enabled, true);
    QVERIFY(active_stream.manual_processing_policy_enabled);
    QCOMPARE(active_stream.manual_display_fps, 19);
    QCOMPARE(active_stream.manual_core_fps, 8);
    QCOMPARE(active_stream.manual_processing_pixels, 640 * 360);

    panel.set_active_line_profile(
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::green),
            .color_mode_id = QStringLiteral("manual"),
            .closed = true,
            .width_text = QStringLiteral("String Heavy"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );

    const line_profile active_line = panel.current_active_line_profile();
    QCOMPARE(active_line.name, QStringLiteral("north"));
    QCOMPARE(active_line.color, QColor(Qt::green));
    QCOMPARE(active_line.color_mode_id, QStringLiteral("manual"));
    QCOMPARE(active_line.closed, true);
    QCOMPARE(active_line.width_text, QStringLiteral("string_heavy"));
    QCOMPARE(active_line.length_text, QStringLiteral("long"));
    QCOMPARE(active_line.response_text, QStringLiteral("resonant"));

    panel.set_active_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("south"),
            .color = QColor(Qt::yellow),
            .color_mode_id = QStringLiteral("negative"),
            .width_text = QStringLiteral("thin"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    const template_apply_settings active_template
        = panel.current_active_template_settings();
    QCOMPARE(active_template.template_name, QStringLiteral("south"));
    QCOMPARE(active_template.color, QColor(Qt::yellow));
    QCOMPARE(active_template.color_mode_id, QStringLiteral("negative_auto"));
    QCOMPARE(active_template.width_text, QStringLiteral("thin"));
    QCOMPARE(active_template.length_text, QStringLiteral("short"));
    QCOMPARE(active_template.response_text, QStringLiteral("dry"));
}

void main_window_tests::settings_edit_panels() {
    settings_panel panel;
    panel.set_existing_names({ QStringLiteral("cam-1") });
    panel.set_template_candidates({ QStringLiteral("north") });
    stream_settings initial_settings;
    initial_settings.stream_name = QStringLiteral("cam-1");
    panel.set_active_stream_settings(initial_settings);

    auto* settings_tabs
        = panel.findChild<QTabWidget*>(QStringLiteral("settings_tabs"));
    QVERIFY(settings_tabs != nullptr);
    QCOMPARE(settings_tabs->count(), 2);
    QCOMPARE(settings_tabs->tabText(0), QStringLiteral("streams"));
    QCOMPARE(settings_tabs->tabText(1), QStringLiteral("stream settings"));
    QVERIFY(
        settings_tabs->widget(0)->findChild<QLineEdit*>(
            QStringLiteral("settings_add_name_edit")
        )
        != nullptr
    );
    QVERIFY(
        settings_tabs->widget(0)->findChild<QTreeWidget*>(
            QStringLiteral("settings_streams_list")
        )
        != nullptr
    );

    auto* line_editor = panel.take_active_editor_widget();
    QVERIFY(line_editor != nullptr);
    auto* line_editor_panel = qobject_cast<active_editor_panel*>(line_editor);
    QVERIFY(line_editor_panel != nullptr);
    line_editor_panel->set_active_candidates({ QStringLiteral("cam-1") });
    line_editor_panel->set_active_current(QStringLiteral("cam-1"));

    auto* line_panel = line_editor_panel->findChild<QGroupBox*>(
        QStringLiteral("settings_active_line_profile_panel")
    );
    auto* template_panel = line_editor_panel->findChild<QGroupBox*>(
        QStringLiteral("settings_active_template_apply_panel")
    );
    auto* line_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_line_summary_label")
    );
    auto* line_color_mode_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("active_line_color_mode_combo")
    );
    auto* line_color_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("settings_active_line_color_button")
    );
    auto* line_random_color_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("active_line_random_color_button")
    );
    auto* line_advanced_checkbox = line_editor_panel->findChild<QCheckBox*>(
        QStringLiteral("settings_active_line_advanced_checkbox")
    );
    auto* line_parameter_mode_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("active_line_parameter_mode_combo")
    );
    auto* line_width_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_line_width_combo")
    );
    auto* line_length_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_line_length_combo")
    );
    auto* line_response_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_line_response_combo")
    );
    auto* template_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_template_summary_label")
    );
    auto* template_color_mode_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("active_template_color_mode_combo")
    );
    auto* template_color_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("settings_active_template_color_button")
    );
    auto* template_random_color_button
        = line_editor_panel->findChild<QPushButton*>(
            QStringLiteral("active_template_random_color_button")
        );
    auto* template_advanced_checkbox = line_editor_panel->findChild<QCheckBox*>(
        QStringLiteral("settings_active_template_advanced_checkbox")
    );
    auto* template_parameter_mode_combo
        = line_editor_panel->findChild<QComboBox*>(
            QStringLiteral("active_template_parameter_mode_combo")
        );
    auto* template_width_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_template_width_combo")
    );
    auto* template_length_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_template_length_combo")
    );
    auto* template_response_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_template_response_combo")
    );
    auto* active_status_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_status_summary_label")
    );
    auto* line_list_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_lines_summary_label")
    );
    auto* line_edit_summary = line_editor_panel->findChild<QLabel*>(
        QStringLiteral("settings_active_edit_summary_label")
    );
    auto* line_edit_combo = line_editor_panel->findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );
    auto* line_edit_points_table = line_editor_panel->findChild<QTableWidget*>(
        QStringLiteral("settings_active_edit_points_table")
    );
    auto* line_edit_name_edit = line_editor_panel->findChild<QLineEdit*>(
        QStringLiteral("active_edit_new_name_edit")
    );
    auto* line_edit_save_button = line_editor_panel->findChild<QPushButton*>(
        QStringLiteral("settings_active_edit_save_button")
    );
    auto* editor_tabs = line_editor_panel->findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );

    QVERIFY(line_panel != nullptr);
    QVERIFY(template_panel != nullptr);
    QVERIFY(line_summary != nullptr);
    QVERIFY(line_color_mode_combo != nullptr);
    QVERIFY(line_color_button != nullptr);
    QVERIFY(line_random_color_button != nullptr);
    QVERIFY(line_advanced_checkbox != nullptr);
    QVERIFY(line_parameter_mode_combo != nullptr);
    QVERIFY(line_width_combo != nullptr);
    QVERIFY(line_length_combo != nullptr);
    QVERIFY(line_response_combo != nullptr);
    QVERIFY(template_summary != nullptr);
    QVERIFY(template_color_mode_combo != nullptr);
    QVERIFY(template_color_button != nullptr);
    QVERIFY(template_random_color_button != nullptr);
    QVERIFY(template_advanced_checkbox != nullptr);
    QVERIFY(template_parameter_mode_combo != nullptr);
    QVERIFY(template_width_combo != nullptr);
    QVERIFY(template_length_combo != nullptr);
    QVERIFY(template_response_combo != nullptr);
    QVERIFY(active_status_summary != nullptr);
    QVERIFY(line_list_summary != nullptr);
    QVERIFY(line_edit_summary != nullptr);
    QVERIFY(line_edit_combo != nullptr);
    QVERIFY(line_edit_points_table != nullptr);
    QVERIFY(line_edit_name_edit != nullptr);
    QVERIFY(line_edit_save_button != nullptr);
    QVERIFY(editor_tabs != nullptr);
    QCOMPARE(editor_tabs->count(), 3);
    QCOMPARE(editor_tabs->tabText(2), QStringLiteral("edit"));
    QVERIFY(active_status_summary->text().contains(QStringLiteral("cam-1")));
    QVERIFY(line_list_summary->text().contains(
        QStringLiteral("No saved lines on this stream yet")
    ));
    QVERIFY(line_edit_summary->text().contains(
        QStringLiteral("Select an enabled line")
    ));
    QVERIFY(
        editor_tabs->widget(0)->findChild<QRadioButton*>(
            QStringLiteral("settings_active_mode_template_radio")
        )
        != nullptr
    );
    QCOMPARE(line_edit_combo->currentText(), QStringLiteral("none"));
    QCOMPARE(line_edit_points_table->rowCount(), 0);
    QVERIFY(line_edit_name_edit->text().isEmpty());
    QVERIFY(!line_edit_save_button->isEnabled());
    QVERIFY(!line_advanced_checkbox->isChecked());
    QVERIFY(line_parameter_mode_combo->isHidden());
    QVERIFY(line_width_combo->isHidden());
    QVERIFY(line_length_combo->isHidden());
    QVERIFY(line_response_combo->isHidden());
    QVERIFY(!template_advanced_checkbox->isChecked());
    QVERIFY(template_parameter_mode_combo->isHidden());
    QVERIFY(template_width_combo->isHidden());
    QVERIFY(template_length_combo->isHidden());
    QVERIFY(template_response_combo->isHidden());

    line_editor_panel->set_line_profile(
        line_profile {
            .name = QStringLiteral("north"),
            .color = QColor(Qt::cyan),
            .color_mode_id = QStringLiteral("manual"),
            .closed = false,
            .width_text = QStringLiteral("string_light"),
            .length_text = QStringLiteral("long"),
            .response_text = QStringLiteral("resonant"),
        }
    );
    line_editor_panel->set_template_settings(
        template_apply_settings {
            .template_name = QStringLiteral("north"),
            .color = QColor(Qt::magenta),
            .color_mode_id = QStringLiteral("negative"),
            .width_text = QStringLiteral("thick"),
            .length_text = QStringLiteral("short"),
            .response_text = QStringLiteral("dry"),
        }
    );

    QVERIFY(
        line_summary->text().contains(QStringLiteral("width=string_light"))
    );
    QVERIFY(line_summary->text().contains(QStringLiteral("length=long")));
    QVERIFY(line_summary->text().contains(QStringLiteral("response=resonant")));
    QVERIFY(line_summary->text().contains(QStringLiteral("manual color")));
    QVERIFY(!line_color_button->isHidden());
    QVERIFY(!line_random_color_button->isHidden());
    QVERIFY(line_random_color_button->isEnabled());

    QVERIFY(template_summary->text().contains(QStringLiteral("north")));
    QVERIFY(template_summary->text().contains(QStringLiteral("width=thick")));
    QVERIFY(template_summary->text().contains(QStringLiteral("length=short")));
    QVERIFY(template_summary->text().contains(QStringLiteral("response=dry")));
    QVERIFY(template_summary->text().contains(QStringLiteral("negative auto")));
    QVERIFY(template_color_button->isHidden());
    QVERIFY(template_random_color_button->isHidden());

    line_advanced_checkbox->setChecked(true);
    template_advanced_checkbox->setChecked(true);
    QVERIFY(!line_parameter_mode_combo->isHidden());
    QVERIFY(!line_width_combo->isHidden());
    QVERIFY(!line_length_combo->isHidden());
    QVERIFY(!line_response_combo->isHidden());
    QVERIFY(!template_parameter_mode_combo->isHidden());
    QVERIFY(!template_width_combo->isHidden());
    QVERIFY(!template_length_combo->isHidden());
    QVERIFY(!template_response_combo->isHidden());
}

void main_window_tests::settings_emit_stream_changes() {
    settings_panel panel;
    panel.set_existing_names({ QStringLiteral("cam-1") });
    stream_settings initial_settings;
    initial_settings.stream_name = QStringLiteral("cam-1");
    panel.set_active_stream_settings(initial_settings);

    QSignalSpy stream_settings_spy(
        &panel, &settings_panel::active_stream_settings_changed
    );

    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_stream_combo")
    );
    auto* active_algorithm_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_stream_editor_algorithm_combo")
    );
    auto* active_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_labels_checkbox")
    );
    auto* standard_labels_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_standard_labels_checkbox")
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
    auto* manual_processing_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_manual_processing_checkbox")
    );
    auto* processing_advanced_checkbox = panel.findChild<QCheckBox*>(
        QStringLiteral("settings_stream_editor_processing_advanced_checkbox")
    );
    auto* manual_display_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_stream_editor_display_fps_spin")
    );
    auto* manual_core_fps_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_stream_editor_core_fps_spin")
    );
    auto* manual_processing_pixels_spin = panel.findChild<QSpinBox*>(
        QStringLiteral("settings_stream_editor_processing_pixels_spin")
    );

    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(active_algorithm_combo != nullptr);
    QVERIFY(active_labels_checkbox != nullptr);
    QVERIFY(standard_labels_checkbox != nullptr);
    QVERIFY(operator_profile_combo != nullptr);
    QVERIFY(algorithm_advanced_checkbox != nullptr);
    QVERIFY(preset_combo != nullptr);
    QVERIFY(processing_advanced_checkbox != nullptr);
    QVERIFY(manual_processing_checkbox != nullptr);
    QVERIFY(manual_display_fps_spin != nullptr);
    QVERIFY(manual_core_fps_spin != nullptr);
    QVERIFY(manual_processing_pixels_spin != nullptr);
    QCOMPARE(active_stream_combo->currentText(), QStringLiteral("cam-1"));
    QVERIFY(!algorithm_advanced_checkbox->isChecked());
    QVERIFY(!active_algorithm_combo->isHidden());
    QVERIFY(preset_combo->isHidden());
    QVERIFY(manual_processing_checkbox->isHidden());

    const int contour_mask_index
        = active_algorithm_combo->findData(QStringLiteral("contour_mask"));
    QVERIFY(contour_mask_index >= 0);
    active_algorithm_combo->setCurrentIndex(contour_mask_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("balanced"));
    }

    const int debug_heavy_index
        = operator_profile_combo->findData(QStringLiteral("debug_heavy"));
    QVERIFY(debug_heavy_index >= 0);
    operator_profile_combo->setCurrentIndex(debug_heavy_index);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
    }

    algorithm_advanced_checkbox->setChecked(true);
    processing_advanced_checkbox->setChecked(true);
    QVERIFY(!manual_processing_checkbox->isHidden());
    manual_processing_checkbox->setChecked(true);
    manual_display_fps_spin->setValue(20);
    manual_core_fps_spin->setValue(10);
    manual_processing_pixels_spin->setValue(512 * 512);

    QVERIFY(stream_settings_spy.count() >= 3);
    {
        const auto args = stream_settings_spy.takeLast();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
        QCOMPARE(settings_value.manual_display_fps, 20);
        QCOMPARE(settings_value.manual_core_fps, 10);
        QCOMPARE(settings_value.manual_processing_pixels, 512 * 512);
    }
    stream_settings_spy.clear();

    active_labels_checkbox->setChecked(false);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(settings_value.labels_enabled, false);
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QCOMPARE(settings_value.algorithm_preset, QStringLiteral("mask_heavy"));
        QVERIFY(settings_value.algorithm_overlay_enabled);
        QVERIFY(settings_value.manual_processing_policy_enabled);
        QCOMPARE(settings_value.manual_display_fps, 20);
    }
    stream_settings_spy.clear();

    standard_labels_checkbox->setChecked(false);

    QCOMPARE(stream_settings_spy.count(), 1);
    {
        const auto args = stream_settings_spy.takeFirst();
        const auto settings_value = qvariant_cast<stream_settings>(args.at(0));
        QCOMPARE(settings_value.stream_name, QStringLiteral("cam-1"));
        QVERIFY(!settings_value.standard_labels_enabled);
        QCOMPARE(settings_value.algorithm_id, QStringLiteral("contour_mask"));
        QVERIFY(settings_value.manual_processing_policy_enabled);
    }
}

// NOLINTEND(readability-convert-member-functions-to-static)
