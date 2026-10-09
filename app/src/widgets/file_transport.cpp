#include "widgets/file_transport.hpp"

#include "widgets/stream_cell.hpp"

#include <QComboBox>
#include <QCoreApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {

constexpr int seek_steps = 10000;

QString media_time(qint64 ms) {
    const auto seconds = std::max(qint64(0), ms) / 1000;
    return QStringLiteral("%1:%2:%3")
        .arg(seconds / 3600, 2, 10, QLatin1Char('0'))
        .arg((seconds / 60) % 60, 2, 10, QLatin1Char('0'))
        .arg(seconds % 60, 2, 10, QLatin1Char('0'));
}

} // namespace

file_transport::file_transport(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("file_transport"));
    setAccessibleName(tr("Active file playback"));
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    auto* buttons = new QHBoxLayout;
    play_ = new QPushButton(tr("Play"), this);
    play_->setObjectName(QStringLiteral("file_play"));
    restart_ = new QPushButton(tr("Restart"), this);
    restart_->setObjectName(QStringLiteral("file_restart"));
    restart_->setAccessibleName(tr("Restart file from beginning"));
    rate_ = new QComboBox(this);
    rate_->setObjectName(QStringLiteral("file_rate"));
    rate_->setAccessibleName(tr("File playback rate"));
    rate_->setToolTip(
        tr("Video playback multiplier; backend support varies. "
           "Does not change analysis sampling or guarantee every frame is "
           "processed.")
    );
    for (const double rate : { .25, .5, 1., 1.5, 2. }) {
        rate_->addItem(tr("%1×").arg(rate), rate);
    }
    buttons->addWidget(play_);
    buttons->addWidget(restart_);
    buttons->addWidget(rate_);
    buttons->addStretch();
    root->addLayout(buttons);
    auto* timeline = new QHBoxLayout;
    seek_ = new QSlider(Qt::Horizontal, this);
    seek_->setObjectName(QStringLiteral("file_seek"));
    seek_->setRange(0, seek_steps);
    seek_->setTracking(false);
    seek_->setSingleStep(10);
    seek_->setPageStep(100);
    seek_->setAccessibleName(tr("File playback position"));
    seek_->setToolTip(
        tr("Drag and release to seek; Escape cancels. Arrow keys "
           "and Home/End also seek. Available only for seekable files.")
    );
    seek_->installEventFilter(this);
    time_ = new QLabel(this);
    time_->setObjectName(QStringLiteral("file_time"));
    time_->setTextFormat(Qt::PlainText);
    timeline->addWidget(seek_, 1);
    timeline->addWidget(time_);
    root->addLayout(timeline);
    status_ = new QLabel(this);
    status_->setObjectName(QStringLiteral("file_status"));
    status_->setTextFormat(Qt::PlainText);
    status_->setWordWrap(true);
    root->addWidget(status_);
    auto* hint = new QLabel(
        tr("Playback only: analysis sampling is unchanged. "
           "Seeking/looping may trigger detector events."),
        this
    );
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::PlainText);
    root->addWidget(hint);

    connect(play_, &QPushButton::clicked, this, [this] {
        if (!can_play()) {
            return;
        }
        auto* player = cell_->player;
        if (player->playbackState() == QMediaPlayer::PlayingState) {
            player->pause();
        } else {
            player->play();
        }
        refresh();
    });
    connect(restart_, &QPushButton::clicked, this, [this] {
        if (can_play() && cell_->player->isSeekable()) {
            cancel_seek();
            cell_->player->setPosition(0);
            cell_->player->play();
        }
    });
    connect(rate_, &QComboBox::activated, this, [this](int index) {
        if (can_play() && index >= 0 && index < rate_->count()) {
            cell_->player->setPlaybackRate(rate_->itemData(index).toDouble());
            refresh();
        }
    });
    connect(seek_, &QSlider::valueChanged, this, [this](int value) {
        if (can_play() && cell_->player->isSeekable()
            && cell_->player->duration() > 0) {
            cell_->player->setPosition(seek_position(value));
        }
    });
    connect(seek_, &QSlider::sliderMoved, this, [this](int value) {
        if (cell_) {
            show_time(seek_position(value));
        }
    });
    hide();
}

void file_transport::set_stream(stream_cell* cell) {
    if (cell != nullptr && cell_ == cell) {
        refresh();
        return;
    }
    cancel_seek();
    for (const auto& connection : connections_) {
        disconnect(connection);
    }
    connections_.clear();
    cell_ = cell;
    if (cell_) {
        auto* player = cell_->player;
        const auto watch = [this, player](auto signal) {
            connections_.push_back(connect(player, signal, this, [this] {
                refresh();
            }));
        };
        watch(&QMediaPlayer::playbackStateChanged);
        watch(&QMediaPlayer::playbackRateChanged);
        watch(&QMediaPlayer::durationChanged);
        watch(&QMediaPlayer::positionChanged);
        watch(&QMediaPlayer::seekableChanged);
        watch(&QMediaPlayer::mediaStatusChanged);
        watch(&QMediaPlayer::errorOccurred);
        connections_.push_back(
            connect(player, &QMediaPlayer::sourceChanged, this, [this] {
                cancel_seek();
                refresh();
            })
        );
        connections_.push_back(connect(
            cell_, &QObject::destroyed, this, [this] { set_stream(nullptr); }
        ));
    }
    refresh();
}

bool file_transport::can_play() const {
    if (!cell_ || !cell_->is_active() || !cell_->application_active()
        || !cell_->player->source().isLocalFile()
        || !cell_->camera_id.isEmpty()) {
        return false;
    }
    const auto* player = cell_->player;
    const auto status = player->mediaStatus();
    return player->isAvailable() && player->error() == QMediaPlayer::NoError
        && status != QMediaPlayer::LoadingMedia
        && status != QMediaPlayer::NoMedia
        && status != QMediaPlayer::InvalidMedia;
}

qint64 file_transport::seek_position(int step) const {
    const qint64 duration
        = cell_ ? std::max(qint64(0), cell_->player->duration()) : 0;
    if (step >= seek_steps) {
        return duration;
    }
    return qint64(
        std::llround(
            static_cast<long double>(std::max(0, step)) * duration / seek_steps
        )
    );
}

void file_transport::show_time(qint64 position) {
    const qint64 duration = cell_ ? cell_->player->duration() : 0;
    time_->setText(tr("%1 / %2").arg(
        media_time(position),
        duration > 0 ? media_time(duration) : tr("unknown")
    ));
}

void file_transport::cancel_seek() {
    if (seek_->isSliderDown()) {
        const QSignalBlocker blocker(seek_);
        const int value = seek_->value();
        // Clear the native pressed handle, not only the sliderDown property.
        const QPointF position(seek_->rect().center());
        QMouseEvent release(
            QEvent::MouseButtonRelease, position,
            seek_->mapToGlobal(position.toPoint()), Qt::LeftButton,
            Qt::NoButton, Qt::NoModifier
        );
        QCoreApplication::sendEvent(seek_, &release);
        seek_->setSliderDown(false);
        seek_->setValue(value);
        seek_->setSliderPosition(value);
        seek_->releaseMouse();
    }
}

bool file_transport::eventFilter(QObject* watched, QEvent* event) {
    if (watched == seek_ && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape
        && seek_->isSliderDown()) {
        cancel_seek();
        refresh();
        return true;
    }
    return QWidget::eventFilter(watched, event);
}

void file_transport::refresh() {
    const bool visible = cell_ && cell_->is_active()
        && cell_->player->source().isLocalFile() && cell_->camera_id.isEmpty();
    setVisible(visible);
    if (!visible) {
        cancel_seek();
        return;
    }
    const auto* player = cell_->player;
    const bool enabled = can_play();
    const bool seekable = enabled && player->isSeekable();
    play_->setEnabled(enabled);
    play_->setText(
        player->playbackState() == QMediaPlayer::PlayingState ? tr("Pause")
                                                              : tr("Play")
    );
    play_->setAccessibleName(play_->text());
    restart_->setEnabled(seekable);
    rate_->setEnabled(enabled);
    const QSignalBlocker rate_blocker(rate_);
    rate_->setCurrentIndex(rate_->findData(player->playbackRate()));
    rate_->setPlaceholderText(
        tr("Backend rate: %1×").arg(player->playbackRate())
    );
    if (!seekable || player->duration() <= 0) {
        cancel_seek();
    }
    seek_->setEnabled(seekable && player->duration() > 0);
    if (!seek_->isSliderDown()) {
        const QSignalBlocker blocker(seek_);
        const int value = player->duration() > 0
            ? int(std::lround(
                  std::clamp(
                      double(player->position()) / double(player->duration()),
                      0., 1.
                  )
                  * seek_steps
              ))
            : 0;
        seek_->setValue(value);
        show_time(player->position());
    }
    QString text;
    if (!player->isAvailable()) {
        text = tr("File decoder unavailable");
    } else if (
        player->error() != QMediaPlayer::NoError
        || player->mediaStatus() == QMediaPlayer::InvalidMedia
    ) {
        text = tr("File unavailable: %1").arg(player->errorString());
    } else if (!cell_->application_active()) {
        text = tr("Playback suspended while the application is inactive");
    } else if (player->mediaStatus() == QMediaPlayer::LoadingMedia) {
        text = tr("Loading file…");
    } else if (player->mediaStatus() == QMediaPlayer::NoMedia) {
        text = tr("Waiting for file media");
    } else if (player->mediaStatus() == QMediaPlayer::EndOfMedia) {
        text = tr("Finished");
    } else if (
        player->mediaStatus() == QMediaPlayer::StalledMedia
        || player->mediaStatus() == QMediaPlayer::BufferingMedia
    ) {
        text = tr("Buffering file…");
    } else if (player->playbackState() == QMediaPlayer::PlayingState) {
        text = tr("Playing");
    } else if (player->playbackState() == QMediaPlayer::PausedState) {
        text = tr("Paused");
    } else {
        text = tr("Stopped");
    }
    if (!player->isSeekable()) {
        text += tr(" — seeking unavailable");
    }
    status_->setText(text);
}
