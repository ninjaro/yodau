#include "widgets/stream_inventory_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/log_area_view.hpp"

#include <QHeaderView>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

stream_inventory_panel::stream_inventory_panel(QWidget* parent)
    : QWidget(parent)
    , streams_list(new QTreeWidget(this))
    , event_log_view(new log_area_view(frontend_log_area::streams, this)) {
    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    streams_list->setColumnCount(3);
    streams_list->setHeaderLabels(
        { str_label("show"), str_label("name"), str_label("source") }
    );
    streams_list->header()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents
    );
    streams_list->header()->setSectionResizeMode(1, QHeaderView::Stretch);
    streams_list->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    layout->addWidget(streams_list);

    connect(
        streams_list, &QTreeWidget::itemChanged, this,
        &stream_inventory_panel::on_stream_item_changed
    );

    event_log_view->setObjectName(QStringLiteral("settings_streams_log_view"));
    event_log_view->setMinimumHeight(160);
    layout->addWidget(event_log_view);
}

void stream_inventory_panel::add_stream_entry(
    const QString& name, const QString& source, const bool checked
) const {
    QSignalBlocker blocker(streams_list);
    for (int i = 0; i < streams_list->topLevelItemCount(); ++i) {
        const auto item = streams_list->topLevelItem(i);
        if (item->text(1) == name) {
            return;
        }
    }

    const auto item = new QTreeWidgetItem();
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
    item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
    item->setText(1, name);
    item->setText(2, source);
    streams_list->addTopLevelItem(item);
}

void stream_inventory_panel::set_stream_checked(
    const QString& name, const bool checked
) const {
    for (int i = 0; i < streams_list->topLevelItemCount(); ++i) {
        const auto item = streams_list->topLevelItem(i);
        if (item->text(1) == name) {
            item->setCheckState(0, checked ? Qt::Checked : Qt::Unchecked);
            break;
        }
    }
}

void stream_inventory_panel::remove_stream_entry(const QString& name) const {
    for (int i = 0; i < streams_list->topLevelItemCount(); ++i) {
        const auto item = streams_list->topLevelItem(i);
        if (item->text(1) == name) {
            delete streams_list->takeTopLevelItem(i);
            break;
        }
    }
}

void stream_inventory_panel::clear_stream_entries() const {
    streams_list->clear();
}

void stream_inventory_panel::set_log_toolbar(log_toolbar_panel* toolbar) {
    if (event_log_view != nullptr) {
        event_log_view->set_log_toolbar(toolbar);
    }
}

bool stream_inventory_panel::append_log_entry(
    const frontend_log_entry& entry
) const {
    return event_log_view != nullptr && event_log_view->append_entry(entry);
}

void stream_inventory_panel::on_stream_item_changed(
    QTreeWidgetItem* item, const int column
) {
    if (item == nullptr || column != 0) {
        return;
    }

    emit show_stream_changed(
        item->text(1), item->checkState(0) == Qt::Checked
    );
}
