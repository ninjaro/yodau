#include "ui/main_window_tests.hpp"

#include "configuration/line_configuration_json.hpp"
#include "shell/stream_controller.hpp"
#include "streams/stream_manager.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/line_profile_panel.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"
#include "widgets/stream_cell.hpp"

#include <QComboBox>
#include <QPushButton>
#include <QSlider>
#include <QtTest/QtTest>

void main_window_tests::wave_configuration() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    QSKIP("Desktop preset to shared configuration workflow");
#else
    yodau::core::stream_manager manager;
    settings_panel settings;
    stream_board board;
    stream_controller controller(&manager, &settings, &board);
    controller.handle_add_file("/tmp/wave-preview.mp4", "sample", false);
    controller.handle_show_stream_changed("sample", true);
    board.grid_mode()->stream_enlarge("sample");
    QCOMPARE(controller.active_configuration_stream_name(), QString("sample"));
    auto* profile = settings.findChild<line_profile_panel*>();
    auto* cell = board.active_cell();
    QVERIFY(profile && cell);
    profile->set_line_profile(
        line_profile {
            .name = "demo",
            .color = Qt::cyan,
            .color_mode_id = "manual",
            .closed = true,
        }
    );
    profile->findChild<QComboBox*>("line_wave_preset")->setCurrentIndex(3);
    const std::vector<QPointF> points { { 15, 20 }, { 85, 20 }, { 85, 70 } };
    cell->set_draft_points_pct(points);
    profile->findChild<QPushButton*>("settings_active_line_save_button")
        ->click();
    QCOMPARE(manager.stream_lines("sample").size(), size_t(1));
    QVERIFY(cell->draft_points_pct().empty());

    QByteArray data;
    QString error;
    QVERIFY2(
        controller.export_line_configuration_data(&data, &error),
        qPrintable(error)
    );
    const auto document = yodau::core::decode_line_configuration_json(
        std::string_view(data.constData(), size_t(data.size()))
    );
    QCOMPARE(document.lines.size(), size_t(1));
    const auto& line = document.lines.front();
    QCOMPARE(line.name, std::string("demo"));
    QVERIFY(line.closed);
    QCOMPARE(line.points.size(), size_t(3));
    QCOMPARE(line.appearance.width_text, std::string("string_heavy"));
    QCOMPARE(line.appearance.length_text, std::string("long"));
    QCOMPARE(line.appearance.response_text, std::string("resonant"));
    QVERIFY(std::abs(line.profile.visual_width - 6.5f) < .01f);
    QVERIFY(std::abs(line.profile.effective_length - 1.35f) < .01f);
    QVERIFY(std::abs(line.profile.damping - .8f) < .01f);

    // Presentation controls never enter the shared GUI/daemon document.
    profile->findChild<QSlider*>("line_wave_age")->setValue(1200);
    profile->findChild<QComboBox*>("line_wave_emphasis")->setCurrentIndex(2);
    QByteArray after;
    QVERIFY2(
        controller.export_line_configuration_data(&after, &error),
        qPrintable(error)
    );
    QCOMPARE(after, data);

    yodau::core::stream_manager imported_manager;
    settings_panel imported_settings;
    stream_board imported_board;
    stream_controller imported(
        &imported_manager, &imported_settings, &imported_board
    );
    QVERIFY2(
        imported.import_line_configuration_data(data, &error), qPrintable(error)
    );
    imported.handle_show_stream_changed("sample", true);
    imported_board.grid_mode()->stream_enlarge("sample");
    QVERIFY2(
        imported.export_line_configuration_data(&after, &error),
        qPrintable(error)
    );
    QCOMPARE(
        yodau::core::decode_line_configuration_json(
            std::string_view(after.constData(), size_t(after.size()))
        ),
        document
    );
#endif
}
