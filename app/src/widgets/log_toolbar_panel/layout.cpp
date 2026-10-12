#include "widgets/log_toolbar_panel.hpp"

#include "shell/str_label.hpp"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <utility>

void log_toolbar_panel::build_ui() {
#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    const auto layout = new QHBoxLayout(this);
#else
    const auto layout = new QVBoxLayout(this);
#endif
    layout->setContentsMargins(0, 0, 0, 0);

    const auto label = new QLabel(str_label("log mode"), this);
    log_mode_combo = new QComboBox(this);
    log_mode_combo->setObjectName(QStringLiteral("settings_log_mode_combo"));
    log_mode_combo->addItem(str_label("release"));
    log_mode_combo->addItem(str_label("debug"));
    log_mode_combo->setCurrentIndex(
        current_log_mode == app_log_mode::release ? 0 : 1
    );

    const auto area_label = new QLabel(str_label("area"), this);
    log_area_filter_combo = new QComboBox(this);
    log_area_filter_combo->setObjectName(
        QStringLiteral("settings_log_area_filter_combo")
    );
    log_area_filter_combo->addItem(str_label("all"), -1);
    log_area_filter_combo->addItem(
        str_label("add"), static_cast<int>(app_log_area::add)
    );
    log_area_filter_combo->addItem(
        str_label("streams"), static_cast<int>(app_log_area::streams)
    );
    log_area_filter_combo->addItem(
        str_label("active"), static_cast<int>(app_log_area::active)
    );

    const auto severity_label = new QLabel(str_label("severity"), this);
    log_severity_filter_combo = new QComboBox(this);
    log_severity_filter_combo->setObjectName(
        QStringLiteral("settings_log_severity_filter_combo")
    );
    log_severity_filter_combo->addItem(str_label("all"), -1);
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::debug),
        static_cast<int>(app_log_severity::debug)
    );
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::info),
        static_cast<int>(app_log_severity::info)
    );
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::warning),
        static_cast<int>(app_log_severity::warning)
    );
    log_severity_filter_combo->addItem(
        app_log_severity_name(app_log_severity::error),
        static_cast<int>(app_log_severity::error)
    );

    const auto event_label = new QLabel(str_label("event"), this);
    log_event_filter_combo = new QComboBox(this);
    log_event_filter_combo->setObjectName(
        QStringLiteral("settings_log_event_filter_combo")
    );

    const auto line_label = new QLabel(str_label("line/region"), this);
    log_line_filter_combo = new QComboBox(this);
    log_line_filter_combo->setObjectName(
        QStringLiteral("settings_log_line_filter_combo")
    );

    const auto algorithm_label = new QLabel(str_label("algorithm"), this);
    log_algorithm_filter_combo = new QComboBox(this);
    log_algorithm_filter_combo->setObjectName(
        QStringLiteral("settings_log_algorithm_filter_combo")
    );

    const auto stream_label = new QLabel(str_label("stream"), this);
    log_stream_filter_combo = new QComboBox(this);
    log_stream_filter_combo->setObjectName(
        QStringLiteral("settings_log_stream_filter_combo")
    );

    const auto subsystem_label = new QLabel(str_label("subsystem"), this);
    log_subsystem_filter_combo = new QComboBox(this);
    log_subsystem_filter_combo->setObjectName(
        QStringLiteral("settings_log_subsystem_filter_combo")
    );

    const auto search_label = new QLabel(str_label("search"), this);
    log_search_filter_edit = new QLineEdit(this);
    log_search_filter_edit->setObjectName(
        QStringLiteral("settings_log_search_filter_edit")
    );
    log_search_filter_edit->setPlaceholderText(
        str_label("stream, line, event, message...")
    );
    log_search_filter_edit->setClearButtonEnabled(true);
    log_search_filter_edit->setMinimumWidth(180);

    copy_logs_btn = new QPushButton(str_label("copy logs"), this);
    copy_logs_btn->setObjectName(QStringLiteral("settings_copy_logs_button"));
    copy_summary_btn = new QPushButton(str_label("copy summary"), this);
    copy_summary_btn->setObjectName(
        QStringLiteral("settings_copy_summary_button")
    );
    save_logs_btn = new QPushButton(str_label("save logs"), this);
    save_logs_btn->setObjectName(QStringLiteral("settings_save_logs_button"));

#if defined(KC_ANDROID) || defined(Q_OS_ANDROID)
    layout->addWidget(label);
    layout->addWidget(log_mode_combo);
    layout->addWidget(area_label);
    layout->addWidget(log_area_filter_combo);
    layout->addWidget(severity_label);
    layout->addWidget(log_severity_filter_combo);
    layout->addWidget(event_label);
    layout->addWidget(log_event_filter_combo);
    layout->addWidget(line_label);
    layout->addWidget(log_line_filter_combo);
    layout->addWidget(algorithm_label);
    layout->addWidget(log_algorithm_filter_combo);
    layout->addWidget(stream_label);
    layout->addWidget(log_stream_filter_combo);
    layout->addWidget(subsystem_label);
    layout->addWidget(log_subsystem_filter_combo);
    layout->addWidget(search_label);
    layout->addWidget(log_search_filter_edit, 1);
    layout->addWidget(copy_logs_btn);
    layout->addWidget(copy_summary_btn);
    layout->addWidget(save_logs_btn);
#else

    const std::pair<QLabel*, QWidget*> fields[] {
        { label, log_mode_combo },
        { area_label, log_area_filter_combo },
        { severity_label, log_severity_filter_combo },
        { event_label, log_event_filter_combo },
        { line_label, log_line_filter_combo },
        { algorithm_label, log_algorithm_filter_combo },
        { stream_label, log_stream_filter_combo },
        { subsystem_label, log_subsystem_filter_combo },
        { search_label, log_search_filter_edit },
    };
    for (const auto& [title, input] : fields) {
        title->setBuddy(input);
        input->setAccessibleName(title->text());
        if (auto* combo = qobject_cast<QComboBox*>(input)) {
            combo->setSizeAdjustPolicy(
                QComboBox::AdjustToMinimumContentsLengthWithIcon
            );
            combo->setMinimumContentsLength(8);
        }
    }
    log_search_filter_edit->setMinimumWidth(0);
    auto* more = new QToolButton(this);
    more->setObjectName(QStringLiteral("log_more_filters"));
    more->setText(tr("More filters"));
    more->setCheckable(true);
    more->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    more->setArrowType(Qt::RightArrow);
    auto* mode = new QHBoxLayout;
    mode->addWidget(label);
    mode->addWidget(log_mode_combo);
    mode->addStretch();
    mode->addWidget(more);
    layout->addLayout(mode);

    auto* basic = new QGridLayout;
    basic->addWidget(area_label, 0, 0);
    basic->addWidget(log_area_filter_combo, 0, 1);
    basic->addWidget(severity_label, 0, 2);
    basic->addWidget(log_severity_filter_combo, 0, 3);
    basic->addWidget(stream_label, 1, 0);
    basic->addWidget(log_stream_filter_combo, 1, 1);
    basic->addWidget(search_label, 1, 2);
    basic->addWidget(log_search_filter_edit, 1, 3);
    basic->setColumnStretch(1, 1);
    basic->setColumnStretch(3, 1);
    layout->addLayout(basic);

    auto* advanced = new QWidget(this);
    advanced->setObjectName(QStringLiteral("log_advanced_filters"));
    auto* filters = new QFormLayout(advanced);
    filters->setContentsMargins(0, 0, 0, 0);
    filters->addRow(event_label, log_event_filter_combo);
    filters->addRow(line_label, log_line_filter_combo);
    filters->addRow(algorithm_label, log_algorithm_filter_combo);
    filters->addRow(subsystem_label, log_subsystem_filter_combo);
    layout->addWidget(advanced);
    advanced->hide();
    connect(more, &QToolButton::toggled, advanced, &QWidget::setVisible);
    connect(more, &QToolButton::toggled, this, [more](bool expanded) {
        more->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
    });
    connect(this, &log_toolbar_panel::view_state_changed, this, [this, more] {
        int active = 0;
        for (auto* combo :
             { log_event_filter_combo, log_line_filter_combo,
               log_algorithm_filter_combo, log_subsystem_filter_combo }) {
            active += !combo->currentData().toString().isEmpty();
        }
        more->setText(
            active > 0 ? tr("More filters (%1 active)").arg(active)
                       : tr("More filters")
        );
    });

    freeze_view_ = new QCheckBox(tr("Freeze view"), this);
    freeze_view_->setObjectName(QStringLiteral("log_freeze_view"));
    freeze_view_->setToolTip(
        tr("Freeze displayed entries, not collection or processing. "
           "Resume to show the current filtered buffer; old entries may be "
           "evicted.")
    );
    follow_latest_ = new QCheckBox(tr("Follow latest"), this);
    follow_latest_->setObjectName(QStringLiteral("log_follow_latest"));
    follow_latest_->setChecked(true);
    follow_latest_->setToolTip(tr("Scroll to the latest displayed entry."));
    auto* clear = new QPushButton(tr("Clear filters"), this);
    clear->setObjectName(QStringLiteral("log_clear_filters"));
    clear->setToolTip(
        tr("Reset log filters only; keep the mode and collected entries.")
    );
    auto* presentation = new QHBoxLayout;
    presentation->addWidget(freeze_view_);
    presentation->addWidget(follow_latest_);
    presentation->addStretch();
    presentation->addWidget(clear);
    layout->addLayout(presentation);
    auto* actions = new QHBoxLayout;
    actions->addWidget(copy_logs_btn);
    actions->addWidget(copy_summary_btn);
    actions->addWidget(save_logs_btn);
    actions->addStretch();
    layout->addLayout(actions);
    auto* state = new QLabel(tr("Collection and export remain live."), this);
    state->setObjectName(QStringLiteral("log_view_state"));
    state->setTextFormat(Qt::PlainText);
    state->setWordWrap(true);
    layout->addWidget(state);
    connect(
        freeze_view_, &QCheckBox::toggled, this, [this, state](bool frozen) {
            state->setText(
                frozen ? tr("View frozen; collection and export remain live.")
                       : tr("Collection and export remain live.")
            );
            emit presentation_changed();
        }
    );
    connect(follow_latest_, &QCheckBox::toggled, this, [this] {
        emit presentation_changed();
    });
    connect(
        clear, &QPushButton::clicked, this, &log_toolbar_panel::clear_filters
    );
#endif

    connect(
        log_mode_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
        this, &log_toolbar_panel::on_log_mode_changed
    );
    connect(
        log_area_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_area_filter_changed
    );
    connect(
        log_severity_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_severity_filter_changed
    );
    connect(
        log_event_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_event_filter_changed
    );
    connect(
        log_line_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_line_filter_changed
    );
    connect(
        log_algorithm_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_algorithm_filter_changed
    );
    connect(
        log_stream_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_stream_filter_changed
    );
    connect(
        log_subsystem_filter_combo,
        QOverload<int>::of(&QComboBox::currentIndexChanged), this,
        &log_toolbar_panel::on_log_subsystem_filter_changed
    );
    connect(
        log_search_filter_edit, &QLineEdit::textChanged, this,
        &log_toolbar_panel::on_log_search_filter_changed
    );
    connect(
        copy_logs_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::copy_logs_requested
    );
    connect(
        copy_summary_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::copy_summary_requested
    );
    connect(
        save_logs_btn, &QPushButton::clicked, this,
        &log_toolbar_panel::save_logs_requested
    );
}

bool log_toolbar_panel::view_paused() const {
    return freeze_view_ != nullptr && freeze_view_->isChecked();
}

bool log_toolbar_panel::follows_latest() const {
    return follow_latest_ == nullptr || follow_latest_->isChecked();
}

void log_toolbar_panel::set_follows_latest(bool follow) {
    if (follow_latest_ != nullptr) {
        follow_latest_->setChecked(follow);
    }
}

void log_toolbar_panel::clear_filters() {
    for (auto* combo : { log_area_filter_combo, log_severity_filter_combo,
                         log_event_filter_combo, log_line_filter_combo,
                         log_algorithm_filter_combo, log_stream_filter_combo,
                         log_subsystem_filter_combo }) {
        const QSignalBlocker blocker(combo);
        combo->setCurrentIndex(0);
    }
    current_log_area_filter = current_log_severity_filter = -1;
    current_log_event_filter.clear();
    current_log_line_filter.clear();
    current_log_algorithm_filter.clear();
    current_log_stream_filter.clear();
    current_log_subsystem_filter.clear();
    current_log_search_filter.clear();
    const QSignalBlocker blocker(log_search_filter_edit);
    log_search_filter_edit->clear();
    emit view_state_changed();
}
