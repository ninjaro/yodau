#include "widgets/stream_cell.hpp"

#include <QCoreApplication>
#include <QPermissions>
#include <algorithm>
#include <cmath>

QString stream_cell::source_status() const {
    if (!application_active_) {
        return tr("Inactive — source suspended");
    }
    if (!last_error.isEmpty()) {
        return tr("Source error: %1").arg(last_error);
    }
    if (!camera_id.isEmpty()) {
        return camera != nullptr && camera->isActive() ? tr("Camera active")
                                                       : tr("Camera inactive");
    }
    if (player == nullptr) {
        return tr("Player unavailable");
    }
    if (player->source().isEmpty()) {
        return tr("No source");
    }
    switch (player->mediaStatus()) {
    case QMediaPlayer::InvalidMedia:
        return tr("Invalid media");
    case QMediaPlayer::NoMedia:
        return tr("No media");
    case QMediaPlayer::LoadingMedia:
        return tr("Loading media");
    case QMediaPlayer::StalledMedia:
        return tr("Media stalled");
    case QMediaPlayer::BufferingMedia:
        return tr("Buffering media");
    case QMediaPlayer::EndOfMedia:
        return tr("End of media");
    default:
        break;
    }
    switch (player->playbackState()) {
    case QMediaPlayer::PlayingState:
        return tr("Playing");
    case QMediaPlayer::PausedState:
        return tr("Paused");
    case QMediaPlayer::StoppedState:
        return tr("Stopped");
    }
    return tr("Source state unavailable");
}

void stream_cell::set_source(const QUrl& source) {
    if (!player) {
        return;
    }

    // A file/URL binding replaces capture, including its activation intent.
    camera_id.clear();
    resume_camera_on_activation_ = false;
    if (camera != nullptr) {
        camera->stop();
        camera->deleteLater();
        camera = nullptr;
    }
    if (session != nullptr) {
        session->setCamera(nullptr);
        session->setVideoSink(nullptr);
    }
    player->setVideoOutput(sink);

    last_error.clear();
    last_frame = QImage();
    if (std::abs(source_frame_aspect_ - 1.5) > 1e-3) {
        source_frame_aspect_ = 1.5;
        emit source_frame_aspect_changed(source_frame_aspect_);
        emit layout_geometry_changed();
    }
    update_accessible_description();

    player->setSource(source);
    player->setPlaybackRate(1.0);
    resume_player_on_activation_ = !source.isEmpty();
    if (application_active_ && resume_player_on_activation_) {
        player->play();
    }
}

void stream_cell::set_loop(const bool on) { loop_enabled = on; }

void stream_cell::set_camera_id(const QByteArray& id) {
    camera_id = id;

    last_error.clear();
    last_frame = QImage();
    if (std::abs(source_frame_aspect_ - 1.5) > 1e-3) {
        source_frame_aspect_ = 1.5;
        emit source_frame_aspect_changed(source_frame_aspect_);
        emit layout_geometry_changed();
    }
    update_accessible_description();

    if (player) {
        player->setSource({});
    }
    resume_player_on_activation_ = false;

    if (camera) {
        camera->stop();
        camera->deleteLater();
        camera = nullptr;
    }

    resume_camera_on_activation_ = !camera_id.isEmpty();
    if (!application_active_ || camera_id.isEmpty()) {
        return;
    }
    start_camera_if_permitted();
}

void stream_cell::set_application_active(const bool active_value) {
    if (application_active_ == active_value) {
        return;
    }
    end_resize(false);
    application_active_ = active_value;

    if (!application_active_) {
        resume_player_on_activation_ = player != nullptr
            && player->playbackState() == QMediaPlayer::PlayingState;
        if (player != nullptr && resume_player_on_activation_) {
            player->pause();
        }
        resume_camera_on_activation_ = camera != nullptr && camera->isActive();
        if (camera != nullptr && camera->isActive()) {
            camera->stop();
        }
        return;
    }

    if (player != nullptr && resume_player_on_activation_
        && !player->source().isEmpty()) {
        player->play();
    }
    if (!camera_id.isEmpty() && (resume_camera_on_activation_ || !camera)) {
        start_camera_if_permitted();
    }
}

void stream_cell::start_camera_if_permitted() {
#if defined(Q_OS_ANDROID)
    auto* application = QCoreApplication::instance();
    if (application == nullptr) {
        last_error = tr("camera permission is unavailable");
        update_accessible_description();
        update();
        return;
    }
    const QCameraPermission camera_permission;
    const Qt::PermissionStatus permission_status
        = application->checkPermission(camera_permission);
    if (permission_status == Qt::PermissionStatus::Undetermined) {
        application->requestPermission(
            camera_permission, this, [this](const QPermission& permission) {
                if (permission.status() == Qt::PermissionStatus::Granted
                    && application_active_) {
                    start_camera();
                    return;
                }
                last_error = tr("camera permission denied");
                update_accessible_description();
                update();
            }
        );
        return;
    }
    if (permission_status != Qt::PermissionStatus::Granted) {
        last_error = tr("camera permission denied");
        update_accessible_description();
        update();
        return;
    }
#endif
    start_camera();
}

void stream_cell::start_camera() {
    if (!application_active_ || camera_id.isEmpty()) {
        return;
    }
    if (camera != nullptr) {
        camera->start();
        resume_camera_on_activation_ = true;
        return;
    }

    if (!session) {
        session = new QMediaCaptureSession(this);
        session->setVideoSink(sink);
    }

    QCameraDevice device;
    const auto cams = QMediaDevices::videoInputs();
    for (const auto& c : cams) {
        if (c.id() == camera_id) {
            device = c;
            break;
        }
    }

    if (device.isNull()) {
        last_error = tr("camera not found");
        update_accessible_description();
        update();
        return;
    }

    camera = new QCamera(device, this);
    session->setCamera(camera);

    connect(
        camera, &QCamera::errorOccurred, this, &stream_cell::on_camera_error
    );

    camera->start();
    resume_camera_on_activation_ = true;
}

void stream_cell::on_frame_changed(const QVideoFrame& frame) {
    if (!frame.isValid()) {
        return;
    }
    if (repaint_timer.isValid()
        && repaint_timer.elapsed() < repaint_interval_ms) {
        return;
    }

    QVideoFrame copy(frame);
    if (!copy.map(QVideoFrame::ReadOnly)) {
        return;
    }

    last_frame = copy.toImage();
    copy.unmap();
    if (!last_frame.isNull() && last_frame.height() > 0) {
        const double next_aspect = static_cast<double>(last_frame.width())
            / static_cast<double>(last_frame.height());
        const double tolerance
            = std::max(1e-3, std::abs(source_frame_aspect_) * 1e-3);
        if (std::isfinite(next_aspect) && next_aspect > 0.0
            && std::abs(next_aspect - source_frame_aspect_) > tolerance) {
            end_resize(false);
            source_frame_aspect_ = next_aspect;
            emit source_frame_aspect_changed(source_frame_aspect_);
            emit layout_geometry_changed();
        }
    }
    update_accessible_description();

    emit frame_ready(name, last_frame);

    if (!repaint_timer.isValid()) {
        repaint_timer.start();
    } else {
        repaint_timer.restart();
    }
    update();
}

void stream_cell::on_media_status_changed(
    const QMediaPlayer::MediaStatus status
) {
    if (!loop_enabled) {
        return;
    }
    if (status != QMediaPlayer::EndOfMedia) {
        return;
    }
    if (!player || player->source().isEmpty()) {
        return;
    }

    player->setPosition(0);
    if (application_active_) {
        player->play();
    }
}

void stream_cell::on_player_error(
    const QMediaPlayer::Error error, const QString& error_string
) {
    Q_UNUSED(error);
    last_error = error_string;
    update_accessible_description();
    update();
}

void stream_cell::on_camera_error(const QCamera::Error error) {
    Q_UNUSED(error);

    if (!camera) {
        return;
    }

    last_error = camera->errorString();
    update_accessible_description();
    update();
}
