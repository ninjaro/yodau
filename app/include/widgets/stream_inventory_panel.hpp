#ifndef YODAU_FRONTEND_WIDGETS_STREAM_INVENTORY_PANEL_HPP
#define YODAU_FRONTEND_WIDGETS_STREAM_INVENTORY_PANEL_HPP

#include "shell/frontend_log.hpp"

#include <QWidget>

class QTreeWidget;
class QTreeWidgetItem;
class log_area_view;
class log_toolbar_panel;

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
    void set_log_toolbar(log_toolbar_panel* toolbar);
    bool append_log_entry(const frontend_log_entry& entry) const;

signals:
    void show_stream_changed(const QString& name, bool show);

private slots:
    void on_stream_item_changed(QTreeWidgetItem* item, int column);

private:
    QTreeWidget* streams_list { nullptr };
    log_area_view* event_log_view { nullptr };
};

#endif // YODAU_FRONTEND_WIDGETS_STREAM_INVENTORY_PANEL_HPP
