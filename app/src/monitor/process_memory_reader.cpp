#include "monitor/process_memory_reader.hpp"

#include <QFile>

qint64 yodau::monitor::read_process_rss_bytes() {
#if defined(__linux__)
    QFile file(QStringLiteral("/proc/self/status"));
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return -1;
    }

    while (!file.atEnd()) {
        const QByteArray line = file.readLine().trimmed();
        if (!line.startsWith("VmRSS:")) {
            continue;
        }

        const QList<QByteArray> parts = line.simplified().split(' ');
        if (parts.size() < 2) {
            return -1;
        }

        bool ok = false;
        const qint64 value_kib = parts.at(1).toLongLong(&ok);
        if (!ok || value_kib < 0) {
            return -1;
        }
        return value_kib * 1024;
    }
#endif
    return -1;
}
