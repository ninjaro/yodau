#include "widgets/processing_readout.hpp"

#include <QFormLayout>
#include <QGroupBox>
#include <QHideEvent>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <utility>

namespace {

constexpr qint64 stale_after_ms = 3000;

void set_text(QLabel* label, const QString& text) {
    if (label->text() != text) {
        label->setText(text);
    }
}

QString rate_text(double fps) {
    return std::isfinite(fps) && fps > 0.0
        ? processing_readout::tr("%1 fps (arrival EMA)").arg(fps, 0, 'f', 1)
        : processing_readout::tr("Rate pending");
}

QString size_text(int width, int height) {
    return width > 0 && height > 0
        ? processing_readout::tr("%1 × %2 px").arg(width).arg(height)
        : processing_readout::tr("Size unavailable");
}

QString sample_text(qint64 age_ms) {
    if (age_ms < 0) {
        return processing_readout::tr("No sample");
    }
    const auto age = processing_readout::tr("%1 s since GUI delivery")
                         .arg(age_ms / 1000.0, 0, 'f', 1);
    return (age_ms >= stale_after_ms ? processing_readout::tr("Stale — %1")
                                     : processing_readout::tr("Recent — %1"))
        .arg(age);
}

} // namespace

processing_readout::processing_readout(QWidget* parent)
    : QWidget(parent) {
    setObjectName(QStringLiteral("processing_readout"));
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName(QStringLiteral("processing_scroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    content->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    auto* rows = new QVBoxLayout(content);
    const auto group = [content, rows](const QString& title) {
        auto* box = new QGroupBox(title, content);
        auto* form = new QFormLayout(box);
        form->setRowWrapPolicy(QFormLayout::WrapAllRows);
        rows->addWidget(box);
        return form;
    };
    const auto value = [](QFormLayout* form, const QString& title,
                          const QString& name) {
        auto* label = new QLabel(form->parentWidget());
        label->setObjectName(name);
        label->setTextFormat(Qt::PlainText);
        label->setWordWrap(true);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setTextInteractionFlags(
            Qt::TextSelectableByMouse | Qt::TextSelectableByKeyboard
        );
        label->setFocusPolicy(Qt::StrongFocus);
        label->setAccessibleName(title);
        form->addRow(title, label);
        if (auto* caption = qobject_cast<QLabel*>(form->labelForField(label))) {
            caption->setTextFormat(Qt::PlainText);
            caption->setWordWrap(true);
        }
        return label;
    };
    auto* context = group(tr("Active stream"));
    target_ = value(context, tr("Target"), "processing_target");
    source_ = value(context, tr("Source state"), "processing_source");
    auto* arrival = group(tr("GUI observations"));
    input_ = value(arrival, tr("Input delivery"), "processing_input");
    result_
        = value(arrival, tr("Processed-result delivery"), "processing_result");
    summary_
        = value(arrival, tr("Last reported summary"), "processing_summary");
    auto* policy = group(tr("Effective policy"));
    algorithm_ = value(
        policy, tr("Configured algorithm / preset"), "processing_algorithm"
    );
    policy_ = value(
        policy, tr("Targets, not measured throughput"), "processing_policy"
    );
    auto* limits = new QLabel(
        tr("Arrival rates use existing GUI-delivery samples, not "
           "decoded/display "
           "FPS or detector timings. Stale means no delivery for 3 s, not a "
           "diagnosed failure. No CPU/RAM utilisation or measured pipeline "
           "stages "
           "are available here."),
        content
    );
    limits->setTextFormat(Qt::PlainText);
    limits->setWordWrap(true);
    limits->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    rows->addWidget(limits);
    rows->addStretch();
    scroll->setWidget(content);
    layout->addWidget(scroll);
    timer_ = new QTimer(this);
    timer_->setObjectName(QStringLiteral("processing_refresh_timer"));
    timer_->setInterval(1000);
    connect(timer_, &QTimer::timeout, this, &processing_readout::refresh);
}

void processing_readout::set_reader(std::function<processing_sample()> reader) {
    reader_ = std::move(reader);
    timer_->stop();
    if (isVisible()) {
        refresh();
        if (reader_) {
            timer_->start();
        }
    }
}

void processing_readout::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    refresh();
    if (reader_) {
        timer_->start();
    }
}

void processing_readout::hideEvent(QHideEvent* event) {
    timer_->stop();
    QWidget::hideEvent(event);
}

void processing_readout::refresh() {
    if (!isVisible()) {
        return;
    }
    const auto sample = reader_ ? reader_() : processing_sample {};
    const auto& metrics = sample.metrics;
    const bool present = !sample.stream_name.isEmpty();
    set_text(target_, present ? sample.stream_name : tr("No active stream"));
    set_text(
        source_,
        present ? sample.source_status
                : tr("Activate a visible stream to inspect it.")
    );
    const auto input = present && sample.input_age_ms >= 0
        ? tr("%1\n%2\n%3")
              .arg(
                  sample_text(sample.input_age_ms),
                  rate_text(metrics.input_fps),
                  size_text(metrics.input_width, metrics.input_height)
              )
        : tr("No sample");
    set_text(input_, input);
    const auto result = present && sample.result_age_ms >= 0
        ? tr("%1\n%2\n%3")
              .arg(
                  sample_text(sample.result_age_ms),
                  rate_text(metrics.core_fps),
                  size_text(metrics.processed_width, metrics.processed_height)
              )
        : tr("No sample");
    const auto inactive = !sample.application_active
        ? tr("Inactive — observations retained\n")
        : QString();
    set_text(
        result_,
        (present && !sample.processing_enabled ? tr("Processing unavailable\n")
                                               : inactive)
            + result
    );
    set_text(
        summary_,
        present && sample.result_age_ms >= 0
            ? (metrics.processing_summary.isEmpty()
                   ? tr("No result details reported")
                   : metrics.processing_summary)
            : tr("No result details reported")
    );
    set_text(
        algorithm_,
        present ? tr("%1 / %2").arg(sample.algorithm_id, sample.preset_id)
                : tr("Unavailable")
    );
    const auto target = [](int fps) {
        return fps > 0 ? processing_readout::tr("%1 fps").arg(fps)
                       : processing_readout::tr("Unavailable");
    };
    set_text(
        policy_,
        present
            ? tr("%1 policy\nRepaint target: %2\nAnalysis target: "
                 "%3\nProcessing pixel target: %4")
                  .arg(
                      metrics.manual_policy_active ? tr("Manual")
                                                   : tr("Automatic"),
                      target(metrics.effective_display_fps),
                      target(metrics.effective_core_fps),
                      metrics.effective_processing_pixels > 0
                          ? tr("%1 px").arg(metrics.effective_processing_pixels)
                          : tr("Unavailable")
                  )
            : tr("Unavailable")
    );
}
