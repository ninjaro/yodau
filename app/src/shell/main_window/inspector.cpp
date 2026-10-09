#include "shell/main_window.hpp"

#include "widgets/active_editor_panel.hpp"

#include <QAction>
#include <QDockWidget>
#include <QMessageBox>
#include <QSettings>

void main_window::setup_context_inspector() {
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    auto* editor = line_dock != nullptr
        ? qobject_cast<active_editor_panel*>(line_dock->widget())
        : nullptr;
    if (editor == nullptr) {
        return;
    }
    QSettings preferences;
    const bool enabled = yodau::shell::load_context_inspector(preferences);
    editor->set_context_inspector(enabled);
    inspector_action_ = new QAction(tr("Contextual &inspector"), this);
    inspector_action_->setObjectName(
        QStringLiteral("desktop_context_inspector")
    );
    inspector_action_->setCheckable(true);
    inspector_action_->setChecked(enabled);
    inspector_action_->setToolTip(
        tr("Present the existing Lines dock as active-context geometry "
           "properties. Its visibility stays independent.")
    );
    connect(
        inspector_action_, &QAction::triggered, this,
        [this, editor](bool contextual) {
            QSettings app_preferences;
            if (!yodau::shell::save_context_inspector(
                    app_preferences, contextual
                )) {
                inspector_action_->setChecked(!contextual);
                QMessageBox::warning(
                    this, tr("Contextual inspector"),
                    tr("Could not save the contextual inspector preference.")
                );
                return;
            }
            editor->set_context_inspector(contextual);
        }
    );
#endif
}
