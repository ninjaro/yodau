#include "widgets/grid_view.hpp"
#include "packing/layout/fixed_size_layout.hpp"
#include "packing/layout/free_order_layout.hpp"
#include "packing/layout/ordered_layout.hpp"
#include "widgets/stream_cell.hpp"

#include <QEvent>
#include <QMessageBox>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>
#include <QtMath>

#include <algorithm>
#include <vector>

namespace {

constexpr int fallback_tile_width = 240;
constexpr int adaptive_minimum_short_side = 160;
constexpr int manual_minimum_short_side = 96;
constexpr int layout_margin = 6;
constexpr int layout_spacing = 6;

packing::scroll_policy
packing_policy(const grid_view::scroll_direction direction) noexcept {
    return direction == grid_view::scroll_direction::horizontal
        ? packing::scroll_policy::horizontal
        : packing::scroll_policy::vertical;
}

struct stream_layout_plan {
    std::vector<packing::rectangle> rectangles;
    std::vector<packing::ordered_group> groups;
    packing::extent content;
    packing::shelf_axis axis { packing::shelf_axis::columns };
};

} // namespace

grid_view::grid_view(QWidget* parent)
    : QWidget(parent)
    , scroll(new QScrollArea(this))
    , grid_container(new QWidget(scroll)) {
    scroll->setObjectName(QStringLiteral("stream_grid_scroll_area"));
    scroll->setWidgetResizable(false);
    scroll->setWidget(grid_container);
    scroll->viewport()->installEventFilter(this);
    update_scrollbar_policies();

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

grid_view::scroll_direction
grid_view::current_scroll_direction() const noexcept {
    return scroll_direction_;
}

bool grid_view::preserves_stream_order() const noexcept {
    return preserve_stream_order_;
}

grid_view::sizing_mode grid_view::current_sizing_mode() const noexcept {
    return sizing_mode_;
}

void grid_view::add_stream(const QString& name) {
    if (name.isEmpty() || tiles.contains(name)) {
        return;
    }

    auto* tile = new stream_cell(name, grid_container);
    tile->set_application_active(application_active_);
    tile->setMinimumSize(1, 1);
    tile->set_fixed_size_controls_visible(sizing_mode_ == sizing_mode::fixed);
    if (sizing_mode_ == sizing_mode::fixed) {
        tile->initialize_manual_short_side(adaptive_minimum_short_side);
    }

    connect(
        tile, &stream_cell::request_close, this, &grid_view::close_requested
    );
    connect(
        tile, &stream_cell::request_focus, this, &grid_view::enlarge_requested
    );
    connect(
        tile, &stream_cell::layout_geometry_changed, this,
        &grid_view::rebuild_layout
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
    cell->set_fixed_size_controls_visible(sizing_mode_ == sizing_mode::fixed);
    if (sizing_mode_ == sizing_mode::fixed && !cell->has_manual_short_side()) {
        cell->initialize_manual_short_side(adaptive_minimum_short_side);
    }
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

void grid_view::set_scroll_direction(const scroll_direction direction) {
    if (scroll_direction_ == direction) {
        return;
    }
    scroll_direction_ = direction;
    update_scrollbar_policies();
    rebuild_layout();
}

void grid_view::set_preserve_stream_order(const bool preserve) {
    if (preserve_stream_order_ == preserve) {
        return;
    }
    preserve_stream_order_ = preserve;
    rebuild_layout();
}

void grid_view::set_sizing_mode(const sizing_mode mode) {
    if (sizing_mode_ == mode) {
        return;
    }

    if (mode == sizing_mode::fixed) {
        bool reset_sizes = !has_remembered_manual_sizes_;
        if (has_remembered_manual_sizes_) {
            reset_sizes
                = QMessageBox::question(
                      this, tr("Restore fixed stream sizes?"),
                      tr("Restore the manual stream sizes from this "
                         "session? Choose No to initialize them from "
                         "the current adaptive layout."),
                      QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes
                  )
                == QMessageBox::No;
        }
        if (reset_sizes) {
            for (stream_cell* tile : tiles) {
                tile->initialize_manual_short_side(
                    std::max(
                        manual_minimum_short_side,
                        std::min(tile->width(), tile->height())
                    )
                );
            }
        }
    } else if (sizing_mode_ == sizing_mode::fixed) {
        has_remembered_manual_sizes_ = !tiles.isEmpty();
    }

    sizing_mode_ = mode;
    update_scrollbar_policies();
    for (stream_cell* tile : tiles) {
        tile->set_fixed_size_controls_visible(mode == sizing_mode::fixed);
    }
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

bool grid_view::eventFilter(QObject* watched, QEvent* event) {
    if (watched == scroll->viewport() && event->type() == QEvent::Resize) {
        schedule_rebuild();
    }
    return QWidget::eventFilter(watched, event);
}

void grid_view::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    rebuild_layout();
}

void grid_view::schedule_rebuild() {
    if (layout_rebuild_pending_) {
        return;
    }
    layout_rebuild_pending_ = true;
    QTimer::singleShot(0, this, [this]() {
        layout_rebuild_pending_ = false;
        rebuild_layout();
    });
}

void grid_view::update_scrollbar_policies() {
    const bool horizontal = scroll_direction_ == scroll_direction::horizontal;
    const Qt::ScrollBarPolicy cross_policy = sizing_mode_ == sizing_mode::fixed
        ? Qt::ScrollBarAsNeeded
        : Qt::ScrollBarAlwaysOff;
    scroll->setHorizontalScrollBarPolicy(
        horizontal ? Qt::ScrollBarAsNeeded : cross_policy
    );
    scroll->setVerticalScrollBarPolicy(
        horizontal ? cross_policy : Qt::ScrollBarAsNeeded
    );
}

void grid_view::rebuild_layout() {
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

    int available_width = viewport_w - layout_margin * 2;
    int available_height = viewport_h - layout_margin * 2;

    if (available_width <= 0) {
        available_width = fallback_tile_width;
    }
    if (available_height <= 0) {
        available_height = adaptive_minimum_short_side;
    }

    const packing::extent viewport {
        static_cast<double>(available_width),
        static_cast<double>(available_height),
    };
    stream_layout_plan plan;
    if (sizing_mode_ == sizing_mode::adaptive) {
        std::vector<packing::ordered_slot> items(static_cast<std::size_t>(n));
        std::size_t item_index = 0;
        for (auto it = tiles.cbegin(); it != tiles.cend(); ++it, ++item_index) {
            items[item_index].aspect = it.value()->source_frame_aspect();
            items[item_index].rotated = it.value()->quarter_turns() % 2 != 0;
        }

        if (preserve_stream_order_) {
            packing::ordered_layout_result ordered
                = packing::layout_ordered_slots(
                    {
                        .viewport = viewport,
                        .items = items,
                        .policy = packing_policy(scroll_direction_),
                        .minimum_short_side = adaptive_minimum_short_side,
                        .spacing = layout_spacing,
                    }
                );
            plan = {
                .rectangles = std::move(ordered.rectangles),
                .groups = std::move(ordered.groups),
                .content = ordered.content,
                .axis = ordered.axis,
            };
        } else {
            packing::free_order_layout_result free
                = packing::layout_free_order_slots(
                    {
                        .viewport = viewport,
                        .items = items,
                        .policy = packing_policy(scroll_direction_),
                        .minimum_short_side = adaptive_minimum_short_side,
                        .spacing = layout_spacing,
                    }
                );
            plan = {
                .rectangles = std::move(free.rectangles),
                .groups = std::move(free.groups),
                .content = free.content,
                .axis = free.axis,
            };
        }
    } else {
        std::vector<packing::fixed_size_item> items;
        items.reserve(static_cast<std::size_t>(n));
        packing::extent fixed_viewport = viewport;
        for (stream_cell* tile : tiles) {
            if (!tile->has_manual_short_side()) {
                tile->initialize_manual_short_side(adaptive_minimum_short_side);
            }
            const double short_side = std::max(
                static_cast<double>(manual_minimum_short_side),
                tile->manual_short_side()
            );
            const double aspect = tile->effective_display_aspect();
            const packing::extent size = aspect >= 1.0
                ? packing::extent { short_side * aspect, short_side }
                : packing::extent { short_side, short_side / aspect };
            items.push_back({ .size = size });
            if (scroll_direction_ == scroll_direction::horizontal) {
                fixed_viewport.height
                    = std::max(fixed_viewport.height, size.height);
            } else {
                fixed_viewport.width
                    = std::max(fixed_viewport.width, size.width);
            }
        }
        packing::fixed_size_layout_result fixed
            = packing::layout_fixed_size_rectangles(
                {
                    .viewport = fixed_viewport,
                    .items = items,
                    .policy = packing_policy(scroll_direction_),
                    .spacing = layout_spacing,
                    .algorithm = preserve_stream_order_
                        ? packing::fixed_size_layout_algorithm::
                              preserve_order_shelf
                        : packing::fixed_size_layout_algorithm::automatic,
                }
            );
        plan = {
            .rectangles = std::move(fixed.rectangles),
            .groups = std::move(fixed.groups),
            .content = fixed.content,
            .axis = fixed.axis,
        };
    }

    int rows = 1;
    int cols = n;
    if (plan.rectangles.size() == static_cast<std::size_t>(n)
        && !plan.groups.empty()) {
        const int group_count = static_cast<int>(plan.groups.size());
        int maximum_group_size = 0;
        for (const packing::ordered_group group : plan.groups) {
            maximum_group_size = std::max(
                maximum_group_size, static_cast<int>(group.end - group.begin)
            );
        }
        if (plan.axis == packing::shelf_axis::columns) {
            rows = maximum_group_size;
            cols = group_count;
        } else {
            rows = group_count;
            cols = maximum_group_size;
        }
    }
    last_layout_rows = rows;
    last_layout_columns = cols;

    std::vector<stream_cell*> ordered_tiles;
    ordered_tiles.reserve(static_cast<std::size_t>(n));
    for (auto it = tiles.cbegin(); it != tiles.cend(); ++it) {
        ordered_tiles.push_back(it.value());
    }

    if (plan.rectangles.size() == ordered_tiles.size()) {
        for (std::size_t index = 0; index < ordered_tiles.size(); ++index) {
            const packing::rectangle& placed = plan.rectangles[index];
            stream_cell* tile = ordered_tiles[index];
            tile->setGeometry(
                layout_margin + qRound(placed.x),
                layout_margin + qRound(placed.y),
                std::max(1, qRound(placed.width)),
                std::max(1, qRound(placed.height))
            );
            tile->show();
        }

        const int content_width = layout_margin * 2 + qCeil(plan.content.width);
        const int content_height
            = layout_margin * 2 + qCeil(plan.content.height);
        grid_container->setFixedSize(
            std::max(viewport_w, content_width),
            std::max(viewport_h, content_height)
        );
    } else {
        // Valid widget dimensions should always produce a shared-library plan.
        // Keep a deterministic one-row fallback for startup/transient geometry.
        for (int index = 0; index < n; ++index) {
            stream_cell* tile = ordered_tiles[static_cast<std::size_t>(index)];
            tile->setGeometry(
                layout_margin + index * (fallback_tile_width + layout_spacing),
                layout_margin, fallback_tile_width, adaptive_minimum_short_side
            );
            tile->show();
        }
        grid_container->setFixedSize(
            layout_margin * 2 + n * fallback_tile_width
                + std::max(0, n - 1) * layout_spacing,
            layout_margin * 2 + adaptive_minimum_short_side
        );
    }
}
