#include "shell/main_window.hpp"
#include <shared_ui/qt/action_group.hpp>
#include <shared_ui/qt/appearance_editor.hpp>
#include <shared_ui/qt/presentation.hpp>
#include <shared_ui/qt/readout_group.hpp>
#include <shared_ui/qt/settings_section.hpp>

#include "shell/desktop_shell.hpp"
#include "shell/mobile_session_store.hpp"
#include "shell/str_label.hpp"
#include "widgets/log_area_view.hpp"
#include "widgets/log_toolbar_panel.hpp"
#include "widgets/settings_panel.hpp"
#include "widgets/stream_board.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCoreApplication>
#include <QDialog>
#include <QDockWidget>
#include <QEvent>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QStackedWidget>
#include <QStyle>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>

namespace {

#ifdef KC_KDE
constexpr auto desktop_profile = shared_ui::qt::platform_profile::desktop_kde;
#else
constexpr auto desktop_profile = shared_ui::qt::platform_profile::desktop_qt;
#endif

#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
// Keeps a vertically scrolling dock as wide as its content's current minimum,
// including after a style sheet changes control metrics.
class scroll_width_follower final : public QObject {
public:
    explicit scroll_width_follower(QScrollArea* scroll)
        : QObject(scroll)
        , scroll_(scroll) { }

    bool eventFilter(QObject* watched, QEvent* event) override {
        if (event->type() == QEvent::LayoutRequest
            || event->type() == QEvent::Show) {
            auto* content = scroll_->widget();
            if (watched == content && content != nullptr) {
                scroll_->setMinimumWidth(
                    content->minimumSizeHint().width()
                    + scroll_->verticalScrollBar()->sizeHint().width()
                );
            }
        }
        return QObject::eventFilter(watched, event);
    }

private:
    QScrollArea* scroll_;
};
#endif

} // namespace

void main_window::setup_platform_layout() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    auto* mobile_shell = new QWidget(this);
    mobile_shell->setObjectName(QStringLiteral("mobile_shell"));
    auto* mobile_layout = new QVBoxLayout(mobile_shell);
    mobile_layout->setContentsMargins(0, 0, 0, 0);
    mobile_layout->setSpacing(0);

    mobile_document_toolbar = new QToolBar(tr("Documents"), mobile_shell);
    mobile_document_toolbar->setObjectName(
        QStringLiteral("mobile_document_toolbar")
    );
    mobile_document_toolbar->setMovable(false);
    mobile_document_toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    mobile_document_toolbar->setMinimumHeight(52);
    mobile_layout->addWidget(mobile_document_toolbar);

    mobile_status_label = new QLabel(mobile_shell);
    mobile_status_label->setObjectName(QStringLiteral("mobile_status_label"));
    mobile_status_label->setWordWrap(true);
    mobile_status_label->setMargin(8);
    mobile_status_label->setAccessibleName(tr("Session status"));
    mobile_status_label->hide();
    mobile_layout->addWidget(mobile_status_label);

    zones_stack = new QStackedWidget(mobile_shell);
    zones_stack->setObjectName(QStringLiteral("mobile_page_stack"));
    zones_stack->addWidget(main_zone);

    auto* streams_scroll = new QScrollArea(zones_stack);
    streams_scroll->setObjectName(QStringLiteral("mobile_streams_scroll"));
    streams_scroll->setWidgetResizable(true);
    streams_scroll->setFrameShape(QFrame::NoFrame);
    streams_scroll->setWidget(settings);
    zones_stack->addWidget(streams_scroll);

    if (auto* line_editor = settings->take_active_editor_widget()) {
        line_editor->setObjectName(QStringLiteral("mobile_line_editor_page"));
        auto* lines_scroll = new QScrollArea(zones_stack);
        lines_scroll->setObjectName(QStringLiteral("mobile_lines_scroll"));
        lines_scroll->setWidgetResizable(true);
        lines_scroll->setFrameShape(QFrame::NoFrame);
        lines_scroll->setWidget(line_editor);
        zones_stack->addWidget(lines_scroll);
    } else {
        zones_stack->addWidget(new QWidget(zones_stack));
    }

    auto* log_page = new QWidget(zones_stack);
    log_page->setObjectName(QStringLiteral("mobile_log_page"));
    auto* log_layout = new QVBoxLayout(log_page);
    log_layout->setContentsMargins(8, 8, 8, 8);
    if (auto* log_toolbar = settings->take_log_toolbar_widget()) {
        log_toolbar->setParent(log_page);
        log_layout->addWidget(log_toolbar);
        auto* log_view = new log_area_view(std::nullopt, log_page);
        log_view->setObjectName(QStringLiteral("mobile_log_view"));
        log_view->set_log_toolbar(log_toolbar);
        log_layout->addWidget(log_view, 1);
    }
    zones_stack->addWidget(log_page);
    zones_stack->setCurrentWidget(main_zone);
    mobile_layout->addWidget(zones_stack, 1);

    mobile_navigation_toolbar = new QToolBar(tr("Navigation"), mobile_shell);
    mobile_navigation_toolbar->setObjectName(
        QStringLiteral("mobile_navigation_toolbar")
    );
    mobile_navigation_toolbar->setMovable(false);
    mobile_navigation_toolbar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    mobile_navigation_toolbar->setMinimumHeight(68);
    mobile_navigation_actions = new QActionGroup(this);
    mobile_navigation_actions->setExclusive(true);

    const auto add_mobile_page_action = [this](
                                            const QString& text,
                                            const QIcon& icon,
                                            const yodau::shell::mobile_page page
                                        ) {
        auto* action = mobile_navigation_toolbar->addAction(icon, text);
        action->setCheckable(true);
        action->setData(static_cast<int>(page));
        action->setObjectName(
            QStringLiteral("mobile_navigation_%1").arg(static_cast<int>(page))
        );
        mobile_navigation_actions->addAction(action);
        return action;
    };
    add_mobile_page_action(
        tr("Monitor"), style()->standardIcon(QStyle::SP_ComputerIcon),
        yodau::shell::mobile_page::monitor
    )
        ->setChecked(true);
    add_mobile_page_action(
        tr("Streams"), style()->standardIcon(QStyle::SP_FileDialogListView),
        yodau::shell::mobile_page::streams
    );
    add_mobile_page_action(
        tr("Lines"), style()->standardIcon(QStyle::SP_FileDialogDetailedView),
        yodau::shell::mobile_page::lines
    );
    add_mobile_page_action(
        tr("Logs"), style()->standardIcon(QStyle::SP_FileDialogInfoView),
        yodau::shell::mobile_page::logs
    );
    mobile_layout->addWidget(mobile_navigation_toolbar);

    connect(
        mobile_navigation_actions, &QActionGroup::triggered, this,
        [this](QAction* action) {
            if (action != nullptr) {
                show_mobile_page(action->data().toInt());
            }
        }
    );
    connect(
        zones_stack, &QStackedWidget::currentChanged, this,
        [this](const int index) {
            if (mobile_navigation_actions != nullptr) {
                for (QAction* action : mobile_navigation_actions->actions()) {
                    action->setChecked(action->data().toInt() == index);
                }
            }
            QSettings mobile_settings(
                QStringLiteral("ninjaro"), QStringLiteral("yodau")
            );
            static_cast<void>(yodau::shell::save_mobile_navigation(
                mobile_settings, yodau::shell::normalized_mobile_page(index)
            ));
        }
    );

    mobile_shell->setStyleSheet(QStringLiteral(
        "QPushButton, QToolButton, QComboBox, QSpinBox, QLineEdit {"
        " min-height: 44px; }"
        "QPushButton { padding: 6px 12px; border-radius: 10px; }"
        "QTabBar::tab { min-height: 44px; padding: 6px 12px; }"
        "QAbstractItemView::item { min-height: 40px; }"
        "QCheckBox, QRadioButton { spacing: 10px; padding: 6px 2px; }"
        "QScrollBar:vertical { width: 18px; }"
        "QScrollBar:horizontal { height: 18px; }"
    ));
    setCentralWidget(mobile_shell);
#else
    settings_dock = new QDockWidget(str_label("streams"), this);
    line_dock = new QDockWidget(str_label("lines"), this);
    log_dock = new QDockWidget(str_label("logs"), this);
    setCentralWidget(main_zone);

    settings_dock->setObjectName(QStringLiteral("main_window_streams_dock"));
    // A short window must scroll the source/grid/inventory forms instead of
    // squeezing their group boxes below their minimum height.
    auto* settings_scroll = new QScrollArea(settings_dock);
    settings_scroll->setObjectName(
        QStringLiteral("main_window_streams_scroll")
    );
    settings_scroll->setWidgetResizable(true);
    settings_scroll->setFrameShape(QFrame::NoFrame);
    settings_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    settings_scroll->setWidget(settings);
    settings->installEventFilter(new scroll_width_follower(settings_scroll));
    settings_dock->setWidget(settings_scroll);
    settings_dock->setAllowedAreas(
        Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
    );
    settings_dock->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
    );
    addDockWidget(Qt::RightDockWidgetArea, settings_dock);
    auto* settings_action = settings_dock->toggleViewAction();
    settings_action->setText(str_label("streams"));

    if (auto* line_editor = settings->take_active_editor_widget()) {
        line_dock->setObjectName(QStringLiteral("main_window_line_dock"));
        line_dock->setWidget(line_editor);
        line_dock->setAllowedAreas(
            Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea
        );
        line_dock->setFeatures(
            QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
        );
        addDockWidget(Qt::LeftDockWidgetArea, line_dock);
    }
    auto* line_action = line_dock->toggleViewAction();
    line_action->setText(str_label("lines"));

    auto* log_dock_content = new QWidget(log_dock);
    log_dock_content->setObjectName(
        QStringLiteral("main_window_log_dock_content")
    );
    auto* log_dock_layout = new QVBoxLayout(log_dock_content);
    log_dock_layout->setContentsMargins(8, 8, 8, 8);

    if (auto* log_toolbar = settings->take_log_toolbar_widget()) {
        log_toolbar->setParent(log_dock_content);
        log_dock_layout->addWidget(log_toolbar);

        auto* dock_log_view = new log_area_view(std::nullopt, log_dock_content);
        dock_log_view->setObjectName(QStringLiteral("main_window_log_view"));
        dock_log_view->set_log_toolbar(log_toolbar);
        log_dock_layout->addWidget(dock_log_view, 1);
    }

    log_dock->setObjectName(QStringLiteral("main_window_log_dock"));
    log_dock->setWidget(log_dock_content);
    log_dock->setAllowedAreas(
        Qt::BottomDockWidgetArea | Qt::RightDockWidgetArea
        | Qt::LeftDockWidgetArea
    );
    log_dock->setFeatures(
        QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable
    );
    addDockWidget(Qt::BottomDockWidgetArea, log_dock);
    auto* log_action = log_dock->toggleViewAction();
    log_action->setText(str_label("logs"));
#endif
}

void main_window::show_mobile_page(const int page_index) {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    if (zones_stack != nullptr) {
        zones_stack->setCurrentIndex(
            static_cast<int>(yodau::shell::normalized_mobile_page(page_index))
        );
    }
#else
    Q_UNUSED(page_index);
#endif
}

void main_window::show_settings_panel() {
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    if (settings_dock != nullptr) {
        settings_dock->show();
        settings_dock->raise();
        settings_dock->setFocus(Qt::ShortcutFocusReason);
    }
#else
    if (zones_stack != nullptr) {
        show_mobile_page(static_cast<int>(yodau::shell::mobile_page::streams));
    }
#endif
}

void main_window::setup_desktop_shell() {
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    setup_canvas_tools();
    setup_context_inspector();
    setup_processing_readout();
    auto* presentation_menu = setup_desktop_presentation_menu();
#endif
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    import_line_configuration_action->setText(tr("Import"));
    export_line_configuration_action->setText(tr("Export"));
    import_line_configuration_action->setShortcut(QKeySequence());
    export_line_configuration_action->setShortcut(QKeySequence());
    import_line_configuration_action->setIcon(
        style()->standardIcon(QStyle::SP_DialogOpenButton)
    );
    export_line_configuration_action->setIcon(
        style()->standardIcon(QStyle::SP_DialogSaveButton)
    );
    if (mobile_document_toolbar != nullptr) {
        mobile_document_toolbar->addAction(import_line_configuration_action);
        mobile_document_toolbar->addAction(export_line_configuration_action);
    }
    return;
#else
    yodau::shell::setup_desktop_shell(
        *this,
        { import_line_configuration_action,
          export_line_configuration_action,
          presentation_menu,
          { settings_dock, line_dock, log_dock, processing_dock_ } },
        [this] { show_settings_panel(); }
    );
#endif
}

QMenu* main_window::setup_desktop_presentation_menu() {
    auto* menu = new QMenu(tr("Desktop &presentation"), this);
    menu->setObjectName(QStringLiteral("desktop_presentation_menu"));
    menu->addSection(tr("Preset (panel toggles override it)"));
    desktop_preset_actions_ = new QActionGroup(menu);
    const auto add_preset
        = [this, menu](
              const QString& label, yodau::shell::desktop_preset preset,
              const QString& name
          ) {
              auto* action = menu->addAction(label);
              action->setObjectName(name);
              action->setCheckable(true);
              action->setData(static_cast<int>(preset));
              desktop_preset_actions_->addAction(action);
              connect(action, &QAction::triggered, this, [this, preset] {
                  yodau::shell::desktop_presentation value;
                  value.preset = preset;
                  choose_desktop_presentation(value);
              });
          };
    add_preset(
        tr("Classic"), yodau::shell::desktop_preset::classic,
        QStringLiteral("desktop_preset_classic")
    );
    add_preset(
        tr("Canvas"), yodau::shell::desktop_preset::canvas,
        QStringLiteral("desktop_preset_canvas")
    );
    auto* reset = menu->addAction(tr("Reset panels to preset"));
    reset->setObjectName(QStringLiteral("desktop_preset_reset"));
    connect(reset, &QAction::triggered, this, [this] {
        auto value = desktop_presentation_.value_or(
            yodau::shell::desktop_presentation {}
        );
        value.reset_overrides();
        choose_desktop_presentation(value);
    });
    if (canvas_tools_action_ != nullptr) {
        menu->addAction(canvas_tools_action_);
    }
    if (inspector_action_ != nullptr) {
        menu->addAction(inspector_action_);
    }
    auto* pool_menu = menu->addMenu(tr("Pool &position"));
    pool_menu->setObjectName(QStringLiteral("desktop_pool_menu"));
    pool_side_actions_ = new QActionGroup(pool_menu);
    const auto add_side =
        [this,
         pool_menu](const QString& label, pool_side side, const QString& name) {
            auto* action = pool_menu->addAction(label);
            action->setObjectName(name);
            action->setCheckable(true);
            action->setData(static_cast<int>(side));
            pool_side_actions_->addAction(action);
            connect(action, &QAction::triggered, this, [this, side] {
                choose_pool_side(side);
            });
        };
    add_side(
        tr("&Bottom"), pool_side::bottom, QStringLiteral("desktop_pool_bottom")
    );
    add_side(
        tr("&Right"), pool_side::right, QStringLiteral("desktop_pool_right")
    );
    add_side(tr("&Left"), pool_side::left, QStringLiteral("desktop_pool_left"));
    refresh_pool_actions();
    auto* appearance = menu->addAction(tr("&Appearance…"));
    appearance->setObjectName(QStringLiteral("desktop_appearance"));
    connect(
        appearance, &QAction::triggered, this,
        &main_window::show_desktop_appearance
    );
    menu->addSeparator();
    for (auto* dock : { settings_dock, line_dock, log_dock }) {
        if (dock != nullptr) {
            menu->addAction(dock->toggleViewAction());
            connect(
                dock->toggleViewAction(), &QAction::triggered, this,
                [this] { persist_desktop_presentation(); }
            );
        }
    }
    return menu;
}

void main_window::refresh_pool_actions() {
    if (pool_side_actions_ == nullptr) {
        return;
    }
    for (auto* action : pool_side_actions_->actions()) {
        action->setChecked(
            action->data().toInt()
            == static_cast<int>(main_zone->current_pool_side())
        );
    }
}

void main_window::choose_pool_side(const pool_side side) {
    QSettings app_preferences;
    if (!yodau::shell::save_pool_side(app_preferences, side)) {
        refresh_pool_actions();
        QMessageBox::warning(
            this, tr("Pool position"),
            tr("Could not save the pool position settings.")
        );
        return;
    }
    main_zone->set_pool_side(side);
    refresh_pool_actions();
}

void main_window::setup_desktop_appearance() {
    using namespace shared_ui::qt;
    chrome_model_ = new appearance_model(this);
    chrome_model_->setObjectName(QStringLiteral("desktop_appearance_model"));
    chrome_model_->set_presentation(material::system, desktop_profile);
    component_family chrome;
    chrome.id = QStringLiteral("chrome");
    chrome.label = tr("Window material");
    chrome.role = family_role::material;
    chrome.options = { { QStringLiteral("system"), tr("Native"),
                         tr("Use the platform style and palette.") },
                       { QStringLiteral("slate"), tr("Slate"),
                         tr("Dark instrument panels and blue accents.") } };
    chrome_model_->set_families({ chrome });
    // A single material default, unrelated to Yodau's Classic/Canvas workspace.
    chrome_model_->set_presets(
        { { QStringLiteral("native"),
            tr("Native default"),
            {},
            { { QStringLiteral("chrome"), QStringLiteral("system") } } } }
    );
    QSettings preferences;
    const auto saved = yodau::shell::load_desktop_chrome(preferences);
    chrome_model_->load(
        { QStringLiteral("native"),
          { { QStringLiteral("chrome"),
              saved == yodau::shell::desktop_chrome::slate
                  ? QStringLiteral("slate")
                  : QStringLiteral("system") } } }
    );
    apply_desktop_chrome(saved);
}

void main_window::show_desktop_appearance() {
    using namespace shared_ui::qt;
    if (appearance_dialog_) {
        appearance_dialog_->show();
        appearance_dialog_->raise();
        appearance_dialog_->activateWindow();
        return;
    }
    auto* dialog = new QDialog(this);
    appearance_dialog_ = dialog;
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->setObjectName(QStringLiteral("desktop_appearance_dialog"));
    dialog->setWindowTitle(tr("Appearance"));
    apply_presentation(*dialog, { material::system, desktop_profile, {} });
    auto* layout = new QVBoxLayout(dialog);
    auto* scroll = new QScrollArea(dialog);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    layout->addWidget(scroll);
    auto* editor = new appearance_editor(chrome_model_);
    scroll->setWidget(editor);
    editor->set_commit_style(commit_style::dialog);
    editor->set_identity(
        { tr("Yodau"), tr("Appearance"),
          tr("Window material only. Workspace, streams and processing stay "
             "unchanged.") }
    );
    editor->set_presentation(material::system, desktop_profile);
    editor->set_preview_caption(tr("Sample controls · no live stream data"));
    const QPointer<appearance_model> model(chrome_model_);
    editor->set_preview_factory(
        [model](const QString&, const QString&) -> QWidget* {
            if (!model)
                return nullptr;
            const auto value = model->presentation_material();
            auto* preview = new QWidget;
            auto* column = new QVBoxLayout(preview);
            auto* section
                = new settings_section(tr("Sample settings"), preview);
            section->add_row(tr("Name"), new QLineEdit(tr("Preview")));
            section->set_presentation(value, desktop_profile);
            column->addWidget(section);
            auto* actions = new action_group(preview);
            auto* open = new QAction(
                preview->style()->standardIcon(QStyle::SP_DialogOpenButton),
                tr("Open"), actions
            );
            auto* stop = new QAction(
                preview->style()->standardIcon(QStyle::SP_MediaStop),
                tr("Stop"), actions
            );
            stop->setEnabled(false);
            actions->set_actions({ open, stop });
            actions->set_presentation(value, desktop_profile);
            column->addWidget(actions);
            auto* readout = new readout_group(preview);
            readout->set_items(
                { { tr("Status"), tr("Idle"), {}, tr("Preview only") } }
            );
            readout->set_presentation(value, desktop_profile);
            column->addWidget(readout);
            apply_presentation(*preview, { value, desktop_profile, {} });
            return preview;
        }
    );
    connect(
        editor, &appearance_editor::save_requested, this,
        [this, editor](quint64 id, const appearance_state& submitted) {
            const auto choice = submitted.overrides.value(
                QStringLiteral("chrome"), QStringLiteral("system")
            );
            QSettings preferences;
            const auto chrome = choice == QStringLiteral("slate")
                ? yodau::shell::desktop_chrome::slate
                : yodau::shell::desktop_chrome::native;
            if ((choice != QStringLiteral("system")
                 && choice != QStringLiteral("slate"))
                || !yodau::shell::save_desktop_chrome(preferences, chrome)) {
                editor->report_save_failed(
                    id, tr("Could not save the window material."), {}
                );
                return;
            }
            apply_desktop_chrome(chrome);
            editor->report_saved(id);
        }
    );
    connect(
        editor, &appearance_editor::finished, dialog, [dialog](bool accepted) {
            if (accepted)
                dialog->accept();
            else
                dialog->reject();
        }
    );
    // Escape and the window close button are cancellation too; no live trial.
    connect(
        dialog, &QDialog::finished, chrome_model_, &appearance_model::revert
    );
    dialog->resize(820, 620);
    dialog->show();
}

void main_window::apply_desktop_chrome(
    const yodau::shell::desktop_chrome chrome
) {
    // Presentation only: docks, pool, streams and processing are untouched.
    shared_ui::qt::apply_presentation(
        *this,
        { chrome == yodau::shell::desktop_chrome::slate
              ? shared_ui::qt::material::slate
              : shared_ui::qt::material::system,
          desktop_profile,
          {} }
    );
}

void main_window::refresh_desktop_preset_actions() {
    if (desktop_preset_actions_ == nullptr) {
        return;
    }
    const auto preset
        = desktop_presentation_.value_or(yodau::shell::desktop_presentation {})
              .preset;
    for (auto* action : desktop_preset_actions_->actions()) {
        action->setChecked(action->data().toInt() == static_cast<int>(preset));
    }
}

void main_window::choose_desktop_presentation(
    yodau::shell::desktop_presentation value
) {
    QSettings app_preferences;
    if (!yodau::shell::save_desktop_presentation(app_preferences, value)) {
        QMessageBox::warning(
            this, tr("Desktop presentation"),
            tr("Could not save the desktop presentation settings.")
        );
        refresh_desktop_preset_actions();
        return;
    }
    desktop_presentation_ = value;
    yodau::shell::apply_desktop_presentation(
        value, { settings_dock, line_dock, log_dock }
    );
    refresh_desktop_preset_actions();
}

void main_window::persist_desktop_presentation() {
    // Do not opt existing users into a new preset simply by restoring/closing
    // their window. Their named Qt docks remain the authority until opt-in.
    if (!desktop_presentation_) {
        return;
    }
    yodau::shell::capture_desktop_panel_overrides(
        *desktop_presentation_, { settings_dock, line_dock, log_dock }
    );
    QSettings app_preferences;
    if (!yodau::shell::save_desktop_presentation(
            app_preferences, *desktop_presentation_
        )) {
        qWarning("Could not save desktop presentation overrides");
    }
}
