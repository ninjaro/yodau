#include "shell/main_window.hpp"

#include "widgets/active_editor_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QDockWidget>
#include <QMessageBox>
#include <QSettings>

void main_window::setup_canvas_tools() {
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    auto* editor = line_dock != nullptr
        ? qobject_cast<active_editor_panel*>(line_dock->widget())
        : nullptr;
    if (editor == nullptr) {
        return;
    }
    auto* tools = editor->make_canvas_tools(main_zone);
    main_zone->add_canvas_tools(tools);
    QSettings preferences;
    tools->setVisible(yodau::shell::load_canvas_tools(preferences));
    canvas_tools_action_ = new QAction(tr("Canvas &tools"), this);
    canvas_tools_action_->setObjectName(QStringLiteral("desktop_canvas_tools"));
    canvas_tools_action_->setCheckable(true);
    canvas_tools_action_->setChecked(!tools->isHidden());
    canvas_tools_action_->setToolTip(
        tr("Show compact editing controls beside the active canvas. The Lines "
           "dock stays available.")
    );
    connect(
        canvas_tools_action_, &QAction::triggered, this,
        [this, tools](bool shown) {
            QSettings app_preferences;
            if (!yodau::shell::save_canvas_tools(app_preferences, shown)) {
                canvas_tools_action_->setChecked(!tools->isHidden());
                QMessageBox::warning(
                    this, tr("Canvas tools"),
                    tr("Could not save the canvas tools preference.")
                );
                return;
            }
            tools->setVisible(shown);
        }
    );
#endif
}
