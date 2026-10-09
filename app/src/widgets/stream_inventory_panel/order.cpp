#include "widgets/stream_inventory_panel.hpp"

#include <QDropEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <functional>
#include <utility>

namespace {

QStringList list_order(const QTreeWidget& list) {
    QStringList names;
    for (int index = 0; index < list.topLevelItemCount(); ++index) {
        names.append(list.topLevelItem(index)->text(1));
    }
    return names;
}

#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
// Keep Qt's internal move/capture implementation, but never allow nesting,
// foreign drops or visibility notifications during an item move.
class stream_list final : public QTreeWidget {
public:
    stream_list(QWidget* parent, std::function<void()> moved)
        : QTreeWidget(parent)
        , moved_(std::move(moved)) {
        setSelectionMode(QAbstractItemView::SingleSelection);
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
        setDropIndicatorShown(true);
        setRootIsDecorated(false);
        setItemsExpandable(false);
    }

protected:
    bool dropMimeData(
        QTreeWidgetItem* parent, int index, const QMimeData* mime,
        Qt::DropAction action
    ) override {
        return parent == nullptr && action == Qt::MoveAction
            && QTreeWidget::dropMimeData(parent, index, mime, action);
    }

    void dropEvent(QDropEvent* event) override {
        if (event->source() != this || event->dropAction() != Qt::MoveAction) {
            event->ignore();
            return;
        }
        const auto before = list_order(*this);
        auto* selected = currentItem();
        {
            const QSignalBlocker blocker(this);
            QTreeWidget::dropEvent(event);
            // Qt may remove/reinsert an item even when its row is unchanged.
            if (selected != nullptr) {
                setCurrentItem(selected);
                scrollToItem(selected);
            }
        }
        if (list_order(*this) != before) {
            moved_();
        }
    }

private:
    std::function<void()> moved_;
};
#endif

} // namespace

QStringList stream_inventory_panel::stream_order() const {
    return list_order(*streams_list);
}

QTreeWidget* stream_inventory_panel::make_stream_list() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    return new QTreeWidget(this);
#else
    return new stream_list(this, [this] { publish_order(); });
#endif
}

void stream_inventory_panel::build_order_controls(QVBoxLayout* layout) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    Q_UNUSED(layout);
#else
    streams_list->setAccessibleName(tr("Configured streams"));
    streams_list->setAccessibleDescription(
        tr("Drag to reorder, or use Control+Up and Control+Down. "
           "Pool placement follows this sequence when Preserve order is on. "
           "Order lasts for this session; processing priority is unchanged.")
    );
    streams_list->setToolTip(streams_list->accessibleDescription());
    streams_list->installEventFilter(this);
    auto* hint = new QLabel(
        tr("Reorder for this session. Enable Preserve order to keep this "
           "sequence in the pool; processing priority is unchanged."),
        this
    );
    hint->setWordWrap(true);
    layout->addWidget(hint);
    auto* row = new QHBoxLayout;
    earlier_button_ = new QPushButton(tr("Move &earlier"), this);
    later_button_ = new QPushButton(tr("Move &later"), this);
    earlier_button_->setObjectName(QStringLiteral("stream_order_earlier"));
    later_button_->setObjectName(QStringLiteral("stream_order_later"));
    earlier_button_->setAccessibleName(tr("Move selected stream earlier"));
    later_button_->setAccessibleName(tr("Move selected stream later"));
    earlier_button_->setToolTip(tr("Move selected stream earlier (Ctrl+Up)"));
    later_button_->setToolTip(tr("Move selected stream later (Ctrl+Down)"));
    row->addWidget(earlier_button_);
    row->addWidget(later_button_);
    row->addStretch();
    layout->addLayout(row);
    connect(earlier_button_, &QPushButton::clicked, this, [this] {
        move_selected(-1);
    });
    connect(later_button_, &QPushButton::clicked, this, [this] {
        move_selected(1);
    });
    connect(streams_list, &QTreeWidget::currentItemChanged, this, [this] {
        update_order_controls();
    });
    update_order_controls();
#endif
}

void stream_inventory_panel::update_order_controls() const {
    if (earlier_button_ == nullptr || later_button_ == nullptr) {
        return;
    }
    const int index
        = streams_list->indexOfTopLevelItem(streams_list->currentItem());
    earlier_button_->setEnabled(index > 0);
    later_button_->setEnabled(
        index >= 0 && index + 1 < streams_list->topLevelItemCount()
    );
}

void stream_inventory_panel::move_selected(const int offset) {
    if (!streams_list->isEnabled()) {
        return;
    }
    const int index
        = streams_list->indexOfTopLevelItem(streams_list->currentItem());
    const int target = index + offset;
    if (index < 0 || target < 0
        || target >= streams_list->topLevelItemCount()) {
        return;
    }
    {
        const QSignalBlocker blocker(streams_list);
        auto* item = streams_list->takeTopLevelItem(index);
        streams_list->insertTopLevelItem(target, item);
        streams_list->setCurrentItem(item);
        streams_list->scrollToItem(item);
    }
    publish_order();
}

void stream_inventory_panel::publish_order() {
    order_edited_ = true;
    update_order_controls();
    emit stream_order_changed(stream_order());
}

bool stream_inventory_panel::eventFilter(QObject* watched, QEvent* event) {
    if (watched == streams_list && event->type() == QEvent::KeyPress) {
        auto* key = static_cast<QKeyEvent*>(event);
        if (key->modifiers() == Qt::ControlModifier
            && (key->key() == Qt::Key_Up || key->key() == Qt::Key_Down)) {
            move_selected(key->key() == Qt::Key_Up ? -1 : 1);
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}
