#include "ui/main_window_tests.hpp"

#include "widgets/active_editor_panel.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/line_profile_panel.hpp"
#include "widgets/template_apply_panel.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QtTest/QtTest>

void main_window_tests::inspector_context() {
    active_editor_panel editor;
    QVERIFY(editor.findChild<QLabel*>("inspector_context") == nullptr);
    editor.set_context_inspector(true);
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QVERIFY(editor.findChild<QLabel*>("inspector_context") == nullptr);
    QVERIFY(editor.findChild<QScrollArea*>("inspector_scroll") == nullptr);
#else
    editor.resize(260, 400);
    editor.show();
    auto* context = editor.findChild<QLabel*>("inspector_context");
    auto* scroll = editor.findChild<QScrollArea*>("inspector_scroll");
    auto* tabs = editor.findChild<QTabWidget*>("settings_active_editor_tabs");
    auto* target
        = editor.findChild<QComboBox*>("settings_active_edit_line_combo");
    auto* points
        = editor.findChild<QTableWidget*>("settings_active_edit_points_table");
    auto* save
        = editor.findChild<QPushButton*>("settings_active_edit_save_button");
    auto* profile = editor.findChild<line_profile_panel*>();
    auto* templates = editor.findChild<template_apply_panel*>();
    QVERIFY(
        context && scroll && tabs && target && points && save && profile
        && templates
    );
    QCOMPARE(context->textFormat(), Qt::PlainText);
    QVERIFY(!context->accessibleName().isEmpty());
    QVERIFY(!scroll->accessibleName().isEmpty());
    QVERIFY(context->text().contains("Activate a visible stream"));
    QVERIFY(scroll->isHidden() && tabs->isHidden());
    editor.set_active_candidates({ "<b>one</b>", "two" });
    editor.set_active_current("<b>one</b>");
    QCOMPARE(scroll->widget(), tabs);
    QVERIFY(!scroll->isHidden() && !tabs->isHidden());
    QVERIFY(context->text().startsWith("<b>one</b> — Draw geometry"));
    QVERIFY(profile->isVisible());
    QVERIFY(
        editor.findChild<QGroupBox*>("settings_active_status_box")->isHidden()
    );
    QVERIFY(editor.findChild<active_stream_panel*>()->isHidden());

    auto* use_template = editor.findChild<QRadioButton*>(
        "settings_active_mode_template_radio"
    );
    QVERIFY(use_template);
    use_template->setFocus(Qt::TabFocusReason);
    QTest::keyClick(use_template, Qt::Key_Space);
    QCOMPARE(tabs->tabText(0), QStringLiteral("Templates"));
    QVERIFY(context->text().contains("No templates available"));
    QVERIFY(!profile->isVisible() && !templates->isVisible());
    editor.add_template_candidate("saved");
    QVERIFY(templates->isVisible());
    QVERIFY(context->text().contains("application settings"));
    editor.set_template_candidates({});
    QVERIFY(context->text().contains("No templates available"));

    editor.set_active_lines(
        { stream_cell::line_instance { .template_name = "north",
                                       .pts_pct = { { 10, 20 }, { 80, 70 } } } }
    );
    tabs->setCurrentIndex(1);
    QVERIFY(context->text().contains("Saved geometry"));
    tabs->setCurrentIndex(2);
    QVERIFY(context->text().contains("Choose enabled geometry"));
    QVERIFY(points->isHidden() && save->isHidden());
    target->setCurrentIndex(target->findData("north"));
    QVERIFY(context->text().startsWith("<b>one</b> — north"));
    QVERIFY(context->text().contains("0–100% frame coordinates"));
    QVERIFY(!points->isHidden() && !save->isHidden());
    editor.set_active_current("two");
    QCOMPARE(target->currentIndex(), 0);
    QVERIFY(context->text().startsWith("two — Edit points"));
    QVERIFY(points->isHidden() && save->isHidden());
    editor.set_active_lines({});
    // Qt chooses an enabled tab after Edit becomes unavailable; the header
    // must describe that actual tab rather than imposing a new mode policy.
    QCOMPARE(
        context->text().section('\n', 0, 0),
        QStringLiteral("two — %1").arg(tabs->tabText(tabs->currentIndex()))
    );
    QVERIFY(!context->text().contains("north"));
    editor.set_active_current({});
    QVERIFY(context->text().contains("Activate a visible stream"));
    QVERIFY(scroll->isHidden() && tabs->isHidden());
    editor.set_context_inspector(false);
    QVERIFY(context->isHidden() && scroll->isHidden());
    QVERIFY(!tabs->isHidden());
    QCOMPARE(tabs->parentWidget(), &editor);
    QVERIFY(!editor.findChild<active_stream_panel*>()->isHidden());
    QVERIFY(
        !editor.findChild<QGroupBox*>("settings_active_status_box")->isHidden()
    );
#endif
}

void main_window_tests::inspector_state() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop inspector only");
#else
    qRegisterMetaType<line_edit_request>();
    qRegisterMetaType<line_profile>();
    active_editor_panel editor;
    editor.set_active_candidates({ "one" });
    editor.set_active_current("one");
    editor.set_line_profile(
        line_profile {
            .name = "draft", .closed = true, .width_text = "string_heavy" }
    );
    auto* advanced = editor.findChild<QCheckBox*>(
        "settings_active_line_advanced_checkbox"
    );
    advanced->setChecked(true);
    editor.set_active_lines(
        { stream_cell::line_instance { .template_name = "north",
                                       .closed = true,
                                       .width_text = "string_heavy",
                                       .pts_pct = { { 10, 20 }, { 80, 70 } } } }
    );
    auto* tabs = editor.findChild<QTabWidget*>("settings_active_editor_tabs");
    auto* target
        = editor.findChild<QComboBox*>("settings_active_edit_line_combo");
    auto* points
        = editor.findChild<QTableWidget*>("settings_active_edit_points_table");
    auto* name = editor.findChild<QLineEdit*>("active_edit_new_name_edit");
    auto* undo
        = editor.findChild<QPushButton*>("settings_active_edit_undo_button");
    tabs->setCurrentIndex(2);
    target->setCurrentIndex(target->findData("north"));
    QVERIFY(editor.select_line_edit_point(1));
    QVERIFY(editor.move_line_edit_point(1, { 65, 50 }));
    name->setText("variant");
    editor.resize(340, 620);
    editor.show();
    QSignalSpy preview(
        &editor, &active_editor_panel::line_edit_preview_changed
    );
    QSignalSpy cleared(
        &editor, &active_editor_panel::line_edit_preview_cleared
    );
    QSignalSpy mode(&editor, &active_editor_panel::edit_mode_changed);
    QSignalSpy selected(&editor, &active_editor_panel::active_stream_selected);
    QSignalSpy profile(&editor, &active_editor_panel::line_profile_changed);
    for (bool contextual : { true, false, true, true, false }) {
        // Restoring Classic must not recreate controls or replay a mode/tab
        // click.
        editor.set_context_inspector(contextual);
        QCOMPARE(
            editor.findChild<QTabWidget*>("settings_active_editor_tabs"), tabs
        );
        QCOMPARE(tabs->currentIndex(), 2);
        QCOMPARE(target->currentData().toString(), QStringLiteral("north"));
        QCOMPARE(points->currentRow(), 1);
        QCOMPARE(points->item(1, 2)->text().toDouble(), 65.0);
        QCOMPARE(points->item(1, 3)->text().toDouble(), 50.0);
        QCOMPARE(name->text(), QStringLiteral("variant"));
        QVERIFY(undo->isEnabled() && advanced->isChecked());
        QCOMPARE(editor.current_line_profile().name, QStringLiteral("draft"));
        QVERIFY(editor.current_line_profile().closed);
        QCOMPARE(
            editor.current_line_profile().width_text,
            QStringLiteral("string_heavy")
        );
    }
    QCOMPARE(preview.count(), 0);
    QCOMPARE(cleared.count(), 0);
    QCOMPARE(mode.count(), 0);
    QCOMPARE(selected.count(), 0);
    QCOMPARE(profile.count(), 0);
    QVERIFY(editor.undo_line_edit_change());
    QCOMPARE(
        qvariant_cast<line_edit_request>(preview.last().at(0)).points_pct[1],
        QPointF(80, 70)
    );
    QVERIFY(editor.redo_line_edit_change());
    QSignalSpy saved(&editor, &active_editor_panel::line_edit_save_requested);
    editor.findChild<QPushButton*>("settings_active_edit_save_button")->click();
    QCOMPARE(saved.count(), 1);
    const auto request = qvariant_cast<line_edit_request>(saved.last().at(0));
    QCOMPARE(request.stream_name, QStringLiteral("one"));
    QCOMPARE(request.source_line_name, QStringLiteral("north"));
    QCOMPARE(request.profile.name, QStringLiteral("variant"));
    QCOMPARE(request.points_pct[1], QPointF(65, 50));
#endif
}
