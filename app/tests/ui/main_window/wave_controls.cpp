#include "ui/main_window_tests.hpp"

#include "widgets/line_profile_panel.hpp"
#include "widgets/line_wave_preview.hpp"

#include <QAbstractItemModel>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QTimer>
#include <QtTest/QtTest>

void main_window_tests::wave_controls() {
    line_profile_panel panel;
    auto* toggle = panel.findChild<QCheckBox*>("line_wave_toggle");
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QVERIFY(toggle == nullptr);
    QVERIFY(panel.findChild<line_wave_preview*>() == nullptr);
#else
    panel.show();
    auto* controls = panel.findChild<QWidget*>("line_wave_controls");
    auto* age = panel.findChild<QSlider*>("line_wave_age");
    auto* gain = panel.findChild<QComboBox*>("line_wave_emphasis");
    auto* preview = panel.findChild<line_wave_preview*>();
    QVERIFY(toggle && controls && age && gain && preview);
    QVERIFY(!toggle->isChecked());
    QVERIFY(controls->isHidden());
    QCOMPARE(age->maximum(), 2400);
    QCOMPARE(gain->currentData().toDouble(), 1.0);
    QVERIFY(!age->accessibleName().isEmpty());
    QVERIFY(!gain->accessibleName().isEmpty());
    QVERIFY(!preview->accessibleDescription().isEmpty());
    QVERIFY(panel.findChildren<QTimer*>().isEmpty());
    QSignalSpy changes(&panel, &line_profile_panel::profile_changed);
    toggle->setFocus();
    QTest::keyClick(toggle, Qt::Key_Space);
    QVERIFY(controls->isVisible());
    age->setFocus();
    QTest::keyClick(age, Qt::Key_Right);
    QCOMPARE(age->value(), 220);
    gain->setFocus();
    QTest::keyClick(gain, Qt::Key_Down);
    QCOMPARE(gain->currentData().toDouble(), 2.0);
    toggle->setChecked(false);
    QVERIFY(controls->isHidden());
    toggle->setChecked(true);
    QCOMPARE(age->value(), 220);
    QCOMPARE(gain->currentData().toDouble(), 2.0);
    QCOMPARE(changes.count(), 0);
#endif
}

void main_window_tests::wave_presets_data() {
    QTest::addColumn<int>("index");
    QTest::addColumn<QString>("width");
    QTest::addColumn<QString>("span");
    QTest::addColumn<QString>("response");
    QTest::newRow("muted") << 1 << QString("thin") << QString("short")
                           << QString("dry");
    QTest::newRow("balanced")
        << 2 << QString("medium") << QString("medium") << QString("balanced");
    QTest::newRow("resonant") << 3 << QString("string_heavy") << QString("long")
                              << QString("resonant");
}

void main_window_tests::wave_presets() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop preview presets only");
#else
    QFETCH(int, index);
    QFETCH(QString, width);
    QFETCH(QString, span);
    QFETCH(QString, response);
    line_profile_panel panel;
    const line_profile custom {
        .name = "draft",
        .color = Qt::cyan,
        .color_mode_id = "manual",
        .closed = true,
        .width_text = "4.0",
        .length_text = "1.1",
        .response_text = "1.2",
    };
    panel.set_line_profile(custom);
    auto* preset = panel.findChild<QComboBox*>("line_wave_preset");
    QVERIFY(preset);
    QVERIFY(!(
        preset->model()->flags(preset->model()->index(0, 0)) & Qt::ItemIsEnabled
    ));
    QCOMPARE(preset->currentIndex(), 0);
    QSignalSpy changes(&panel, &line_profile_panel::profile_changed);
    preset->setCurrentIndex(index);
    QCOMPARE(changes.count(), 1);
    const auto value = qvariant_cast<line_profile>(changes.front().front());
    QCOMPARE(value.width_text, width);
    QCOMPARE(value.length_text, span);
    QCOMPARE(value.response_text, response);
    QCOMPARE(value.name, custom.name);
    QCOMPARE(value.color, custom.color);
    QCOMPARE(value.color_mode_id, custom.color_mode_id);
    QCOMPARE(value.closed, custom.closed);
    auto* advanced
        = panel.findChild<QCheckBox*>("settings_active_line_advanced_checkbox");
    QVERIFY(advanced && !advanced->isChecked());
    auto* width_control
        = panel.findChild<QComboBox*>("settings_active_line_width_combo");
    QVERIFY(width_control && width_control->isHidden());
    QCOMPARE(width_control->currentText(), width);
    QSignalSpy saves(&panel, &line_profile_panel::save_requested);
    QSignalSpy undos(&panel, &line_profile_panel::undo_requested);
    panel.findChild<QPushButton*>("settings_active_line_save_button")->click();
    QCOMPARE(saves.count(), 1);
    QCOMPARE(
        qvariant_cast<line_profile>(saves.front().front()).width_text, width
    );
    panel.findChild<QPushButton*>("settings_active_line_undo_button")->click();
    QCOMPARE(undos.count(), 1);
#endif
}

void main_window_tests::wave_custom_profile() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop preview presets only");
#else
    line_profile_panel panel;
    QSignalSpy changes(&panel, &line_profile_panel::profile_changed);
    line_profile custom {
        .name = "<b>custom</b>",
        .color = Qt::green,
        .color_mode_id = "negative_auto",
        .closed = true,
        .width_text = "7.2",
        .length_text = "1.4",
        .response_text = "1.1",
    };
    panel.set_line_profile(custom);
    auto* preset = panel.findChild<QComboBox*>("line_wave_preset");
    auto* width
        = panel.findChild<QComboBox*>("settings_active_line_width_combo");
    auto* span
        = panel.findChild<QComboBox*>("settings_active_line_length_combo");
    auto* response
        = panel.findChild<QComboBox*>("settings_active_line_response_combo");
    QVERIFY(preset && width && span && response);
    QCOMPARE(preset->currentIndex(), 0);
    QCOMPARE(panel.current_line_profile().width_text, custom.width_text);
    QCOMPARE(panel.current_line_profile().length_text, custom.length_text);
    QCOMPARE(panel.current_line_profile().response_text, custom.response_text);
    panel.findChild<QCheckBox*>("line_wave_toggle")->setChecked(true);
    panel.findChild<QSlider*>("line_wave_age")->setValue(1400);
    panel.findChild<QComboBox*>("line_wave_emphasis")->setCurrentIndex(2);
    panel.set_line_closed(false);
    QCOMPARE(changes.count(), 0);
    width->setCurrentText("thin");
    span->setCurrentText("short");
    response->setCurrentText("dry");
    QCOMPARE(changes.count(), 3);
    QCOMPARE(preset->currentIndex(), 1);
    response->setCurrentText("1.3");
    QCOMPARE(preset->currentIndex(), 0);
    QCOMPARE(panel.current_line_profile().color_mode_id, custom.color_mode_id);
    QCOMPARE(panel.current_line_profile().name, custom.name);
    panel.reset_form();
    QCOMPARE(preset->currentIndex(), 2);
    QCOMPARE(changes.count(), 4);
#endif
}
