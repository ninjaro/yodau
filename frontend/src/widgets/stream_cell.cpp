#include "widgets/stream_cell.hpp"

#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

#include "helpers/icon_loader.hpp"

stream_cell::stream_cell(const QString& name, QWidget* parent)
    : QWidget(parent)
    , name(name)
    , close_btn(nullptr)
    , focus_btn(nullptr)
    , name_label(nullptr) {
    build_ui();
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    // setMinimumSize(120, 90);
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

const QString& stream_cell::get_name() const { return name; }

bool stream_cell::is_active() const { return active; }

void stream_cell::set_active(const bool val) {
    if (active == val) {
        return;
    }
    active = val;
    update_icon();
    update();
}

void stream_cell::paintEvent(QPaintEvent* event) {
    QStyleOption opt;
    opt.initFrom(this);
    QPainter p(this);
    style()->drawPrimitive(QStyle::PE_Widget, &opt, &p, this);

    // if (active) {
    //     QColor hl = palette().color(QPalette::Highlight);
    //     hl.setAlpha(40);
    //     p.fillRect(rect(), hl);
    // }

    QWidget::paintEvent(event);

    p.drawRect(rect().adjusted(0, 0, -1, -1));
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
