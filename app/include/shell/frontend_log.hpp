#ifndef YODAU_FRONTEND_SHELL_FRONTEND_LOG_HPP
#define YODAU_FRONTEND_SHELL_FRONTEND_LOG_HPP

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

enum class frontend_log_area { add, streams, active };
enum class frontend_log_severity { debug, info, warning, error };
enum class frontend_log_mode { release, debug };

struct frontend_log_entry {
    QDateTime timestamp;
    frontend_log_area area { frontend_log_area::active };
    frontend_log_severity severity { frontend_log_severity::info };
    QString subsystem;
    QString stream_name;
    QString algorithm_id;
    QString message;
    QString detail;
};

Q_DECLARE_METATYPE(frontend_log_entry)

QString frontend_log_area_name(frontend_log_area area);
QString frontend_log_severity_name(frontend_log_severity severity);
QString format_frontend_log_entry(
    frontend_log_mode mode, const frontend_log_entry& entry
);

class frontend_log_buffer final : public QObject {
    Q_OBJECT

public:
    explicit frontend_log_buffer(QObject* parent = nullptr);

    const QVector<frontend_log_entry>& entries() const;
    QVector<frontend_log_entry> entries_for_area(frontend_log_area area) const;
    void append(frontend_log_entry entry);
    void clear();

signals:
    void entry_appended(frontend_log_entry entry);
    void cleared();

private:
    QVector<frontend_log_entry> entry_list;
};

#endif // YODAU_FRONTEND_SHELL_FRONTEND_LOG_HPP
