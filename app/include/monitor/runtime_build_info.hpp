#ifndef YODAU_FRONTEND_MONITOR_RUNTIME_BUILD_INFO_HPP
#define YODAU_FRONTEND_MONITOR_RUNTIME_BUILD_INFO_HPP

#include <QString>
#include <QStringList>

namespace yodau::monitor {

QString runtime_build_id();
QStringList runtime_debug_flags();

} // namespace yodau::monitor

#endif // YODAU_FRONTEND_MONITOR_RUNTIME_BUILD_INFO_HPP
