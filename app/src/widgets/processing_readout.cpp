#include "widgets/processing_readout.hpp"

#include <QHideEvent>
#include <QLabel>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <cmath>
#include <utility>

namespace {

using shared_ui::qt::readout_freshness;
using shared_ui::qt::readout_item;

constexpr qint64 stale_after_ms = 3000;

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
    const auto age = processing_readout::tr("%1 s since GUI delivery")
                         .arg(static_cast<double>(age_ms) / 1000.0, 0, 'f', 1);
    return (age_ms >= stale_after_ms ? processing_readout::tr("Stale — %1")
                                     : processing_readout::tr("Recent — %1"))
        .arg(age);
}

readout_item value(const QString& label, const QString& text) {
    readout_item item;
    item.label = label;
    item.value = text;
    return item;
}

readout_item missing(const QString& label, const QString& reason) {
    readout_item item;
    item.label = label;
    item.detail = reason;
    item.available = false;
    return item;
}

// One GUI-delivery path: its own age decides its freshness; the other path
// and a retained summary never refresh it.
readout_item delivery(
    const QString& label, qint64 age_ms, double fps, int width, int height
) {
    auto item = value(
        label,
        processing_readout::tr("%1\n%2\n%3")
            .arg(sample_text(age_ms), rate_text(fps), size_text(width, height))
    );
    item.freshness = age_ms >= stale_after_ms ? readout_freshness::stale
                                              : readout_freshness::live;
    return item;
}

} // namespace

QList<QList<readout_item>>
processing_readout_items(const processing_sample& sample) {
    const auto& metrics = sample.metrics;
    const bool present = !sample.stream_name.isEmpty();
    const auto no_stream = processing_readout::tr("No active stream");
    const auto no_sample = processing_readout::tr("No sample");
    QList<readout_item> stream {
        present ? value(processing_readout::tr("Target"), sample.stream_name)
                : missing(processing_readout::tr("Target"), no_stream),
        present
            ? value(
                  processing_readout::tr("Source state"), sample.source_status
              )
            : missing(
                  processing_readout::tr("Source state"),
                  processing_readout::tr(
                      "Activate a visible stream to inspect it."
                  )
              ),
    };

    const auto input_label = processing_readout::tr("Input delivery");
    auto input = present && sample.input_age_ms >= 0
        ? delivery(
              input_label, sample.input_age_ms, metrics.input_fps,
              metrics.input_width, metrics.input_height
          )
        : missing(input_label, no_sample);
    // Unavailable processing and application suspension qualify the result
    // path; its retained observations stay historical, not current.
    const auto state = present && !sample.processing_enabled
        ? processing_readout::tr("Processing unavailable")
        : !sample.application_active
        ? processing_readout::tr("Inactive — observations retained")
        : QString();
    const auto result_label
        = processing_readout::tr("Processed-result delivery");
    auto result = present && sample.result_age_ms >= 0
        ? delivery(
              result_label, sample.result_age_ms, metrics.core_fps,
              metrics.processed_width, metrics.processed_height
          )
        : missing(result_label, no_sample);
    if (!state.isEmpty())
        result.detail = result.available ? state : state + u'\n' + no_sample;
    const auto summary_label = processing_readout::tr("Last reported summary");
    const auto no_summary
        = processing_readout::tr("No result details reported");
    // A summary has no age of its own; the newest callback does not date it.
    auto summary = present && sample.result_age_ms >= 0
            && !metrics.processing_summary.isEmpty()
        ? value(summary_label, metrics.processing_summary)
        : missing(summary_label, no_summary);
    QList<readout_item> observations { input, result, summary };

    const auto algorithm_label
        = processing_readout::tr("Configured algorithm / preset");
    const auto policy_label
        = processing_readout::tr("Targets, not measured throughput");
    const auto target = [](int fps) {
        return fps > 0 ? processing_readout::tr("%1 fps").arg(fps)
                       : processing_readout::tr("Unavailable");
    };
    QList<readout_item> policy {
        present ? value(
                      algorithm_label,
                      processing_readout::tr("%1 / %2").arg(
                          sample.algorithm_id, sample.preset_id
                      )
                  )
                : missing(algorithm_label, no_stream),
        present ? value(
                      policy_label,
                      processing_readout::tr(
                          "%1 policy\nRepaint target: %2\nAnalysis target: "
                          "%3\nProcessing pixel target: %4"
                      )
                          .arg(
                              metrics.manual_policy_active
                                  ? processing_readout::tr("Manual")
                                  : processing_readout::tr("Automatic"),
                              target(metrics.effective_display_fps),
                              target(metrics.effective_core_fps),
                              metrics.effective_processing_pixels > 0
                                  ? processing_readout::tr("%1 px").arg(
                                        metrics.effective_processing_pixels
                                    )
                                  : processing_readout::tr("Unavailable")
                          )
                  )
                : missing(policy_label, no_stream),
    };
    return { stream, observations, policy };
}

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
    for (const auto& [title, name] :
         { std::pair { tr("Active stream"), "processing_stream" },
           std::pair { tr("GUI observations"), "processing_observations" },
           std::pair { tr("Effective policy"), "processing_policy" } }) {
        auto* section = new shared_ui::qt::readout_group(content);
        section->setObjectName(QString::fromLatin1(name));
        section->set_variant(shared_ui::qt::readout_variant::list);
        section->set_density(shared_ui::qt::readout_density::detailed);
        section->set_text_selectable(true);
        section->set_title(title);
        rows->addWidget(section);
        sections_.append(section);
    }
    auto* limits = new QLabel(
        tr("Arrival rates use existing GUI-delivery samples, not "
           "decoded/display "
           "FPS or detector timings. Stale means no delivery for 3 s, not a "
           "diagnosed failure. No CPU/RAM utilisation or measured pipeline "
           "stages "
           "are available here."),
        content
    );
    limits->setObjectName(QStringLiteral("processing_limits"));
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
    set_presentation(
        shared_ui::qt::material::system,
#ifdef KC_KDE
        shared_ui::qt::platform_profile::desktop_kde
#else
        shared_ui::qt::platform_profile::desktop_qt
#endif
    );
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

void processing_readout::set_presentation(
    shared_ui::qt::material value, shared_ui::qt::platform_profile profile
) {
    for (auto* section : sections_) {
        section->set_presentation(value, profile);
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
    const auto groups
        = processing_readout_items(reader_ ? reader_() : processing_sample {});
    for (qsizetype index = 0; index < sections_.size(); ++index) {
        sections_[index]->set_items(groups[index]);
    }
}
