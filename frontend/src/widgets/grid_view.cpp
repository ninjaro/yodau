#include "widgets/grid_view.hpp"
#include "widgets/stream_cell.hpp"

#include <QGridLayout>
#include <QLayoutItem>
#include <QtMath>

grid_view::grid_view(QWidget* parent)
    : QWidget(parent)
    , grid_layout(new QGridLayout(this)) {
    grid_layout->setContentsMargins(6, 6, 6, 6);
    grid_layout->setSpacing(6);
    // setLayout(grid_layout);
}

bool grid_view::has_stream(const QString& name) const {
    return tiles.contains(name);
}

QStringList grid_view::stream_names() const { return tiles.keys(); }

void grid_view::add_stream(const QString& name) {
    if (name.isEmpty() || tiles.contains(name)) {
        return;
    }

    auto* tile = new stream_cell(name, this);
    tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(
        tile, &stream_cell::request_close, this, &grid_view::close_requested
    );
    connect(
        tile, &stream_cell::request_focus, this, &grid_view::enlarge_requested
    );

    tiles.insert(name, tile);
    rebuild_layout();
}

void grid_view::remove_stream(const QString& name) {
    const auto it = tiles.find(name);
    if (it == tiles.end()) {
        return;
    }

    auto* tile = it.value();
    tiles.erase(it);

    grid_layout->removeWidget(tile);
    tile->deleteLater();

    rebuild_layout();
}

void grid_view::close_requested(const QString& name) {
    if (!tiles.contains(name)) {
        return;
    }
    remove_stream(name);
    emit stream_closed(name);
}

void grid_view::enlarge_requested(const QString& name) {
    if (!tiles.contains(name)) {
        return;
    }
    emit stream_enlarge(name);
}

static int ceil_div(const int a, const int b) { return (a + b - 1) / b; }

void grid_view::rebuild_layout() {
    while (grid_layout->count() > 0) {
        auto* item = grid_layout->takeAt(0);
        if (item->widget()) {
            item->widget()->hide();
        }
        delete item;
    }

    const int n = static_cast<int>(tiles.size());
    if (n == 0) {
        return;
    }

    const int cols = qCeil(qSqrt(static_cast<double>(n)));
    const int rows = ceil_div(n, cols);

    for (int c = 0; c < cols; ++c) {
        grid_layout->setColumnStretch(c, 1);
    }
    for (int r = 0; r < rows; ++r) {
        grid_layout->setRowStretch(r, 1);
    }

    int idx = 0;
    for (auto it = tiles.cbegin(); it != tiles.cend(); ++it, ++idx) {
        const int r = idx / cols;
        const int c = idx % cols;
        auto* tile = it.value();
        tile->show();
        grid_layout->addWidget(tile, r, c);
    }

    updateGeometry();
}
