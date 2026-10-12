#include "widgets/active_editor_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/line_profile_panel.hpp"
#include "widgets/template_apply_panel.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QTableWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

void active_editor_panel::build_ui() {
    setObjectName(QStringLiteral("settings_active_editor_panel"));

    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    const auto status_box = new QGroupBox(str_label("line editing"), this);
    status_box_ = status_box;
    status_box->setObjectName(QStringLiteral("settings_active_status_box"));
    const auto status_layout = new QVBoxLayout(status_box);
    status_summary_label = new QLabel(status_box);
    status_summary_label->setObjectName(
        QStringLiteral("settings_active_status_summary_label")
    );
    status_summary_label->setWordWrap(true);
    status_layout->addWidget(status_summary_label);
    layout->addWidget(status_box);

    active_stream_panel_widget = new active_stream_panel(
        active_stream_panel::panel_mode::line_editor,
        QStringLiteral("settings_active"), this
    );
    layout->addWidget(active_stream_panel_widget);
    connect(
        active_stream_panel_widget, &active_stream_panel::stream_selected, this,
        [this](const QString& name) {
            update_tools();
            emit active_stream_selected(name);
        }
    );
    connect(
        active_stream_panel_widget, &active_stream_panel::edit_mode_changed,
        this, [this](const bool drawing_new) {
            update_tools();
            emit edit_mode_changed(drawing_new);
        }
    );

    editor_tabs = new QTabWidget(this);
    editor_tabs->setObjectName(QStringLiteral("settings_active_editor_tabs"));
    layout->addWidget(editor_tabs, 1);

    const auto draw_tab = new QWidget(editor_tabs);
    draw_tab->setObjectName(QStringLiteral("settings_active_draw_tab"));
    const auto draw_layout = new QVBoxLayout(draw_tab);
    draw_layout->setContentsMargins(0, 0, 0, 0);
    draw_layout->setSpacing(10);

    if (auto* edit_mode_widget
        = active_stream_panel_widget->take_edit_mode_widget()) {
        edit_mode_widget->setParent(draw_tab);
        draw_layout->addWidget(edit_mode_widget);
    }

    active_line_panel = new line_profile_panel(draw_tab);
    active_line_panel->setObjectName(
        QStringLiteral("settings_active_line_profile_panel")
    );
    draw_layout->addWidget(active_line_panel);
    connect(
        active_line_panel, &line_profile_panel::profile_changed, this,
        &active_editor_panel::line_profile_changed
    );
    connect(
        active_line_panel, &line_profile_panel::save_requested, this,
        &active_editor_panel::line_save_requested
    );
    connect(
        active_line_panel, &line_profile_panel::undo_requested, this,
        &active_editor_panel::line_undo_requested
    );

    active_template_panel = new template_apply_panel(draw_tab);
    active_template_panel->setObjectName(
        QStringLiteral("settings_active_template_apply_panel")
    );
    draw_layout->addWidget(active_template_panel);
    connect(
        active_template_panel, &template_apply_panel::settings_changed, this,
        &active_editor_panel::template_settings_changed
    );
    connect(
        active_template_panel, &template_apply_panel::add_requested, this,
        &active_editor_panel::template_add_requested
    );

    draw_layout->addStretch(1);
    editor_tabs->addTab(draw_tab, str_label("draw"));

    const auto lines_tab = new QWidget(editor_tabs);
    lines_tab->setObjectName(QStringLiteral("settings_active_lines_tab"));
    const auto lines_layout = new QVBoxLayout(lines_tab);
    lines_layout->setContentsMargins(0, 0, 0, 0);
    lines_layout->setSpacing(8);

    line_summary_label = new QLabel(lines_tab);
    line_summary_label->setObjectName(
        QStringLiteral("settings_active_lines_summary_label")
    );
    line_summary_label->setWordWrap(true);
    lines_layout->addWidget(line_summary_label);

    line_list_widget = new QTreeWidget(lines_tab);
    line_list_widget->setObjectName(
        QStringLiteral("settings_active_lines_list")
    );
    line_list_widget->setColumnCount(3);
    line_list_widget->setHeaderLabels(
        { str_label("on"), str_label("line"), str_label("profile") }
    );
    line_list_widget->header()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents
    );
    line_list_widget->header()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents
    );
    line_list_widget->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    lines_layout->addWidget(line_list_widget, 1);
    connect(
        line_list_widget, &QTreeWidget::itemChanged, this,
        &active_editor_panel::on_line_item_changed
    );
    connect(
        line_list_widget, &QTreeWidget::itemSelectionChanged, this,
        &active_editor_panel::on_line_selection_changed
    );

    line_detach_button_
        = new QPushButton(str_label("detach from stream"), lines_tab);
    line_detach_button_->setObjectName(
        QStringLiteral("settings_active_detach_line_button")
    );
    line_detach_button_->setToolTip(QStringLiteral(
        "Detach the selected saved line from this stream without removing "
        "the underlying template definition."
    ));
    line_detach_button_->setEnabled(false);
    lines_layout->addWidget(line_detach_button_);
    connect(
        line_detach_button_, &QPushButton::clicked, this,
        &active_editor_panel::on_detach_selected_line_clicked
    );

    editor_tabs->addTab(lines_tab, str_label("lines"));
    edit_tab_widget_ = new QWidget(editor_tabs);
    edit_tab_widget_->setObjectName(QStringLiteral("settings_active_edit_tab"));
    const auto edit_layout = new QVBoxLayout(edit_tab_widget_);
    edit_layout->setContentsMargins(0, 0, 0, 0);
    edit_layout->setSpacing(8);

    line_edit_summary_label_ = new QLabel(edit_tab_widget_);
    line_edit_summary_label_->setObjectName(
        QStringLiteral("settings_active_edit_summary_label")
    );
    line_edit_summary_label_->setWordWrap(true);
    edit_layout->addWidget(line_edit_summary_label_);

    line_edit_combo_ = new QComboBox(edit_tab_widget_);
    line_edit_combo_->setObjectName(
        QStringLiteral("settings_active_edit_line_combo")
    );
    edit_layout->addWidget(line_edit_combo_);

    line_edit_points_table_ = new QTableWidget(edit_tab_widget_);
    line_edit_points_table_->setObjectName(
        QStringLiteral("settings_active_edit_points_table")
    );
    line_edit_points_table_->setColumnCount(4);
    line_edit_points_table_->setSelectionBehavior(
        QAbstractItemView::SelectRows
    );
    line_edit_points_table_->setSelectionMode(
        QAbstractItemView::SingleSelection
    );
    line_edit_points_table_->setHorizontalHeaderLabels(
        {
            str_label("use"),
            str_label("index"),
            str_label("x %"),
            str_label("y %"),
        }
    );
    line_edit_points_table_->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents
    );
    line_edit_points_table_->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::ResizeToContents
    );
    line_edit_points_table_->horizontalHeader()->setSectionResizeMode(
        2, QHeaderView::Stretch
    );
    line_edit_points_table_->horizontalHeader()->setSectionResizeMode(
        3, QHeaderView::Stretch
    );
    edit_layout->addWidget(line_edit_points_table_, 1);

    const auto line_edit_button_row = new QHBoxLayout();
    line_edit_button_row->setContentsMargins(0, 0, 0, 0);
    line_edit_button_row->setSpacing(6);

    line_edit_undo_button_
        = new QPushButton(str_label("undo"), edit_tab_widget_);
    line_edit_undo_button_->setObjectName(
        QStringLiteral("settings_active_edit_undo_button")
    );
    line_edit_button_row->addWidget(line_edit_undo_button_);

    line_edit_redo_button_
        = new QPushButton(str_label("redo"), edit_tab_widget_);
    line_edit_redo_button_->setObjectName(
        QStringLiteral("settings_active_edit_redo_button")
    );
    line_edit_button_row->addWidget(line_edit_redo_button_);

    line_edit_revert_button_
        = new QPushButton(str_label("revert"), edit_tab_widget_);
    line_edit_revert_button_->setObjectName(
        QStringLiteral("settings_active_edit_revert_button")
    );
    line_edit_button_row->addWidget(line_edit_revert_button_);

    line_edit_delete_button_
        = new QPushButton(str_label("delete point"), edit_tab_widget_);
    line_edit_delete_button_->setObjectName(
        QStringLiteral("settings_active_edit_delete_button")
    );
    line_edit_button_row->addWidget(line_edit_delete_button_);
    edit_layout->addLayout(line_edit_button_row);

    line_edit_name_edit_ = new QLineEdit(edit_tab_widget_);
    line_edit_name_edit_->setObjectName(
        QStringLiteral("active_edit_new_name_edit")
    );
    line_edit_name_edit_->setPlaceholderText(str_label("new line name"));
    edit_layout->addWidget(line_edit_name_edit_);

    line_edit_save_button_
        = new QPushButton(str_label("save edited line"), edit_tab_widget_);
    line_edit_save_button_->setObjectName(
        QStringLiteral("settings_active_edit_save_button")
    );
    edit_layout->addWidget(line_edit_save_button_);

    connect(
        editor_tabs, &QTabWidget::currentChanged, this,
        &active_editor_panel::on_editor_tab_changed
    );
    connect(
        line_edit_combo_, &QComboBox::currentIndexChanged, this,
        &active_editor_panel::on_line_edit_selection_changed
    );
    connect(
        line_edit_points_table_, &QTableWidget::itemChanged, this,
        &active_editor_panel::on_line_point_item_changed
    );
    connect(
        line_edit_points_table_, &QTableWidget::itemSelectionChanged, this,
        &active_editor_panel::on_line_table_selection_changed
    );
    connect(
        line_edit_name_edit_, &QLineEdit::textChanged, this,
        &active_editor_panel::on_line_name_text_changed
    );
    connect(
        line_edit_undo_button_, &QPushButton::clicked, this,
        &active_editor_panel::on_line_edit_undo_clicked
    );
    connect(
        line_edit_redo_button_, &QPushButton::clicked, this,
        &active_editor_panel::on_line_edit_redo_clicked
    );
    connect(
        line_edit_revert_button_, &QPushButton::clicked, this,
        &active_editor_panel::on_line_edit_revert_clicked
    );
    connect(
        line_edit_delete_button_, &QPushButton::clicked, this,
        &active_editor_panel::on_line_edit_delete_clicked
    );
    connect(
        line_edit_save_button_, &QPushButton::clicked, this,
        &active_editor_panel::on_line_edit_save_clicked
    );

    editor_tabs->addTab(edit_tab_widget_, str_label("edit"));
    refresh_line_edit_candidates();
    refresh_line_edit_table();
    update_tools();
}
