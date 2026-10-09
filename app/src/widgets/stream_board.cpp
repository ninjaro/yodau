#include "widgets/stream_board.hpp"

#include <QBoxLayout>
#include <QEvent>
#include <QVBoxLayout>

#include "widgets/file_transport.hpp"
#include "widgets/grid_view.hpp"
#include "widgets/stream_cell.hpp"

stream_board::stream_board(QWidget* parent)
    : QWidget(parent)
    , grid(new grid_view(this))
    , active_container(new QWidget(this))
    , active_layout(new QVBoxLayout(active_container))
    , active_tile(nullptr)
    , board_layout(new QBoxLayout(QBoxLayout::TopToBottom, this)) {
    active_layout->setContentsMargins(6, 6, 6, 6);
    active_layout->setSpacing(6);
    active_container->setLayout(active_layout);

    board_layout->setContentsMargins(0, 0, 0, 0);
    board_layout->setSpacing(6);
    board_layout->addWidget(active_container, 3);
    board_layout->addWidget(grid, 1);
    active_container->hide();
}

grid_view* stream_board::grid_mode() const { return grid; }

stream_cell* stream_board::active_cell() const { return active_tile; }

void stream_board::add_canvas_tools(QWidget* tools) {
    if (tools != nullptr) {
        tools->setParent(active_container);
        active_layout->insertWidget(0, tools);
    }
}

pool_side stream_board::current_pool_side() const { return pool_side_; }

void stream_board::set_pool_side(const pool_side side) {
    switch (side) {
    case pool_side::bottom:
    case pool_side::right:
    case pool_side::left:
        break;
    default:
        return;
    }
    if (side == pool_side_) {
        return;
    }
    pool_side_ = side;
    update_pool_layout();
}

void stream_board::update_pool_layout() {
    auto direction = QBoxLayout::TopToBottom;
    if (pool_side_ != pool_side::bottom) {
        // QBoxLayout mirrors horizontal directions in RTL. The side choice
        // describes physical canvas placement, not text reading order.
        const bool pool_on_left = pool_side_ == pool_side::left;
        const bool rtl = layoutDirection() == Qt::RightToLeft;
        direction = pool_on_left != rtl ? QBoxLayout::RightToLeft
                                        : QBoxLayout::LeftToRight;
    }
    board_layout->setDirection(direction);
    updateGeometry();
}

void stream_board::changeEvent(QEvent* event) {
    QWidget::changeEvent(event);
    if (event->type() == QEvent::LayoutDirectionChange) {
        update_pool_layout();
    }
}

void stream_board::set_active_stream(const QString& name) {
    if (!grid || name.isEmpty()) {
        return;
    }
    if (active_tile && active_tile->get_name() == name) {
        return;
    }

    // Resolve the replacement before changing the current view: a stale
    // selection must leave both the active cell and pool membership intact.
    stream_cell* cell = grid->take_stream_cell(name);
    if (!cell) {
        return;
    }

    if (active_tile) {
        active_layout->removeWidget(active_tile);
        active_tile->set_active(false);
        grid->put_stream_cell(active_tile);
        active_tile = nullptr;
    }

    active_container->show();

    cell->setParent(active_container);
    cell->set_application_active(application_active_);
    cell->set_active(true);

    cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cell->show();
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    active_layout->addWidget(cell);
#else
    if (file_controls_ == nullptr) {
        file_controls_ = new file_transport(active_container);
        active_layout->addWidget(file_controls_);
    }
    active_layout->insertWidget(active_layout->indexOf(file_controls_), cell);
    file_controls_->set_stream(cell);
#endif
    active_tile = cell;

    active_container->updateGeometry();
}

void stream_board::clear_active() {
    if (!active_tile || !grid) {
        return;
    }
    if (file_controls_ != nullptr) {
        file_controls_->set_stream(nullptr);
    }
    active_layout->removeWidget(active_tile);
    active_tile->set_active(false);
    grid->put_stream_cell(active_tile);
    active_tile = nullptr;

    active_container->hide();
    active_container->updateGeometry();
}

stream_cell* stream_board::take_active_cell() {
    if (!active_tile) {
        return nullptr;
    }
    if (file_controls_ != nullptr) {
        file_controls_->set_stream(nullptr);
    }

    active_layout->removeWidget(active_tile);
    active_tile->set_active(false);

    active_container->hide();
    active_container->updateGeometry();

    stream_cell* out = active_tile;
    active_tile = nullptr;
    return out;
}

void stream_board::set_application_active(const bool active) {
    application_active_ = active;
    if (active_tile != nullptr) {
        active_tile->set_application_active(active);
    }
    if (grid != nullptr) {
        grid->set_application_active(active);
    }
    if (file_controls_ != nullptr) {
        file_controls_->set_stream(active_tile);
    }
}
