#include "ui/main_window_tests.hpp"

#include "widgets/line_wave_preview.hpp"
#include "widgets/line_wave_renderer.hpp"

#include <QImage>
#include <QtTest/QtTest>

#include <limits>

void main_window_tests::wave_render_data() {
    QTest::addColumn<bool>("closed");
    QTest::addColumn<QString>("color_mode");
    for (const bool closed : { false, true }) {
        for (const auto* mode : { "manual", "auto_palette", "negative_auto" }) {
            const auto name
                = QString("%1-%2").arg(closed ? "closed" : "open", mode);
            QTest::newRow(qPrintable(name)) << closed << QString(mode);
        }
    }
}

void main_window_tests::wave_render() {
    QFETCH(bool, closed);
    QFETCH(QString, color_mode);
    line_wave_preview preview;
    preview.resize(240, 150);
    line_profile profile {
        .name = {},
        .color = QColor("#168abf"),
        .color_mode_id = color_mode,
        .closed = closed,
        .width_text = "thin",
        .length_text = "short",
        .response_text = "dry",
    };
    const auto render = [&](int age, double gain) {
        preview.set_sample(profile, age, gain);
        QImage image(preview.size(), QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        preview.render(&image);
        return image;
    };
    const auto muted = render(200, 1.0);
    QCOMPARE(muted, render(200, 1.0));
    QVERIFY(muted != render(200, 3.0));
    QVERIFY(muted != render(1200, 1.0));
    QCOMPARE(render(-10, 1.0), render(0, 1.0));
    const auto expired = render(2400, 1.0);
    QCOMPARE(expired, render(9999, 3.0));
    profile.width_text = "string_heavy";
    profile.length_text = "long";
    profile.response_text = "resonant";
    QVERIFY(muted != render(200, 1.0));
    profile.closed = !closed;
    QVERIFY(expired != render(2400, 1.0));
}

void main_window_tests::wave_gain_bounds() {
    using namespace line_wave_renderer;
    const auto geometry
        = build_path_geometry({ { 10, 50 }, { 90, 50 } }, false);
    const auto hit = project_hit(geometry, { 50, 50 });
    QVERIFY(hit);
    QVector<pulse> pulses;
    add_pulses(
        pulses, *hit, 0, .8, "neg_to_pos", .8, "medium", "medium", "balanced"
    );
    QCOMPARE(pulses.size(), 2);
    const auto displacement = displacement_px(geometry, 0, 45, 200, pulses);
    QVERIFY(std::abs(displacement) > .05);
    const auto render
        = [&](double gain, bool default_gain = false, int age = 200) {
              QImage image(240, 150, QImage::Format_ARGB32_Premultiplied);
              image.fill(Qt::transparent);
              QPainter painter(&image);
              if (default_gain) {
                  draw(
                      painter, geometry, pulses, image.size(), Qt::cyan,
                      "medium", "medium", "balanced", age
                  );
              } else {
                  draw(
                      painter, geometry, pulses, image.size(), Qt::cyan,
                      "medium", "medium", "balanced", age, gain
                  );
              }
              painter.end();
              return image;
          };
    const auto standard = render(1);
    QCOMPARE(standard, render(1, true));
    QCOMPARE(standard, render(0));
    QCOMPARE(standard, render(-10));
    QCOMPARE(standard, render(std::numeric_limits<double>::quiet_NaN()));
    QCOMPARE(standard, render(std::numeric_limits<double>::infinity()));
    QCOMPARE(render(3), render(999));
    QVERIFY(standard != render(2));
    QVERIFY(render(2) != render(3));
    QCOMPARE(displacement_px(geometry, 0, 45, 200, pulses), displacement);
    QImage empty(240, 150, QImage::Format_ARGB32_Premultiplied);
    empty.fill(Qt::transparent);
    QCOMPARE(render(3, false, int(pulse_ttl_ms)), empty);
}
