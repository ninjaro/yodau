#include "widgets/board.hpp"

#include <QVBoxLayout>

#include "widgets/grid_view.hpp"
#include "widgets/stream_cell.hpp"

board::board(QWidget* parent)
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

grid_view* board::grid_mode() const { return grid; }

stream_cell* board::active_cell() const { return active_tile; }

void board::set_active_stream(const QString& name) {
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
    cell->set_active(true);

    cell->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    cell->show();
    active_layout->addWidget(cell);
    active_tile = cell;

    active_container->updateGeometry();
}

void board::clear_active() {
    qDebug() << "board::clear_active"
             << "active_tile="
             << (active_tile ? active_tile->get_name() : "null");

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

stream_cell* board::take_active_cell() {
    qDebug() << "board::take_active_cell"
             << "active_tile="
             << (active_tile ? active_tile->get_name() : "null");

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
