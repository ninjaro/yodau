#include "widgets/grid_view.hpp"
#include "widgets/stream_cell.hpp"

#include <QGridLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QtMath>

static constexpr int kMinTileW = 240;
static constexpr int kMinTileH = 160;

grid_view::grid_view(QWidget* parent)
    : QWidget(parent)
    , scroll(new QScrollArea(this))
    , grid_container(new QWidget(scroll))
    , grid_layout(new QGridLayout(grid_container)) {
    grid_layout->setContentsMargins(6, 6, 6, 6);
    grid_layout->setSpacing(6);

    grid_container->setLayout(grid_layout);

    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setWidget(grid_container);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    setLayout(outer);
}

bool grid_view::has_stream(const QString& name) const {
    return tiles.contains(name);
}

QStringList grid_view::stream_names() const { return tiles.keys(); }

stream_cell* grid_view::take_stream_cell(const QString& name) {
    auto it = tiles.find(name);
    if (it == tiles.end()) {
        return nullptr;
    }

    stream_cell* cell = it.value();

    tiles.erase(it);
    grid_layout->removeWidget(cell);
    cell->hide();

    rebuild_layout();
    return cell;
}

void grid_view::put_stream_cell(stream_cell* cell) {
    if (!cell) {
        return;
    }

    const QString name = cell->get_name();
    if (tiles.contains(name)) {
        return;
    }

    cell->setParent(grid_container);
    tiles.insert(name, cell);
    cell->show();

    rebuild_layout();
}

void grid_view::add_stream(const QString& name) {
    if (name.isEmpty() || tiles.contains(name)) {
        return;
    }

    auto* tile = new stream_cell(name, grid_container);
    tile->setMinimumSize(kMinTileW, kMinTileH);
    tile->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    connect(
        tile, &stream_cell::request_close, this, &grid_view::close_requested
    );
    connect(
        tile, &stream_cell::request_focus, this, &grid_view::enlarge_requested
    );

    tiles.insert(name, tile);
    rebuild_layout();
}

// void grid_view::set_active_stream(const QString& name) {
//     for (auto it = tiles.begin(); it != tiles.end(); ++it) {
//         auto* tile = it.value();
//         const bool is_act = (!name.isEmpty() && it.key() == name);
//         tile->set_active(is_act);
//     }
// }

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
    // if (!tiles.contains(name)) {
    //     return;
    // }
    remove_stream(name);
    emit stream_closed(name);
}

void grid_view::enlarge_requested(const QString& name) {
    // if (!tiles.contains(name)) {
    //     return;
    // }
    emit stream_enlarge(name);
}

static int ceil_div(const int a, const int b) { return (a + b - 1) / b; }

void grid_view::rebuild_layout() {
    // todo fix layout
    while (grid_layout->count() > 0) {
        auto* item = grid_layout->takeAt(0);
        if (item->widget()) {
            item->widget()->hide();
        }
        delete item;
    }

    const int n = static_cast<int>(tiles.size());
    if (n == 0) {
        this->hide();
        updateGeometry();
        return;
    }
    this->show();

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

    grid_container->updateGeometry();
}