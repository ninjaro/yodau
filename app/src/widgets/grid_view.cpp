#include "widgets/grid_view.hpp"
#include "packing/ordered_layout.hpp"
#include "widgets/stream_cell.hpp"

#include <QGridLayout>
#include <QLayoutItem>
#include <QScrollArea>
#include <QVBoxLayout>

#include <algorithm>
#include <vector>

static constexpr int minimum_tile_width = 240;
static constexpr int minimum_tile_height = 160;

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

int grid_view::layout_row_count() const { return last_layout_rows; }

int grid_view::layout_column_count() const { return last_layout_columns; }

int grid_view::layout_cell_count() const {
    return last_layout_rows * last_layout_columns;
}

void grid_view::add_stream(const QString& name) {
    if (name.isEmpty() || tiles.contains(name)) {
        return;
    }

    auto* tile = new stream_cell(name, grid_container);
    tile->set_application_active(application_active_);
    tile->setMinimumSize(minimum_tile_width, minimum_tile_height);
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
    cell->set_application_active(application_active_);
    tiles.insert(name, cell);
    cell->show();

    rebuild_layout();
}

void grid_view::set_application_active(const bool active) {
    application_active_ = active;
    for (stream_cell* tile : tiles) {
        if (tile != nullptr) {
            tile->set_application_active(active);
        }
    }
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
        last_layout_rows = 0;
        last_layout_columns = 0;
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
        available_width = minimum_tile_width;
    }
    if (available_height <= 0) {
        available_height = minimum_tile_height;
    }

    int h_spacing = grid_layout->horizontalSpacing();
    if (h_spacing < 0) {
        h_spacing = grid_layout->spacing();
    }
    int v_spacing = grid_layout->verticalSpacing();
    if (v_spacing < 0) {
        v_spacing = grid_layout->spacing();
    }

    std::vector<packing::ordered_slot> ordered_items(
        static_cast<std::size_t>(n)
    );
    for (packing::ordered_slot& slot : ordered_items) {
        slot.aspect = static_cast<double>(minimum_tile_width)
            / static_cast<double>(minimum_tile_height);
    }
    const double spacing = static_cast<double>(std::max(h_spacing, v_spacing));
    const packing::ordered_layout_result plan = packing::layout_ordered_slots(
        {
            .viewport = { static_cast<double>(available_width),
                          static_cast<double>(available_height) },
            .items = ordered_items,
            .policy = packing::scroll_policy::horizontal,
            .minimum_short_side = static_cast<double>(minimum_tile_height),
            .spacing = spacing,
        }
    );

    int rows = 1;
    int cols = n;
    if (plan.rectangles.size() == ordered_items.size() && !plan.groups.empty()
        && plan.axis == packing::shelf_axis::columns) {
        rows = 0;
        cols = static_cast<int>(plan.groups.size());
        for (const packing::ordered_group group : plan.groups) {
            rows = std::max(rows, static_cast<int>(group.end - group.begin));
        }
    }
    last_layout_rows = rows;
    last_layout_columns = cols;

    for (int c = 0; c < cols; ++c) {
        grid_layout->setColumnStretch(c, 1);
    }
    for (int r = 0; r < rows; ++r) {
        grid_layout->setRowStretch(r, 1);
    }

    std::vector<stream_cell*> ordered_tiles;
    ordered_tiles.reserve(static_cast<std::size_t>(n));
    for (auto it = tiles.cbegin(); it != tiles.cend(); ++it) {
        ordered_tiles.push_back(it.value());
    }

    if (plan.rectangles.size() == ordered_tiles.size() && !plan.groups.empty()
        && plan.axis == packing::shelf_axis::columns) {
        for (std::size_t column = 0; column < plan.groups.size(); ++column) {
            const packing::ordered_group group = plan.groups[column];
            for (std::size_t index = group.begin; index < group.end; ++index) {
                stream_cell* tile = ordered_tiles[index];
                tile->show();
                grid_layout->addWidget(
                    tile, static_cast<int>(index - group.begin),
                    static_cast<int>(column)
                );
            }
        }
    } else {
        // Valid widget dimensions should always produce a shared-library plan.
        // Keep a deterministic one-row fallback for startup/transient geometry.
        for (int index = 0; index < n; ++index) {
            stream_cell* tile = ordered_tiles[static_cast<std::size_t>(index)];
            tile->show();
            grid_layout->addWidget(tile, 0, index);
        }
    }

    grid_container->updateGeometry();
}
