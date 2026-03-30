#include "shell/stream_route_state.hpp"

#include <QUrl>

namespace stream_route_state_support {

QString trimmed_name(const QString& value) { return value.trimmed(); }

} // namespace stream_route_state_support

stream_route_state::add_source_validation
stream_route_state::validate_add_source(
    const QString& source, const QString& type
) {
    if (type != QStringLiteral("url")) {
        return {};
    }

    const QUrl url(source);
    const QString scheme = url.scheme().toLower();

    if (!url.isValid() || scheme.isEmpty()) {
        return add_source_validation {
            .valid = false,
            .message = QStringLiteral("invalid url"),
            .detail = source,
        };
    }

    if (scheme != QStringLiteral("rtsp") && scheme != QStringLiteral("http")
        && scheme != QStringLiteral("https")) {
        return add_source_validation {
            .valid = false,
            .message = QStringLiteral("unsupported url scheme"),
            .detail = scheme,
        };
    }

    return {};
}

QString stream_route_state::source_description(
    const QString& source, const QString& type
) {
    return QStringLiteral("%1:%2").arg(type, source);
}

bool stream_route_state::has_active_stream() const {
    return !active_stream_name_.isEmpty();
}

const QString& stream_route_state::active_stream_name() const {
    return active_stream_name_;
}

bool stream_route_state::is_active_stream(const QString& stream_name) const {
    return active_stream_name_ == normalized_stream_name(stream_name);
}

void stream_route_state::set_active_stream(const QString& stream_name) {
    active_stream_name_ = normalized_stream_name(stream_name);
}

void stream_route_state::clear_active_stream() { active_stream_name_.clear(); }

bool stream_route_state::hide_stream(const QString& stream_name) {
    if (!is_active_stream(stream_name)) {
        return false;
    }

    clear_active_stream();
    return true;
}

QString
stream_route_state::next_active_stream_for_enlarge(
    const QString& stream_name
) const {
    const QString normalized_name = normalized_stream_name(stream_name);
    if (normalized_name.isEmpty()) {
        return active_stream_name_;
    }

    return active_stream_name_ == normalized_name ? QString() : normalized_name;
}

QString stream_route_state::normalized_stream_name(const QString& stream_name) {
    return stream_route_state_support::trimmed_name(stream_name);
}
