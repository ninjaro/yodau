#include "configuration/line_configuration_file.hpp"
#include "configuration/line_configuration_json.hpp"
#include "shell/app_settings.hpp"
#include "shell/stream_controller.hpp"
#include "streams/stream_manager.hpp"
#include "ui/main_window_tests.hpp"

#include "widgets/grid_view.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <filesystem>
#include <string_view>

// QtTest discovers these private slots through the meta-object.
// NOLINTBEGIN(readability-convert-member-functions-to-static)

void main_window_tests::controller_configuration_round_trip() {
    using namespace yodau::core;

    QTemporaryDir temporary_dir;
    QVERIFY(temporary_dir.isValid());
    const QString import_path
        = temporary_dir.filePath(QStringLiteral("import.yodau.json"));
    const QString export_path
        = temporary_dir.filePath(QStringLiteral("export.yodau.json"));

    line_configuration_document source_document;
    source_document.stream.name = "config-cam";
    source_document.stream.source = "/tmp/config-cam.mp4";
    source_document.stream.type = "file";
    source_document.stream.loop = false;
    source_document.stream.virtual_camera_path = "/dev/custom-yodau7";
    source_document.stream.analysis_interval_ms = 100;
    source_document.stream.algorithm
        = default_processing_algorithm_settings("spot_grid");
    source_document.stream.algorithm.preset_id = "dense";
    source_document.stream.labels_enabled = false;
    source_document.stream.standard_labels_enabled = false;
    source_document.stream.movement_display_mode = "bubbles";
    source_document.stream.manual_processing_policy_enabled = true;
    source_document.stream.manual_display_fps = 20;
    source_document.stream.manual_core_fps = 10;
    source_document.stream.manual_processing_pixels = 640 * 360;

    configured_line line_value;
    line_value.name = "door";
    line_value.points = {
        point { 10.0f, 25.0f },
        point { 90.0f, 25.0f },
    };
    line_value.enabled = false;
    line_value.direction = tripwire_dir::pos_to_neg;
    line_value.profile = make_line_profile("door", 3.0f, 4.0f, 1.5f, 0.75f);
    line_value.appearance = line_configuration_appearance {
        .color = "#ff336699",
        .color_mode = "manual",
        .width_text = "thick",
        .length_text = "long",
        .response_text = "resonant",
    };
    source_document.lines.push_back(std::move(line_value));
    yodau::core::save_line_configuration_file_atomic(
        source_document, std::filesystem::path(import_path.toStdString())
    );

    stream_manager manager;
    settings_panel settings;
    stream_board board;
    stream_controller controller(&manager, &settings, &board);
    controller.handle_add_file(
        QStringLiteral("/tmp/already-active.mp4"),
        QStringLiteral("already-active"), false
    );
    controller.handle_show_stream_changed(
        QStringLiteral("already-active"), true
    );
    board.grid_mode()->stream_enlarge(QStringLiteral("already-active"));
    QCOMPARE(
        controller.active_configuration_stream_name(),
        QStringLiteral("already-active")
    );

    QString error;
    QVERIFY2(
        controller.import_line_configuration_from(import_path, &error),
        qPrintable(error)
    );
    QVERIFY(manager.find_stream("config-cam") != nullptr);
    QVERIFY(manager.find_stream("already-active") != nullptr);
    QCOMPARE(manager.stream_lines("config-cam").size(), size_t(0));

    controller.handle_show_stream_changed(QStringLiteral("config-cam"), true);
    board.grid_mode()->stream_enlarge(QStringLiteral("config-cam"));
    QCOMPARE(
        controller.active_configuration_stream_name(),
        QStringLiteral("config-cam")
    );
    QVERIFY2(
        controller.export_line_configuration_to(export_path, &error),
        qPrintable(error)
    );

    const auto exported = yodau::core::load_line_configuration_file(
        std::filesystem::path(export_path.toStdString())
    );
    QVERIFY(exported.stream.name == "config-cam");
    QVERIFY(exported.stream.source == "/tmp/config-cam.mp4");
    QVERIFY(exported.stream.type == "file");
    QVERIFY(!exported.stream.loop);
    QVERIFY(exported.stream.virtual_camera_path == "/dev/custom-yodau7");
    QVERIFY(exported.stream.algorithm.algorithm_id == "spot_grid");
    QVERIFY(exported.stream.algorithm.preset_id == "dense");
    QVERIFY(!exported.stream.labels_enabled);
    QVERIFY(!exported.stream.standard_labels_enabled);
    QVERIFY(exported.stream.movement_display_mode == "bubbles");
    QVERIFY(exported.stream.manual_processing_policy_enabled);
    QCOMPARE(exported.stream.manual_display_fps, 20);
    QCOMPARE(exported.stream.manual_core_fps, 10);
    QCOMPARE(exported.stream.manual_processing_pixels, 640 * 360);
    QCOMPARE(exported.lines.size(), size_t(1));
    QVERIFY(exported.lines.front().name == "door");
    QVERIFY(!exported.lines.front().enabled);
    QVERIFY(exported.lines.front().direction == tripwire_dir::pos_to_neg);
    QVERIFY(exported.lines.front() == source_document.lines.front());
    QVERIFY(
        exported.lines.front().profile == source_document.lines.front().profile
    );
    QVERIFY(exported.lines.front().appearance.color == "#ff336699");
    QVERIFY(
        exported.lines.front().appearance
        == source_document.lines.front().appearance
    );

    QByteArray exported_data;
    QVERIFY2(
        controller.export_line_configuration_data(&exported_data, &error),
        qPrintable(error)
    );
    const auto exported_from_data = yodau::core::decode_line_configuration_json(
        std::string_view(
            exported_data.constData(), static_cast<size_t>(exported_data.size())
        )
    );
    QVERIFY(exported_from_data == exported);

    stream_manager data_manager;
    settings_panel data_settings;
    stream_board data_board;
    stream_controller data_controller(
        &data_manager, &data_settings, &data_board
    );
    QString imported_stream_name;
    QVERIFY2(
        data_controller.import_line_configuration_data(
            exported_data, &error, &imported_stream_name
        ),
        qPrintable(error)
    );
    QCOMPARE(imported_stream_name, QStringLiteral("config-cam"));
    QVERIFY(data_manager.find_stream("config-cam") != nullptr);
    QVERIFY(!data_controller.import_line_configuration_data(
        QByteArrayLiteral("{broken"), &error
    ));
    QVERIFY(!error.isEmpty());

    QVERIFY2(
        controller.import_line_configuration_from(export_path, &error),
        qPrintable(error)
    );
    QCOMPARE(manager.line_names().size(), size_t(1));
    QCOMPARE(manager.stream_lines("config-cam").size(), size_t(0));
}

// NOLINTEND(readability-convert-member-functions-to-static)
