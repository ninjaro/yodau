#include "widgets/stream_cell.hpp"

#include "shell/icon_loader.hpp"

#include <QAccessible>
#include <QAction>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <shared_ui/qt/action_group.hpp>

void stream_cell::build_ui() {
    const auto root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    display_actions = new shared_ui::qt::action_group(this);
    display_actions->setObjectName(QStringLiteral("stream_cell_actions"));
    display_actions->setAccessibleName(tr("Stream %1 actions").arg(name));
    display_actions->set_variant(shared_ui::qt::action_variant::compact);
    display_actions->set_toolbar_style(true);
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    // Retain the mobile inline strip and touch targets; no desktop overflow.
    display_actions->set_overflow(shared_ui::qt::action_overflow::host);
    display_actions->set_presentation(
        shared_ui::qt::material::system,
        shared_ui::qt::platform_profile::android
    );
#endif
    manual_size_decrease_action
        = new QAction(tr("Make stream %1 smaller").arg(name), this);
    manual_size_decrease_action->setObjectName(
        QStringLiteral("stream_cell_size_decrease")
    );
    manual_size_decrease_action->setIconText(QStringLiteral("-"));
    manual_size_decrease_action->setToolTip(tr("Make stream cell smaller"));
    manual_size_increase_action
        = new QAction(tr("Make stream %1 larger").arg(name), this);
    manual_size_increase_action->setObjectName(
        QStringLiteral("stream_cell_size_increase")
    );
    manual_size_increase_action->setIconText(QStringLiteral("+"));
    manual_size_increase_action->setToolTip(tr("Make stream cell larger"));
    rotate_action
        = new QAction(tr("Rotate stream %1 clockwise").arg(name), this);
    rotate_action->setObjectName(QStringLiteral("stream_cell_rotate"));
    rotate_action->setIconText(QStringLiteral("90\u00b0"));
    rotate_action->setToolTip(tr("Rotate stream clockwise by 90 degrees"));
    aspect_mode_action = new QAction(tr("Fit"), this);
    aspect_mode_action->setObjectName(
        QStringLiteral("stream_cell_aspect_mode")
    );
    focus_action = new QAction(this);
    focus_action->setObjectName(QStringLiteral("stream_cell_focus"));
    update_icon();
    close_action = new QAction(tr("Close stream %1").arg(name), this);
    close_action->setObjectName(QStringLiteral("stream_cell_close"));
    close_action->setToolTip(tr("close"));
    close_action->setWhatsThis(tr("Remove this stream from the visible grid"));
    close_action->setIcon(icon_loader::title_bar_close_icon());
    display_actions->set_actions(
        { manual_size_decrease_action, manual_size_increase_action,
          rotate_action, aspect_mode_action, focus_action, close_action }
    );
    root->addWidget(display_actions, 0, Qt::AlignRight);
    root->addStretch(1);

    sink = new QVideoSink(this);
    connect(
        sink, &QVideoSink::videoFrameChanged, this,
        &stream_cell::on_frame_changed
    );

    player = new QMediaPlayer(this);
    player->setVideoOutput(sink);
    animation_timer = new QTimer(this);
    animation_timer->setInterval(line_wave_renderer::animation_interval_ms);
    // player->setSource(QUrl::fromLocalFile("/home/yarro/Pictures/kino/1080.mp4"));
    // player->play();

    connect(
        close_action, &QAction::triggered, this, &stream_cell::on_close_clicked
    );
    connect(
        focus_action, &QAction::triggered, this, &stream_cell::on_focus_clicked
    );
    connect(
        rotate_action, &QAction::triggered, this,
        &stream_cell::on_rotate_clicked
    );
    connect(
        aspect_mode_action, &QAction::triggered, this,
        &stream_cell::on_aspect_mode_clicked
    );
    connect(
        manual_size_decrease_action, &QAction::triggered, this,
        &stream_cell::on_manual_size_decrease_clicked
    );
    connect(
        manual_size_increase_action, &QAction::triggered, this,
        &stream_cell::on_manual_size_increase_clicked
    );
    connect(
        player, &QMediaPlayer::mediaStatusChanged, this,
        &stream_cell::on_media_status_changed
    );
    connect(
        player, &QMediaPlayer::errorOccurred, this,
        &stream_cell::on_player_error
    );
    connect(
        animation_timer, &QTimer::timeout, this, &stream_cell::on_animation_tick
    );
    update_display_controls();
}

void stream_cell::update_icon() {
    if (!focus_action) {
        return;
    }
    if (active) {
        focus_action->setToolTip(tr("shrink"));
        focus_action->setText(tr("Return %1 to the stream grid").arg(name));
        focus_action->setWhatsThis(tr("Leave the enlarged stream view"));
        focus_action->setIcon(icon_loader::title_bar_restore_icon());
    } else {
        focus_action->setToolTip(tr("enlarge"));
        focus_action->setText(tr("Enlarge stream %1").arg(name));
        focus_action->setWhatsThis(
            tr("Open this stream in the focused editor view")
        );
        focus_action->setIcon(icon_loader::title_bar_maximize_icon());
    }
}

void stream_cell::update_display_controls() {
    if (rotate_action != nullptr) {
        rotate_action->setIconText(
            QStringLiteral("%1\u00b0").arg(quarter_turns_ * 90)
        );
        rotate_action->setToolTip(
            tr("Current rotation: %1 degrees. Rotate clockwise by 90 degrees.")
                .arg(quarter_turns_ * 90)
        );
    }
    if (aspect_mode_action != nullptr) {
        aspect_mode_action->setVisible(active);
        aspect_mode_action->setIconText(
            keep_frame_aspect_ratio_ ? tr("Stretch") : tr("Fit")
        );
        aspect_mode_action->setToolTip(
            keep_frame_aspect_ratio_
                ? tr("Stretch the active frame to fill the cell")
                : tr("Keep the active frame's aspect ratio")
        );
        aspect_mode_action->setText(aspect_mode_action->toolTip());
    }
    const bool show_manual = fixed_size_controls_visible_ && !active;
    if (manual_size_decrease_action != nullptr) {
        manual_size_decrease_action->setVisible(show_manual);
        manual_size_decrease_action->setEnabled(
            manual_short_side() > manual_minimum
        );
    }
    if (manual_size_increase_action != nullptr) {
        manual_size_increase_action->setVisible(show_manual);
    }
}

void stream_cell::update_accessible_description() {
    QStringList details;
    if (!last_error.isEmpty()) {
        details.push_back(tr("Stream error: %1").arg(last_error));
    } else if (!last_frame.isNull()) {
        details.push_back(tr("Receiving video at %1 by %2 pixels")
                              .arg(last_frame.width())
                              .arg(last_frame.height()));
    } else {
        details.push_back(tr("No video signal is currently available"));
    }

    if (!persistent_lines.empty()) {
        details.push_back(tr("%1 configured lines, %2 enabled")
                              .arg(static_cast<int>(persistent_lines.size()))
                              .arg(enabled_line_count(persistent_lines)));
    }
    if (!processing_overlays.empty()) {
        details.push_back(
            tr("%1 processing overlays")
                .arg(static_cast<int>(processing_overlays.size()))
        );
    }
    if (!events.isEmpty()) {
        details.push_back(tr("Recent motion or line event marker available"));
    }
    if (line_edit_preview_.has_value()) {
        QString edit_detail
            = tr("Editing line %1 with %2 points")
                  .arg(line_edit_preview_->template_name)
                  .arg(static_cast<int>(line_edit_preview_->pts_pct.size()));
        if (const auto selected = line_edit_interaction_.selected_vertex()) {
            edit_detail += tr(", point %1 selected").arg(*selected + 1);
        } else {
            edit_detail += tr(", whole shape selected");
        }
        details.push_back(edit_detail);
    } else if (drawing_enabled && active) {
        details.push_back(
            tr("Drawing a new line with %1 placed points; use arrow keys to "
               "move the cursor and Space to place a point")
                .arg(static_cast<int>(draft_line_points_pct.size()))
        );
    }

    const QString description = details.join(QStringLiteral(". "));
    if (accessibleDescription() == description) {
        return;
    }
    setAccessibleDescription(description);
    QAccessibleEvent accessibility_event(this, QAccessible::DescriptionChanged);
    QAccessible::updateAccessibility(&accessibility_event);
}

void stream_cell::on_close_clicked() { emit request_close(name); }

void stream_cell::on_focus_clicked() { emit request_focus(name); }

void stream_cell::on_rotate_clicked() { set_quarter_turns(quarter_turns_ + 1); }

void stream_cell::on_aspect_mode_clicked() {
    set_keep_frame_aspect_ratio(!keep_frame_aspect_ratio_);
}

void stream_cell::on_manual_size_decrease_clicked() {
    set_manual_size(manual_short_side() - manual_step);
}

void stream_cell::on_manual_size_increase_clicked() {
    set_manual_size(manual_short_side() + manual_step);
}
