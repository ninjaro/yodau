#include "widgets/active_editor_panel.hpp"

#include "widgets/active_stream_panel.hpp"
#include "widgets/line_profile_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScopedValueRollback>
#include <QSignalBlocker>
#include <QStandardItemModel>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {

bool discard_changes(QWidget* parent, const QString& message) {
    return QMessageBox::question(
               parent, QObject::tr("Canvas tools"), message,
               QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Cancel
           )
        == QMessageBox::Discard;
}

} // namespace

QWidget* active_editor_panel::make_canvas_tools(stream_board* board) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    Q_UNUSED(board);
    return nullptr;
#else
    if (canvas_.root != nullptr || board == nullptr) {
        return canvas_.root;
    }
    canvas_.board = board;
    auto* root = new QWidget(board);
    canvas_.root = root;
    root->setObjectName(QStringLiteral("canvas_tools"));
    root->setAccessibleName(tr("Canvas tools"));
    auto* layout = new QVBoxLayout(root);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);
    auto* tools = new QHBoxLayout;
    canvas_.tool = new QComboBox(root);
    canvas_.tool->setObjectName(QStringLiteral("canvas_tool"));
    canvas_.tool->setAccessibleName(tr("Canvas tool"));
    canvas_.tool->setToolTip(tr(
        "Select stops drawing (templates remain in the Lines dock). "
        "Draw geometry adds points; Edit points adjusts an enabled geometry."
    ));
    canvas_.tool->setModel(new QStandardItemModel(canvas_.tool));
    canvas_.tool->addItem(tr("Select"), static_cast<int>(canvas_tool::select));
    canvas_.tool->addItem(
        tr("Draw geometry"), static_cast<int>(canvas_tool::draw)
    );
    canvas_.tool->addItem(
        tr("Edit points"), static_cast<int>(canvas_tool::edit)
    );
    tools->addWidget(canvas_.tool);
    canvas_.target = new QComboBox(root);
    canvas_.target->setObjectName(QStringLiteral("canvas_target"));
    canvas_.target->setAccessibleName(tr("Geometry to edit"));
    canvas_.target->setToolTip(
        tr("Choose an enabled geometry on the active stream.")
    );
    canvas_.target->setSizeAdjustPolicy(
        QComboBox::AdjustToMinimumContentsLengthWithIcon
    );
    canvas_.target->setMinimumContentsLength(4);
    tools->addWidget(canvas_.target, 1);
    tools->addStretch();
    layout->addLayout(tools);

    canvas_.details = new QWidget(root);
    auto* details = new QHBoxLayout(canvas_.details);
    details->setContentsMargins(0, 0, 0, 0);
    canvas_.name = new QLineEdit(canvas_.details);
    canvas_.name->setObjectName(QStringLiteral("canvas_name"));
    canvas_.name->setAccessibleName(tr("Geometry name"));
    canvas_.name->setPlaceholderText(tr("Geometry name"));
    canvas_.name->setMinimumWidth(60);
    details->addWidget(canvas_.name, 1);
    canvas_.closed = new QCheckBox(tr("Closed"), canvas_.details);
    canvas_.closed->setObjectName(QStringLiteral("canvas_closed"));
    canvas_.closed->setToolTip(
        tr("Close the new geometry using the existing profile setting.")
    );
    details->addWidget(canvas_.closed);
    canvas_.save = new QPushButton(tr("Save"), canvas_.details);
    canvas_.save->setObjectName(QStringLiteral("canvas_save"));
    details->addWidget(canvas_.save);
    layout->addWidget(canvas_.details);

    canvas_.history = new QWidget(root);
    auto* history = new QHBoxLayout(canvas_.history);
    history->setContentsMargins(0, 0, 0, 0);
    const auto button = [&](const QString& text, const QString& name) {
        auto* result = new QPushButton(text, canvas_.history);
        result->setObjectName(name);
        result->setAccessibleName(text);
        history->addWidget(result);
        return result;
    };
    canvas_.undo = button(tr("Undo"), QStringLiteral("canvas_undo"));
    canvas_.redo = button(tr("Redo"), QStringLiteral("canvas_redo"));
    canvas_.revert = button(tr("Revert"), QStringLiteral("canvas_revert"));
    history->addStretch();
    layout->addWidget(canvas_.history);

    connect(canvas_.tool, &QComboBox::activated, this, [this](int index) {
        choose_canvas_tool(
            static_cast<canvas_tool>(canvas_.tool->itemData(index).toInt())
        );
    });
    connect(canvas_.target, &QComboBox::activated, this, [this](int index) {
        const auto name = canvas_.target->itemData(index).toString();
        if (name != line_edit_source_name_ && line_edit_has_unsaved_changes()
            && !discard_changes(
                canvas_.root,
                tr("Discard changes to the current edit before choosing "
                   "another geometry?")
            )) {
            refresh_canvas_tools();
            return;
        }
        line_edit_combo_->setCurrentIndex(
            name.isEmpty() ? 0 : line_edit_combo_->findData(name)
        );
    });
    connect(
        canvas_.name, &QLineEdit::textEdited, this,
        [this](const QString& name) {
            const QScopedValueRollback typing(canvas_.typing, true);
            if (current_canvas_tool() == canvas_tool::edit) {
                line_edit_name_edit_->setText(name);
            } else {
                auto profile = current_line_profile();
                profile.name = name;
                set_line_profile(profile);
                emit line_profile_changed(current_line_profile());
            }
        }
    );
    connect(canvas_.closed, &QCheckBox::toggled, this, [this](bool closed) {
        active_line_panel->set_line_closed(closed);
    });
    connect(canvas_.save, &QPushButton::clicked, this, [this] {
        if (current_canvas_tool() == canvas_tool::edit) {
            on_line_edit_save_clicked();
        } else {
            emit line_save_requested(current_line_profile());
        }
    });
    connect(canvas_.undo, &QPushButton::clicked, this, [this] {
        undo_line_edit_change();
    });
    connect(canvas_.redo, &QPushButton::clicked, this, [this] {
        redo_line_edit_change();
    });
    connect(canvas_.revert, &QPushButton::clicked, this, [this] {
        revert_line_edit_changes();
    });
    connect(this, &active_editor_panel::line_profile_changed, this, [this] {
        refresh_canvas_tools();
    });
    refresh_canvas_tools();
    return root;
#endif
}

active_editor_panel::canvas_tool
active_editor_panel::current_canvas_tool() const {
    if (editor_tabs->currentWidget() == edit_tab_widget_) {
        return canvas_tool::edit;
    }
    return active_stream_panel_widget->drawing_new_mode() ? canvas_tool::draw
                                                          : canvas_tool::select;
}

void active_editor_panel::choose_canvas_tool(const canvas_tool tool) {
    if (!active_stream_panel_widget->has_active_stream()
        || tool == current_canvas_tool()) {
        refresh_canvas_tools();
        return;
    }
    if (tool == canvas_tool::select
        && active_stream_panel_widget->drawing_new_mode()) {
        auto* cell = canvas_.board->active_cell();
        if (cell != nullptr && !cell->is_draft_preview()
            && !cell->draft_points_pct().empty()
            && !discard_changes(
                canvas_.root,
                tr("Discard the unfinished drawing and switch to Select?")
            )) {
            refresh_canvas_tools();
            return;
        }
    }
    if (tool == canvas_tool::edit) {
        const int index = editor_tabs->indexOf(edit_tab_widget_);
        if (editor_tabs->isTabEnabled(index)) {
            editor_tabs->setCurrentIndex(index);
        }
    } else {
        editor_tabs->setCurrentIndex(0);
        active_stream_panel_widget->set_drawing_new_mode(
            tool == canvas_tool::draw
        );
    }
    refresh_canvas_tools();
}

void active_editor_panel::refresh_canvas_tools() const {
    if (canvas_.root == nullptr) {
        return;
    }
    const bool active = active_stream_panel_widget->has_active_stream();
    const auto tool = current_canvas_tool();
    const bool editing = tool == canvas_tool::edit;
    const QSignalBlocker tool_block(canvas_.tool);
    canvas_.tool->setCurrentIndex(
        canvas_.tool->findData(static_cast<int>(tool))
    );
    canvas_.tool->setEnabled(active);
    auto* modes = static_cast<QStandardItemModel*>(canvas_.tool->model());
    modes->item(2)->setEnabled(active && line_edit_combo_->count() > 1);
    {
        const QSignalBlocker blocker(canvas_.target);
        bool same = canvas_.target->count() == line_edit_combo_->count();
        for (int index = 0; same && index < line_edit_combo_->count();
             ++index) {
            same = canvas_.target->itemText(index)
                    == line_edit_combo_->itemText(index)
                && canvas_.target->itemData(index)
                    == line_edit_combo_->itemData(index);
        }
        if (!same) {
            canvas_.target->clear();
            for (int index = 0; index < line_edit_combo_->count(); ++index) {
                canvas_.target->addItem(
                    line_edit_combo_->itemText(index),
                    line_edit_combo_->itemData(index)
                );
            }
        }
        canvas_.target->setCurrentIndex(line_edit_combo_->currentIndex());
    }
    canvas_.target->setVisible(editing);
    canvas_.target->setEnabled(line_edit_combo_->isEnabled());
    canvas_.details->setVisible(tool != canvas_tool::select);
    canvas_.history->setVisible(editing);
    const QSignalBlocker name_block(canvas_.name);
    const auto name
        = editing ? line_edit_name_edit_->text() : current_line_profile().name;
    if (!canvas_.typing && canvas_.name->text() != name) {
        canvas_.name->setText(name);
    }
    canvas_.name->setEnabled(
        active && (!editing || line_edit_name_edit_->isEnabled())
    );
    canvas_.name->setToolTip(
        editing ? tr("Save an edited variant; existing replacement/validation "
                     "rules apply.")
                : tr("Name the new geometry; at least two points are required "
                     "to save.")
    );
    canvas_.closed->setVisible(!editing);
    const QSignalBlocker closed_block(canvas_.closed);
    canvas_.closed->setChecked(current_line_profile().closed);
    canvas_.closed->setEnabled(active);
    canvas_.save->setEnabled(
        active && (!editing || line_edit_save_button_->isEnabled())
    );
    canvas_.save->setAccessibleName(
        editing ? tr("Save edited geometry") : tr("Save new geometry")
    );
    canvas_.save->setToolTip(canvas_.name->toolTip());
    canvas_.undo->setEnabled(line_edit_undo_button_->isEnabled());
    canvas_.redo->setEnabled(line_edit_redo_button_->isEnabled());
    canvas_.revert->setEnabled(line_edit_revert_button_->isEnabled());
}
