#ifndef YODAU_APP_SHELL_APP_LOG_HPP
#define YODAU_APP_SHELL_APP_LOG_HPP

#include <QColor>
#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

enum class app_log_area { add, streams, active };
enum class app_log_severity { debug, info, warning, error };
enum class app_log_mode { release, debug };

Q_DECLARE_METATYPE(app_log_mode)

struct app_log_entry {
    QDateTime timestamp;
    app_log_area area { app_log_area::active };
    app_log_severity severity { app_log_severity::info };
    QString subsystem;
    QString stream_name;
    QString line_name;
    QString algorithm_id;
    QString event_type;
    QString message;
    QString detail;
    QColor line_color;
};

Q_DECLARE_METATYPE(app_log_entry)

QString app_log_area_name(app_log_area area);
QString app_log_severity_name(app_log_severity severity);
QString app_log_mode_name(app_log_mode mode);
QString format_app_log_entry(app_log_mode mode, const app_log_entry& entry);

class app_log_buffer final : public QObject {
    Q_OBJECT

public:
    explicit app_log_buffer(QObject* parent = nullptr);

    const QVector<app_log_entry>& entries() const;
    QVector<app_log_entry> entries_for_area(app_log_area area) const;
    void append(app_log_entry entry);
    void clear();

signals:
    void entry_appended(app_log_entry entry);
    void cleared();

private:
    QVector<app_log_entry> entry_list;
};

#endif // YODAU_APP_SHELL_APP_LOG_HPP
