#ifndef YODAU_APP_WIDGETS_STREAM_INVENTORY_PANEL_HPP
#define YODAU_APP_WIDGETS_STREAM_INVENTORY_PANEL_HPP

#include "shell/app_log.hpp"

#include <QStringList>
#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;
class QPushButton;
class QVBoxLayout;

class stream_inventory_panel final : public QWidget {
    Q_OBJECT

public:
    explicit stream_inventory_panel(QWidget* parent = nullptr);

    void add_stream_entry(
        const QString& name, const QString& source, bool checked = false
    ) const;
    void set_stream_checked(const QString& name, bool checked) const;
    void remove_stream_entry(const QString& name) const;
    void clear_stream_entries() const;
    [[nodiscard]] QStringList stream_order() const;

signals:
    void show_stream_changed(const QString& name, bool show);
    void stream_order_changed(const QStringList& names);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void on_stream_item_changed(QTreeWidgetItem* item, int column);

private:
    void refresh_summary() const;
    QTreeWidget* make_stream_list();
    void build_order_controls(QVBoxLayout* layout);
    void update_order_controls() const;
    void move_selected(int offset);
    void publish_order();

    QTreeWidget* streams_list { nullptr };
    QLabel* summary_label { nullptr };
    QPushButton* earlier_button_ { nullptr };
    QPushButton* later_button_ { nullptr };
    bool order_edited_ { false };
};

#endif // YODAU_APP_WIDGETS_STREAM_INVENTORY_PANEL_HPP
