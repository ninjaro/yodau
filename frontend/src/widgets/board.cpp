#include "widgets/board.hpp"

#include <QStackedWidget>
#include <QVBoxLayout>

#include "widgets/carousel_view.hpp"
#include "widgets/grid_view.hpp"

board::board(QWidget* parent)
    : QWidget(parent)
    , mode_stack(new QStackedWidget(this))
    , grid(new grid_view(mode_stack))
    , carousel(new carousel_view(mode_stack)) {
    mode_stack->addWidget(grid);
    mode_stack->addWidget(carousel);
    mode_stack->setCurrentWidget(grid);

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mode_stack);
    // setLayout(layout);
}

void board::show_grid() { mode_stack->setCurrentWidget(grid); }

void board::show_carousel() { mode_stack->setCurrentWidget(carousel); }

grid_view* board::grid_mode() const { return grid; }

carousel_view* board::carousel_mode() const { return carousel; }
