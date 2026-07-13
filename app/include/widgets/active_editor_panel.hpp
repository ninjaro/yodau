#ifndef YODAU_APP_WIDGETS_ACTIVE_EDITOR_PANEL_HPP
#define YODAU_APP_WIDGETS_ACTIVE_EDITOR_PANEL_HPP

#include "shell/app_settings.hpp"
#include "widgets/stream_cell.hpp"

#include <QColor>
#include <QWidget>

#include <utility>
#include <vector>

class active_stream_panel;
class line_profile_panel;
class template_apply_panel;
class QLabel;
class QLineEdit;
class QPushButton;
class QTabWidget;
class QTableWidget;
class QTreeWidget;
class QTreeWidgetItem;
class QComboBox;
class QTableWidgetItem;

class active_editor_panel final : public QWidget {
    Q_OBJECT

public:
    explicit active_editor_panel(QWidget* parent = nullptr);

    void set_active_candidates(const QStringList& names) const;
    void set_active_current(const QString& name) const;
    void set_active_lines(const std::vector<stream_cell::line_instance>& lines);

    void add_template_candidate(const QString& name) const;
    void set_template_candidates(const QStringList& names) const;
    void set_line_profile(const line_profile& profile);
    [[nodiscard]] line_profile current_line_profile() const;
    void set_template_settings(const template_apply_settings& settings_value);
    [[nodiscard]] template_apply_settings current_template_settings() const;

    void reset_line_form();
    void reset_template_form();

    void set_line_closed(bool closed) const;

    [[nodiscard]] QString current_template_name() const;
    [[nodiscard]] QColor preview_color() const;
    bool select_line_edit_point(int visible_index);
    bool translate_line_edit_shape(const QPointF& delta_pct);
    bool move_line_edit_point(int visible_index, const QPointF& point_pct);
    bool split_line_edit_point(int visible_index);
    bool insert_line_edit_point_after(
        int visible_segment_index, const QPointF& point_pct
    );
    bool delete_line_edit_point(int visible_index);
    bool
    rotate_line_edit_shape(double delta_degrees, int visible_pivot_index = -1);
    void begin_line_edit_change();
    void finish_line_edit_change();
    bool undo_line_edit_change();
    bool redo_line_edit_change();
    bool revert_line_edit_changes();

signals:
    void active_stream_selected(const QString& name);
    void edit_mode_changed(bool drawing_new);
    void line_profile_changed(line_profile profile_value);
    void line_save_requested(line_profile profile_value);
    void line_undo_requested();
    void line_enabled_changed(const QString& line_name, bool enabled);
    void line_detach_requested(const QString& line_name);
    void line_edit_preview_changed(line_edit_request request);
    void line_edit_preview_cleared();
    void line_edit_save_requested(line_edit_request request);
    void template_add_requested(template_apply_settings settings_value);
    void template_settings_changed(template_apply_settings settings_value);

private:
    struct line_edit_snapshot;

    void build_ui();
    void update_tools() const;
    void refresh_status_summary() const;
    void refresh_line_list() const;
    void refresh_line_summary() const;
    void refresh_line_action_state() const;
    void refresh_line_edit_candidates();
    void refresh_line_edit_table();
    void refresh_line_edit_summary() const;
    void refresh_line_edit_state() const;
    void initialize_line_points_from_source();
    void emit_line_preview_if_visible();
    [[nodiscard]] line_edit_request current_line_edit_request() const;
    void clear_line_edit_history();
    void record_line_edit_mutation();
    void push_line_edit_undo_snapshot();
    void restore_line_edit_snapshot(const line_edit_snapshot& snapshot);
    void refresh_line_edit_after_mutation();
    [[nodiscard]] bool line_edit_has_unsaved_changes() const;
    bool delete_line_edit_row(int row);
    void emit_line_enabled_changed_queued(QString line_name, bool enabled);
    [[nodiscard]] QString selected_line_name() const;

private slots:
    void on_line_item_changed(QTreeWidgetItem* item, int column);
    void on_line_selection_changed();
    void on_detach_selected_line_clicked();
    void on_editor_tab_changed(int index);
    void on_line_edit_selection_changed(int index);
    void on_line_table_selection_changed();
    void on_line_point_item_changed(QTableWidgetItem* item);
    void on_line_name_text_changed(const QString& text);
    void on_line_edit_undo_clicked();
    void on_line_edit_redo_clicked();
    void on_line_edit_revert_clicked();
    void on_line_edit_delete_clicked();
    void on_line_edit_save_clicked();

private:
    struct line_edit_snapshot {
        std::vector<QPointF> points_pct;
        std::vector<bool> point_enabled;
        int selected_row { -1 };
    };

    [[nodiscard]] line_edit_snapshot current_line_edit_snapshot() const;
    [[nodiscard]] bool
    snapshot_matches_current(const line_edit_snapshot& snapshot) const;

    active_stream_panel* active_stream_panel_widget { nullptr };
    line_profile_panel* active_line_panel { nullptr };
    template_apply_panel* active_template_panel { nullptr };
    QLabel* status_summary_label { nullptr };
    QLabel* line_summary_label { nullptr };
    QTabWidget* editor_tabs { nullptr };
    QTreeWidget* line_list_widget { nullptr };
    QPushButton* line_detach_button_ { nullptr };
    QWidget* edit_tab_widget_ { nullptr };
    QComboBox* line_edit_combo_ { nullptr };
    QLabel* line_edit_summary_label_ { nullptr };
    QTableWidget* line_edit_points_table_ { nullptr };
    QLineEdit* line_edit_name_edit_ { nullptr };
    QPushButton* line_edit_undo_button_ { nullptr };
    QPushButton* line_edit_redo_button_ { nullptr };
    QPushButton* line_edit_revert_button_ { nullptr };
    QPushButton* line_edit_delete_button_ { nullptr };
    QPushButton* line_edit_save_button_ { nullptr };
    std::vector<stream_cell::line_instance> active_lines_;
    std::vector<std::pair<QString, bool>> pending_line_toggles_;
    std::vector<QPointF> line_edit_points_pct_;
    std::vector<bool> line_edit_point_enabled_;
    QString line_edit_source_name_;
    int line_edit_selected_row_ { -1 };
    std::vector<line_edit_snapshot> line_edit_undo_stack_;
    std::vector<line_edit_snapshot> line_edit_redo_stack_;
    bool line_edit_transaction_active_ { false };
    bool syncing_line_edit_ui_ { false };
    bool line_toggle_flush_scheduled_ { false };
};

#endif // YODAU_APP_WIDGETS_ACTIVE_EDITOR_PANEL_HPP
