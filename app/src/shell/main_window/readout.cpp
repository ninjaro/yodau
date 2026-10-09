#include "shell/main_window.hpp"

#include "shell/stream_controller.hpp"
#include "widgets/processing_readout.hpp"

#include <QAction>
#include <QDockWidget>
#include <QKeySequence>
#include <QPointer>

#ifdef KC_KDE
#include <KActionCollection>
#endif

void main_window::setup_processing_readout() {
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    processing_dock_ = new QDockWidget(tr("Processing readout"), this);
    processing_dock_->setObjectName("main_window_processing_dock");
    processing_dock_->setAllowedAreas(
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
        | Qt::BottomDockWidgetArea
    );
    processing_dock_->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
    );
    auto* readout = new processing_readout(processing_dock_);
    const QPointer<stream_controller> controller(app_stream_controller);
    readout->set_reader([controller] {
        return controller ? controller->active_processing_sample()
                          : processing_sample {};
    });
    processing_dock_->setWidget(readout);
    addDockWidget(Qt::RightDockWidgetArea, processing_dock_);
    processing_dock_->hide();
    auto* action = processing_dock_->toggleViewAction();
    action->setObjectName("view_processing_dock");
    action->setText(tr("Processing &readout"));
#ifdef KC_KDE
    KActionCollection::setDefaultShortcut(
        action, QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P)
    );
#else
    action->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_P));
#endif
    action->setToolTip(
        tr("Inspect the active stream's existing GUI observations and "
           "processing policy. No profiler or pipeline editor.")
    );
#endif
}
