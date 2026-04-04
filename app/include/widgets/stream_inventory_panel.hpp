#ifndef YODAU_APP_WIDGETS_STREAM_INVENTORY_PANEL_HPP
#define YODAU_APP_WIDGETS_STREAM_INVENTORY_PANEL_HPP

#include "shell/app_log.hpp"

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class QLabel;

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

signals:
    void show_stream_changed(const QString& name, bool show);

private slots:
    void on_stream_item_changed(QTreeWidgetItem* item, int column);

private:
    void refresh_summary() const;

    QTreeWidget* streams_list { nullptr };
    QLabel* summary_label { nullptr };
};

#endif // YODAU_APP_WIDGETS_STREAM_INVENTORY_PANEL_HPP
