#include "widgets/stream_cell.hpp"

#include "shell/icon_loader.hpp"

#include <QAccessible>
#include <QHBoxLayout>
#include <QPushButton>
#include <QVBoxLayout>

void stream_cell::build_ui() {
    const auto root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    const auto top_row = new QHBoxLayout();
    top_row->setContentsMargins(0, 0, 0, 0);
    top_row->setSpacing(4);

    top_row->addStretch();

#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    constexpr int display_control_size = 48;
#else
    constexpr int display_control_size = 32;
#endif

    manual_size_decrease_btn = new QPushButton(QStringLiteral("-"), this);
    manual_size_decrease_btn->setObjectName(
        QStringLiteral("stream_cell_size_decrease_button")
    );
    manual_size_decrease_btn->setFixedSize(
        display_control_size, display_control_size
    );
    manual_size_decrease_btn->setToolTip(tr("Make stream cell smaller"));
    manual_size_decrease_btn->setAccessibleName(
        tr("Make stream %1 smaller").arg(name)
    );
    manual_size_decrease_btn->setFlat(true);
    top_row->addWidget(manual_size_decrease_btn);

    manual_size_increase_btn = new QPushButton(QStringLiteral("+"), this);
    manual_size_increase_btn->setObjectName(
        QStringLiteral("stream_cell_size_increase_button")
    );
    manual_size_increase_btn->setFixedSize(
        display_control_size, display_control_size
    );
    manual_size_increase_btn->setToolTip(tr("Make stream cell larger"));
    manual_size_increase_btn->setAccessibleName(
        tr("Make stream %1 larger").arg(name)
    );
    manual_size_increase_btn->setFlat(true);
    top_row->addWidget(manual_size_increase_btn);

    rotate_btn = new QPushButton(QStringLiteral("90\u00b0"), this);
    rotate_btn->setObjectName(QStringLiteral("stream_cell_rotate_button"));
    rotate_btn->setFixedSize(display_control_size, display_control_size);
    rotate_btn->setToolTip(tr("Rotate stream clockwise by 90 degrees"));
    rotate_btn->setAccessibleName(tr("Rotate stream %1 clockwise").arg(name));
    rotate_btn->setFlat(true);
    top_row->addWidget(rotate_btn);

    aspect_mode_btn = new QPushButton(tr("Fit"), this);
    aspect_mode_btn->setObjectName(
        QStringLiteral("stream_cell_aspect_mode_button")
    );
    aspect_mode_btn->setFixedHeight(display_control_size);
    aspect_mode_btn->setMinimumWidth(display_control_size + 12);
    aspect_mode_btn->setFlat(true);
    top_row->addWidget(aspect_mode_btn);

    focus_btn = new QPushButton(this);
    focus_btn->setObjectName(QStringLiteral("stream_cell_focus_button"));
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    focus_btn->setFixedSize(48, 48);
#else
    focus_btn->setFixedSize(32, 32);
#endif
    focus_btn->setIconSize(QSize(20, 20));
    focus_btn->setFlat(true);
    focus_btn->setFocusPolicy(Qt::StrongFocus);
    update_icon();
    top_row->addWidget(focus_btn);

    close_btn = new QPushButton(this);
    close_btn->setObjectName(QStringLiteral("stream_cell_close_button"));
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    close_btn->setFixedSize(48, 48);
#else
    close_btn->setFixedSize(32, 32);
#endif
    close_btn->setIconSize(QSize(20, 20));
    close_btn->setToolTip(tr("close"));
    close_btn->setAccessibleName(tr("Close stream %1").arg(name));
    close_btn->setAccessibleDescription(
        tr("Remove this stream from the visible grid")
    );
    close_btn->setFlat(true);
    close_btn->setFocusPolicy(Qt::StrongFocus);
    close_btn->setIcon(icon_loader::title_bar_close_icon());
    top_row->addWidget(close_btn);
    root->addLayout(top_row);
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
        close_btn, &QPushButton::clicked, this, &stream_cell::on_close_clicked
    );
    connect(
        focus_btn, &QPushButton::clicked, this, &stream_cell::on_focus_clicked
    );
    connect(
        rotate_btn, &QPushButton::clicked, this, &stream_cell::on_rotate_clicked
    );
    connect(
        aspect_mode_btn, &QPushButton::clicked, this,
        &stream_cell::on_aspect_mode_clicked
    );
    connect(
        manual_size_decrease_btn, &QPushButton::clicked, this,
        &stream_cell::on_manual_size_decrease_clicked
    );
    connect(
        manual_size_increase_btn, &QPushButton::clicked, this,
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
    if (!focus_btn) {
        return;
    }
    if (active) {
        focus_btn->setToolTip(tr("shrink"));
        focus_btn->setAccessibleName(
            tr("Return %1 to the stream grid").arg(name)
        );
        focus_btn->setAccessibleDescription(
            tr("Leave the enlarged stream view")
        );
        focus_btn->setIcon(icon_loader::title_bar_restore_icon());
    } else {
        focus_btn->setToolTip(tr("enlarge"));
        focus_btn->setAccessibleName(tr("Enlarge stream %1").arg(name));
        focus_btn->setAccessibleDescription(
            tr("Open this stream in the focused editor view")
        );
        focus_btn->setIcon(icon_loader::title_bar_maximize_icon());
    }
}

void stream_cell::update_display_controls() {
    if (rotate_btn != nullptr) {
        rotate_btn->setText(
            QStringLiteral("%1\u00b0").arg(quarter_turns_ * 90)
        );
        rotate_btn->setToolTip(
            tr("Current rotation: %1 degrees. Rotate clockwise by 90 degrees.")
                .arg(quarter_turns_ * 90)
        );
    }
    if (aspect_mode_btn != nullptr) {
        aspect_mode_btn->setVisible(active);
        aspect_mode_btn->setText(
            keep_frame_aspect_ratio_ ? tr("Stretch") : tr("Fit")
        );
        aspect_mode_btn->setToolTip(
            keep_frame_aspect_ratio_
                ? tr("Stretch the active frame to fill the cell")
                : tr("Keep the active frame's aspect ratio")
        );
        aspect_mode_btn->setAccessibleName(aspect_mode_btn->toolTip());
    }
    const bool show_manual = fixed_size_controls_visible_ && !active;
    if (manual_size_decrease_btn != nullptr) {
        manual_size_decrease_btn->setVisible(show_manual);
        manual_size_decrease_btn->setEnabled(
            manual_short_side() > manual_minimum
        );
    }
    if (manual_size_increase_btn != nullptr) {
        manual_size_increase_btn->setVisible(show_manual);
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
