#include "widgets/view_zone.hpp"

#include <QStackedWidget>
#include <QVBoxLayout>

#include "widgets/carousel_view.hpp"
#include "widgets/grid_view.hpp"

view_zone::view_zone(QWidget* parent)
    : QWidget(parent)
    , mode_stack(new QStackedWidget(this))
    , grid(new grid_view(this))
    , carousel(new carousel_view(this)) {
    mode_stack->addWidget(grid);
    mode_stack->addWidget(carousel);
    mode_stack->setCurrentWidget(grid);

    const auto layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(mode_stack);
    setLayout(layout);
}

void view_zone::show_grid() const { mode_stack->setCurrentWidget(grid); }

void view_zone::show_carousel() const {
    mode_stack->setCurrentWidget(carousel);
}
