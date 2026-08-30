#include "monitor/runtime_build_info.hpp"

#include <QCoreApplication>

QString yodau::monitor::runtime_build_id() {
    const QString version = QCoreApplication::applicationVersion().trimmed();
#if defined(NDEBUG)
    const QString build_mode = QStringLiteral("release");
#else
    const QString build_mode = QStringLiteral("debug");
#endif
    return version.isEmpty() ? build_mode
                             : QStringLiteral("%1-%2").arg(version, build_mode);
}

QStringList yodau::monitor::runtime_debug_flags() {
#if defined(NDEBUG)
    return {};
#else
    return { QStringLiteral("debug_build"), QStringLiteral("monitor_opt_in") };
#endif
}
