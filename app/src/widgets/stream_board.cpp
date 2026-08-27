#include "widgets/stream_board.hpp"

#include <QVBoxLayout>

#include "widgets/grid_view.hpp"
#include "widgets/stream_cell.hpp"

stream_board::stream_board(QWidget* parent)
    : QWidget(parent)
    , grid(new grid_view(this))
    , active_container(new QWidget(this))
    , active_layout(new QVBoxLayout(active_container))
    , active_tile(nullptr) {
    active_layout->setContentsMargins(6, 6, 6, 6);
    active_layout->setSpacing(6);
    active_container->setLayout(active_layout);

    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(6);

    outer->addWidget(active_container, 3);

    outer->addWidget(grid, 1);

    setLayout(outer);
    active_container->hide();
}

grid_view* stream_board::grid_mode() const { return grid; }

stream_cell* stream_board::active_cell() const { return active_tile; }

void stream_board::set_active_stream(const QString& name) {
    if (!grid || name.isEmpty()) {
        return;
    }
    if (active_tile && active_tile->get_name() == name) {
        return;
    }

    if (active_tile) {
        active_layout->removeWidget(active_tile);
        active_tile->set_active(false);
        grid->put_stream_cell(active_tile);
        active_tile = nullptr;
    }

    stream_cell* cell = grid->take_stream_cell(name);
    if (!cell) {
        return;
    }
    active_container->show();

    cell->setParent(active_container);
    cell->set_application_active(application_active_);
    cell->set_active(true);

    cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cell->show();
    active_layout->addWidget(cell);
    active_tile = cell;

    active_container->updateGeometry();
}

void stream_board::clear_active() {
    if (!active_tile || !grid) {
        return;
    }
    active_layout->removeWidget(active_tile);
    active_tile->set_active(false);
    grid->put_stream_cell(active_tile);
    // active_tile->deleteLater();
    active_tile = nullptr;

    active_container->hide();
    active_container->updateGeometry();
}

stream_cell* stream_board::take_active_cell() {
    if (!active_tile) {
        return nullptr;
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
}
