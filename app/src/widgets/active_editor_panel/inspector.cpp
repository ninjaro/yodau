#include "widgets/active_editor_panel.hpp"

#include "shell/str_label.hpp"
#include "widgets/active_stream_panel.hpp"
#include "widgets/template_apply_panel.hpp"

#include <QApplication>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

void active_editor_panel::set_context_inspector(const bool enabled) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    Q_UNUSED(enabled);
#else
    if (contextual_ == enabled) {
        return;
    }
    auto* root = static_cast<QVBoxLayout*>(layout());
    if (context_label_ == nullptr) {
        context_label_ = new QLabel(this);
        context_label_->setObjectName(QStringLiteral("inspector_context"));
        context_label_->setTextFormat(Qt::PlainText);
        context_label_->setWordWrap(true);
        context_label_->setAccessibleName(tr("Geometry context"));
        root->insertWidget(0, context_label_);
        inspector_scroll_ = new QScrollArea(this);
        inspector_scroll_->setObjectName(QStringLiteral("inspector_scroll"));
        inspector_scroll_->setAccessibleName(tr("Geometry properties"));
        inspector_scroll_->setWidgetResizable(true);
        inspector_scroll_->setFrameShape(QFrame::NoFrame);
    }
    QPointer<QWidget> focused = QApplication::focusWidget();
    const bool had_focus = focused != nullptr && isAncestorOf(focused);
    contextual_ = enabled;
    if (enabled) {
        delete root->replaceWidget(editor_tabs, inspector_scroll_);
        inspector_scroll_->setWidget(editor_tabs);
    } else {
        inspector_scroll_->takeWidget();
        editor_tabs->setParent(this);
        delete root->replaceWidget(inspector_scroll_, editor_tabs);
    }
    refresh_inspector();
    if (had_focus && focused != nullptr && focused->isVisible()) {
        focused->setFocus(Qt::OtherFocusReason);
    }
#endif
}

void active_editor_panel::refresh_inspector() const {
    if (context_label_ == nullptr) {
        return;
    }
    const bool active = active_stream_panel_widget->has_active_stream();
    const bool target = active && !line_edit_source_name_.isEmpty();
    status_box_->setVisible(!contextual_);
    active_stream_panel_widget->setVisible(!contextual_);
    context_label_->setVisible(contextual_);
    inspector_scroll_->setVisible(contextual_ && active);
    editor_tabs->setVisible(!contextual_ || active);
    line_edit_summary_label_->setVisible(!contextual_);
    for (auto* control : { static_cast<QWidget*>(line_edit_points_table_),
                           static_cast<QWidget*>(line_edit_name_edit_),
                           static_cast<QWidget*>(line_edit_save_button_),
                           static_cast<QWidget*>(line_edit_undo_button_),
                           static_cast<QWidget*>(line_edit_redo_button_),
                           static_cast<QWidget*>(line_edit_revert_button_),
                           static_cast<QWidget*>(line_edit_delete_button_) }) {
        control->setVisible(!contextual_ || target);
    }
    const bool drawing = active_stream_panel_widget->drawing_new_mode();
    editor_tabs->setTabText(
        0,
        contextual_ ? (drawing ? tr("Draw geometry") : tr("Templates"))
                    : str_label("draw")
    );
    editor_tabs->setTabText(
        1, contextual_ ? tr("Saved geometry") : str_label("lines")
    );
    editor_tabs->setTabText(
        2, contextual_ ? tr("Edit points") : str_label("edit")
    );
    if (!contextual_) {
        return;
    }
    QString text;
    if (!active) {
        text = tr(
            "Geometry inspector\nActivate a visible stream from the pool. "
            "Stream management remains in the Streams dock."
        );
    } else {
        const auto stream
            = active_stream_panel_widget->current_stream_settings().stream_name;
        if (editor_tabs->currentWidget() == edit_tab_widget_) {
            text = target
                ? tr("%1 — %2\nEdit points in 0–100% frame coordinates. "
                     "Save creates an edited variant.")
                      .arg(stream, line_edit_source_name_)
                : tr("%1 — Edit points\nChoose enabled geometry below to "
                     "inspect its points.")
                      .arg(stream);
        } else if (editor_tabs->currentIndex() == 1) {
            text = tr("%1 — Saved geometry\nEnable or detach geometry on this "
                      "stream. Detaching keeps the template definition.")
                       .arg(stream);
        } else if (drawing) {
            text = tr("%1 — Draw geometry\nThese properties apply to the "
                      "unfinished drawing, not a saved geometry.")
                       .arg(stream);
        } else if (active_template_panel->has_template_candidates()) {
            text = tr("%1 — Templates\nChoose a template and its application "
                      "settings for this stream.")
                       .arg(stream);
        } else {
            text = tr("%1 — Templates\nNo templates available. Draw and save "
                      "geometry to create one, or import a configuration.")
                       .arg(stream);
        }
    }
    if (context_label_->text() != text) {
        context_label_->setText(text);
    }
}
