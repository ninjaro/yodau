#ifndef YODAU_APP_MONITOR_RUNTIME_BUILD_INFO_HPP
#define YODAU_APP_MONITOR_RUNTIME_BUILD_INFO_HPP

#include <QString>
#include <QStringList>

namespace yodau::monitor {

QString runtime_build_id();
QStringList runtime_debug_flags();

} // namespace yodau::monitor

#endif // YODAU_APP_MONITOR_RUNTIME_BUILD_INFO_HPP
