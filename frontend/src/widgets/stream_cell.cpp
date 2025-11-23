#include "widgets/stream_cell.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QPushButton>
#include <QStyle>
#include <QStyleOption>
#include <QVBoxLayout>
#include <algorithm>

#include "helpers/icon_loader.hpp"

stream_cell::stream_cell(const QString& name, QWidget* parent)
    : QWidget(parent)
    , name(name)
    , close_btn(nullptr)
    , focus_btn(nullptr)
    , name_label(nullptr)
    , player(nullptr)
    , sink(nullptr)
    , camera(nullptr)
    , session(nullptr) {
    build_ui();
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // setMinimumSize(120, 90);
    repaint_timer.start();
}

const QString& stream_cell::get_name() const { return name; }

bool stream_cell::is_active() const { return active; }

std::vector<QPointF> stream_cell::draft_points_pct() const {
    return draft_line_points_pct;
}

bool stream_cell::draft_closed() const { return draft_line_closed; }

QString stream_cell::draft_name() const { return draft_line_name; }

QColor stream_cell::draft_color() const { return draft_line_color; }

bool stream_cell::is_draft_preview() const { return draft_preview; }

void stream_cell::set_active(const bool val) {
    if (active == val) {
        return;
    }
    active = val;
    if (!active) {
        set_drawing_enabled(false);
        clear_draft();
    }
    update_icon();
    update();
}

void stream_cell::set_drawing_enabled(const bool on) {
    drawing_enabled = on;
    if (!on) {
        hover_point_pct.reset();
    }
    setMouseTracking(drawing_enabled);
    update();
}

void stream_cell::set_draft_params(
    const QString& n, const QColor& color, const bool closed
) {
    draft_line_name = n;
    draft_line_color = color;
    draft_line_closed = closed;
    update();
}

void stream_cell::set_draft_points_pct(const std::vector<QPointF>& pts) {
    draft_line_points_pct = pts;
    update();
}

void stream_cell::clear_draft() {
    draft_line_points_pct.clear();
    hover_point_pct.reset();
    draft_preview = false;
    update();
}

void stream_cell::set_persistent_lines(
    const std::vector<line_instance>& lines
) {
    persistent_lines = lines;
    update();
}

void stream_cell::add_persistent_line(const line_instance& line) {
    persistent_lines.push_back(line);
    update();
}

void stream_cell::clear_persistent_lines() {
    persistent_lines.clear();
    update();
}

void stream_cell::set_draft_preview(const bool on) {
    draft_preview = on;
    update();
}

void stream_cell::set_labels_enabled(const bool on) {
    if (labels_enabled == on) {
        return;
    }
    labels_enabled = on;
    update();
}

void stream_cell::set_source(const QUrl& source) {
    if (!player) {
        return;
    }

    last_error.clear();
    last_frame = QImage();

    player->setSource(source);
    player->play();
}

void stream_cell::set_loop(const bool on) { loop_enabled = on; }

void stream_cell::set_camera_id(const QByteArray& id) {
    camera_id = id;

    last_error.clear();
    last_frame = QImage();

    if (player) {
        player->stop();
    }

    if (camera) {
        camera->stop();
        camera->deleteLater();
        camera = nullptr;
    }

    if (!session) {
        session = new QMediaCaptureSession(this);
        session->setVideoSink(sink);
    }

    QCameraDevice device;
    const auto cams = QMediaDevices::videoInputs();
    for (const auto& c : cams) {
        if (c.id() == id) {
            device = c;
            break;
        }
    }

    if (device.isNull()) {
        last_error = "camera not found";
        update();
        return;
    }

    camera = new QCamera(device, this);
    session->setCamera(camera);

    connect(
        camera, &QCamera::errorOccurred, this, &stream_cell::on_camera_error
    );

    camera->start();
}

void stream_cell::add_event(const QPointF& pos_pct, const QColor& color) {
    event_instance e;
    e.pos_pct = pos_pct;
    e.color = color;
    e.ts = QDateTime::currentDateTime();

    events.push_back(e);
    update();
}

void stream_cell::set_repaint_interval_ms(const int ms) {
    if (ms <= 0) {
        return;
    }
    repaint_interval_ms = ms;
}

void stream_cell::highlight_line(const QString& line_name) {
    if (line_name.isEmpty()) {
        return;
    }
    line_highlights[line_name] = QDateTime::currentDateTime();
    update();
}

void stream_cell::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);

    QPainter p(this);
    if (!last_frame.isNull()) {
        p.drawImage(rect(), last_frame);
    } else {
        QString txt = last_error.isEmpty() ? "no signal" : last_error;

        QRect r = rect().adjusted(6, 6, -6, -6);
        p.setPen(palette().color(QPalette::Text));
        p.drawText(r, Qt::AlignCenter, txt);
    }

    draw_stream_name(p);

    QWidget::paintEvent(event);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);

    p.drawRect(rect().adjusted(0, 0, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, true);

    draw_persistent(p);
    draw_draft(p);
    draw_hover_point(p);
    draw_hover_coords(p);
    draw_preview_segment(p);
    draw_events(p);
}

void stream_cell::mousePressEvent(QMouseEvent* event) {
    if (!drawing_enabled || !active) {
        QWidget::mousePressEvent(event);
        return;
    }

    auto* child = childAt(event->pos());
    if (child == close_btn || child == focus_btn) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() == Qt::LeftButton) {
        setFocus();
        draft_line_points_pct.push_back(to_pct(event->pos()));
        update();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void stream_cell::mouseMoveEvent(QMouseEvent* event) {
    if (!drawing_enabled || !active) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    hover_point_pct = to_pct(event->pos());
    update();
    event->accept();
}

void stream_cell::leaveEvent(QEvent* event) {
    hover_point_pct.reset();
    update();
    QWidget::leaveEvent(event);
}

void stream_cell::keyPressEvent(QKeyEvent* event) {
    if (!(drawing_enabled && active)) {
        QWidget::keyPressEvent(event);
        return;
    }

    const bool undo_key = (event->key() == Qt::Key_Backspace)
        || (event->key() == Qt::Key_Z
            && (event->modifiers() & Qt::ControlModifier));

    if (!undo_key) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (!draft_line_points_pct.empty()) {
        draft_line_points_pct.pop_back();
        hover_point_pct.reset();
        update();
    }

    event->accept();
}

void stream_cell::build_ui() {
    const auto root = new QVBoxLayout(this);
    root->setContentsMargins(6, 6, 6, 6);
    root->setSpacing(6);

    const auto top_row = new QHBoxLayout();
    top_row->setContentsMargins(0, 0, 0, 0);
    top_row->setSpacing(4);

    top_row->addStretch();

    focus_btn = new QPushButton(this);
    focus_btn->setFixedSize(24, 24);
    focus_btn->setIconSize(QSize(16, 16));
    focus_btn->setFlat(true);
    focus_btn->setFocusPolicy(Qt::NoFocus);
    update_icon();
    top_row->addWidget(focus_btn);

    close_btn = new QPushButton(this);
    close_btn->setFixedSize(24, 24);
    close_btn->setIconSize(QSize(16, 16));
    close_btn->setToolTip(tr("close"));
    close_btn->setFlat(true);
    close_btn->setFocusPolicy(Qt::NoFocus);

#if defined(KC_KDE)
    close_btn->setIcon(
        icon_loader::themed(
            { "window-close", "dialog-close", "edit-delete" },
            QStyle::SP_TitleBarCloseButton
        )
    );
#else
    close_btn->setIcon(
        icon_loader::themed(
            { "window-close", "dialog-close" }, QStyle::SP_TitleBarCloseButton
        )
    );
#endif
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
    // player->setSource(QUrl::fromLocalFile("/home/yarro/Pictures/kino/1080.mp4"));
    // player->play();

    connect(close_btn, &QPushButton::clicked, this, [this]() {
        emit request_close(name);
    });
    connect(focus_btn, &QPushButton::clicked, this, [this]() {
        emit request_focus(name);
    });
    connect(
        player, &QMediaPlayer::mediaStatusChanged, this,
        &stream_cell::on_media_status_changed
    );
    connect(
        player, &QMediaPlayer::errorOccurred, this,
        &stream_cell::on_player_error
    );
}

void stream_cell::update_icon() {
    if (!focus_btn) {
        return;
    }
    if (active) {
        focus_btn->setToolTip(tr("shrink"));
#if defined(KC_KDE)
        focus_btn->setIcon(
            icon_loader::themed(
                { "view-restore", "window-restore", "transform-scale" },
                QStyle::SP_TitleBarNormalButton
            )
        );
#else
        focus_btn->setIcon(
            icon_loader::themed(
                { "view-restore", "window-restore" },
                QStyle::SP_TitleBarNormalButton
            )
        );
#endif
    } else {
        focus_btn->setToolTip(tr("enlarge"));
#if defined(KC_KDE)
        focus_btn->setIcon(
            icon_loader::themed(
                { "view-fullscreen", "window-maximize", "transform-scale" },
                QStyle::SP_TitleBarMaxButton
            )
        );
#else
        focus_btn->setIcon(
            icon_loader::themed(
                { "view-fullscreen", "fullscreen", "window-maximize" },
                QStyle::SP_TitleBarMaxButton
            )
        );
#endif
    }
}

void stream_cell::draw_poly_with_points(
    QPainter& p, const std::vector<QPointF>& pts_pct, const QColor& color,
    bool closed, Qt::PenStyle style, qreal width
) const {
    if (pts_pct.size() < 2) {
        return;
    }

    QPen pen(color);
    pen.setWidthF(width);
    pen.setStyle(style);
    p.setPen(pen);

    QPolygonF poly;
    poly.reserve(static_cast<int>(pts_pct.size()));
    for (const auto& pt_pct : pts_pct) {
        poly << to_px(pt_pct);
    }

    if (closed && poly.size() >= 3) {
        p.drawPolygon(poly);
    } else {
        p.drawPolyline(poly);
    }

    for (const auto& pt_px : poly) {
        p.drawEllipse(pt_px, 3.0, 3.0);
    }
}

void stream_cell::draw_persistent(QPainter& p) const {
    for (const auto& l : persistent_lines) {
        draw_poly_with_points(
            p, l.pts_pct, l.color, l.closed, Qt::SolidLine, 2.0
        );

        if (!(active && labels_enabled)) {
            continue;
        }

        const auto text = l.template_name.trimmed();
        if (text.isEmpty()) {
            continue;
        }

        p.setPen(l.color);
        p.drawText(label_pos_px(l), text);
    }
}

void stream_cell::draw_draft(QPainter& p) const {
    if (draft_line_points_pct.empty()) {
        return;
    }

    draw_poly_with_points(
        p, draft_line_points_pct, draft_line_color, draft_line_closed,
        Qt::DashLine, 2.0
    );
}

void stream_cell::draw_hover_point(QPainter& p) const {
    if (!hover_point_pct.has_value()) {
        return;
    }

    QPen hpen(draft_line_color);
    hpen.setWidthF(1.0);
    hpen.setStyle(Qt::DashLine);
    p.setPen(hpen);

    p.drawEllipse(to_px(*hover_point_pct), 4.0, 4.0);
}

void stream_cell::draw_hover_coords(QPainter& p) const {
    if (!(hover_point_pct.has_value() && drawing_enabled && active)) {
        return;
    }

    const auto& hp = *hover_point_pct;
    QString txt
        = QString("x=%1  y=%2").arg(hp.x(), 0, 'f', 1).arg(hp.y(), 0, 'f', 1);

    QRect r = rect().adjusted(6, 6, -6, -6);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(r, Qt::AlignLeft | Qt::AlignBottom, txt);
}

void stream_cell::draw_preview_segment(QPainter& p) const {
    if (!(drawing_enabled && active && hover_point_pct.has_value()
          && !draft_line_points_pct.empty())) {
        return;
    }

    QPen pen(draft_line_color);
    pen.setWidthF(1.5);
    pen.setStyle(Qt::DashLine);
    p.setPen(pen);

    const QPointF last_px = to_px(draft_line_points_pct.back());
    const QPointF hover_px = to_px(*hover_point_pct);

    p.drawLine(last_px, hover_px);

    if (draft_line_closed && draft_line_points_pct.size() >= 2) {
        const QPointF first_px = to_px(draft_line_points_pct.front());
        p.drawLine(hover_px, first_px);
    }
}

void stream_cell::draw_stream_name(QPainter& p) const {
    if (name.isEmpty()) {
        return;
    }

    QRect r = rect().adjusted(6, 6, -6, -6);
    p.setPen(palette().color(QPalette::Text));
    p.drawText(r, Qt::AlignLeft | Qt::AlignTop, name);
}

QPointF stream_cell::label_pos_px(const line_instance& l) const {
    if (l.pts_pct.empty()) {
        return {};
    }

    const QPointF anchor_pct = l.closed ? l.pts_pct.back() : l.pts_pct.front();
    const QPointF anchor_px = to_px(anchor_pct);
    return { anchor_px.x() + 6.0, anchor_px.y() + 14.0 };
}

QPointF stream_cell::to_pct(const QPointF& pos_px) const {
    if (width() <= 0 || height() <= 0) {
        return {};
    }

    float x
        = static_cast<float>(pos_px.x()) / static_cast<float>(width()) * 100.0f;
    float y = static_cast<float>(pos_px.y()) / static_cast<float>(height())
        * 100.0f;

    x = std::clamp(x, 0.f, 100.f);
    y = std::clamp(y, 0.f, 100.f);

    return { x, y };
}

QPointF stream_cell::to_px(const QPointF& pos_pct) const {
    return { pos_pct.x() / 100.0 * width(), pos_pct.y() / 100.0 * height() };
}

void stream_cell::draw_events(QPainter& p) {
    const auto now = QDateTime::currentDateTime();
    const int ttl_ms = 2000;

    const QRect r = rect();
    const double w = static_cast<double>(r.width());
    const double h = static_cast<double>(r.height());
    const double base = std::min(w, h);
    const double radius = base * 0.04;

    QVector<event_instance> alive;
    alive.reserve(events.size());

    for (const auto& e : events) {
        const int age = static_cast<int>(e.ts.msecsTo(now));
        if (age >= ttl_ms) {
            continue;
        }

        alive.push_back(e);

        const double k = 1.0 - static_cast<double>(age) / ttl_ms;
        int a = static_cast<int>(120.0 * k);
        if (a < 0) {
            a = 0;
        }

        QColor c = e.color;
        c.setAlpha(a);

        const double x = r.left() + w * (e.pos_pct.x() / 100.0);
        const double y = r.top() + h * (e.pos_pct.y() / 100.0);

        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawEllipse(QPointF(x, y), radius, radius);
    }

    events = std::move(alive);
}

void stream_cell::on_frame_changed(const QVideoFrame& frame) {
    if (!frame.isValid()) {
        return;
    }

    QVideoFrame copy(frame);
    if (!copy.map(QVideoFrame::ReadOnly)) {
        return;
    }

    last_frame = copy.toImage();
    copy.unmap();

    if (!repaint_timer.isValid()) {
        repaint_timer.start();
        update();
        return;
    }

    if (repaint_timer.elapsed() < repaint_interval_ms) {
        return;
    }

    repaint_timer.restart();
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
    if (!player) {
        return;
    }

    player->setPosition(0);
    player->play();
}

void stream_cell::on_player_error(
    const QMediaPlayer::Error error, const QString& error_string
) {
    Q_UNUSED(error);
    last_error = error_string;
    update();
}

void stream_cell::on_camera_error(const QCamera::Error error) {
    Q_UNUSED(error);

    if (!camera) {
        return;
    }

    last_error = camera->errorString();
    update();
}
