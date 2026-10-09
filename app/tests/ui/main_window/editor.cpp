#include "shell/app_settings.hpp"
#include "ui/main_window/fixtures.hpp"
#include "ui/main_window_tests.hpp"
#include "widgets/active_editor_panel.hpp"
#include "widgets/stream_cell.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QtTest/QtTest>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::editor_tools_and_log() {
    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_template_candidates({ QStringLiteral("north") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(10.0, 10.0),
                    QPointF(90.0, 15.0),
                },
            },
            stream_cell::line_instance {
                .template_name = QStringLiteral("south"),
                .color = QColor(Qt::yellow),
                .color_mode_id = QStringLiteral("negative_auto"),
                .enabled = false,
                .closed = true,
                .width_text = QStringLiteral("thin"),
                .length_text = QStringLiteral("short"),
                .response_text = QStringLiteral("dry"),
                .pts_pct = {
                    QPointF(10.0, 80.0),
                    QPointF(90.0, 85.0),
                    QPointF(50.0, 60.0),
                },
            },
        }
    );

    QSignalSpy active_stream_spy(
        &panel, &active_editor_panel::active_stream_selected
    );
    QSignalSpy line_enabled_spy(
        &panel, &active_editor_panel::line_enabled_changed
    );
    QSignalSpy line_detach_spy(
        &panel, &active_editor_panel::line_detach_requested
    );
    QObject::connect(
        &panel, &active_editor_panel::line_enabled_changed, &panel,
        [&panel](const QString& line_name, const bool enabled) {
            auto lines = std::vector<stream_cell::line_instance> {
                stream_cell::line_instance {
                    .template_name = QStringLiteral("north"),
                    .color = QColor(Qt::green),
                    .color_mode_id = QStringLiteral("manual"),
                    .enabled = line_name == QStringLiteral("north") ? enabled : true,
                    .closed = false,
                    .width_text = QStringLiteral("string_heavy"),
                    .length_text = QStringLiteral("long"),
                    .response_text = QStringLiteral("resonant"),
                    .pts_pct = {
                        QPointF(10.0, 10.0),
                        QPointF(90.0, 15.0),
                    },
                },
                stream_cell::line_instance {
                    .template_name = QStringLiteral("south"),
                    .color = QColor(Qt::yellow),
                    .color_mode_id = QStringLiteral("negative_auto"),
                    .enabled = line_name == QStringLiteral("south") ? enabled : false,
                    .closed = true,
                    .width_text = QStringLiteral("thin"),
                    .length_text = QStringLiteral("short"),
                    .response_text = QStringLiteral("dry"),
                    .pts_pct = {
                        QPointF(10.0, 80.0),
                        QPointF(90.0, 85.0),
                        QPointF(50.0, 60.0),
                    },
                },
            };
            panel.set_active_lines(lines);
        }
    );
    QObject::connect(
        &panel, &active_editor_panel::line_detach_requested, &panel,
        [&panel](const QString& line_name) {
            auto lines = std::vector<stream_cell::line_instance> {
                stream_cell::line_instance {
                    .template_name = QStringLiteral("north"),
                    .color = QColor(Qt::green),
                    .color_mode_id = QStringLiteral("manual"),
                    .enabled = true,
                    .closed = false,
                    .width_text = QStringLiteral("string_heavy"),
                    .length_text = QStringLiteral("long"),
                    .response_text = QStringLiteral("resonant"),
                    .pts_pct = {
                        QPointF(10.0, 10.0),
                        QPointF(90.0, 15.0),
                    },
                },
                stream_cell::line_instance {
                    .template_name = QStringLiteral("south"),
                    .color = QColor(Qt::yellow),
                    .color_mode_id = QStringLiteral("negative_auto"),
                    .enabled = false,
                    .closed = true,
                    .width_text = QStringLiteral("thin"),
                    .length_text = QStringLiteral("short"),
                    .response_text = QStringLiteral("dry"),
                    .pts_pct = {
                        QPointF(10.0, 80.0),
                        QPointF(90.0, 85.0),
                        QPointF(50.0, 60.0),
                    },
                },
            };
            lines.erase(
                std::remove_if(
                    lines.begin(), lines.end(),
                    [&line_name](const stream_cell::line_instance& line_value) {
                        return line_value.template_name == line_name;
                    }
                ),
                lines.end()
            );
            panel.set_active_lines(lines);
        }
    );

    auto* line_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_line_profile_panel")
    );
    auto* template_panel = panel.findChild<QGroupBox*>(
        QStringLiteral("settings_active_template_apply_panel")
    );
    auto* template_radio = panel.findChild<QRadioButton*>(
        QStringLiteral("settings_active_mode_template_radio")
    );
    auto* status_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_status_summary_label")
    );
    auto* line_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_lines_summary_label")
    );
    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_list = panel.findChild<QTreeWidget*>(
        QStringLiteral("settings_active_lines_list")
    );
    auto* detach_line_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_active_detach_line_button")
    );
    auto* active_stream_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_stream_combo")
    );

    QVERIFY(line_panel != nullptr);
    QVERIFY(template_panel != nullptr);
    QVERIFY(template_radio != nullptr);
    QVERIFY(status_summary != nullptr);
    QVERIFY(line_summary != nullptr);
    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_list != nullptr);
    QVERIFY(detach_line_button != nullptr);
    QVERIFY(active_stream_combo != nullptr);
    QVERIFY(
        editor_tabs->widget(0)->findChild<QRadioButton*>(
            QStringLiteral("settings_active_mode_template_radio")
        )
        != nullptr
    );

    QVERIFY(!line_panel->isHidden());
    QVERIFY(line_panel->isEnabled());
    QVERIFY(template_panel->isHidden());
    QVERIFY(status_summary->text().contains(QStringLiteral("cam-1")));
    QVERIFY(status_summary->text().contains(QStringLiteral("draw new")));
    QVERIFY(status_summary->text().contains(QStringLiteral("1 enabled of 2")));
    QVERIFY(line_summary->text().contains(QStringLiteral("2 saved lines")));
    QVERIFY(!detach_line_button->isEnabled());
    QCOMPARE(line_list->topLevelItemCount(), 2);
    QCOMPARE(line_list->topLevelItem(0)->checkState(0), Qt::Checked);
    QCOMPARE(line_list->topLevelItem(1)->checkState(0), Qt::Unchecked);

    template_radio->click();
    QVERIFY(line_panel->isHidden());
    QVERIFY(!template_panel->isHidden());
    QVERIFY(template_panel->isEnabled());
    QVERIFY(status_summary->text().contains(QStringLiteral("use template")));

    line_list->topLevelItem(1)->setCheckState(0, Qt::Checked);
    QCoreApplication::processEvents();
    QCOMPARE(line_enabled_spy.size(), 1);
    QCOMPARE(line_enabled_spy.at(0).at(0).toString(), QStringLiteral("south"));
    QCOMPARE(line_enabled_spy.at(0).at(1).toBool(), true);

    line_list->setCurrentItem(line_list->topLevelItem(1));
    QVERIFY(detach_line_button->isEnabled());
    detach_line_button->click();
    QCoreApplication::processEvents();
    QCOMPARE(line_detach_spy.size(), 1);
    QCOMPARE(line_detach_spy.at(0).at(0).toString(), QStringLiteral("south"));
    QCOMPARE(line_list->topLevelItemCount(), 1);
    QVERIFY(line_summary->text().contains(QStringLiteral("1 saved lines")));

    active_stream_combo->setCurrentIndex(0);
    active_stream_combo->setCurrentText(QStringLiteral("cam-1"));
    QVERIFY(!active_stream_spy.empty());
    QCOMPARE(
        active_stream_spy.takeLast().at(0).toString(), QStringLiteral("cam-1")
    );
}

void main_window_tests::editor_edit_tab_and_signals() {
    qRegisterMetaType<line_edit_request>("line_edit_request");

    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(10.0, 10.0),
                    QPointF(50.0, 20.0),
                    QPointF(90.0, 25.0),
                },
            },
            stream_cell::line_instance {
                .template_name = QStringLiteral("south"),
                .color = QColor(Qt::yellow),
                .color_mode_id = QStringLiteral("negative_auto"),
                .enabled = false,
                .closed = true,
                .width_text = QStringLiteral("thin"),
                .length_text = QStringLiteral("short"),
                .response_text = QStringLiteral("dry"),
                .pts_pct = {
                    QPointF(20.0, 80.0),
                    QPointF(80.0, 75.0),
                },
            },
        }
    );

    QSignalSpy preview_spy(
        &panel, &active_editor_panel::line_edit_preview_changed
    );
    QSignalSpy clear_spy(
        &panel, &active_editor_panel::line_edit_preview_cleared
    );
    QSignalSpy save_spy(&panel, &active_editor_panel::line_edit_save_requested);

    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_edit_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );
    auto* line_edit_table = panel.findChild<QTableWidget*>(
        QStringLiteral("settings_active_edit_points_table")
    );
    auto* line_edit_name = panel.findChild<QLineEdit*>(
        QStringLiteral("active_edit_new_name_edit")
    );
    auto* line_edit_save_button = panel.findChild<QPushButton*>(
        QStringLiteral("settings_active_edit_save_button")
    );
    auto* line_edit_summary = panel.findChild<QLabel*>(
        QStringLiteral("settings_active_edit_summary_label")
    );

    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_edit_combo != nullptr);
    QVERIFY(line_edit_table != nullptr);
    QVERIFY(line_edit_name != nullptr);
    QVERIFY(line_edit_save_button != nullptr);
    QVERIFY(line_edit_summary != nullptr);
    QCOMPARE(editor_tabs->count(), 3);
    QCOMPARE(editor_tabs->tabText(2), QStringLiteral("edit"));
    QVERIFY(line_edit_summary->text().contains(
        QStringLiteral("Select an enabled line")
    ));
    QCOMPARE(line_edit_combo->count(), 2);
    QCOMPARE(line_edit_combo->itemText(0), QStringLiteral("none"));
    QCOMPARE(line_edit_combo->itemText(1), QStringLiteral("north"));

    editor_tabs->setCurrentIndex(2);
    QCOMPARE(clear_spy.size(), 1);

    line_edit_combo->setCurrentIndex(1);
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.stream_name, QStringLiteral("cam-1"));
        QCOMPARE(request.source_line_name, QStringLiteral("north"));
        QCOMPARE(request.profile.name, QStringLiteral("north_edit"));
        QCOMPARE(request.points_pct.size(), 3);
    }
    QCOMPARE(line_edit_table->rowCount(), 3);
    QCOMPARE(line_edit_table->item(0, 1)->text(), QStringLiteral("1"));
    QCOMPARE(line_edit_table->item(1, 2)->text(), QStringLiteral("50.00"));
    QCOMPARE(line_edit_table->item(2, 3)->text(), QStringLiteral("25.00"));
    QCOMPARE(line_edit_name->text(), QStringLiteral("north_edit"));
    QVERIFY(
        line_edit_summary->text().contains(QStringLiteral("keeping 3 of 3"))
    );

    line_edit_table->item(1, 0)->setCheckState(Qt::Unchecked);
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 2);
    }
    QVERIFY(
        line_edit_summary->text().contains(QStringLiteral("keeping 2 of 3"))
    );

    line_edit_name->setText(QStringLiteral("north_variant"));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.profile.name, QStringLiteral("north_variant"));
        QCOMPARE(request.points_pct.size(), 2);
    }
    QVERIFY(line_edit_save_button->isEnabled());

    line_edit_save_button->click();
    QCOMPARE(save_spy.size(), 1);
    {
        const auto args = save_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.source_line_name, QStringLiteral("north"));
        QCOMPARE(request.profile.name, QStringLiteral("north_variant"));
        QCOMPARE(request.points_pct.size(), 2);
    }
    QVERIFY(clear_spy.size() >= 2);
    QCOMPARE(line_edit_combo->currentIndex(), 0);
    QVERIFY(line_edit_name->text().isEmpty());
}

void main_window_tests::editor_applies_stream_edit_updates() {
    qRegisterMetaType<line_edit_request>("line_edit_request");

    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(10.0, 10.0),
                    QPointF(50.0, 20.0),
                    QPointF(90.0, 25.0),
                },
            },
        }
    );

    QSignalSpy preview_spy(
        &panel, &active_editor_panel::line_edit_preview_changed
    );

    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_edit_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );
    auto* line_edit_table = panel.findChild<QTableWidget*>(
        QStringLiteral("settings_active_edit_points_table")
    );

    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_edit_combo != nullptr);
    QVERIFY(line_edit_table != nullptr);

    editor_tabs->setCurrentIndex(2);
    line_edit_combo->setCurrentIndex(1);
    QCOMPARE(preview_spy.size(), 1);
    preview_spy.clear();

    QVERIFY(panel.select_line_edit_point(1));
    QCOMPARE(line_edit_table->currentRow(), 1);

    QVERIFY(panel.move_line_edit_point(1, QPointF(55.0, 33.0)));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 3);
        QCOMPARE(request.points_pct.at(1), QPointF(55.0, 33.0));
    }
    QCOMPARE(line_edit_table->item(1, 2)->text(), QStringLiteral("55.00"));
    QCOMPARE(line_edit_table->item(1, 3)->text(), QStringLiteral("33.00"));

    QVERIFY(panel.translate_line_edit_shape(QPointF(5.0, -3.0)));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.at(0), QPointF(15.0, 7.0));
        QCOMPARE(request.points_pct.at(1), QPointF(60.0, 30.0));
        QCOMPARE(request.points_pct.at(2), QPointF(95.0, 22.0));
    }

    QVERIFY(panel.split_line_edit_point(1));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 4);
        QCOMPARE(request.points_pct.at(0), QPointF(15.0, 7.0));
        QCOMPARE(request.points_pct.at(1), QPointF(37.5, 18.5));
        QCOMPARE(request.points_pct.at(2), QPointF(77.5, 26.0));
        QCOMPARE(request.points_pct.at(3), QPointF(95.0, 22.0));
    }
    QCOMPARE(line_edit_table->rowCount(), 4);
    QCOMPARE(line_edit_table->item(1, 2)->text(), QStringLiteral("37.50"));
    QCOMPARE(line_edit_table->item(2, 2)->text(), QStringLiteral("77.50"));
}

void main_window_tests::editor_rotates_preview() {
    qRegisterMetaType<line_edit_request>("line_edit_request");

    active_editor_panel panel;
    panel.set_active_candidates({ QStringLiteral("cam-1") });
    panel.set_active_current(QStringLiteral("cam-1"));
    panel.set_active_lines(
        {
            stream_cell::line_instance {
                .template_name = QStringLiteral("north"),
                .color = QColor(Qt::green),
                .color_mode_id = QStringLiteral("manual"),
                .enabled = true,
                .closed = false,
                .width_text = QStringLiteral("string_heavy"),
                .length_text = QStringLiteral("long"),
                .response_text = QStringLiteral("resonant"),
                .pts_pct = {
                    QPointF(40.0, 50.0),
                    QPointF(50.0, 50.0),
                    QPointF(60.0, 50.0),
                },
            },
        }
    );

    QSignalSpy preview_spy(
        &panel, &active_editor_panel::line_edit_preview_changed
    );

    auto* editor_tabs = panel.findChild<QTabWidget*>(
        QStringLiteral("settings_active_editor_tabs")
    );
    auto* line_edit_combo = panel.findChild<QComboBox*>(
        QStringLiteral("settings_active_edit_line_combo")
    );

    QVERIFY(editor_tabs != nullptr);
    QVERIFY(line_edit_combo != nullptr);

    editor_tabs->setCurrentIndex(2);
    line_edit_combo->setCurrentIndex(1);
    QCOMPARE(preview_spy.size(), 1);
    preview_spy.clear();

    QVERIFY(panel.rotate_line_edit_shape(180.0));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 3);
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(0), QPointF(60.0, 50.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(1), QPointF(50.0, 50.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(2), QPointF(40.0, 50.0)
            )
        );
    }

    QVERIFY(panel.select_line_edit_point(1));
    QVERIFY(panel.rotate_line_edit_shape(90.0, 1));
    QCOMPARE(preview_spy.size(), 1);
    {
        const auto args = preview_spy.takeFirst();
        const auto request = qvariant_cast<line_edit_request>(args.at(0));
        QCOMPARE(request.points_pct.size(), 3);
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(0), QPointF(50.0, 60.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(1), QPointF(50.0, 50.0)
            )
        );
        QVERIFY(
            main_window_tests_support::point_close(
                request.points_pct.at(2), QPointF(50.0, 40.0)
            )
        );
    }
}

// NOLINTEND(readability-convert-member-functions-to-static)
