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

stream_cell* grid_view::peek_stream_cell(const QString& name) const {
    const auto it = tiles.find(name);
    if (it == tiles.end()) {
        return nullptr;
    }
    return it.value();
}

void grid_view::close_requested(const QString& name) {
    remove_stream(name);
    emit stream_closed(name);
}

void grid_view::enlarge_requested(const QString& name) {
    emit stream_enlarge(name);
}

static int ceil_div(const int a, const int b) { return (a + b - 1) / b; }

void grid_view::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuild_layout();
}

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
        this->hide();
        updateGeometry();
        return;
    }
    this->show();

    const int viewport_w = scroll->viewport()->width();
    const int viewport_h = scroll->viewport()->height();

    const QMargins margins = grid_layout->contentsMargins();
    int available_width = viewport_w - margins.left() - margins.right();
    int available_height = viewport_h - margins.top() - margins.bottom();

    if (available_width <= 0) {
        available_width = kMinTileW;
    }
    if (available_height <= 0) {
        available_height = kMinTileH;
    }

    int h_spacing = grid_layout->horizontalSpacing();
    if (h_spacing < 0) {
        h_spacing = grid_layout->spacing();
    }
    int v_spacing = grid_layout->verticalSpacing();
    if (v_spacing < 0) {
        v_spacing = grid_layout->spacing();
    }

    const int denom = kMinTileH + v_spacing;
    int max_rows = 1;
    if (denom > 0) {
        max_rows = (available_height + v_spacing) / denom;
    }
    if (max_rows < 1) {
        max_rows = 1;
    }
    if (max_rows > n) {
        max_rows = n;
    }

    const qint64 overflow_weight = 1000000;
    const qint64 slack_weight = 1;
    const qint64 row_weight = kMinTileH;

    int best_rows = 1;
    int best_cols = n;
    qint64 best_score = 0;
    bool has_best = false;

    for (int rows = 1; rows <= max_rows; ++rows) {
        const int cols = ceil_div(n, rows);
        const int cols_minus_one = cols > 0 ? cols - 1 : 0;
        const int needed_width = cols * kMinTileW + cols_minus_one * h_spacing;

        const int overflow = needed_width > available_width
            ? needed_width - available_width
            : 0;
        const int slack = available_width > needed_width
            ? available_width - needed_width
            : 0;

        const qint64 score = static_cast<qint64>(overflow) * overflow_weight
            + static_cast<qint64>(slack) * slack_weight
            + static_cast<qint64>(rows) * row_weight;

        if (!has_best || score < best_score) {
            has_best = true;
            best_score = score;
            best_rows = rows;
            best_cols = cols;
        }
    }

    const int rows = best_rows;
    const int cols = best_cols;

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