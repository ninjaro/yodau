#include "widgets/stream_cell.hpp"

#include <QImage>
#include <QtTest/QtTest>

namespace {

stream_cell::line_instance sample_line() {
    stream_cell::line_instance line_value;
    line_value.template_name = QStringLiteral("bench-line");
    line_value.color = QColor(QStringLiteral("#4fd1b5"));
    line_value.closed = false;
    line_value.pts_pct = {
        QPointF(16.0, 22.0),
        QPointF(52.0, 38.0),
        QPointF(81.0, 66.0),
    };
    return line_value;
}

QImage render_cell(stream_cell& cell) {
    QImage image(cell.size(), QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    cell.render(&image);
    return image;
}

} // namespace

class stream_cell_benchmarks final : public QObject {
    Q_OBJECT

private slots:
    void render_idle_tile();
    void render_line_overlay_tile();
};

void stream_cell_benchmarks::render_idle_tile() {
    stream_cell cell(QStringLiteral("bench-idle"));
    cell.resize(320, 200);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );

    QBENCHMARK {
        const QImage image = render_cell(cell);
        QVERIFY(!image.isNull());
    }
}

void stream_cell_benchmarks::render_line_overlay_tile() {
    stream_cell cell(QStringLiteral("bench-overlay"));
    cell.resize(320, 200);
    cell.setStyleSheet(
        QStringLiteral("background-color: black; color: white;")
    );
    cell.set_persistent_lines({ sample_line() });
    cell.highlight_line_at(QStringLiteral("bench-line"), QPointF(52.0, 38.0));

    QBENCHMARK {
        const QImage image = render_cell(cell);
        QVERIFY(!image.isNull());
    }
}

QTEST_MAIN(stream_cell_benchmarks)

#include "stream_cell_benchmarks.moc"
