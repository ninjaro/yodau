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
    , name_label(nullptr) {
    build_ui();
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    // setMinimumSize(120, 90);
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

void stream_cell::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);

    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);
    QWidget::paintEvent(event);

    p.drawRect(rect().adjusted(0, 0, -1, -1));
    p.setRenderHint(QPainter::Antialiasing, true);

    draw_persistent(p);
    draw_draft(p);
    draw_hover_point(p);
    draw_hover_coords(p);
    draw_preview_segment(p);
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

    name_label = new QLabel(name, this);
    name_label->setAlignment(Qt::AlignCenter);
    name_label->setAttribute(Qt::WA_TransparentForMouseEvents);
    root->addWidget(name_label, 1);

    // setLayout(root);

    connect(close_btn, &QPushButton::clicked, this, [this]() {
        emit request_close(name);
    });
    connect(focus_btn, &QPushButton::clicked, this, [this]() {
        emit request_focus(name);
    });
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
