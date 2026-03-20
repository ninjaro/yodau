#ifndef YODAU_FRONTEND_MONITOR_PROCESS_MEMORY_READER_HPP
#define YODAU_FRONTEND_MONITOR_PROCESS_MEMORY_READER_HPP

#include <QtTypes>

namespace yodau::monitor {

qint64 read_process_rss_bytes();

} // namespace yodau::monitor

#endif // YODAU_FRONTEND_MONITOR_PROCESS_MEMORY_READER_HPP
