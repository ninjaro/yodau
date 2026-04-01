#include "widgets/active_editor_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/line_profile_panel.hpp"
#include "widgets/template_apply_panel.hpp"

#include <QComboBox>
#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QLineF>
#include <QLineEdit>
#include <QMetaObject>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
#include <optional>

namespace active_editor_panel_support {

QString none_text() { return str_label("none"); }

QString suggested_variant_name(const QString& source_name) {
    const QString trimmed_name = source_name.trimmed();
    return trimmed_name.isEmpty() ? QString() : trimmed_name + QStringLiteral("_edit");
}

std::optional<stream_cell::line_instance> find_line_by_name(
    const std::vector<stream_cell::line_instance>& lines, const QString& line_name
) {
    const QString trimmed_name = line_name.trimmed();
    const auto it = std::find_if(
        lines.cbegin(), lines.cend(),
        [&trimmed_name](const stream_cell::line_instance& line_value) {
            return line_value.template_name.trimmed() == trimmed_name;
        }
    );
    if (it == lines.cend()) {
        return std::nullopt;
    }
    return *it;
}

QPointF clamped_pct(QPointF point_pct) {
    point_pct.setX(std::clamp(point_pct.x(), 0.0, 100.0));
    point_pct.setY(std::clamp(point_pct.y(), 0.0, 100.0));
    return point_pct;
}

std::vector<int> enabled_row_indices(const std::vector<bool>& enabled_rows) {
    std::vector<int> indices;
    indices.reserve(enabled_rows.size());

    for (int row = 0; row < static_cast<int>(enabled_rows.size()); row += 1) {
        if (enabled_rows.at(static_cast<std::vector<bool>::size_type>(row))) {
            indices.push_back(row);
        }
    }

    return indices;
}

std::optional<int> row_for_visible_index(
    const std::vector<bool>& enabled_rows, const int visible_index
) {
    const auto visible_rows = enabled_row_indices(enabled_rows);
    if (visible_index < 0 || visible_index >= static_cast<int>(visible_rows.size())) {
        return std::nullopt;
    }

    return visible_rows.at(static_cast<std::vector<int>::size_type>(visible_index));
}

QPointF average_point_pct(
    const std::vector<QPointF>& points_pct, const std::vector<int>& rows
) {
    if (rows.empty()) {
        return {};
    }

    QPointF sum;
    for (const int row : rows) {
        sum += points_pct.at(static_cast<std::vector<QPointF>::size_type>(row));
    }

    return sum / static_cast<double>(rows.size());
}

QPointF wire_centroid_pct(
    const std::vector<QPointF>& points_pct, const std::vector<int>& rows,
    const bool closed
) {
    if (rows.empty()) {
        return {};
    }
    if (rows.size() == 1) {
        return points_pct.at(static_cast<std::vector<QPointF>::size_type>(rows.front()));
    }

    QPointF weighted_sum;
    double total_weight = 0.0;

    auto accumulate_segment = [&](const QPointF& a, const QPointF& b) {
        const QLineF segment(a, b);
        const double length = segment.length();
        if (length <= std::numeric_limits<double>::epsilon()) {
            return;
        }
        weighted_sum += (a + b) / 2.0 * length;
        total_weight += length;
    };

    for (int index = 1; index < static_cast<int>(rows.size()); index += 1) {
        const auto previous_row_index
            = static_cast<std::vector<int>::size_type>(index - 1);
        const auto current_row_index
            = static_cast<std::vector<int>::size_type>(index);
        const QPointF a
            = points_pct.at(static_cast<std::vector<QPointF>::size_type>(
                rows.at(previous_row_index)
            ));
        const QPointF b
            = points_pct.at(static_cast<std::vector<QPointF>::size_type>(
                rows.at(current_row_index)
            ));
        accumulate_segment(a, b);
    }

    if (closed && rows.size() >= 3) {
        const QPointF a
            = points_pct.at(static_cast<std::vector<QPointF>::size_type>(rows.back()));
        const QPointF b
            = points_pct.at(static_cast<std::vector<QPointF>::size_type>(rows.front()));
        accumulate_segment(a, b);
    }

    if (total_weight <= std::numeric_limits<double>::epsilon()) {
        return average_point_pct(points_pct, rows);
    }

    return weighted_sum / total_weight;
}

QPointF rotated_point_pct(
    const QPointF& point_pct, const QPointF& pivot_pct, const double radians
) {
    const QPointF relative = point_pct - pivot_pct;
    const double sin_value = std::sin(radians);
    const double cos_value = std::cos(radians);
    return {
        pivot_pct.x() + relative.x() * cos_value - relative.y() * sin_value,
        pivot_pct.y() + relative.x() * sin_value + relative.y() * cos_value
    };
}

} // namespace active_editor_panel_support

active_editor_panel::active_editor_panel(QWidget* parent)
    : QWidget(parent) {
    build_ui();
    set_line_profile(line_profile {});
    set_template_settings(template_apply_settings {});
    refresh_line_list();
}

void active_editor_panel::set_active_candidates(const QStringList& names) const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    active_stream_panel_widget->set_active_candidates(names);
    update_tools();
}

void active_editor_panel::set_active_current(const QString& name) const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    active_stream_panel_widget->set_active_current(name);
    update_tools();
}

void active_editor_panel::set_active_lines(
    const std::vector<stream_cell::line_instance>& lines
) {
    const QString previous_source_name = current_line_edit_source_name_;
    active_lines_ = lines;
    refresh_line_list();
    refresh_line_edit_candidates();
    if (current_line_edit_source_name_ != previous_source_name) {
        initialize_line_edit_points_from_source();
    }
    refresh_line_edit_table();
    if (!previous_source_name.isEmpty()
        && current_line_edit_source_name_.isEmpty()) {
        emit line_edit_preview_cleared();
    }
    update_tools();
}

void active_editor_panel::add_template_candidate(const QString& name) const {
    if (active_template_panel == nullptr || name.isEmpty()) {
        return;
    }

    active_template_panel->add_template_candidate(name);
    update_tools();
}

void active_editor_panel::set_template_candidates(const QStringList& names) const {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->set_template_candidates(names);
    update_tools();
}

void active_editor_panel::set_line_profile(const line_profile& profile) {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->set_line_profile(profile);
}

line_profile active_editor_panel::current_line_profile() const {
    return active_line_panel != nullptr ? active_line_panel->current_line_profile()
                                        : line_profile {};
}

void active_editor_panel::set_template_settings(
    const template_apply_settings& settings_value
) {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->set_template_settings(settings_value);
}

template_apply_settings active_editor_panel::current_template_settings() const {
    return active_template_panel != nullptr
        ? active_template_panel->current_template_settings()
        : template_apply_settings {};
}

void active_editor_panel::reset_line_form() {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->reset_form();
    emit line_profile_changed(current_line_profile());
}

void active_editor_panel::reset_template_form() {
    if (active_template_panel == nullptr) {
        return;
    }

    active_template_panel->reset_form();
    emit template_settings_changed(current_template_settings());
}

void active_editor_panel::set_line_closed(const bool closed) const {
    if (active_line_panel == nullptr) {
        return;
    }

    active_line_panel->set_line_closed(closed);
}

QString active_editor_panel::current_template_name() const {
    if (active_template_panel == nullptr) {
        return {};
    }

    return active_template_panel->current_template_name();
}

QColor active_editor_panel::preview_color() const {
    return active_template_panel != nullptr
        ? active_template_panel->preview_color()
        : QColor(Qt::red);
}

bool active_editor_panel::select_line_edit_point(const int visible_index) {
    const auto selected_row = active_editor_panel_support::row_for_visible_index(
        line_edit_point_enabled_, visible_index
    );
    if (!selected_row.has_value()) {
        return false;
    }

    line_edit_selected_row_ = *selected_row;
    refresh_line_edit_table();
    return true;
}

bool active_editor_panel::translate_line_edit_shape(const QPointF& delta_pct) {
    const auto visible_rows = active_editor_panel_support::enabled_row_indices(
        line_edit_point_enabled_
    );
    if (current_line_edit_source_name_.isEmpty() || visible_rows.empty()
        || line_edit_points_pct_.size() != line_edit_point_enabled_.size()) {
        return false;
    }

    for (const int row : visible_rows) {
        line_edit_points_pct_.at(static_cast<std::vector<QPointF>::size_type>(row))
            = active_editor_panel_support::clamped_pct(
                line_edit_points_pct_.at(
                    static_cast<std::vector<QPointF>::size_type>(row)
                )
                + delta_pct
            );
    }

    refresh_line_edit_table();
    emit_line_edit_preview_if_visible();
    return true;
}

bool active_editor_panel::move_line_edit_point(
    const int visible_index, const QPointF& point_pct
) {
    const auto selected_row = active_editor_panel_support::row_for_visible_index(
        line_edit_point_enabled_, visible_index
    );
    if (!selected_row.has_value() || current_line_edit_source_name_.isEmpty()
        || *selected_row >= static_cast<int>(line_edit_points_pct_.size())) {
        return false;
    }

    line_edit_selected_row_ = *selected_row;
    line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(*selected_row)
    ) = active_editor_panel_support::clamped_pct(point_pct);

    refresh_line_edit_table();
    emit_line_edit_preview_if_visible();
    return true;
}

bool active_editor_panel::split_line_edit_point(const int visible_index) {
    const auto visible_rows = active_editor_panel_support::enabled_row_indices(
        line_edit_point_enabled_
    );
    if (current_line_edit_source_name_.isEmpty() || visible_rows.size() < 2
        || visible_index < 0
        || visible_index >= static_cast<int>(visible_rows.size())) {
        return false;
    }

    const int row = visible_rows.at(
        static_cast<std::vector<int>::size_type>(visible_index)
    );
    if (row < 0 || row >= static_cast<int>(line_edit_points_pct_.size())) {
        return false;
    }

    const QPointF current = line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(row)
    );

    QPointF first_split = current;
    QPointF second_split = current;

    if (visible_index == 0) {
        const QPointF next = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(visible_rows.at(1))
        );
        first_split = current + (next - current) / 3.0;
        second_split = current + (next - current) * (2.0 / 3.0);
    } else if (visible_index == static_cast<int>(visible_rows.size()) - 1) {
        const QPointF previous = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(
                visible_rows.at(
                    static_cast<std::vector<int>::size_type>(visible_index - 1)
                )
            )
        );
        first_split = previous + (current - previous) / 3.0;
        second_split = previous + (current - previous) * (2.0 / 3.0);
    } else {
        const QPointF previous = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(
                visible_rows.at(
                    static_cast<std::vector<int>::size_type>(visible_index - 1)
                )
            )
        );
        const QPointF next = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(
                visible_rows.at(
                    static_cast<std::vector<int>::size_type>(visible_index + 1)
                )
            )
        );
        first_split = previous + (current - previous) / 2.0;
        second_split = current + (next - current) / 2.0;
    }

    line_edit_selected_row_ = row;
    line_edit_points_pct_.at(
        static_cast<std::vector<QPointF>::size_type>(row)
    ) = active_editor_panel_support::clamped_pct(first_split);
    line_edit_points_pct_.insert(
        line_edit_points_pct_.begin() + row + 1,
        active_editor_panel_support::clamped_pct(second_split)
    );
    line_edit_point_enabled_.insert(
        line_edit_point_enabled_.begin() + row + 1, true
    );

    refresh_line_edit_table();
    emit_line_edit_preview_if_visible();
    return true;
}

bool active_editor_panel::rotate_line_edit_shape(
    const double delta_degrees, const int visible_pivot_index
) {
    const auto visible_rows = active_editor_panel_support::enabled_row_indices(
        line_edit_point_enabled_
    );
    if (current_line_edit_source_name_.isEmpty() || visible_rows.size() < 2
        || std::abs(delta_degrees) <= std::numeric_limits<double>::epsilon()
        || line_edit_points_pct_.size() != line_edit_point_enabled_.size()) {
        return false;
    }

    const auto source_line = active_editor_panel_support::find_line_by_name(
        active_lines_, current_line_edit_source_name_
    );
    if (!source_line.has_value()) {
        return false;
    }

    QPointF pivot_pct;
    if (visible_pivot_index >= 0) {
        const auto pivot_row = active_editor_panel_support::row_for_visible_index(
            line_edit_point_enabled_, visible_pivot_index
        );
        if (!pivot_row.has_value()) {
            return false;
        }
        line_edit_selected_row_ = *pivot_row;
        pivot_pct = line_edit_points_pct_.at(
            static_cast<std::vector<QPointF>::size_type>(*pivot_row)
        );
    } else {
        pivot_pct = active_editor_panel_support::wire_centroid_pct(
            line_edit_points_pct_, visible_rows, source_line->closed
        );
    }

    const double radians = delta_degrees * std::numbers::pi / 180.0;
    for (const int row : visible_rows) {
        const auto row_index = static_cast<std::vector<QPointF>::size_type>(row);
        line_edit_points_pct_.at(row_index)
            = active_editor_panel_support::clamped_pct(
                active_editor_panel_support::rotated_point_pct(
                    line_edit_points_pct_.at(row_index), pivot_pct, radians
                )
            );
    }

    refresh_line_edit_table();
    emit_line_edit_preview_if_visible();
    return true;
}

void active_editor_panel::build_ui() {
    setObjectName(QStringLiteral("settings_active_editor_panel"));

    const auto layout = new QVBoxLayout(this);
    layout->setSpacing(10);

    const auto status_box = new QGroupBox(str_label("line editing"), this);
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
        active_stream_panel_widget, &active_stream_panel::edit_mode_changed, this,
        [this](const bool drawing_new) {
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

    if (auto* edit_mode_widget = active_stream_panel_widget->take_edit_mode_widget()) {
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
    line_list_widget->setObjectName(QStringLiteral("settings_active_lines_list"));
    line_list_widget->setColumnCount(3);
    line_list_widget->setHeaderLabels(
        { str_label("on"), str_label("line"), str_label("profile") }
    );
    line_list_widget->header()->setSectionResizeMode(
        0, QHeaderView::ResizeToContents
    );
    line_list_widget->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    line_list_widget->header()->setSectionResizeMode(2, QHeaderView::Stretch);
    lines_layout->addWidget(line_list_widget, 1);
    connect(
        line_list_widget, &QTreeWidget::itemChanged, this,
        &active_editor_panel::on_line_item_changed
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

    line_edit_name_edit_ = new QLineEdit(edit_tab_widget_);
    line_edit_name_edit_->setObjectName(
        QStringLiteral("settings_active_edit_new_name_edit")
    );
    line_edit_name_edit_->setPlaceholderText(str_label("new line name"));
    edit_layout->addWidget(line_edit_name_edit_);

    line_edit_save_button_ = new QPushButton(str_label("save edited line"), edit_tab_widget_);
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
        &active_editor_panel::on_line_edit_point_item_changed
    );
    connect(
        line_edit_name_edit_, &QLineEdit::textChanged, this,
        &active_editor_panel::on_line_edit_name_text_changed
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

void active_editor_panel::update_tools() const {
    if (active_stream_panel_widget == nullptr) {
        return;
    }

    const bool has_active = active_stream_panel_widget->has_active_stream();
    const bool drawing_mode = active_stream_panel_widget->drawing_new_mode();

    if (editor_tabs != nullptr) {
        editor_tabs->setEnabled(has_active);
    }

    if (active_line_panel != nullptr) {
        active_line_panel->set_panel_active(has_active && drawing_mode);
    }

    if (active_template_panel != nullptr) {
        const bool has_templates = active_template_panel->has_template_candidates();
        active_template_panel->set_panel_active(
            has_active && has_templates && !drawing_mode
        );
    }

    refresh_status_summary();
    refresh_line_summary();
    refresh_line_edit_state();
}

void active_editor_panel::refresh_status_summary() const {
    if (status_summary_label == nullptr || active_stream_panel_widget == nullptr) {
        return;
    }

    if (!active_stream_panel_widget->has_active_stream()) {
        status_summary_label->setText(
            QStringLiteral(
                "Activate a visible stream to draw lines, apply templates, "
                "enable and disable saved lines, or save edited variants."
            )
        );
        return;
    }

    const QString mode_text = active_stream_panel_widget->drawing_new_mode()
        ? QStringLiteral("draw new")
        : QStringLiteral("use template");
    const int total_lines = static_cast<int>(active_lines_.size());
    const int enabled_lines = static_cast<int>(std::count_if(
        active_lines_.cbegin(), active_lines_.cend(),
        [](const stream_cell::line_instance& line_value) {
            return line_value.enabled;
        }
    ));
    const QString template_text
        = active_template_panel != nullptr
            && active_template_panel->has_template_candidates()
        ? QStringLiteral("templates ready")
        : QStringLiteral("no templates yet");

    status_summary_label->setText(
        QStringLiteral("%1 | %2 | %3 enabled of %4 | %5")
            .arg(active_stream_panel_widget->current_stream_settings().stream_name)
            .arg(mode_text)
            .arg(enabled_lines)
            .arg(total_lines)
            .arg(template_text)
    );
}

void active_editor_panel::refresh_line_list() const {
    if (line_list_widget == nullptr) {
        return;
    }

    line_list_widget->blockSignals(true);
    line_list_widget->clear();

    for (const auto& line_value : active_lines_) {
        const auto item = new QTreeWidgetItem(line_list_widget);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, line_value.enabled ? Qt::Checked : Qt::Unchecked);
        item->setText(1, line_value.template_name);
        item->setData(1, Qt::UserRole, line_value.template_name);
        item->setText(
            2,
            QStringLiteral("%1 | mode=%2")
                .arg(
                    line_profile_summary_text(
                        line_value.width_text, line_value.length_text,
                        line_value.response_text
                    )
                )
                .arg(normalized_line_color_mode_id(line_value.color_mode_id))
        );
    }

    line_list_widget->blockSignals(false);
    refresh_line_summary();
}

void active_editor_panel::refresh_line_summary() const {
    if (line_summary_label == nullptr || active_stream_panel_widget == nullptr) {
        return;
    }

    if (!active_stream_panel_widget->has_active_stream()) {
        line_summary_label->setText(
            QStringLiteral("No active stream selected for line routing.")
        );
        return;
    }

    const int total_lines = static_cast<int>(active_lines_.size());
    const int enabled_lines = static_cast<int>(std::count_if(
        active_lines_.cbegin(), active_lines_.cend(),
        [](const stream_cell::line_instance& line_value) {
            return line_value.enabled;
        }
    ));

    if (total_lines == 0) {
        line_summary_label->setText(
            QStringLiteral(
                "No saved lines on this stream yet. Draw a new line or apply "
                "a template from the draw tab."
            )
        );
        return;
    }

    line_summary_label->setText(
        QStringLiteral("%1 saved lines | %2 enabled | %3 disabled")
            .arg(total_lines)
            .arg(enabled_lines)
            .arg(std::max(0, total_lines - enabled_lines))
    );
}

void active_editor_panel::refresh_line_edit_candidates() {
    if (line_edit_combo_ == nullptr) {
        return;
    }

    const QString previous_source_name = current_line_edit_source_name_;
    QSignalBlocker blocker(line_edit_combo_);
    line_edit_combo_->clear();
    line_edit_combo_->addItem(active_editor_panel_support::none_text(), QVariant());

    for (const auto& line_value : active_lines_) {
        if (!line_value.enabled) {
            continue;
        }

        line_edit_combo_->addItem(
            line_value.template_name, line_value.template_name
        );
    }

    const int selected_index = previous_source_name.isEmpty()
        ? 0
        : line_edit_combo_->findData(previous_source_name);
    line_edit_combo_->setCurrentIndex(selected_index >= 0 ? selected_index : 0);
    current_line_edit_source_name_
        = line_edit_combo_->currentData().toString().trimmed();
}

void active_editor_panel::initialize_line_edit_points_from_source() {
    line_edit_points_pct_.clear();
    line_edit_point_enabled_.clear();
    line_edit_selected_row_ = -1;

    const auto line_value = active_editor_panel_support::find_line_by_name(
        active_lines_, current_line_edit_source_name_
    );
    if (!line_value.has_value()) {
        return;
    }

    line_edit_points_pct_ = line_value->pts_pct;
    line_edit_point_enabled_.assign(line_edit_points_pct_.size(), true);
}

void active_editor_panel::refresh_line_edit_table() {
    if (line_edit_points_table_ == nullptr) {
        return;
    }

    syncing_line_edit_ui_ = true;
    line_edit_points_table_->blockSignals(true);
    line_edit_points_table_->clearContents();
    line_edit_points_table_->setRowCount(0);

    const auto line_value = active_editor_panel_support::find_line_by_name(
        active_lines_, current_line_edit_source_name_
    );
    if (!line_value.has_value()) {
        line_edit_points_pct_.clear();
        line_edit_point_enabled_.clear();
        line_edit_selected_row_ = -1;
        line_edit_points_table_->blockSignals(false);
        syncing_line_edit_ui_ = false;
        refresh_line_edit_summary();
        refresh_line_edit_state();
        return;
    }

    if (line_edit_points_pct_.empty() && !line_value->pts_pct.empty()) {
        initialize_line_edit_points_from_source();
    } else if (line_edit_point_enabled_.size() != line_edit_points_pct_.size()) {
        line_edit_point_enabled_.assign(line_edit_points_pct_.size(), true);
    }

    line_edit_points_table_->setRowCount(
        static_cast<int>(line_edit_points_pct_.size())
    );
    for (int row = 0; row < static_cast<int>(line_edit_points_pct_.size()); row += 1) {
        const auto point_index
            = static_cast<std::vector<QPointF>::size_type>(row);
        const QPointF& point_value = line_edit_points_pct_.at(point_index);
        const auto enabled_item = new QTableWidgetItem;
        enabled_item->setFlags(enabled_item->flags() | Qt::ItemIsUserCheckable);
        enabled_item->setCheckState(
            line_edit_point_enabled_.at(
                static_cast<std::vector<bool>::size_type>(row)
            )
                ? Qt::Checked
                : Qt::Unchecked
        );
        line_edit_points_table_->setItem(row, 0, enabled_item);

        const auto index_item = new QTableWidgetItem(QString::number(row + 1));
        index_item->setFlags(index_item->flags() & ~Qt::ItemIsEditable);
        line_edit_points_table_->setItem(row, 1, index_item);

        const auto x_item = new QTableWidgetItem(
            QString::number(point_value.x(), 'f', 2)
        );
        x_item->setFlags(x_item->flags() & ~Qt::ItemIsEditable);
        line_edit_points_table_->setItem(row, 2, x_item);

        const auto y_item = new QTableWidgetItem(
            QString::number(point_value.y(), 'f', 2)
        );
        y_item->setFlags(y_item->flags() & ~Qt::ItemIsEditable);
        line_edit_points_table_->setItem(row, 3, y_item);
    }

    if (line_edit_selected_row_ >= 0
        && line_edit_selected_row_ < line_edit_points_table_->rowCount()) {
        line_edit_points_table_->selectRow(line_edit_selected_row_);
    } else {
        line_edit_points_table_->clearSelection();
    }

    line_edit_points_table_->blockSignals(false);
    syncing_line_edit_ui_ = false;
    refresh_line_edit_summary();
    refresh_line_edit_state();
}

void active_editor_panel::refresh_line_edit_summary() const {
    if (line_edit_summary_label_ == nullptr) {
        return;
    }

    const auto line_value = active_editor_panel_support::find_line_by_name(
        active_lines_, current_line_edit_source_name_
    );
    if (line_value.has_value()) {
        const int kept_points = static_cast<int>(std::count(
            line_edit_point_enabled_.cbegin(), line_edit_point_enabled_.cend(), true
        ));
        const QString selected_text = line_edit_selected_row_ >= 0
                && line_edit_selected_row_
                    < static_cast<int>(line_edit_points_pct_.size())
            ? QStringLiteral(" Selected point row: %1.")
                  .arg(line_edit_selected_row_ + 1)
            : QString();
        line_edit_summary_label_->setText(
            QStringLiteral(
                "Select an enabled line, exclude points you do not want, then "
                "save the edited variant under a new name. "
                "Current source: %1 | keeping %2 of %3 points.%4 "
                "Drag the shape, click a point to move just that point, use "
                "arrow keys to nudge, and use the mouse wheel to rotate."
            )
                .arg(line_value->template_name)
                .arg(kept_points)
                .arg(line_edit_points_pct_.size())
                .arg(selected_text)
        );
        return;
    }

    line_edit_summary_label_->setText(
        QStringLiteral(
            "Select an enabled line for this stream to preview it as a dashed "
            "editable variant, exclude points, nudge it with the arrow keys, "
            "rotate it with the mouse wheel, and save it under a new name."
        )
    );
}

void active_editor_panel::refresh_line_edit_state() const {
    if (line_edit_combo_ == nullptr || line_edit_points_table_ == nullptr
        || line_edit_name_edit_ == nullptr || line_edit_save_button_ == nullptr
        || editor_tabs == nullptr) {
        return;
    }

    const bool has_active = active_stream_panel_widget != nullptr
        && active_stream_panel_widget->has_active_stream();
    const int enabled_line_count = static_cast<int>(std::count_if(
        active_lines_.cbegin(), active_lines_.cend(),
        [](const stream_cell::line_instance& line_value) {
            return line_value.enabled;
        }
    ));
    const bool has_selected_line
        = !current_line_edit_source_name_.trimmed().isEmpty();
    const int kept_points = static_cast<int>(std::count(
        line_edit_point_enabled_.cbegin(), line_edit_point_enabled_.cend(), true
    ));
    const bool can_save = has_active && has_selected_line
        && !line_edit_name_edit_->text().trimmed().isEmpty()
        && kept_points >= 2;

    line_edit_combo_->setEnabled(has_active && enabled_line_count > 0);
    line_edit_points_table_->setEnabled(has_active && has_selected_line);
    line_edit_name_edit_->setEnabled(has_active && has_selected_line);
    line_edit_save_button_->setEnabled(can_save);

    const int edit_tab_index = editor_tabs->indexOf(edit_tab_widget_);
    if (edit_tab_index >= 0) {
        editor_tabs->setTabEnabled(edit_tab_index, has_active && enabled_line_count > 0);
    }
}

void active_editor_panel::emit_line_edit_preview_if_visible() {
    if (editor_tabs != nullptr && editor_tabs->currentWidget() == edit_tab_widget_
        && !current_line_edit_source_name_.isEmpty()) {
        emit line_edit_preview_changed(current_line_edit_request());
    }
}

line_edit_request active_editor_panel::current_line_edit_request() const {
    line_edit_request request;
    if (active_stream_panel_widget == nullptr) {
        return request;
    }

    const auto line_value = active_editor_panel_support::find_line_by_name(
        active_lines_, current_line_edit_source_name_
    );
    if (!line_value.has_value()) {
        return request;
    }

    request.stream_name
        = active_stream_panel_widget->current_stream_settings().stream_name;
    request.source_line_name = line_value->template_name;
    request.profile = line_profile {
        .name = line_edit_name_edit_ != nullptr
            ? line_edit_name_edit_->text().trimmed()
            : QString(),
        .color = line_value->color,
        .color_mode_id = line_value->color_mode_id,
        .closed = line_value->closed,
        .width_text = line_value->width_text,
        .length_text = line_value->length_text,
        .response_text = line_value->response_text,
    };

    for (int index = 0; index < static_cast<int>(line_edit_points_pct_.size()); index += 1) {
        if (index >= static_cast<int>(line_edit_point_enabled_.size())
            || !line_edit_point_enabled_.at(
                static_cast<std::vector<bool>::size_type>(index)
            )) {
            continue;
        }
        request.points_pct.push_back(
            line_edit_points_pct_.at(
                static_cast<std::vector<QPointF>::size_type>(index)
            )
        );
    }

    return request;
}

void active_editor_panel::on_line_item_changed(
    QTreeWidgetItem* item, const int column
) {
    if (item == nullptr || column != 0) {
        return;
    }

    emit_line_enabled_changed_queued(
        item->data(1, Qt::UserRole).toString(),
        item->checkState(0) == Qt::Checked
    );
}

void active_editor_panel::on_editor_tab_changed(const int index) {
    Q_UNUSED(index);

    if (editor_tabs == nullptr || edit_tab_widget_ == nullptr) {
        return;
    }

    if (editor_tabs->currentWidget() == edit_tab_widget_) {
        if (!current_line_edit_source_name_.isEmpty()) {
            emit_line_edit_preview_if_visible();
        } else {
            emit line_edit_preview_cleared();
        }
        return;
    }

    emit line_edit_preview_cleared();
}

void active_editor_panel::on_line_edit_selection_changed(const int index) {
    Q_UNUSED(index);

    if (line_edit_combo_ == nullptr || syncing_line_edit_ui_) {
        return;
    }

    current_line_edit_source_name_
        = line_edit_combo_->currentData().toString().trimmed();
    initialize_line_edit_points_from_source();

    const auto line_value = active_editor_panel_support::find_line_by_name(
        active_lines_, current_line_edit_source_name_
    );
    if (line_value.has_value()) {
        if (line_edit_name_edit_ != nullptr) {
            QSignalBlocker blocker(line_edit_name_edit_);
            line_edit_name_edit_->setText(
                active_editor_panel_support::suggested_variant_name(
                    line_value->template_name
                )
            );
        }
    } else if (line_edit_name_edit_ != nullptr) {
        QSignalBlocker blocker(line_edit_name_edit_);
        line_edit_name_edit_->clear();
    }

    refresh_line_edit_table();

    if (line_value.has_value()) {
        emit_line_edit_preview_if_visible();
        return;
    }

    emit line_edit_preview_cleared();
}

void active_editor_panel::on_line_edit_point_item_changed(QTableWidgetItem* item) {
    if (item == nullptr || syncing_line_edit_ui_
        || item->column() != 0) {
        return;
    }

    const int row = item->row();
    if (row < 0 || row >= static_cast<int>(line_edit_point_enabled_.size())) {
        return;
    }

    line_edit_point_enabled_.at(static_cast<std::vector<bool>::size_type>(row))
        = item->checkState() == Qt::Checked;
    if (!line_edit_point_enabled_.at(static_cast<std::vector<bool>::size_type>(row))
        && line_edit_selected_row_ == row) {
        line_edit_selected_row_ = -1;
    }
    refresh_line_edit_summary();
    refresh_line_edit_state();
    emit_line_edit_preview_if_visible();
}

void active_editor_panel::on_line_edit_name_text_changed(const QString& text) {
    Q_UNUSED(text);

    refresh_line_edit_state();
    emit_line_edit_preview_if_visible();
}

void active_editor_panel::on_line_edit_save_clicked() {
    const line_edit_request request = current_line_edit_request();
    if (request.source_line_name.isEmpty() || request.profile.name.isEmpty()
        || request.points_pct.size() < 2) {
        return;
    }

    emit line_edit_save_requested(request);

    current_line_edit_source_name_.clear();
    line_edit_points_pct_.clear();
    line_edit_point_enabled_.clear();
    line_edit_selected_row_ = -1;
    if (line_edit_combo_ != nullptr) {
        QSignalBlocker blocker(line_edit_combo_);
        line_edit_combo_->setCurrentIndex(0);
    }
    if (line_edit_name_edit_ != nullptr) {
        QSignalBlocker blocker(line_edit_name_edit_);
        line_edit_name_edit_->clear();
    }
    refresh_line_edit_table();
    emit line_edit_preview_cleared();
}

void active_editor_panel::emit_line_enabled_changed_queued(
    QString line_name, const bool enabled
) {
    pending_line_toggles_.push_back(std::make_pair(std::move(line_name), enabled));
    if (line_toggle_flush_scheduled_) {
        return;
    }

    line_toggle_flush_scheduled_ = true;

    QMetaObject::invokeMethod(
        this,
        [this]() {
            line_toggle_flush_scheduled_ = false;
            const auto queued_toggles = pending_line_toggles_;
            pending_line_toggles_.clear();

            for (const auto& [queued_line_name, queued_enabled] : queued_toggles) {
                emit line_enabled_changed(queued_line_name, queued_enabled);
            }
        },
        Qt::QueuedConnection
    );
}
