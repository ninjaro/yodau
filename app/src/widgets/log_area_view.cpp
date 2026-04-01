#include "widgets/log_area_view.hpp"

#include "shell/frontend_settings.hpp"
#include "widgets/log_toolbar_panel.hpp"

#include <QFont>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>

namespace {

QTextCharFormat format_with_color(
    const QColor& color, const bool bold = false, const bool italic = false
) {
    QTextCharFormat format;
    format.setForeground(color);
    format.setFontWeight(bold ? QFont::DemiBold : QFont::Normal);
    format.setFontItalic(italic);
    return format;
}

QColor severity_color(const frontend_log_severity severity) {
    switch (severity) {
    case frontend_log_severity::debug:
        return QColor(QStringLiteral("#9ca3af"));
    case frontend_log_severity::warning:
        return QColor(QStringLiteral("#f4a261"));
    case frontend_log_severity::error:
        return QColor(QStringLiteral("#e63946"));
    case frontend_log_severity::info:
    default:
        return QColor(QStringLiteral("#dce1de"));
    }
}

QColor event_color(const QString& event_type) {
    const QString normalized = event_type.trimmed().toLower();
    if (normalized == QStringLiteral("tripwire")) {
        return QColor(QStringLiteral("#e63946"));
    }
    if (normalized == QStringLiteral("roi")) {
        return QColor(QStringLiteral("#2a9d8f"));
    }
    if (normalized == QStringLiteral("motion")) {
        return QColor(QStringLiteral("#f4a261"));
    }
    return QColor(QStringLiteral("#adb5bd"));
}

class log_entry_highlighter final : public QSyntaxHighlighter {
public:
    explicit log_entry_highlighter(log_area_view* owner)
        : QSyntaxHighlighter(owner->document())
        , owner_(owner) {}

protected:
    void highlightBlock(const QString& text) override {
        if (owner_ == nullptr) {
            return;
        }

        const int block_index = currentBlock().blockNumber();
        const auto& entries = owner_->visible_entries();
        if (block_index < 0 || block_index >= entries.size()) {
            return;
        }

        const frontend_log_entry& entry = entries.at(block_index);
        const frontend_log_mode mode = owner_->active_mode();

        const qsizetype timestamp_end = text.indexOf(']');
        if (timestamp_end >= 0) {
            setFormat(
                0, static_cast<int>(timestamp_end + 1),
                format_with_color(QColor(QStringLiteral("#7f8c8d")), false, true)
            );
        }

        const QString severity_token = frontend_log_severity_name(entry.severity);
        const qsizetype severity_index = text.indexOf(severity_token);
        if (severity_index >= 0) {
            setFormat(
                static_cast<int>(severity_index),
                static_cast<int>(severity_token.size()),
                format_with_color(severity_color(entry.severity), true)
            );
        }

        highlight_token(
            text,
            mode == frontend_log_mode::debug
                ? QStringLiteral("stream=%1").arg(entry.stream_name)
                : entry.stream_name,
            format_with_color(QColor(QStringLiteral("#74c0fc")), true)
        );

        const QColor effective_line_color = entry.line_color.isValid()
            ? entry.line_color
            : QColor(QStringLiteral("#ff6b6b"));
        highlight_token(
            text,
            entry.line_name.isEmpty()
                ? QString()
                : QStringLiteral("line=%1").arg(entry.line_name),
            format_with_color(effective_line_color, true)
        );

        highlight_token(
            text,
            mode == frontend_log_mode::debug
                ? QStringLiteral("event=%1").arg(entry.event_type)
                : entry.event_type,
            format_with_color(event_color(entry.event_type), true)
        );

        highlight_token(
            text,
            entry.algorithm_id.isEmpty()
                ? QString()
                : QStringLiteral("alg=%1").arg(entry.algorithm_id),
            format_with_color(algorithm_badge_color(entry.algorithm_id), true)
        );
    }

private:
    void highlight_token(
        const QString& text, const QString& token, const QTextCharFormat& format
    ) {
        if (token.trimmed().isEmpty()) {
            return;
        }

        qsizetype start = text.indexOf(token);
        while (start >= 0) {
            setFormat(
                static_cast<int>(start), static_cast<int>(token.size()), format
            );
            start = text.indexOf(token, start + token.size());
        }
    }

    log_area_view* owner_ { nullptr };
};

} // namespace

log_area_view::log_area_view(
    const std::optional<frontend_log_area> area, QWidget* parent
)
    : QPlainTextEdit(parent)
    , log_area(area) {
    setReadOnly(true);
    log_highlighter_ = new log_entry_highlighter(this);
}

void log_area_view::set_log_toolbar(log_toolbar_panel* toolbar) {
    if (log_toolbar_widget == toolbar) {
        rebuild_from_toolbar();
        return;
    }

    if (log_toolbar_widget != nullptr) {
        disconnect(log_toolbar_widget, nullptr, this, nullptr);
    }

    log_toolbar_widget = toolbar;

    if (log_toolbar_widget != nullptr) {
        connect(
            log_toolbar_widget, &log_toolbar_panel::view_state_changed, this,
            [this]() { rebuild_from_toolbar(); }
        );
    }

    rebuild_from_toolbar();
}

bool log_area_view::append_entry(const frontend_log_entry& entry) const {
    if (log_area.has_value() && entry.area != *log_area) {
        return false;
    }

    if (log_toolbar_widget != nullptr
        && !log_toolbar_widget->entry_matches(entry)) {
        return false;
    }

    const auto self = const_cast<log_area_view*>(this);
    self->visible_entries_.push_back(entry);
    self->appendPlainText(
        format_frontend_log_entry(
            log_toolbar_widget != nullptr ? log_toolbar_widget->log_mode()
                                          : frontend_log_mode::release,
            entry
        )
    );
    return true;
}

void log_area_view::rebuild_from_toolbar() const {
    const auto self = const_cast<log_area_view*>(this);
    self->clear();
    self->visible_entries_.clear();

    if (log_toolbar_widget == nullptr) {
        return;
    }

    self->set_visible_entries(log_toolbar_widget->filtered_entries(log_area));
}

const QVector<frontend_log_entry>& log_area_view::visible_entries() const {
    return visible_entries_;
}

frontend_log_mode log_area_view::active_mode() const {
    return log_toolbar_widget != nullptr ? log_toolbar_widget->log_mode()
                                         : frontend_log_mode::release;
}

void log_area_view::set_visible_entries(
    const QVector<frontend_log_entry>& entries
) const {
    visible_entries_ = entries;

    QStringList lines;
    lines.reserve(visible_entries_.size());
    const frontend_log_mode mode = active_mode();

    for (const frontend_log_entry& entry : visible_entries_) {
        lines.push_back(format_frontend_log_entry(mode, entry));
    }

    const auto self = const_cast<log_area_view*>(this);
    self->setPlainText(lines.join('\n'));
}
