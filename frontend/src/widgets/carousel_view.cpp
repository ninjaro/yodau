#include "widgets/carousel_view.hpp"
#include "widgets/stream_cell.hpp"

#include <QDockWidget>
#include <QHBoxLayout>
#include <QLayoutItem>
#include <QMainWindow> // todo: remove
#include <QResizeEvent>
#include <QScrollArea>
#include <QVBoxLayout>

carousel_view::carousel_view(QWidget* parent)
    : QWidget(parent)
    , root(new QMainWindow(this))
    , active_container(new QWidget(root))
    , active_layout(new QVBoxLayout(active_container))
    , dock(new QDockWidget(tr("streams"), root))
    , scroll(new QScrollArea(dock))
    , strip_container(new QWidget(scroll))
    , strip_layout(new QHBoxLayout(strip_container)) {

    active_layout->setContentsMargins(6, 6, 6, 6);
    active_layout->setSpacing(6);
    active_container->setLayout(active_layout);
    root->setCentralWidget(active_container);

    dock->setAllowedAreas(Qt::TopDockWidgetArea | Qt::BottomDockWidgetArea);
    dock->setFeatures(QDockWidget::DockWidgetMovable);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    strip_layout->setContentsMargins(6, 6, 6, 6);
    strip_layout->setSpacing(6);
    strip_layout->addStretch();
    strip_container->setLayout(strip_layout);
    scroll->setWidget(strip_container);
    dock->setWidget(scroll);

    root->addDockWidget(Qt::BottomDockWidgetArea, dock);

    const auto outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(root);

    adjust_dock_height();
}

bool carousel_view::has_stream(const QString& name) const {
    return tiles.contains(name);
}

QStringList carousel_view::stream_names() const {
    QStringList out;
    out.reserve(tiles.size());
    for (auto it = tiles.cbegin(); it != tiles.cend(); ++it) {
        out << it.key();
    }
    return out;
}

QString carousel_view::active_stream() const { return active_name; }

void carousel_view::add_stream(const QString& name) {
    if (name.isEmpty() || tiles.contains(name)) {
        return;
    }

    auto* tile = new stream_cell(name, strip_container);
    tile->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    connect(
        tile, &stream_cell::request_close, this, &carousel_view::close_requested
    );
    connect(
        tile, &stream_cell::request_focus, this, &carousel_view::focus_requested
    );

    tiles.insert(name, tile);

    if (!active_tile) {
        set_active_stream(name);
    } else {
        rebuild_strip();
    }
}

void carousel_view::remove_stream(const QString& name) {
    const auto it = tiles.find(name);
    if (it == tiles.end()) {
        return;
    }

    auto* tile = it.value();
    tiles.erase(it);

    if (tile == active_tile) {
        active_layout->removeWidget(tile);
        active_tile = nullptr;
        active_name.clear();
        tile->deleteLater();

        if (!tiles.isEmpty()) {
            set_active_stream(tiles.firstKey());
        } else {
            rebuild_strip();
        }
        return;
    }

    strip_layout->removeWidget(tile);
    tile->deleteLater();

    rebuild_strip();
}

void carousel_view::close_requested(const QString& name) {
    if (!tiles.contains(name)) {
        return;
    }
    remove_stream(name);
    emit stream_closed(name);
}

void carousel_view::focus_requested(const QString& name) {
    if (!tiles.contains(name)) {
        return;
    }

    if (name == active_name) {
        emit stream_enlarge(name);
        return;
    }

    set_active_stream(name);
    emit stream_enlarge(name);
}

void carousel_view::set_active_stream(const QString& name) {
    if (!tiles.contains(name)) {
        return;
    }
    if (active_name == name) {
        return;
    }

    if (active_tile) {
        active_tile->set_active(false);
        active_layout->removeWidget(active_tile);
    }

    auto* new_tile = tiles.value(name);
    strip_layout->removeWidget(new_tile);

    active_layout->addWidget(new_tile);
    new_tile->set_active(true);

    active_tile = new_tile;
    active_name = name;

    rebuild_strip();
    adjust_dock_height();
}

void carousel_view::rebuild_strip() {
    while (strip_layout->count() > 0) {
        auto* item = strip_layout->takeAt(0);
        if (auto* w = item->widget()) {
            w->hide();
        }
        delete item;
    }

    for (auto it = tiles.cbegin(); it != tiles.cend(); ++it) {
        auto* tile = it.value();
        if (tile == active_tile) {
            continue;
        }
        tile->set_active(false);
        tile->show();
        strip_layout->addWidget(tile);
    }

    strip_layout->addStretch();
}

void carousel_view::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    adjust_dock_height();
}

void carousel_view::adjust_dock_height() {
    const int h = height();
    constexpr int min_h = 110;
    const int target = qMax(min_h, h / 4);

    // dock->setMinimumHeight(target);
    dock->setMaximumHeight(target);
}
