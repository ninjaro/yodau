#include "widgets/stream_cell.hpp"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QResizeEvent>
#include <QStyle>
#include <QStyleOptionSizeGrip>

#include <algorithm>
#include <cmath>

namespace {

// Paint the native grip without QSizeGrip's independent widget resizing.
class resize_grip final : public QWidget {
public:
    explicit resize_grip(QWidget* parent)
        : QWidget(parent) {
        setCursor(Qt::SizeFDiagCursor);
        setFocusPolicy(Qt::NoFocus);
        setMouseTracking(true);
        setLayoutDirection(Qt::LeftToRight);
        const int side
            = std::max(16, style()->pixelMetric(QStyle::PM_SizeGripSize));
        setFixedSize(side, side);
    }

protected:
    void paintEvent(QPaintEvent*) override {
        QStyleOptionSizeGrip option;
        option.initFrom(this);
        option.corner = Qt::BottomRightCorner;
        QPainter painter(this);
        style()->drawControl(QStyle::CE_SizeGrip, &option, &painter, this);
    }
};

} // namespace

void stream_cell::build_resize_handle() {
#if !defined(KC_ANDROID) && !defined(Q_OS_ANDROID)
    resize_handle_ = new resize_grip(this);
    resize_handle_->setObjectName(QStringLiteral("stream_cell_resize_handle"));
    resize_handle_->setAccessibleName(tr("Resize stream %1").arg(name));
    resize_handle_->setToolTip(
        tr("Drag to resize this thumbnail; Escape cancels. "
           "The + and - buttons also change its size.")
    );
    resize_handle_->setAccessibleDescription(resize_handle_->toolTip());
    resize_handle_->installEventFilter(this);
    update_resize_handle();
#endif
}

void stream_cell::update_resize_handle() {
    if (resize_handle_ != nullptr) {
        resize_handle_->setVisible(fixed_size_controls_visible_ && !active);
        place_resize_handle();
    }
}

void stream_cell::place_resize_handle() {
    if (resize_handle_ == nullptr) {
        return;
    }
    resize_handle_->move(
        std::max(0, width() - resize_handle_->width() - 1),
        std::max(0, height() - resize_handle_->height() - 1)
    );
    resize_handle_->raise();
}

bool stream_cell::resize_enabled() const {
    return resize_handle_ != nullptr && fixed_size_controls_visible_ && !active
        && application_active_ && isEnabled();
}

void stream_cell::begin_resize(const QPointF& global_pos) {
    if (resizing_ || !resize_enabled() || !resize_handle_->isVisible()) {
        return;
    }
    setFocus(Qt::MouseFocusReason);
    resize_origin_ = global_pos;
    resize_start_ = manual_short_side_;
    resize_aspect_ = effective_display_aspect();
    resizing_ = true;
    resize_handle_->grabMouse();
}

void stream_cell::update_resize(const QPointF& global_pos) {
    if (!resizing_) {
        return;
    }
    if (!resize_enabled() || resize_aspect_ != effective_display_aspect()) {
        end_resize(false);
        return;
    }
    // Project onto the aspect-preserving size vector. Use the press snapshot
    // and global delta: Packing can move/resize this cell during the gesture.
    const double x_scale = std::max(1.0, resize_aspect_);
    const double y_scale = std::max(1.0, 1.0 / resize_aspect_);
    const QPointF delta = global_pos - resize_origin_;
    const double change = (delta.x() * x_scale + delta.y() * y_scale)
        / (x_scale * x_scale + y_scale * y_scale);
    set_manual_size(
        resize_start_.value_or(manual_minimum) + std::round(change)
    );
}

void stream_cell::end_resize(const bool commit) {
    if (!resizing_) {
        return;
    }
    resizing_ = false;
    if (QWidget::mouseGrabber() == resize_handle_) {
        resize_handle_->releaseMouse();
    }
    if (!commit && manual_short_side_ != resize_start_) {
        manual_short_side_ = resize_start_;
        update_display_controls();
        emit layout_geometry_changed();
    }
}

void stream_cell::set_manual_size(const double short_side) {
    if (!std::isfinite(short_side)) {
        return;
    }
    const double next = std::max(manual_minimum, short_side);
    if (next == manual_short_side()) {
        return;
    }
    manual_short_side_ = next;
    update_display_controls();
    emit layout_geometry_changed();
}

bool stream_cell::eventFilter(QObject* watched, QEvent* event) {
    if (watched != resize_handle_) {
        return QWidget::eventFilter(watched, event);
    }
    switch (event->type()) {
    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() == Qt::LeftButton && resize_enabled()) {
            begin_resize(mouse->globalPosition());
            return true;
        }
        break;
    }
    case QEvent::MouseMove: {
        if (resizing_) {
            auto* mouse = static_cast<QMouseEvent*>(event);
            if (mouse->buttons() & Qt::LeftButton) {
                update_resize(mouse->globalPosition());
            } else {
                end_resize(false);
            }
            return true;
        }
        break;
    }
    case QEvent::MouseButtonRelease: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (resizing_ && mouse->button() == Qt::LeftButton) {
            update_resize(mouse->globalPosition());
            end_resize(true);
            return true;
        }
        break;
    }
    case QEvent::UngrabMouse:
    case QEvent::Hide:
        end_resize(false);
        break;
    default:
        break;
    }
    return QWidget::eventFilter(watched, event);
}

bool stream_cell::event(QEvent* event) {
    switch (event->type()) {
    case QEvent::Hide:
    case QEvent::FocusOut:
    case QEvent::WindowDeactivate:
    case QEvent::ParentAboutToChange:
        end_resize(false);
        break;
    case QEvent::EnabledChange:
        if (!isEnabled()) {
            end_resize(false);
        }
        break;
    default:
        break;
    }
    return QWidget::event(event);
}

void stream_cell::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    place_resize_handle();
}
