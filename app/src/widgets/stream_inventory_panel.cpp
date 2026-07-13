#include "widgets/stream_inventory_panel.hpp"

#include "shell/str_label.hpp"

#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>

stream_inventory_panel::stream_inventory_panel(QWidget* parent)
    : QWidget(parent)
    , streams_list(new QTreeWidget(this)) {
    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    summary_label = new QLabel(this);
    summary_label->setObjectName(
        QStringLiteral("settings_streams_summary_label")
    );
    summary_label->setWordWrap(true);
    layout->addWidget(summary_label);

    streams_list->setObjectName(QStringLiteral("settings_streams_list"));
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

    refresh_summary();
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
    refresh_summary();
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
    refresh_summary();
}

void stream_inventory_panel::clear_stream_entries() const {
    streams_list->clear();
    refresh_summary();
}

void stream_inventory_panel::on_stream_item_changed(
    QTreeWidgetItem* item, const int column
) {
    if (item == nullptr || column != 0) {
        return;
    }

    emit show_stream_changed(item->text(1), item->checkState(0) == Qt::Checked);
    refresh_summary();
}

void stream_inventory_panel::refresh_summary() const {
    if (summary_label == nullptr || streams_list == nullptr) {
        return;
    }

    const int total = streams_list->topLevelItemCount();
    int shown = 0;
    for (int i = 0; i < total; i += 1) {
        const auto item = streams_list->topLevelItem(i);
        if (item != nullptr && item->checkState(0) == Qt::Checked) {
            shown += 1;
        }
    }

    if (total == 0) {
        summary_label->setText(QStringLiteral(
            "No configured streams yet. Add a source, then enable it in "
            "the grid from this list."
        ));
        return;
    }

    summary_label->setText(
        QStringLiteral("%1 configured | %2 shown in grid | %3 hidden")
            .arg(total)
            .arg(shown)
            .arg(std::max(0, total - shown))
    );
}
